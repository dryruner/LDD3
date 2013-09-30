1. Address types in linux:
Linux is a virtual memory system, meaning that the address seen by user space do not directly correspond to the physical addresses used by the hardware.
• User virtual addresses:
Seen by the user space
• Physical addresses:
Used between the processor and the system's memory.
• Bus addresses:
Used between peripheral buses and memory. Often they are the same as the physical addresses used by the processor, but that is not necessarily the case.
• Kernel logical addresses:
These addresses map some portion (perhaps all) of main memory and are often treated as if they were physical addresses. On most architectures, logical addresses and their associated physical addresses differ only by a constant offset. Logical addresses use the hardware's native pointer size, so may be smaller than the whole physical address space. kmalloc() and __get_free_pages() return logical addresses.
• Kernel virtual addresses:
Kernel virtual addresses are a mapping from kernel-space addresses to physical addresses. Kernel virtual addresses do not necessarily have the linear, one-to-one mapping to physical addresses that like logical addresses do. All logical addresses are kernel virtual addresses, but many kernel virtual addresses are not logical addresses, like hign-mem addresses. vmalloc(), ioremap() and kmap() return the kernel virtual addresses.
	#include <asm/page.h>
	unsigned long __pa(void *kaddr); // convert a logical address to physical address
	void * __va(unsigned long physaddr); // convert a physical address back to logical address, but only for low-memory pages. (high memory physical address has no corresponding logical address)

• High and low memory:
The difference between logical and kernel virtual addresses is highlighted on 32-bit systems that are equipped with large amounts of memory. The kernel (on x86 architecture) splits the 4GB virtual address space between user-space and the kernel. A typical split is 3GB user space and 1GB kernel space. The kernel's code and data structures must fit in this 1GB, but the biggest consumer of kernel address space is virtual mappings for physical memory. The kernel cannot directly manipulate memory that is not mapped into the kernel's address space. The kernel, in other words, needs its own virtual address for any memory it must touch directly. So for some 32-bit systems that have large than 4GB memory, only the lowest portion of memory has logical addresses, the rest (high memory) doesn not. Before accessing a specific high-memory page, the kernel must set up an explicit virtual mapping to make that page available in the kernel's address space. (Thus many kernel data structures must be placed in low memory; high memory tends to be reserved for user space process pages.)
* Low memory:
Memory for which logical addresses exist in kernel space. On almost every system you will likely encounter, all memory is low memory.
* High memory:
Memory for which logical address doesn't exist.
On i386 systems, the boundary between low and high memory is usually set just under 1GB. This boundary is a limit set by the kernel itself as it splits 32-bit address space between kernel and user space.

• There is one struct page for each physical page on the system. 
	struct page *virt_to_page(void *kaddr); // Convert a logical address to its corresponding page structure.
	struct page *pfn_to_page(int pfn); // Return the page structure of the given page frame number (PFN).
	void *page_address(struct page *page); // Return the kernel virtual address (NOT logical address!) of this page. Only works for low memory and high memory that has been explicitly mapped.
	#include <linux/highmem.h>
	void *kmap(struct page *page); // Return a kernel virtual address for any page in the system. For low memory pages it just returns the logical address of that page; for high memory pages it creates a special mapping in a dedicated part of kernel address space. A number of such mappings is available, so it is better not to hold on to them for too long.
	void kunmap(struct page *page);
	#include <asm/kmap_types.h>
	void *kmap_atomic(struct page *page); // A high-performance form of kmap(). Each architecture maintains a small list of slots for atomic kmaps.  
	void kunmap_atomic(void *addr);

• Virtual memory areas (VMA):
Virtual memory area (VMA) is a kernel data structure used to manage distinct regions of a process's address space. The fields in /proc/<pid>/maps is like:
	start-end perm offset major:minor inode image
