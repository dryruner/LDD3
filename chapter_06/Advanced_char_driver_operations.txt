1. ioctl (<linux/ioctl.h>):
i)  In user-space, the ioctl syscall has the following prototype:
	int ioctl(int fd, unsigned long cmd, ...);
* Note that in the above prototype, the "..." doesn't represent a variant number of arguments; but a single optional argument, traditionally identified as char *argp. The actual nature of the third argument depends on the specific control command issued (the 2nd argument). Some commands take no argument, some take an integer, some take a pointer. (Using a pointer is the way to pass arbitrary data to the ioctl call, the device is then able to exchange any amount of data with user space.)
* Each ioctl command is a separate, usually undocumented syscall. However, ioctl is often the easiest and most straightforward choice for true device operations.

ii) In kernel-space (driver), ioctl has the following prototype:
	int (*ioctl)(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
The inode and filp are the values corresponding to the file descriptor fd passed to by the user-space ioctl, and are the same parameters passed to the open method. The cmd and arg are passed from the user.

• Choosing the ioctl commands:
The ioctl command numbers should be unique across the system in order to prevent errors caused by issuing the right command to the wrong device. To choose ioctl numbers for your driver, you should first check include/asm/ioctl.h and Documentation/ioctl-number.txt. 
The approved way to define ioctl command numbers use 4 bitfields:
* type: 
The magic number, 8 bits (_IOC_TYPEBITS).
* number:
The ordinal (sequential) number, 8 bits (_IOC_NRBITS).
* direction:
The direction of data transfer, 2 bits. The possible values are: _IOC_NONE (no data transfer); _IOC_READ; _IOC_WRITE; and (_IOC_WRITE | _IOC_READ). Note that: data transfer is seen from the application's point of view: _IOC_READ means reading from the device.
* size:
The size of user data invloved. The bit-width of this field is architecture dependent: but it's usually 13 or 14 bits. You will check _IOC_SIZEBITS to see the real bit-width. It's not mandatory to use the size field, the kernel doesn't check it.

There're also many macros defined in the header file that could be useful: _IO(type, nr); _IOR(type, nr, datasize); _IOW(type, nr, datasize); _IOWR(type, nr, datasize); _IOC_DIR(nr); _IOC_TYPE(nr); _IOC_NR(nr); _IOC_SIZE(nr).

• The return value:
The implementation of ioctl is usually a switch statement based on the command number. If none of "case" branch matches, the default section could return -EINVAL(invalid argument), or return -ENOTTY(inappropriate ioctl for device.)

• The predefined commands:
Note that these predefined commands are decoded before one's own file operations are called. 
The predefined commands are divided into three groups: i) Those that can be issued on any file (regular, device, FIFO, or socket); ii) Those that are issued only on regular files; iii) Those specific to the filesystem type. We're only interested in the first group of commands, whose magic number is "T". Refer to LDD3 P141 for more details.

• Using the ioctl argument:
* Address verification (without transferring data) is implemented by the function access_ok(), whichis declared in <asm/uaccess.h>:
	int access_ok(int type, const void *addr, unsigned long size);
The 1st argument should be either VERIFY_READ (the action to be performed is to read the user-space memory) or VERIFY_WRITE. (VERIFY_WRITE is a superset of VERIFY_READ)
The 2nd argument holds a user-space address.
The 3rd argument is a byte count.
access_ok() returns 1 for success (access is OK), and 0 for failure. If it returns false, the driver should usually return -EFAULT to the caller.
* put_user(datum, ptr)/__put_user(datum, ptr):
These macros write the datum to user space; they're relatively fast and should be called instead of copy_to_user whenever single values are being transferred. The size of data transfer depends on the type of the ptr argument and is determined at compile time. put_user() checks to ensure that the process is able to write to the given memory address. It returns 0 on success, and -EFAULT on error.
* get_user(local, ptr)/__get_user(local, ptr):
Used to retrieve a single datum from user space. The value retrieved is stored in the local variable local.
* Note that if size of the data transferred doesn't fit one of the specific sizes, like 1,2,4,8, then copy_to_user()/copy_from_user() should be used.

