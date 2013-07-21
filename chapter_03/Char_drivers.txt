1. Major and minor numbers:
Major number identifies the driver associated with the device. Modern linux kernels allow multiple drivers to share majors, but most devices are still organized on the one-major-one-driver principle.
The minor number is used as an index into a local array of devices. 

• dev_t type: defined in <linux/types.h>, used to hold device numbers - both major and minor numbers.
MAJOR(dev_t dev);
MINOR(dev_t dev);
MKDEV(int major, int minor);

2. Allocating and freeing device numbers:
One of the first things the driver need to do when setting up a char device is to obtain one or more device numbers. The function is declared in <linux/fs.h>:
	int register_chrdev_region(dev_t first, unsigned int count, char* name);
	@first - the beginning device number of the range you would like to allocate.
	@count - total number of contiguous device numbers you are requesting.
	@name - the name of the device that should be associated with this number range, it will appear in /proc/devices and sysfs.
Note that register_chrdev_region() works fine if you know the major number in advance; otherwise, you should use dynamicaly-allocated device numbers:
	int alloc_chrdev_region(dev_t* dev, unsigned int firstminor, unsigned int count, char* name);
	@dev - if allocate successfully, dev holds the first number in the allocated range.
	@firstminor - requested first minor number to use; it is usually 0.
	@count - same
	@name - same
Device numbers allocated in both ways should be freed using:
	void unregister_chrdev_region(dev_t first, unsigned int count);

Note that before a user-space program can access one of those device numbers, the driver needs to connect them to its internal functions that implement the device's operations.
• The best way to assign major numbers is by defaulting to dynamic allocation while providing the option of specifying the major number at load time or compile time. For example: scull_major, a global variable, which is initialized to SCULL_MAJOR (default 0, which means use dynamic assignment). The user can accept the default or modify the macro or by specifying a value for scull_major on the insmod command line:
===============================================
if(scull_major){
	dev = MKDEV(scull_major, scull_minor);
	result = register_chrdev_region(dev, scull_nr_devs, "scull");
}else{
	result = alloc_chrdev_region(&dev, scull_minor, scull_nr_devs, "scull");
	scull_major = MAJOR(dev);
}
if(result < 0){
	printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
	return result;
}
===============================================

3. Some important data structures:
• struct file_operations (in <linux/fs.h>):
Using the gcc-style to initialize struct, and the field will be left NULL if not specified. And if one operation is not specified, then the default kernel behavior will be conducted.
Note that some parameters include the string "__user". This is a annotation that a pointer is a user-space address that cannot be dereferenced directly.
Field:
	struct module* owner; // This field is used to prevent the module from being unloaded while its operations are in use. It is simply initialized to THIS_MODULE, a macro defined in <linux/module.h>.
	ssize_t (*readv)(struct file *, const struct iovec *, unsigned long, loff_t *);  // scatter/gather read and write operations.
	ssize_t (*writev)(struct file *, const struct iovec *, unsigned long, loff_t *);
More fields should be refer to the source code and LDD3.

• struct file (in <linux/fs.h>):
Note that struct file has nothing to do with the FILE pointer in user-space applications. struct file represents an open file in the kernel.
Field:
	mode_t f_mode; // identifies the file as either readable or writable or both. FMODE_READ and FMODE_WRITE. You might wan to check this field for r/w permission in open or ioctl
	loff_t f_pos; // the current file position. Note that read and write should update a position using the pointer they receive as the last parameter, rather than acting on f_pos directly. llseek method could act on f_pos directly, which is this function's purpose.
	unsigned int f_flags; // file flags such as O_RDONLY, O_NONBLOCK, O_SYNC. All the flags are defined in <linux/fcntl.h>
	struct file_operations* f_op; // Note that the value in filp->f_op is never saved by the kernel for later reference; this means the operations associated with file could be changed on the fly.
	void* private_data; // You can use this field to point to allocated data, but you must free that memory in the release method. This field is a useful resource for preserving state information across system calls.
	struct dentry* f_dentry; // Directory entry associated with the file. Device drivers normally needn't concern about this field, other than to access the inode structure as: filp->f_dentry->d_inode.
More fields should be refer to source code and LDD3.

• struct inode:
A struct inode represents a file on disk. Two fields in this structure are of interest for writing driver code:
	dev_t i_rdev; // For inodes that represent device files, this field contains the actual device number.
	struct cdev* i_cdev; // struct cdev is the kernel'd internal structure represents a char device. This field contains a pointer to that structure when the inode represents a char device file.
Note that kernel provides two macros to obtain the major and minor number from an inode:
	unsigned int iminor(struct inode* inode);
	unsigned int imajor(struct inode* inode);

• Note:
In the proc_dir_entry definition, it includes both file_operations and inode_operations. The difference is that file_operations deals with the file itself, like it has the methods read, write, llseek, etc; inode_operations deals with the ways to access the file, like it has the methods mkdir, unlink, permission, getattr, etc.

4. Char device registration:
The kernel uses struct cdev (defined in <linux/cdev.h>) to represent char devices. So before the kernel could invoke your device's operations, you must allocate and register one or more of these structures.
There're 2 ways to allocate and initialize it:
i)  If you just wanna a standalone cdev structure at runtime:
		struct cdev* my_cdev = cdev_alloc();
		mt_cdev->ops = &my_fops;