start, end: The beginning and ending virtual addresses for this memory area.
perm: This field describes what the process is allowed to do with pages belonging to this area. The last character in this field is either 'p' for private or 's' for shared.
offset: Where the memory area begins in the file that it is mapped to.
major, minor: The major and minor number of the device holding the file that has been mapped.
inode: The inode number of the mapped file.
image: The name of the file that has been mapped.
* The vm_area_struct structure:
See LKD/chapter_15 for more details.
Each process in the system (with exception of kernel threads) has a struct mm_struct that contains the process's list of virtual memory areas, page tables, and various other bits of memory management information, along with a semaphore (mmap_sem) and a spinlock (page_table_lock). Note that the mm_struct could be shared between processes, the linux implementation of threads works in this way.


2. The mmap device operation:
As far as drivers are concerned, memory mapping can be implemented to provide user programs with direct access to device memory. Whenever the program reads or writes in the assigned address range, it is actually accessing the device.
The mmap method is part of the file_operations structure and is invoked when the mmap syscall is issued. The mmap() in the file_operations structure has the prototype:
	int (*mmap)(struct file *file, struct vm_area_struct *vma);
	@vma contains the information about the virtual address range that is used to access the device. Therefore much of the work has been done by the kernel, to implement mmap, the driver only has to build suitable page tables for the address range, and if necessary, replace vma->vm_ops with a new set of operations.
There're 2 ways of building the page tables: i) doing it all at once with a function called remap_pfn_range(); or ii) doing it one page at a time via fault() VMA method.
i)  Using remap_pfn_range():
The job of building new page tables to map a range of physical addresses is handled by remap_pfn_range():
	int remap_pfn_range(struct vm_area_struct *vma, unsigned long virt_addr, unsigned long pfn, unsigned long size, pgprot_t prot);
	@vma: The virtual memory area into which the page range is being mapped.
	@virt_addr: The user virtual address where remapping should begin. The function builds page tables for the virtual address range between (virt_addr) ~ (virt_addr + size).
	@pfn: The page frame number corresponding to the physical address to which the virtual address should be mapped. The function affects physical addresses from:
		(pfn << PAGE_SHIFT) ~ (pfn << PAGE_SHIFT + size)
	@size: The size in bytes that the area being remapped.
	@prot: The "protection" requested for the new VMA. The driver can (and should) use the value found in vma->vm_page_prot.
• Adding VMA operations:
The open method is invoked anytime a process forks and creates a new reference to the VMA. Note the explicit call to simple_vma_open, since the open method is not invoked on the initial mmap, we must call it explicitly if we want it to run.

ii) Mapping memory with fault method:
	int (*fault)(struct vm_area_struct *vma, struct vm_fault *vmf);
	struct vm_fault {
		unsigned int flags; // FAULT_FLAG_XXX flags
		pgoff_t pgoff;  // logical page offset based on vma
		void __user *virtual_address; // The faulting user virtual address.
		struct page *page; // The fault handler should set a page here, unless VM_FAULT_NOPAGE is set.
	};
When a user process attempts to access a page in a VMA that is not present in memory, the associated fault method is called. fault() must set the struct page inside struct vm_fault that refers to the page the user wanted. It must also take care to increment the usage count for the page by calling get_page():
	void get_page(struct page *page);