• Capabilities and restricted operations: (<linux/capability.h>)
Access to device is controlled by the permissions on the device file, but kernel also uses capabilities exclusively for permissions management. Usual capabilities:
CAP_DAC_OVERRIDE: The ability to override access restrictions (data access control, DAC) on files and directories.
CAP_NET_ADMIN: The ability to perform network administration tasks, including those that affect network interfaces.
CAP_SYS_MODULE: The ability to load and remove kernel modules.
CAP_SYS_RAWIO: 
CAP_SYS_ADMIN:
CAP_SYS_TTY_CONFIG: The ability to perform tty configuration tasks.
* Before performing a privileged operation, a device driver should check that the calling process has the appropriate capability:
	int capable(int capability);
See LDD3 scull source code for more ioctl details.


2. Blocking I/O:
i) Introduction to sleeping:
The 1st rule is that: never sleep when you are running in an atomic context, which is a state where multiple steps must be performed without any sort of concurrent access. So the driver cannot sleep while holding a spinlock, seqlock, or RCU lock. You also cannot sleep if you have disabled interrupts. 
The 2nd rule is that: when you wake up you never know if another process may have been sleeping for the same event and grab whatever resource you were waiting for. So the result is that you cannot make any assumptions about the state of the system after you wake up, and you must check to ensure that the condition you were waiting for is indeed true.
Linux provides wait queue. A wait queue is a list of processes, all waiting for a (the same type of) specific event. A wait queue is managed by a "wait queue head", of the type: wait_queue_head_t, which is defined in <linux/wait.h>:
	DECLARE_WAIT_QUEUE_HEAD(name); // define and initialize statically
	wait_queue_head_t my_queue;    // define and initialize dynamically
	init_waitqueue_head(&my_queue);
* Simple sleeping:
The simplest way of sleeping in the linux kernel is a macro called wait_event():
	wait_event(queue, condition);
	wait_event_interruptible(queue, condition);
	wait_event_timeout(queue, condition, timeout);
	wait_event_interruptible_timeout(queue, condition, timeout);
"queue" is the wait queue head to use.
"condition" is a boolean expression without side effects. It is evaluated before and after sleeping, if condition is true, then do not sleep.
If the macros (interruptible) return nonzero it means that the sleep is interrupted by signal, so the driver may probably return -ERESTARTSYS.
* The basic functions that wake up sleeping process:
	void wake_up(wait_queue_head_t *queue);
	void wake_up_interruptible(wait_queue_head_t *queue);
	void wake_up_interruptible_sync(wait_queue_head_t *queue);
wake_up(): wakes up all processes waiting on the given queue (though the situation is more complecated than that.)
Only the read(), write(), and open() file operations are affected by the nonblocking flag. (O_NONBLOCK)
See the blocking read and blocking write example in the scull source code and LDD3 P153. (The use of wait_event_interruptible() is very much like the use of condition variables, before put sleep should release lock, after wake up, acquire the lock again, and after acquiring the lock, to check the condition again. (while))

* Advanced sleeping: (a deeper understanding of how the kernel wait queue mechanism works)
wait_queue_head_t is simply a spinlock and a linked list. wait queue entries are of the type wait_queue_t, which contains the information about the sleeping process and how it would like to be woken up.
i)   The 1st step of putting a process to sleep is to allocate and initialize a wait_queue_t structure, followed by its addition to the proper wait queue:
	DEFINE_WAIT(my_wait);
	wait_queue_t my_wait;
	init_wait(&my_wait);
