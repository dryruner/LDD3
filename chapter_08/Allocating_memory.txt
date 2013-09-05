1. The real story of kmalloc:
Kernel tries to keep several free pages in order to fulfill atomic allocation, if GFP_ATOMIC flag is set, kernel could even use the last free page. If that last page doesn't exist, the allocation fails.
GFP_* flags refer to LKD chapter12 for more details.
__GFP_HIGH: marks a high-priority request, which is allowed to consume even the last pages of memory set aside by the kernel for emergencies.
__GFP_REPEAT: if the allocation fails, try again once, but the allocation may still fail.
__GFP_NOFAIL: tries to never fail, strongly discouraged to use this.
__GFP_NORETRY: never retry when the first allocation fails.

• Memory zones:
__GFP_DMA and __GFP_HIGHMEM: 
Linux kernel knows a minimum of three memory zones: i) DMA-capable memory, ii) normal memory, iii) high memory.
High memory is a mechanism used to allow access to large amount of memory on 32-bit platforms. This memory cannot be directly accessed from kernel without first setting up a special mapping and is generally harder to work with. But if your driver uses large amounts of memory, it will work better on large systems if it can use high memory.
If __GFP_DMA is specified, only DMA zone is searched when allocating memory, if no memory is available at low addresses, allocation fails. If none of the two flags are set, allocation searches from both DMA zone and normal zone. If __GFP_HIGHMEM is set, all three zones are used to search for free pages. (Note, however, kmalloc() and __get_free_pages() cannot specify __GFP_HIGHMEM, because they both return a logical address, not a page structure. And since high memory may not be mapped in the kernel address space, and thus doesn't has a logical address. See LKD.)


2. Lookaside caches: (The slab allocator layer, also see LKD chapter 12)
	struct kmem_cache* kmem_cache_create(const char *name, size_t size, size_t align, unsigned long flags, void (*ctor)(void *));
The function creates a new cache object that host any number of memory areas all of the same size, specified by the @size field. @align field is the offset of the first object in the slab, which is used to ensure a particular alignment within the page. You may most likely to use 0 of this field.
@flags field: see LKD p249 for more details, LDD3 memtioned these flags:
* SLAB_HWCACHE_ALIGN: This flag requires each data object to be aligned to a cache line. This option can be a good choice if your cache contains items that are frequently accessed on SMP machines. The padding required to achieve cache line alignment can end up wasting significant amounts of memory.
* SLAB_CACHE_DMA: This flag requires each data object to be allocated in the DMA zone.
More like SLAB_POISON and SLAB_RED_ZONE, etc see LKD p249 ~ p250 for more details.

* Once a cache of objects is created, you can allocate objects from it by calling kmem_cache_alloc():
	void *kmem_cache_alloc(struct kmem_cache *cachep, gfp_t flags);
	void kmem_cache_free(struct kmem_cache *cachep, void *objp);
	int kmem_cache_destroy(struct kmem_cache *cachep);
• One side benefit to using slab allocator is that: kernel maintains statistics on cache usage. These statistics could be obtained from /proc/slabinfo.


3. Memory pools (<linux/mempool.h>):
There're places in the kernel where memory allocations cannot be allowed to fail. So kernel developers created an abstraction known as a memory pool, which is just a form of lookaside cache that tries to always keep a list of free memory around for use in emergencies.
A memory pool can be created by:
	mempool_t *mempool_create(int min_nr, mempool_alloc_t *alloc_fn, mempool_free_t *free_fn, void *pool_data);
@min_nr: the minimum number of allocated objects that the pool should always keep around
@alloc_fn: typedef void *(mempool_alloc_t)(gfp_t gfp_mask, void *pool_data);
@free_fn: typedef void (mempool_free_t)(void *element, void *pool_data);
@pool_data: this argument is passed to alloc_fn and free_fn.
The actual allocation and freeing objects are handled by alloc_fn and free_fn. If needed, you can write special-purpose functions to handle memory allocations for mempools. But usually you just want to let the kernel slab allocator to handle that task for you. (use mempool_alloc_slab and mempool_free_slab) So the code that sets up memory pools often looks like the following:
	cache = kmem_cache_create(...);
	pool = mempool_create(MY_POOL_MINIMUM, mempool_alloc_slab, mempool_free_slab, cache);

* Once the pool has been created, objects can be allocated and freed with:
	void *mempool_alloc(mempool_t *pool, gft_t gfp_mask);
	void mempool_free(void *element, mempool_t *pool);
When the mempool is created, the allocation function will be called enough times to create a poll of pre-allocated objects. Call to mempool_alloc attempts to acquire additional objects from the allocation function; should that allocation fail, one of the pre-allocated objects (if any remain) is returned. When an object is freed with mempoll_free, it is kept in the pool if the number of pre-allocated objects is currently below the minimum; otherwise, it is to be returned to the system.

* A mempool can be resized with:
	int mempool_resize(mempool_t *pool, int new_min_nr, gfp_t gfp_mask);
This call, if successful, resize the pool to have at least new_min_nr objects.

* A memory pool could be returned to the system if no longer needed by:
	void mempool_destroy(mempool_t *pool);
Note that you must return all allocated objects before destroying the mempool, or a kernel oops happens.

• Note that mempool allocate a chunk of memory that sits in a list, idle and unavailable for any real use. The preferred alternative is to do without mempool and simply deal with the possibility of allocation failures instead. Use of mempools in driver code should be rare.


4. get_free_page family (also see LKD chapter 12):
	unsigned long get_zeroed_page(gfp_t gfp_mask);
	unsigned long __get_free_page(gfp_t gfp_mask);
	unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order);
	void free_page(unsigned long addr);
	void free_pages(unsigned long addr, unsigned int order);