This step is necessary to keep the reference count correct on the mapped pages. When a VMA is unmapped, the kernel decrements the usage count for every page in the area.
• If the fault() method is left NULL, kernel code that handles page fault maps the zero page to the faulting virtual address. The zero page is a copy-on-write page that reads as 0. If a process writes to the page, it ends up modifying a private copy. (Therefore, if a process extends a mapped region by calling mremap(), and the driver hasn't implemented fault handler, the process ends up with zero-filled memory instead of a segmentation fault.)

* Remapping RAM:
An interesting limitation of remap_pfn_range() is that it gives access only to reserved pages and physical addresses above the top of physical memory. In linux a page of physical addresses is marked as "reserved" in the memory map to indicate that it is not available for memory management. Reserved pages are locked in memory and are the only ones that can be safely mapped to user space. 
Therefore, remap_pfn_range() won't allow you to remap conventional addresses, which include the pages you obtain by calling __get_free_pages().
The way to map real RAM to user space is to use vm_ops->fault to deal with page fault one at a time. (Don't quite understand about this part.)

* Remapping kernel virtual addresses:
Although it is really rare. 
	struct page *vmalloc_to_page(void *vmaddr); // convert a kernel virtual address to its corresponding page structure.
You should use remap_pfn_range() to remap I/O memory areas into user space. Addresses from ioremap are special and cannot be treated like normal kernel virtual addresses. (You should use readb and other I/O functions introduced in chapter 9.)

3. Performing direct I/O:
There are cases where it can be beneficial to perform I/O directly to or from user-space buffer. If the amount of data being transferred is large, transferring data directly without an extra copy through kernel space can speed up. But note that direct I/O doesn't always provide the performance boost that one might expect. The overhead of setting up direct I/O (which involves faulting in and pinning down the relevant user pages) can be significant, and the benefits of buffered I/O are lost. 
Applications that use direct I/O often use asynchronous I/O operations as well. The key to implementing direct I/O in the 2.6 kernel is a function called: get_user_pages():
	int get_user_pages(struct task_struct *tsk, struct mm_struct *mm, unsigned long start, int len, int write, int force, struct page **pages, struct vm_area_struct **vmas);
	@tsk: A pointer to the task performing the I/O. This argument is almost always passed as current.
	@mm: A pointer to the memory management structure describing the address space to be mapped. For driver use, this argument should always be current->mm.
	@start: The page-aligned address of the user-space buffer.
	@len: The length of the buffer in PAGE_SIZE.
	@write: If write is nonzero, the pages are mapped for write access (that user space is performing a read operation).
	@force: If set, tells get_user_pages() to override the protections on the given pages to provide the requested access. Drivers should always pass 0 to it.
	@pages, @vmas: Output parameters. Upon successful completion, pages contain a list of pointers to the struct page structures describing the user-space buffer. And @vmas contains pointers to the associated VMAs. The parameters should point to arrays capable of holding at least @len pointers. Either parameter can be NULL, but you need at least the struct page pointers to actually operate on the buffer.
This function also requires the mmap rw-semaphore for the address space be acquired in read mode before the call.
The return value is the number of pages actually mapped. Upon successful completion, the caller has a pages array pointing to the user-space buffer, which is locked into memory. To operate on the buffer directly, the kernel-space code must turn each struct page pointer into a kernel virtual address with kmap() or kmap_atomic(). Usually, devices for which direct I/O is justified are using DMA operations, so your driver will probably want to create a scatter/gather list from the array of struct page pointers. See the following section "Scatter/gather mappings".
• Once the direct I/O operation is complete, you must release the user pages. Before doing so, you might need to inform the kernel if you changed the contents of those pages, otherwise the kernel may free the pages without writing them out to the backing store:
	#include <linux/page-flags.h>  // see TESTPAGEFLAG() macro
	if (!PageReserved(page))  // Most of the code that performs this operation checks first to ensure that the page is not in the reserved part of memory map, which is never swapped out.
		set_page_dirty(page);
	
	void page_cache_release(struct page *page); // Regardless of whether the pages have been changed, they must be freed from the page cache.

* Asynchronous I/O:
Asynchronous I/O allows user space to initiate operations without waiting for their completion. Block and network drivers are fully asynchronous at all times, so only char drivers are candidates for explicit asynchronous I/O support. A char device can benefit from this support if there are good reasons for having more than one I/O operation outstanding at any given time.
Drivers supporting asynchronous I/O should include <linux/aio.h>. There're 3 file_operations methods for the implementation of asynchronous I/O:
	ssize_t (*aio_read)(struct kiocb *iocb, const struct iovec *iovec, unsigned long count, loff_t offset);
	ssize_t (*aio_write)(struct kiocb *iocb, const struct iovec *iovec, unsigned long count, loff_t offset);
	int (*aio_fsync)(struct kiocb *iocb, int datasync);
The purpose of aio_read and aio_write methods is to initiate a read or write operation that may or may not be complete by the time they return. So it is entirely correct for the aio_read to perform like read, although it is useless.
• If you support asynchronous I/O, you must be aware of the fact that the kernel can, on occasion, create "synchronous IOCBs". That is to say, asynchronous operations that must actually be executed synchronously. Your driver can query the status with: 
	int is_sync_kiocb(struct kiocb *iocb); // If it returns a nonzero value, your driver must execute the operation synchronously.
If your driver is able to initiate the asynchronous operation, it must do two things: i) remembering everything it needs to know about the operation, and return -EIOCBQUEUED to the caller. ii) When "later" comes, your driver must inform the kernel that the operation has completed. This is done with a call to aio_complete:
	int aio_complete(struct kiocb *iocb, long res, long res2); // res is the usual result status for the operation; res2 is a second result code that will be returned to user space. Most asynchronous I/O implementations pass res2 as 0. Once you call aio_complete, you should not touch the IOCB or user buffer again.