ii)  The 2nd step is to set the state of the process to mark it as being asleep using: void set_current_state(int new_state); By changing the current state, you've changed the way the scheduler treats a process, but you haven't yet yielded the processor.
iii) Giving up the processor is the final step. But you must check the condition you are sleeping for first: if(!condition) {schedule();} If the wakeup happens between the test in the if statement and the call of schedule(), it's still OK. The wakeup resets the process state to TASK_RUNNING and schedule() returns - although not necessarily right away. As long as the test happens after the process has put itself on the wait queue and changed its state, thing will work.
	void prepare_to_wait(wait_queue_head_t *queue, wait_queue_t *wait, int state); // this function adds the wait_queue_t entry to the wait queue (if the wait_queue_t entry isn't in the queue) and set the process state. (So you could call add_wait_queue() before it, and it could be used in the while() statement.)
iv)  After the if test and schedule() returns, there's some cleanup needs to be done. The state should be reset back to TASK_RUNNING; it's also necessary to remove the process from the wait queue, thus preventing it from being awakened more than once.
	void finish_wait(wait_queue_head_t *queue, wait_queue_t *wait); // handles the cleanup.
See scull pipe.c source code for more details about how to do manual sleeping.

* Exclusive wait:
If a process calls wake_up() on a wait queue, all the processes (special cases will be stated below) waiting on that queue are made runnable, but only one of them will acquire the lock, the rest will sleep again. If the number of processes in the wait queue is large, this will degrade the system performance. So "exclusive wait" is provided:
When a wait queue entry has the WQ_FLAG_EXCLUSIVE flag set, it is added to the end of the wait queue; entries without that flag are added to the beginning (see source code of add_wait_queue()).
when wake_up() is called on a wait queue, it stops after waking the first process that has the WA_FLAG_EXCLUSIVE flag set. (it will still wake up all the non-exclusive-wait processes.)
• Note that employing exclusive waits in a driver needs to consider two conditions: i) You expect significant contention for a resource; ii) Waking a single process is sufficient to completely consume the resource when it becomes available. (Exclusive wait works well for the Apache web server for example.)
Putting a process into an interruptible wait is a simple matter of calling:
	void prepare_to_wait_exclusive(wait_queue_head_t *queue, wait_queue_t *wait, int state);
This call sets the "exclusive" flag in the wait queue entry and adds the process to the end of the wait queue. Note that there's no way to perform exclusive waits using wait_event and its variants.

* Details of waking up:
The actual behavior when a process is awakened is controlled by a function in the wait queue entry. The default wakeup function sets the process into a runnable state, and possibly performs a context switch to that process if it has a higher priority. (Device drivers should never supply a different wake function.)
	void wake_up(wait_queue_head_t *queue): awakens every process on the queue that is not an exclusive process, and exactly one exclusive waiter, if it exists.
	void wake_up_nr(wait_queue_head_t *queue, int nr): exactly as wake_up(), but it can awaken up to nr exclusive waiters.
	void wake_up_interruptible_sync(wait_queue_head_t *queue): Normally, a process that is awakened may preempt the current process and be scheduled into the processor before wake up returns. (In short, a call to wake_up may not be atomic.) If the process calling wake_up is running in an atomic context (if holds a spinlock, or is an interrupt handler), this rescheduling doesn't happen. Normally that protection is adequate. If, however, you need to explicitly ask not to be scheduled out of the processor at this time, you can use this "_sync" version. It is most often used when the caller is about to reschedule anyway, and it is more efficient to simply finish what little work remains first.

• poll and select:
Applications that use nonblocking I/O often use the poll, select, and epoll syscall. They allow a process to determine whether it can read or write to one or more open files without blocking. These calls can also block a process until any of a given set of file descriptors becomes available for reading or writing. Therefore they are often used in applications that must use multiple input or output streams without getting stuck on any one of them.
* The support for any of these three calls require support from the device driver. Kernel support these three through a function in file_operations structure:
	unsigned int (*poll)(struct file *filp, poll_table *wait);
The device method (poll() method provided by the driver code) is in charge of these two steps:
i)  Call poll_wait() on one or more wait queues that could indicate a change in the poll status.
ii) Return a bit mask describing the operations that could be immediately performed without blocking. (For example, if the device has data available, a read would complete without sleeping; the poll method should indicate this state of affairs.)

