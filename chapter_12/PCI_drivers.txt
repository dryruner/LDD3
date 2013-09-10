1. PCI (Peripheral Component Interconnect) interface:
PCI is actually a set of specifications defining how different parts of a computer should interact.
• PCI addressing:
Each PCI peripheral is identified by a bus number, a device number, and a function number. Each PCI domain can host up to 256 buses, each bus hosts up to 32 devices, and each device can be a multifunction board with a maximum of eight functions.
• lspci, /proc/bus/pci/devices, /sys/bus/pci/devices/
• The hardware circuitry of each peripheral board answers queries pertaining to three address spaces: i) memory locations, ii) I/O ports, iii) configuration registers. The first two address spaces are shared by all the devices on the same PCI bus. Configuration queries address only one slot at a time, and they never collide. 
* Memory and I/O regions are accessed in the usual ways via inb, ioread8, and so forth. Configuration transactions, on the other hand, are performed by calling specific kernel functions to access configuration registers.
* Every PCI slot has four interrupt pins, and PCI specification requires interrupt lines to be shareable.
* The I/O space in a PCI bus uses a 32-bit address bus, while the memory space can be accessed with either 32-bit or 64-bit addresses.
* PCI configuration space consists of 256 bytes for each device function, and the layout of the configuration registers is standardized.

2. Configuration registers and initialization:
All PCI devices feature at least 256-byte address space, the first 64 bytes are standardized, while the rest are device dependent. See LDD3 P308 for more details. 
• Note that the PCI registers are always little-endian.
	#include <linux/pci.h>
	struct pci_dev; // structure that represents a PCI device within the kernel
	struct pci_driver; // structure that represents a PCI driver. All PCI drivers must define this.
	struct pci_device_id; // structure that describes the types of PCI devices this driver supports.

• MODULE_DEVICE_TABLE:
This pci_device_id structure needs to be exported to user space to allow the hotplug and module loading systems know what module works with what hardware devices. The macro MODULE_DEVICE_TABLE accomplishes this.
• Registering a PCI driver:
	int pci_register_driver(struct pci_driver *drv);
	int pci_module_init(struct pci_driver *drv);
	void pci_unregister_driver(struct pci_driver *drv);
• PCI probing:
	struct pci_dev *pci_get_device(unsigned int vendor, unsigned int device, struct pci_dev *from); // scans the list of PCI devices currently present in the system, and if the input argument match the specified @vendor and @device IDs, it increments the count on the struct pci_dev variable found.
	struct pci_dev *pci_get_subsys(unsigned int vendor, unsigned int device, unsigned int ss_vendor, unsigned int ss_device, struct pci_dev *from); // This function also allows the subsystem vendor and subsystem device IDs to be specified when looking for device.
	struct pci_dev *pci_get_slot(struct pci_bus *bus, unsigned int devfn); // see source code and document.
Note that all of these functions cannot be called from interrupt context, if they are, a warning is printed out to the system log.
• Enabling the PCI device:
In the probe function for the PCI driver, before the driver can access any device resource (I/O region or interrupt) of the PCI device, the driver must call:
	int pci_enable_device(struct pci_dev *dev);
• Accessing the configuration space:
Linux offers standard interface to access the configuration space:
	#include <linux/pci.h>
	int pci_read_config_byte(struct pci_dev *dev, int where, u8 *val);
	int pci_read_config_word(struct pci_dev *dev, int where, u16 *val);
	int pci_read_config_dword(struct pci_dev *dev, int where, u32 *val);
	// Read 1, 2, or 4 bytes from the configuration space of the device identified by @dev. @where is the byte offset from the beginning of the configuration space. The return value is an error code. The word and dword functions convert the value to little-endian before writing to the peripheral device.
	int pci_write_config_byte(struct pci_dev *dev, int where, u8 val);
	int pci_write_config_word(struct pci_dev *dev, int where, u16 val);
	int pci_write_config_dword(struct pci_dev *dev, int where, u32 val);
* All of the previous functions are implemented as inline functions that really call the following functions:
	int pci_bus_read_config_byte(struct pci_bus *bus, unsigned int devfn, int where, u8 *val);
	int pci_bus_read_config_word(struct pci_bus *bus, unsigned int devfn, int where, u16 *val);
	int pci_bus_read_config_dword(struct pci_bus *bus, unsigned int devfn, int where, u32 *val);
	int pci_bus_write_config_byte(struct pci_bus *bus, unsigned int devfn, int where, u8 val);
	int pci_bus_write_config_word(struct pci_bus *bus, unsigned int devfn, int where, u16 val);
	int pci_bus_write_config_dword(struct pci_bus *bus, unsigned int devfn, int where, u32 val);

• Accessing thr I/O and memory spaces:
	unsigned long pci_resource_start(struct pci_dev *dev, int bar); // returns the first address (memory address or I/O port number) associated with one of the six PCI I/O regions. The region is selected by the integer bar, ranging from 0-5.
	unsigned long pci_resource_end(struct pci_dev *dev, int bar);
	unsigned long pci_resource_flags(struct pci_dev *dev, int bar); // returns the flags associated with this resource. All resource flags are defined in <linux/ioport.h>, including: IORESOURCE_IO, IORESOURCE_MEM, IORESOURCE_PREFETCH, IORESOURCE_READONLY, etc.

• PCI interrupts:
By the time linux boots, firmware has already assigned a unique interrupt number to the device, and the driver just needs to use it. The interrupt number is stored in configuration register 60 (PCI_INTERRUPT_LINE), which is one byte wide.