4. Direct memory access:
DMA is the hardware mechanism that allows peripheral components to transfer their I/O data directly to and from main memory without the need to involve the processor. The efficient DMA handling relies on interrupt reporting. DMA also requires device drivers to allocate one or more special buffers suited to DMA.
• Allocating the DMA buffer:
When the DMA buffer is bigger than one page, they must occupy contiguous pages in physical memory, because the device transfers data using ISA or PCI buses, both of which carry physical addresses. 
• DMA-based hardware uses bus addresses rather than physical address. Although ISA and PCI bus addresses are simply physical addresses on the PC, this is not true for every platform. The kernel provides a portable solution:
	#include <asm/io.h>
	unsigned long virt_to_bus(volatile void *address); // convert kernel logical address to bus address.
	void *bus_to_virt(unsigned long address);
They are discouraged to use, because they do not work in any situation where an I/O memory management unit must be programmed or where bounce buffers must be used. The right way of performing this conversion is with the generic DMA layer.

• The generic DMA layer:
i)  Dealing with difficult hardware:
By default, the kernel assumes your device can perform DMA to any 32-bit address. If this is not the case, you must inform the kernel:
	#include <linux/dma-mapping.h>
	int dma_set_mask(struct device *dev, u64 mask); // If your device is limited to 24 bits, you should pass mask as 0x00FFFFFF.
It returns nonzero if DMA is possible with the given mask; otherwise, you are not able to use DMA operations with this device.
ii) DMA mappings:
• A DMA mapping is a combination of allocation a DMA buffer and generating an address for that buffer that is accessible by the device.
• Setting up a useful address for the device may also, in some cases, require the establishment of a bounce buffer. Bounce buffer is created when a driver attempts to perform DMA on an address that is not reachable by the peripheral device. Data is then copied to and from the bounce buffer as needed.
• The PCI code distinguishes between two types of DMA mappings, depending on how long the DMA buffer is expected to stay around:
i)  Coherent DMA mappings:
These mappings usually exist for the life of the driver. It can be expensive to set up and use.
ii) Streaming DMA mappings:
Streaming mappings are usually set up for a single operation. Some architectures allow significant optimizations when streaming mappings are used.

i)  Setting up coherant DMA mappings:
	void *dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle, gfp_t flag); // @size: the size of the buffer needed. 
	The return value is the kernel virtual address for the buffer; the associated bus address is stored in @dma_handle.
	void dma_free_coherent(struct device *dev, size_t size, void *vaddr, dma_addr_t dma_handle); // return the buffer to the system; note that the size, kernel virtual address, bus address should be same as dma_alloc_coherent.