The driver adds a wait queue to the poll_table structure by calling the function poll_wait():
	void poll_wait(struct file *, wait_queue_head_t *, poll_table *);
Several flags are used to indicate the possible operations:
POLLIN:       This bit must be set if the device can be read without blocking.
POLLRDNORM:   This bit must be set if "normal" data is available for reading. A readable device returns (POLLIN | POLLRDNORM).
POLLDBAND:    This bit indicates that out-of-band data is available for reading from the device. This bit is not generally applicable to device drivers.
POLLPRI:      High-priority data (out of band) can be read without blocking. 
POLLHUP:      When a process reading this device sees end-of-file, the driver must set POLLHUP. 
POLLERR:      An error condition has occurred to the device.
POLLOUT:      This bit is set in the return value, if the device can be written to without blocking.
POLLWRNORM:   A writable device returns (POLLOUT | POLLWRNORM).
POLLWRBAND:   Like POLLRDBAND, this bit is seldom (never) used by normal drvice drivers.
• Note that POLLRDBAND and POLLWRBAND are meaningful only with file descriptors associated with sockets; device drivers won't normally use these flags.

* Interaction with read and write:
i)  Reading data from the device:
If there's data in the input buffer, the read call should return immediately, even if less data is available than the application requested. If there's no data is in the input buffer, by default read must block until there's at least 1 byte there. If O_NONBLOCK is set, read returns immediately with a return value of -EAGAIN. In these cases, poll must report that the device is unreadable until at least one byte arrives. If we're at EOF, read should return immediately with a return value of 0, independent of O_NONBLOCK. poll() should report POLLHUP in this case.
ii) Writing to the device:
Essentially the same as read. And never make a write call wait for data transmission before returning, even if O_NONBLOCK is clear. If the program wants to ensure that the data it enqueues in the output buffer is actually transmitted, the driver must provide a fync() method.

* The underlying data structures:
Whenever a user application calls poll, select, or epoll, the kernel invokes the poll method of all files referenced by the syscall, passing the same poll_table to each of them. The poll_table is a wrapper around a function that builds the actual data structure. The structure, for select and poll, is a linked list of memory pages containing poll_table_entry structures. Each poll_table_entry holds the struct file and wait_queue_head_t pointers passed to poll_wait(), along with an associated wait queue entry. 
• The driver's poll method may be called with a NULL pointer as poll_table paramater. Because if the application calling poll has provided a timeout value of 0 (indicating that no wait should be done), there's no to accumulate wait queues. The poll_table parameter is also set to NULL immediately after any driver being polled indicates that I/O is possible. Since the kernel knows that at this point no wait will occur, it doesn't build up a list of wait queues.
When the poll call completes, the poll_table structure is deallocated, and all wait queue entries previously added to the poll table are removed from the table and their wait queues.
Because there're applications that work with thousands of file descriptors. So the allocation and deallocation of this data structures between every I/O operation becomes expensive. The epoll syscall family allows setting up the internal kernel data structures exactly once and to use it many times.

* Asynchronous notification:
User programs have to execute two steps to enable asynchronous notification from an input file. i) First, they specify a process as the owner of the file. When the process invokes the F_SETOWN command using fcntl syscall, the pid of the owner process is saved in filp->f_owner, this is necessary for the kernel to know whom to notify. ii) In order to actually enable asynchronous notification, the user programs must set the FASYNC flag in the device by means of the F_SETFL fcntl command. After the two calls have been executed, the input file can request delivery of a SIGIO signal whenever new data arrives. The signal is sent to the process (or process group if the value is negative) stored in filp->f_owner.
For example:
	signal(SIGIO, &input_handler);
	fcntl(STDIN_FILENO, F_SETOWN, getpid());
	oflags = fcntl(STDIN_FILENO, F_GETFL);
	fcntl(STDIN_FILENO, F_SETFL, oflags | FASYNC);
Note that when a process receives a SIGIO, it doesn't know which input file has new input to offer. If more than one file is enabled to asynchronously notify a process, it may still has to resort to poll or select to find out what happened.