ii) However, struct cdev is always embedded in a device-specific structure of your own, so just initialize the structure that you've already allocated with:
		void cdev_init(struct cdev* cdev, struct file_operations* fops);
Note that cdev also has an owner field that should be set to THIS_MODULE.
Once the cdev structure is set up, the final step is to tell the kernel about it with a call:
	int cdev_add(struct cdev* dev, dev_t num, unsigned int count); // @num - the first device number to which this device responds; @count - number of device numbers that should be associated with the device. (count is always 1, but here is the situation that more than one device number correspond to each physical device.)
Note that cdev_add() may fail, and as soon as cdev_add() returns, the device is "live" and its operations can be called by the kernel. So be sure the driver is completely ready to handle operations on the device, before call cdev_add.

• To remove a char device from the system, call:
	void cdev_del(struct cdev* dev);

• The old way:
The old way to register a char device is with:
	int register_chrdev(unsigned int major, const char* name, struct file_operations* fops);
Note that we don't pass in minor number to register_chrdev(); this is because the kernel doesn't care about the minor number, only our driver uses it.
If 0 is passed to the @major field in register_chrdev(), the return value will be the dynamically allocated major number. The downside is that you can't make a device file in advance, since you don't know what the major number will be. There are a couple of ways to do this. i) the driver itself can print the newly assigned number and we can make the device file by hand. ii) the newly registered device will have an entry in /proc/devices, and we can either make the device file by hand or write a shell script to read the file in and make the device file. iii) we can have our driver make the the device file using the mknod system call after a successful registration and rm during the call to cleanup_module.

If you use the old way, the proper way to remove the device is:
	void unregister_chrdev(unsigned int major, const char* name);

• There's a counter keeps track of how many processes are using this module; it is listed in /proc/modules file, the third column. If this number isn't zero, rmmod will fail. You shouldn't use this counter directly, but here's function in <linux/module.h> to let you increase, decrease and display this counter:
	try_module_get(THIS_MODULE);  // Increase the use count
	module_put(THIS_MODULE);      // Decrease the use count

5. Open and release:
• The open method:
In most drivers, open should perform the following tasks:
i)   Check for device-specific errors (like device not ready, hardware problems...)
ii)  Initialize the device if it is being opened for the first time
iii) Update the f_op pointer if necessary.
iv)  Allocate and fill any data structure to be put in filp->private_data.

• The release method:
Perform the following tasks:
i)  Deallocate anything that open allocated in filp->private_data;
ii) Shut down the device on last close. (Note that not every close syscall causes the release method to be invoked.)

6. Read and write:
	ssize_t read(struct file* filp, char __user *buff, size_t count, loff_t* offp);
	ssize_t write(struct file* filp, const char __user *buff, size_t count, loff_t* offp);
Note that the buff argument to the read/write methods is a user-space pointer. Here're some functions that could perform data transfers between kernel and user space in a safe and correct way. Defined in <asm/uaccess.h>:
	unsigned long copy_to_user(void __user *to, const void *from, unsigned long count);
	unsigned long copy_from_user(void *to, const void __user *from, unsigned long count);
* Note that copy_to_user() and copy_from_user() may block because user page may not in memory, so the virtual memory subsystem can put the process to sleep while the page is being transfered into place.
* So the result for driver writer is that any function that access user space must be reentrant, must be able to execute concurrently with other driver functions, and inparticular, must be in a position where it can legally sleep.
* These two functions also check whether the user-space pointers are valid or not. So if you don't need to check the user-space pointer you can just invoke:
	__copy_to_user() and __copy_from_user().
* read() method should invoke copy_to_user(), because read from (device) file; write() method should invoke copy_from_user() because write to (device) file.
* read/write method should update the *offp to represent the current file position after successful completion of the system call. The kernel then propagates the file position change back into the struct file when appropriate.

i)  The read() method:
Return value of read() could be:
• return_value == count passed to the read syscall; this is the optimal case.
• return_value > 0, but return_value != count; only part of the data is transferred.
• return_value == 0, EOF reaches.
• return_value < 0; there was an error. <linux/errno.h>
Another situation is: there's no data, but it may arrive later. In this case, the read syscall should block. Blocking I/O will be dealt with in chapter 6.

ii) The write() method:
Return value of write() could be:
• return_value == count; perfectly.
• return_value > 0, but return_value != count, only partially write. The program will most likely retry writing the rest of the data.
• return_value == 0, nothing is writen, the retult is not an error, and there's no reason to return an error code. (Blocking write)
• return_value < 0, error occurred.

iii) readv and writev:
	ssize_t (*readv)(struct file* filp, const struct iovec* iov, unsigned long count, loff_t* ppos);
	ssize_t (*writev)(struct file* filp, const struct iovec* iov, unsigned long count, loff_t* ppos);
If the driver doesn't provide methods to handle the vector operations, readv and writev are implemented with multiple calls to read and write methods.
Defined in <linux/uio.h>:
=================================
struct iovec{
	void __user *iov_base;
	__kernel_size_t iov_len;
};
=================================
Each iovec describes one chunk of data to be transferred; it starts at iov_base in user space and is iov_len bytes long. The count parameter tells how many iovec structures there are. 
* Note that these iovec structures are created by the application, but the kernel copies them into kernel space before calling the driver.
