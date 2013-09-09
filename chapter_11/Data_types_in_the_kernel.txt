1. Use of standard C types:
Generic memory addresses in the kernel are usually unsigned long, exploiting the fact that pointers and long integers are always the same size. Note that although SPARC 64 architecture runs with a 32-bit user space, even though they are 64-bit wide in kernel space. This can be verified by using sizeof(ptr) in user-space and using sizeof(ptr) in kernel space.

2. Assigning an explicit size to data items:
u8, u16, u32, u64 are used in kernel space, if in user space you need to use the "underscore" version: __u8, __u16, __u32, __u64.

3. Interface-specific types:
Like pid_t, etc. The best way to print some interface-specific data is to cast the value to the biggest possible type (usually long or unsigned long) and then printing it through the corresponding format.

4. Other portability issues:
i) 	 HZ:

ii)  PAGE_SIZE;
In kernel space, using PAGE_SIZE and PAGE_SHIFT; in user space, using getpagesize(). If in the driver you want to get a buffer of 16KB, using:
	#include <asm/page.h>
	int order = get_order(16 * 1024);
	buf = __get_free_pages(GFP_KERNEL, order);
Note that the argument to get_order() must be a power of 2.

iii) Byte order:
The include file <asm/byteorder.h> defines either __BIG_ENDIAN or __LITTLE_ENDIAN, depending on the processor's byte ordering.
Linux kernel also defines a set of macros that handle conversions between the processor's byte ordering and that of the data you need to store or load in a specific byte order. For example:
	u32 cpu_to_le32(u32);
	u32 le32_to_cpu(u32);
This works whether your CPU is big-endian or little-endian, and whether it is a 32-bit processor or not.
• When dealing with pointers, you can also use functions like cpu_to_le32p(), which take a pointer to the value to be converted rather than the value itself. See LKD or the source code for more details.

iv)  Data alignment:
How to access unaligned data, for example, how to read a 4-byte value stored at an address that isn't a multiple of 4 bytes:
	#include <asm/unaligned.h>
	get_unaligned(ptr);
	put_unaligned(val, ptr);
These macros are typeless and work for every data item, whether it's one, two, four, or eight bytes long.
• Natural alignment:
Storing data items at an address that is a multiple of their size. But not all platforms align 64-bit values on 64-bit boundaries.
Also note that compiler may quitely insert padding into structures to ensure that every field is aligned for good performance on the target processor. The way to avoid this is to tell the compiler that the structure must be "packed", with no fields added. For example:
	struct {
		u16 id;
		u64 lun;
		u16 reserved1;
		u32 reserved2;
	}__attribute__((packed)) scsi;

v)   Pointers and error values:
Some kernel interfaces return error code in forms of a pointer value (like (void *)-1, etc.):
	#include <linux/err.h>
	void *ERR_PTR(long error); // return a pointer type that represents an error code
	long IS_ERR(const void *ptr); // test a returned pointer is an error code or not.
	long PTR_ERR(const void *ptr); // If you need the actual error code, use this function; but note that you could use this only when IS_ERR() returns true, any other value is a valid pointer.

5. Kernel linked lists:
See LKD and source code for more details. But note that the list interface performs no locking, even using list_for_each_safe, it just stores the next entry at the beginning of the loop, so it could just cover the situation that delete the current node while traversing the list; if any concurrent modification may happen to the list, you still need locks.
• hlist is a doubly linked list with a separate, single-pointer list head type. It's often used for creation of hash tables and similar structures.
