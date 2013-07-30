• Semaphore:
Non-interruptible operations are a good way to create unkillable processes (the "D" seen in ps) (down() method). down_interruptible() needs some extra care, if the operation is interrupted, the function returns a nonzero value, and the caller doesn't hold the semaphore. Proper use of down_interruptible() requires always checking the return value and responding accordingly.
=====================================
if(down_interruptible(&dev->sem))
	return -ERESTARTSYS;
=====================================
If down_interruptible() returns nonzero, the operation was interrupted, and the usual thing to do is to return -ERESTARTSYS. Upon seeing this return code, the higher layer of the kernel will either restart the call from the beginning or return the error to the user. If you return -ERESTARTSYS, you must first undo any user-visible changes that might have been made, so that the right thing happens when the system call is retried. If you cannot undo things in this manner, you should return -EINTR instead.

* Reader/Writer semaphores:
If you have a situation where a writer lock is needed for a quick change, followed by a longer period of read-only access, you can use downgrade_write() to allow other readers in once you have finished making changes.
Also note that current rw-semaphore may lead to reader starvation - it is the second rwlock.

• Completions:
Completion is a lightweight mechanism with one task: allowing one thread to tell another that the job is done. 
	void wait_for_completion(struct completion*); // Note that this function performs an uninterruptible wait. 
	void complete(struct completion *c);
	void complete_all(struct completion *c);
complete() wakes up only one of the waiting threads while complete_all() allows all of them to proceed.

* Completion is used once then discarded. It's possible, however, to reuse completion structures if proper care is taken. If complete_all() is not used, a completion structure can be reused without any problems. If you use complete_all(), however, you must reinitialize the completion structure before reusing it.
* Example:
===========================================
DECLARE_COMPLETION(comp);

ssize_t complete_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
	printk(KERN_DEBUG "process %i (%s) going to sleep\n", current->pid, current->comm);
	wait_for_completion(&comp);
	printk(KERN_DEBUG "awoken %i (%s)\n", current->pid, current->comm);
	return 0;
}

ssize_t complete_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos)
{
	printk(KERN_DEBUG "process %i (%s) awakening the readers...\n", current->pid, current->comm);
	complete(&comp);
	return count;
}
===========================================
Note that if complete() is called before wait_for_completion(), then wait_for_completion() returns immediately, not blocking there. (There's no paradise lost for complete()...)


• Spinlock:
* Spinlocks and atomic context:
i)   The concept of using the spinlock is that: any code, while holding a spinlock, be atomic. It cannot sleep, in fact, it cannot relinquish the processor for any reason except to service interrupts (and sometimes not even could service the interrupts).
ii)  Kernel preemption is handled by the spinlock. Any time kernel code holds a spinlock, preemption is disabled on the relevant processor.

iii) Interrupt handler (could preempt) bottom halves (could preempt) process context.

iv)  If you use a spinlock in interrupt handler, you could disable local interrupts before acquire lock to prevent the deadlock. (spin_lock_irqsave(), etc). If you use a spinlock in bottom halves, you could disable bottom halves to prevent deadlock (spin_lock_bh()).

v)   Note that you must call spin_lock_irqsave() and spin_unlock_irqrestore() in the same function; otherwise your code may break on some architectures. (The flags argument passed to spin_unlock_irqrestore() must be the same variable passed to spin_lock_irqsave())

* rw-spinlock:
Note that reader/writer locks can starve readers just as rw-semaphore can. (The second rwlock.)

• Locking Traps:
* Locking orders: When multiple locks must be acquired, they should always be acquired in the same order. If you must acquire a lock that is local to your code along with a lock belonging to a more central part of the kernel, take your lock first. If you have a combination of semaphores and spinlocks, you must obtain the semaphores first. 
* As a general rule, you should start with relatively coarse locking unless you have a real reason to believe that contention could be a problem. You may find the lockmeter tool useful. (http://oss.sgi.com/projects/lockmeter) This patch instruments the kernel to measure time spent waiting in locks.

• Alternatives to locking:
i)   Lock-free algorithms:
See CMU lecture and search the web.
ii)  Atomic variables:
iii) Bit operations:
iv)  Seqlocks:
Seqlocks work in situations where the resource to be protected is small, simple, and frequently accessed, and where write access is rare but must be fast. (Essentially, they work by allowing readers free access to the resource but requiring those readers to check for collisions with writers and, when such a collision happens, retry their access.)
Note that seqlocks generally cannot be used to protect data structures involving pointers, because the reader may be following a pointer that is invalid while the writer is changing the data structure.
See LKD for example.
v)   Read-Copy-Update(RCU):
See http://www.rdrop.com/users/paulmck/intro/rclock_intro.html. Actually I don't quite understand this part.
RCU is optimized for situations where reads are common and writes are rare. The resources being protected should be accessed via pointers, and all references to those resources must be held only by atomic code.
