#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define PROC_NAME "myseq"
#define PROC_MODE 0644

MODULE_LICENSE("GPL");
MODULE_AUTHOR("hsy");

static void* seq_start(struct seq_file *seq_file, loff_t *pos)
{
	static unsigned long counter = 0;
	printk(KERN_INFO "in %s\n", __func__);
	/* beginning a new sequence? */
	printk(KERN_INFO "in %s, *pos = %llu\n", __func__, *pos);
	if (*pos == 0) {
		return &counter;  // use counter as entry number used in stop,next,show(v)
	} else {
//		*pos = 0;
		return NULL;
	}
}

static void seq_stop(struct seq_file *seq_file, void *v)
{
	printk(KERN_INFO "in %s\n", __func__);
}

static void* seq_next(struct seq_file *seq_file, void *v, loff_t *pos)
{
	unsigned long *temp_v = (unsigned long*)v;
	printk(KERN_INFO "in %s\n", __func__);
	(*temp_v)++;
	(*pos)++;
	return NULL;
}

static int seq_show(struct seq_file *seq_file, void *v)
{
	unsigned long *temp = (unsigned long*)v;
	printk(KERN_INFO "in %s\n", __func__);
	seq_printf(seq_file, "%lu\n", *temp);
	return 0;
}

static struct seq_operations my_seq_ops = {
	.start = seq_start,
	.stop = seq_stop,
	.next = seq_next,
	.show = seq_show,
};

static int my_seq_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &my_seq_ops);
}

static struct file_operations my_fops = {
	.owner = THIS_MODULE,
	.open = my_seq_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

int init_module(void)
{
	struct proc_dir_entry *p_pde = NULL;
	printk(KERN_INFO "%s called!\n", __func__);
	if((p_pde = create_proc_entry(PROC_NAME, PROC_MODE, NULL)) == NULL)
	{
		remove_proc_entry(PROC_NAME, NULL);
		return -ENOMEM;
	}
	p_pde->proc_fops = &my_fops;
	printk(KERN_INFO "/proc/%s created\n", PROC_NAME);
	return 0;
}

void cleanup_module(void)
{
	printk(KERN_INFO "%s called!\n", __func__);
	remove_proc_entry(PROC_NAME, NULL);
}