• DMA pools:
A DMA pool is an allocation mechanism for small, coherent DMA mappings. Note that mappings obtained from dma_alloc_coherent() may have a minimum size of one page. If you need smaller DMA areas than that, you should use a DMA pool.
	struct dma_pool *dma_pool_create(const char *name, struct device *dev, size_t size, size_t align, size_t allocation);
	@size: the size of the buffer to be allocated from this pool.
	@align: the required hardware alignment for allocations from the pool in bytes.
	@allocation: if nonzero, a memory boundary that allocations should not exceed.
	void dma_pool_destroy(struct dma_pool *pool);
	void *dma_pool_alloc(struct dma_pool *pool, gfp_t mem_flags, dma_addr_t *handle); // a region of memory (of the size specified when the pool is created) is allocated and returned. As with dma_alloc_coherent(), the address of the resulting DMA buffer is returned as the kernel virtual address and bus address is stored in handle.
	void dma_pool_free(struct dma_pool *pool, void *vaddr, dma_addr_t addr);

ii) Setting up streaming DMA mappings:
When setting up a streaming mapping, you must tell the kernel in which direction the data is moving: enum dma_data_direction
DMA_TO_DEVICE
DMA_FROM_DEVICE
DMA_BIDIRECTIONAL: data can moved in either direction.
DMA_NONE: only as a debugging aid.
When you have a single buffer to transfer, use:
	dma_addr_t dma_map_single(struct device *dev, void *buffer, size_t size, enum dma_data_direction direction);
* Note that once the buffer has been mapped, it belongs to the device, not the processor. Until the buffer has been unmapped, the driver should not touch its contents in any way. Only after dma_unmap_single() has been called is it safe for the driver to access the contents of the buffer.
* Occasionally a driver needs to access the contents of a streaming DMA buffer without unmapping it:
	void dma_sync_single_for_cpu(struct device *dev, dma_handle_t bus_addr, size_t size, enum dma_data_direction direction); // This function should be called before the processor accesses a streaming DMA buffer. Once the call is made, the CPU owns the DMA buffer and can work with it as needed.
	void dma_sync_single_for_device(struct device *dev, dma_handle_t bus_addr, size_t size, enum dma_data_direction direction);

• Single-page streaming mappings:
Occasionally you may want to set uo a mapping on a buffer for which you have a struct page pointer (This can happen with user-space buffers mapped with get_user_pages). You could use the following:
	dma_addr_t dma_map_page(struct device *dev, struct page *page, unsigned long offset, size_t size, enum dma_data_direction direction);
	void dma_unmap_page(struct device *dev, dma_addr_t dma_address, unsigned long offset, size_t size, enum dma_data_direction direction);
	The @offset and @size arguments can be used to map part of a page, but this is not recommended.

• Scatter/gather mappings:
Scatter/gather mapping is a special type of streaming DMA mapping. Many devices can accept a scatterlist of array pointers and lengths, and transfer them all in one DMA operation. 
	#include <asm/scatterlist.h>
	struct scatterlist { // see the source code for more details
		unsigned long page_link; // contains the struct page * pointer corresponding to the buffer to be used in the scatter/gather operation.
		unsigned int offset;
		unsigned int length; // the length of the buffer and its offset with in the page
		dma_addr_t dma_address;
	};
	dma_addr_t sg_dma_address(struct scatterlist *sg); // return the bus address from this scatter list entry.
	unsigned int sg_dma_len(struct scatterlist *sg); // return the length of this buffer.
	int dma_map_sg(struct device *dev, struct scatterlist *sg, int nents, enum dma_data_direction direction);
	void dma_unmap_sg(struct device *dev, struct scatterlist *list, int nents, enum dma_data_direction direction);
	void dma_sync_sg_for_cpu(struct device *dev, struct scatterlist *sg, int nents, enum dma_data_direction direction);
	void dma_sync_sG_for_device(struct device *dev, struct scatterlist *sg, int nents, enum dma_data_direction direction);

• A simple PCI DMA example
See LDD3 P453.

• DMA for ISA devices:
See LDD3 for more details.