* Note that the maximum value of @order is 10 or 11. /proc/buddyinfo tells how many blocks of each order are available for each memory zone on the system.

• The alloc_pages interface:
struct page is an internal kernel structure that describes every physical page. It is especially useful where you might be dealing with high memory, which doesn't have a constant address in kernel space.
The core of linux page allocator is:
	struct page *alloc_page_node(int nid, gfp_t gfp_mask, unsigned int order);
	struct page *alloc_pages(gfp_t gfp_mask, unsigned int order);
	struct page *alloc_page(gfp_t gfp_mask);
The core function alloc_pages_node() takes three arguments. nid is the NUMA node ID* where memory should be allocated. (NUMA (nonuniform memory access) computers are multiprocessor systems where memory is "local" to specific groups of processors ("nodes"). Access to local memory is faster than access to nonlocal memory. On such systems, allocating memory on the correct node is important. Driver author do not normally have to worry about NUMA issues, however.)

* To release pages allocated in this manner:
	void __free_page(struct page *page);
	void __free_pages(struct page *page, unsigned int order);
	void free_hot_page(struct page *page);
	void free_cold_page(struct page *page);
If you have specific knowledge of whether a single page's contents are likely to be resident in the processor cache, you could use free_hot_page (for cache-resident pages) or free_cold_page.


5.  vmalloc family:
	#include <linux/vmalloc.h>
	void *vmalloc(unsigned long size);
	void *ioremap(unsigned long offset, unsigned long size);
	void iounmap(void *addr);
vmalloc allocates a contiguous memory region in the virtual address space, each page is retrieved with a separate call to alloc_page, and each allocation builds the virtual memory area by suitably setting up the page tables. (And you could compare the differences of the pointer returned by kmalloc and vmalloc. Addresses available for vmalloc are in the range from VMALLOC_START to VMALLOC_END. Both symbols are defined in <asm/pgtable.h>)

