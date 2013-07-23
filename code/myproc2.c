#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/cred.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("hsy");

#define PROC_NAME "buffer2k"
#define MAX_LEN 2048

static char proc_buf[MAX_LEN];
static unsigned int proc_buf_len = 0;
static struct proc_dir_entry *p_pde = NULL;
static struct proc_dir_entry *p_root = NULL;

static ssize_t proc_read(struct file *file, char __user *buffer, size_t length, loff_t *offset)
{
	static int finished = 0; // local static variable
	if(finished)
	{
		printk(KERN_INFO "procfs_read END\n");
		finished = 0;
		return 0;
	}
	if(copy_to_user(buffer, proc_buf, proc_buf_len))
		return -EFAULT;
	finished = 1;
	printk(KERN_INFO "procfs read, read %u bytes\n", proc_buf_len);
	*offset += proc_buf_len;
	return proc_buf_len;
}

static ssize_t proc_write(struct file *file, const char __user *buffer, size_t length, loff_t *offset)
{
	proc_buf_len = length > MAX_LEN ? MAX_LEN : length;
	if(copy_from_user(proc_buf, buffer, proc_buf_len))
		return -EFAULT;
	printk(KERN_INFO "procfs write, write %u bytes\n", proc_buf_len);
	*offset += proc_buf_len;
	return proc_buf_len;
}

static int proc_open(struct inode *inode, struct file *file)
{
	try_module_get(THIS_MODULE);
	return 0;
}

static int proc_close(struct inode *inode, struct file *file)
{
	module_put(THIS_MODULE);
	return 0;
}

static int proc_permission(struct inode *inode, int op, unsigned int wtf)
{
	if(op == 4 || op == 2)
		return 0;
	return -EACCES;
}

static struct file_operations proc_fops = {
	.owner = THIS_MODULE,
	.read = proc_read,
	.write = proc_write,
	.open = proc_open,
	.release = proc_close,
};

static struct inode_operations proc_iops = {
	.permission = proc_permission,
};

int init_module(void)
{
	printk(KERN_INFO "%s called!\n", __func__);
	if((p_pde = create_proc_entry(PROC_NAME, 0644, p_root)) == NULL)
	{
		remove_proc_entry(PROC_NAME, p_root);
		return -ENOMEM;
	}
	p_pde->size = 100000;
	p_pde->uid = 0;
	p_pde->gid = 0;
	p_pde->mode = S_IFREG | S_IRUGO | S_IWUSR;
//	p_pde->proc_iops = &proc_iops;
	p_pde->proc_fops = &proc_fops;
	printk(KERN_INFO "/proc/%s created\n", PROC_NAME);
	return 0;
}

void cleanup_module(void)
{
	printk(KERN_INFO "%s called!\n", __func__);
	remove_proc_entry(PROC_NAME, p_root);
}
