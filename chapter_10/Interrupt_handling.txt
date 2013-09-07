1. Installing an interrupt handler:
Interrupt lines are a precious and often limited resource. A module is expected to request an interrupt channel before using it and to release it when finished.
	#include <linux/interrupt.h>
	int request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags, const char *name, void *dev);
	@irq: the interrupt number being requested.
	@handler: typedef irqreturn_t (*irq_handler_t)(int, void *); the interrupt handler function, (upper-half)
	@flags: see LKD/chapter07, include IRQF_DISABLED, IRQF_SAMPLE_RANDOM, IRQF_TIMER, IRQF_SHARED, etc.
	@name: the string name used in /proc/interrupts to show the owner of the interrupt.
	@dev: the pointer used for shared interrupt lines. If interrupt line is not shared, it could be set as NULL. But it's a good idea to set dev to the device structure.
* The interrupt handler can be installed either at driver initialization or when the device is first opened. Although installing within the module's initialization function may sound like a good idea, but it often isn't. Requesting the interrupt at device open allows some sharing of resources.
	int can_request_irq(unsigned int irq, unsigned long flags); // not recommend to use, because of TOCTTOU bug.

* The /proc interface:
i)  /proc/interrupts: see LKD/chapter07 for more details.
ii) /proc/stat: see the line begins with "intr". The 1st number is the total of all interrupts, while each of the others represents a single IRQ line, starting with interrupt 0. All of the counts are summed across all processors in the system.

2. Autodetecting the IRQ number:
One of the most challenging problems for the driver at initialization time can be how to determine which IRQ line is going to be used by the device. Aotodetection of the interrupt number is a basic requirement for driver usability.
Some devices are more advanced in design and simply "announce" which interrupt they're going to use. In this case the driver retrieves the interrupt number by reading a status byte from one of the device's I/O ports or PCI configuration space. 
But not every device is programmer friendly, and autodetection might require some probing. The technique is quite simple: the driver tells the device to generate interrupts and watch what happens.

i)  Kernel-assisted probing:
Linux kernel offers a low-level facility for probing the interrupt number. It works for only nonshared interrupts; but most hardware that is capable of working in a shared interrupt mode provides better ways of finding the configured interrupt number anyway. 
	#include <linux/interrupt.h>
	unsigned long probe_irq_on(void); // this function returns a bit mask of unassigned interrupts, the driver must pass the return value to probe_irq_off() later. After this call, the driver should arrange for its device to generate at least one interrupt.
	int probe_irq_off(unsigned long); // After the device has requested an interrupt, the driver calls this function. It returns the number of the interrupt that was issued after "probe_on". If no interrupts occured, return 0; if more than one interrupt occured (ambiguous detection), it returns a negative value.
• The procedure of probing: (see <linux/interrupt.h> comment of probe_irq_on())
* Clear and/or mask the device's internal interrupts.
* sti();
* irqs = probe_irq_on();
* enable the device and cause it to trigger an interrupt;
* wait for the device to interrupt, using polling or delay;
* irq = probe_irq_off(irqs);
* service the device to clear its pending interrupt.
* loop again if necessary.

• Probing might be a lengthy task, therefore it's best to probe for the interrupt line only once, at the module initialization; independently of whether you install the handler at device open (as you should) or within the initialization function.

ii) Do-it-yourself probing:
The mechanism is the same as the one described above: enable all unused interrupts, then wait to see what happens. Often a device can be configured to use one IRQ number from a set of three or four, then just need to probe those IRQs. See LDD3 P267 ~ P268 for more details.
But you might not know in advance what the possible IRQ values are. In this case, you need to probe all the free interrupts. You have to probe from IRQ 0 to IRQ (NR_IRQS-1), where NR_IRQ is defined in <asm/irq.h>.

3. Implementing a handler:
Interrupt handler runs in interrupt context and thus has several restrictions:
i)   A IRQ handler can't transfer data to or from user space, because it doesn't execute in the context of a process.
ii)  Handlers cannot do anything that would sleep.
iii) Handlers cannot call schedule() family.
• Handler arguments and return value: typedef irqreturn_t (*irq_handler_t)(int irq, void *dev);
@int irq: interrupt number.
@void *dev: Usually passing a pointer to the device data structure, so a driver can manage several instances of the same device.
• Return value:
Interrupt handler should return a value indicating whether there was actually an interrupt to handle (IRQ_HANDLED) or not (IRQ_NONE).

4. Enabling and disabling interrupts:
Often, interrupts must be blocked while holding a spinlock to avoid deadlocking the system. 
i)  Disabling a single interrupt line (for the whole system):
	void disable_irq(int irq);
	void disable_irq_nosync(int irq);
	void enable_irq(int irq);
Note that the use of these APIs is discouraged because you cannot disable shared interrupt lines.
Also note that these functions can be nested. For each call to disable_irq() or disable_irq_nosync() on a given interrupt line, only on the last call to enable_irq() is the interrupt line actually enabled.

ii) Disabling all the interrupts for a local processor:
	void local_irq_save(unsigned long flags);
	void local_irq_disable(void);
	void local_irq_restore(unsigned long flags);
	void local_irq_enable(void);
Note that if more than one function in the call chain might need to disable interrupts, local_irq_save() should be used. See LKD/chapter07 for more details.

5. Top and bottom halves:
• The big difference between the top half and bottom half is that all interrupts are enabled during execution of the bottom half.
• Tasklets are often preferred mechanism for bottom-half processing: they're very fast, but all tasklet code must be atomic. The alternative to tasklet is workqueue, which may have a higher latency but workqueue is allowed to sleep.
• A device may generate a great many interrupts in a brief period, so it is common for several to arrive before the bottom half is executed. Drivers must always be prepared for this possibility and must be able to determine how much work there is to perform from the information left by the top half.

6. Interrupt sharing:
• Installing a shared handler:
i)   IRQF_SHARED must be set in the flags argument of request_irq(). All drivers sharing the interrupt line must all specify IRQF_SHARED.
ii)  The dev argument must be unique to each registered handler.
iii) The interrupt handler must be able to differentiate whether its device actually generated an interrupt.
• No probing function is available for shared handlers.
• You also couldn't diable/enable the shared interrupt line.
• When the kernel receives an interrupt, all the registered handlers are invoked. A shared handler must be able to distinguish between interrupts that it needs to handle and interrupts generated by other devices.

7. Interrupt-driven I/O:
LDD3 P284: "There's no point in awakening a writer every time we take one byte out of the buffer; instead, we should wait until that process is able to move a substantial amount of data into the buffer at once." This technique is common in buffering, interrupt-driven drivers.