• Addresses allocated by vmalloc can't be used outside of the microprocessor because they make sense only on top of the processor's MMU. When a driver needs a real physical address (such as DMA address), you cannot easily use vmalloc. The right time to use vmalloc is when you are allocating memory for a large sequential buffer that exists only in software. It's important to note that vmalloc has more overhead than __get_free_pages, because it must both retrieve memory and build the page tables. Therefore, it doesn't make sense to call vmalloc to allocate just one page. (One example of using vmalloc is the create_module() syscall.)
• ioremap builds new page tables, but it doesn't actually allocate any memory. The return value of ioremap() is a special virtual address that can be used to access the specified physical address range; the virtual address obtained is eventually released by calling iounmap(). ioremap is most useful for mapping the physical address of a PCI buffer to virtual kernel space. Such buffers are usually mapped at high physical addresses, outside of the address range for which the kernel builds page tables at boot time. It's worth noting that for the sake of portability, you shouldn't directly access addresses returned by ioremap. You should always use readb and the other I/O functions introduced in chapter 9.
(ioremap() remaps a physical address range into the processor's virtual address space, making it available to the kernel.)
• One minor drawback of vmalloc is that it can't be used in atomic context because internally it uses kmalloc(GFP_KERNEL) to acquire storage for the page tables, and therefore could sleep.
• vmalloc is faster than other functions in allocating several pages, but somewhat slower when retrieving a single page. (Allocating more than one page with __get_free_pages() is failure prone, and even when it succeeds, it can be slow.)


6. Per-CPU variables (See LKD chapter 12):
When you create a per-CPU variable, each processor on the system gets its own copy of that variable. 
To create a per-CPU variable at compile time, use this macro:
	DEFINE_PER_CPU(type, name);
If the variable is an array, for example, a per-CPU array of three integers would be created with:
	DEFINE_PER_CPU(int[3], my_percpu_array);
To use per-CPU variables, i) it's not good for a processor to be preempted in the middle of a critical section that modifies a per-CPU variable; ii) it's also not good if your process were to be moved to another processor in the middle of a per-CPU variable access. So you must explicitly use the get_cpu_var() macro to access the current processor's copy of a given variable, and call put_cpu_var() when done.
• get_cpu_var() returns a lvalue for the current processor's version of the variable and disables preemption. (Since lvalue is returned, it can be assigned to or operated on directly, like get_cpu_var(sockets_in_use)++). You can access abother processor's copy of the variable with: per_cpu(variable, int cpu_id);

• Dynamically allocated per-CPU variables are also possible:
	void *alloc_percpu(type);
	void *__alloc_percpu(size_t size, size_t align); // if a particular alignment is required.
	void free_percpu(const void *);
Access to a dynamically allocated per-CPU variable is done via per_cpu_ptr():
	per_cpu_ptr(void *per_cpu_var, int cpu_id);
If the entirety of your access to the per-CPU variable happens with a spinlock held, all is well. However, you need to use get_cpu() to disabl preemption while working with the variable: (also see LKD chapter 12)
	int cpu;
	cpu = get_cpu();
	ptr = per_cpu_ptr(per_cpu_var, cpu);
	/* work with ptr */
	put_cpu();
• Per-CPU variables can be exported to modules using:
	EXPORT_PER_CPU_SYMBOL(per_cpu_var);
	EXPORT_PER_CPU_SYMBOL_GPL(per_cpu_var);
To access such a variable within a module, declare it with:
	DECLARE_PER_CPU(type, name); // resemble using "extern"
• If you want to use per-cpu variables to create a simple integer counter, see <linux/percpu_counter.h>. Note that some archistectures have a limited amount of address available for per-CPU variables. So you should try to keep them small.


7. Obtaining large buffers:
• Allocations of large, contiguous memory buffers are prone to failure. Before you try to obtain a large memory area, you should really consider the alternatives. 
• If you really need a huge buffer of physically contiguous memory, the best approach is often to allocate it by requesting memory at boot time. (So a module can't allocate memory at boot time, only drivers directly linked to the kernel can do that.)
• Boot-time memory allocation is performed by calling:
	#include <linux/bootmem.h>
	void *alloc_bootmem(unsigned long size);
	void *alloc_bootmem_low(unsigned long size);
	void *alloc_bootmem_pages(unsigned long size);
	void *alloc_bootmem_low_pages(unsigned long size);
The functions allocate either whole pages (if end with "_pages", or non-page-aligned memory areas.) The allocated memory may be high memory, unless one of the "_low" versions is used. And if you're allocating buffer for a device driver, you probably want to use it for DMA operation, so you probably want to use one of the "_low" variants.

It's rare to free memory allocated at boot time, because you'll almost not be able to get it back later if you want it. But here's still an interface to free this memory:
	void free_bootmem(unsigned long addr, unsigned long size);
* Note that partial pages freed in this manner are not returned to the system. And if you must use boot-time allocation, you need to link the driver directly into the kernel. See Documentation/kbuild for more information about how to do it.
