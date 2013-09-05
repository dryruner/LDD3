1. I/O ports and I/O memory:
Every peripheral device is controlled by writing and reading its registers. Registers are accessed at consecutive addresses, either in the memory address space or in the I/O address space.

2. Memory barriers:
The driver must ensure that no caching is performed and no read or write reordering takes place when accessing registers.
rmb(), wmb(), mb(), read_barrier_depends(), smp_rmb(), smb_wmb(), barrier(), see LKD/chapter10 for more details.

• It's worth noting that most of the other kernel primitives dealing with synchronization, such as spinlock and atomic_t operations, also function as memory barriers.

• Some architectures allow the efficient combination of an assignment and a memory barrier. Kernel provides a few macros that perform this combination:
	#define set_mb(var, value)  do {var = value; mb();} while (0)
	#define set_rmb(var, value)  do {var = value; rmb();} while (0)
	#define set_wmb(var, value)  do {var = value; wmb();} while (0)
<asm/system.h> defines these macros to use architecture-specific instructions that accomplish task more quickly.

3. Using I/O ports:
i)   I/O port allocation:
	#include <linux/ioport.h>
	struct resource *request_region(unsigned long first, unsigned long n, const char *name);
It tells kernel that you'd like to make use of @n ports, starting with @first. @name should be the name of the device. If return value is NULL, you'll not be able to use the desired ports.
• All port allocations show up in /proc/ioports.
	void release_region(unsigned long first, unsigned long n);
	int check_region(unsigned long first, unsigned long n); // return value is negative if the given port is not available. Note that this function is deprecated because of TOCTTOU bug.

ii)  Manipulating I/O ports:
	#include <asm/io.h>
	unsigned inb(unsigned port);
	void outb(unsigned char byte, unsigned port);
The @port argument is defined as unsigned long for some platforms and unsigned short for others. The return type of inb() and outb() is also different across architectures.
	unsigned inw(unsigned port);
	unsigned inl(unsigned port);
	void outw(unsigned short word, unsigned port);
	void outl(unsigned long word, unsigned port);
• Note that no 64-bit port I/O operations are defined. Even on 64-bit architectures, the port address space use a 32-bit (maximum) data path.

* I/O port access from user space:
The functions described above can also be used from user space, at least on PC-class computers. GNU C library defines them in <sys/io.h>, and following conditions shoule apply in order for inb() family to be used in user-space code:
• The program must be compiled with -O option to force expansion of inline functions.
• The ioperm() or iopl() syscalls must be used to get permission to perform I/O operations on ports. ioperm() gets permission for individual port, while iopl() gets permission for the entire I/O space. Both of these functions are x86-specific.
• The program must run as root to invoke ioperm or iopl. (Actually it must have CAP_SYS_RAWIO capability.)
If the host platform has no ioperm and no iopl syscalls, user space can still access I/O ports by using /dev/port device file. See misc-progs/inp.c for more details for using /dev/port file.

iii) String operations:
	void insb(unsigned port, void *addr, unsigned long count);
	void outsb(unsigned port, void *addr, unsigned long count);
Read/write count bytes starting at the memory address addr. Data is read from or written to the single port @port.
	void insw(unsigned port, void *addr, unsigned long count);
	void outsw(unsigned port, void *addr, unsigned long count);
	void insl(unsigned port, void *addr, unsigned long count);
	void outsl(unsigned port, void *addr, unsigned long count);
• Note that: above functions move a straight byte stream to or from @port. When the port and the host system have different byte ordering rules, the results can be surprising. (However, inw() family would swap the bytes if necessary to make the value match the host byte-ordering.) The string functions, instead, do not perform this swapping.

iv)  Pausing I/O:
Some platforms - most notably the i386 - can have problems when the processor tries to transfer data too quickly to or from the bus. The solution is to insert a small delay after each I/O instruction if another such instruction follows. See <asm/io.h> for the implementation.
If your device misses some data, or if you fear it might miss some, you can use pausing functions in place of the normal ones. The pausing I/O just ends with "_p", like inb_p(), etc.

4. Using I/O memory:
Despite the popularity of I/O ports in the x86 world, the main mechanism used to communicate with devices is through memory-mapped registers and device memory.
Whether or not ioremap() is required to access I/O memory, direct use of pointers to I/O memory is discouraged.

• I/O memory allocation and mapping:
	#include <linux/ioport.h>
	struct resource *request_memory_region(unsigned long start, unsigned long len, char *name); // allocates a memory region of @len bytes, starting at @start. All I/O memory allocation are listed in /proc/iomem.
	void release_mem_region(unsigned long start, unsigned long len);
	int check_mem_region(unsigned long start, unsigned long len); // discoraged to use
* Allocation of I/O memory is not the only required step before that memory may be accessed, you must also ensure that this I/O memory has been made accessible to the kernel. This is the role of ioremap() family introduced in chapter08. The function is designed specifically to assign virtual addresses to I/O memory regions.
* Once equipped with ioremap(), a device driver can access any I/O memory address, whether or not it is directly mapped to virtual address space. Note that the addresses returned from ioremap() shouldn't be deferenced directly; instead, accessor functions provided by the kernel should be used. 

• Accessing I/O memory:
* To read from I/O memory, use one of the following:
	unsigned int ioread8(void *addr);
	unsigned int ioread16(void *addr);
	unsigned int ioread32(void *addr);
@addr is the address obtained from ioremap() (perhaps with an integer offset). The return value is what was read from the given I/O memory.
	void iowrite8(u8 value, void *addr);
	void iowrite16(u16 value, void *addr);
	void iowrite32(u32 value, void *addr);

* If you must read or write a series of values to a given I/O memory address, you can use the repeating versions of the functions:
	void ioread8_rep(void *addr, void *buf, unsigned long count);
	void ioread16_rep(void *addr, void *buf, unsigned long count);
	void ioread32_rep(void *addr, void *buf, unsigned long count);
	void iowrite8_rep(void *addr, void *buf, unsigned long count);
	void iowrite16_rep(void *addr, void *buf, unsigned long count);
	void iowrite32_rep(void *addr, void *buf, unsigned long count);
These functions read or write @count values starting from the given @buf to the given @addr.
The functions described above perform all I/O to the given addr. If you need to operate on a block of I/O memory, you can use one of the following:
	void memset_io(void *addr, u8 value, unsigned int count);
	void memcpy_fromio(void *dest, void *source, unsigned int count);
	void memcpy_toio(void *dest, void *source, unsigned int count);
These functions behave like their C library analogs.

* If you read through kernel source, you will see old I/O memory interfaces:
	unsigned readb(address);
	unsigned readw(address);
	unsigned readl(address);
	void writeb(usnigned value, address);
	void writew(unsigned value, address);
	void writel(unsigned value, address);
Do not use these functions any more.

• Ports as I/O memory:
As a way of minimizing the apparent differences between I/O port and memory accesses, the 2.6 kernel provides:
	void *ioport_map(unsigned long port, unsigned int count); // remaps @count I/O ports and makes them appear to be I/O memory. From that point thereafter, the driver may use ioread8() family on the returned addresses.
	void ioport_unmap(void *addr);
These functions make I/O ports look like memory. But do note that the I/O ports must still be allocated with request_region() before they can be remapped in this way.