• The driver's point of view:
i)   When F_SETOWN is invoked, nothing happens except that a value is assigned to filp->f_owner.
ii)  When F_SETFL is executed to turn on FASYNC, the driver's fasync method is called. This method is called whenever the value of FASYNC is changed in filp->f_flags to notify the driver of the change. The flag is cleared by default when the file is opened.
iii) When data arrives, all the processes registered for asynchronous notification must be sent a SIGIO signal.
<linux/fs.h> defines the data structure fasync_structure and two functions:
	int fasync_helper(int fd, struct file *filp, int mode, struct fasync_struct **fa);
	void kill_fasync(struct fasync_struct **fa, int sig, int band);
fasync_helper() is invoked to add or remove entries from the list of interested processes when the FASYNC flag changes for an open file. kill_fasync() is used to signal the interested process when data arrives. Its arguments are the signal to send (usually SIGIO) and the band (always POLL_IN, but may be used to send "urgent" or out-of-band data in the networking code)  (POLL_IN and POLL_OUT are symbols used in asynchronous notification code.)
• Note that we must invoke our fasync method when the file is closed to remove the file from the list of active asynchronous readers. Although this call is required only if filp->f_flags has FASYNC set, calling the function anyway doesn't hurt and is the usual implementation:
	scull_p_fasync(-1, filp, 0);
The data structure underlying asynchronous notification is almost identical to the structure struct wait_queue; the difference is that struct file is used in place of struct task_struct.


3. Seeking a device:
If seeking a device doesn't make any sense, you must inform the kernel, because the default llseek method allows seeking. You should inform the kernel that your device doesn't support seeking by i) calling nonseekable_open() in the open method; ii) set .llseek = no_llseek. See scull source for more references.

4. Access control on a device file:
i)   Single-open devices:
The brute-force way to permit a device to be opened by only one process at a time. (Using atomic_t type as a use-count.)
ii)  Restricting access to a single user at a time: for example:
========================================================================
spin_lock(&scull_u_lock);
if(scull_u_count && 
		(scull_u_owner != current->uid) &&  /* allow user */
		(scull_u_owner != current->euid) && /* allow whoever did su */
		!capable(CAP_DAC_OVERRIDE)) {  /* still allow root */
	spin_unlock(&scull_u_count);
	return -EBUSY; /* -EPERM would confuse the user */
}
if(scull_u_count == 0)
	scull_u_owner = current->pid;
scull_u_count++;
spin_unlock(&scull_u_lock);
========================================================================
• Blocking open as an alternative to EBUSY:
When the device isn't accessible, return an error is usually the most sensible approach, but there're situations in which user would prefer to wait for the device. For example:
========================================================================
spin_lock(&scull_w_lock);
while(!scull_w_available()) {
	spin_unlock(&scull_w_lock);
	if(filp->f_flags & O_NONBLOCK)
		return -EAGAIN;
	if(wait_event_interruptible(scull_w_wait, scull_w_available()))
		return -ERESTARTSYS;
	spin_lock(&scull_w_lock);
}
if(scull_w_count == 0)
	scull_w_owner = current->uid;
scull_w_count++;
spin_unlock(&scull_w_lock);
========================================================================
static int scull_w_release(struct inode *inode, struct file *filp)
{
	int temp;
	spin_lock(&scull_w_lock);
	scull_w_count--;
	temp = scull_w_count;
	spin_unlock(&scull_w_lock);
	if(temp == 0)
		wake_up_interruptible_sync(&scull_w_wait);   // awake another uid
	return 0;
}
========================================================================
iii) Cloning the device on open:
Anothe technique to manage access control is to create different private copies of the device, depending on the process opening it. (This is possible only if the device is not bound to a hardware object.) The internals of /dev/tty use a similar technique in order to give its process a different "view" of what the /dev entry point represents. When copies of the device are created by the software driver, we call them virtual devices, just as virtual consoles use a single physical tty device.
