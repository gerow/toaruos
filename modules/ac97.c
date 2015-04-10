/*
 * AC'97 Audio driver
 */

#include <module.h>
#include <printf.h>
#include <pci.h>
#include <mod/shell.h>

struct ac97_device {
	uint32_t pci_device;
};

static struct ac97_device _device;

static void find_ac97(uint32_t device, uint16_t vendorid, uint16_t deviceid, void * extra) {

	struct ac97_device * ac97 = extra;

	if ((vendorid == 0x8086) && (deviceid == 0x2415)) {
		ac97->pci_device = device;
	}

}

DEFINE_SHELL_FUNCTION(ac97_test, "[debug] AC'97 experiments") {

	if (!_device.pci_device) {
		fprintf(tty, "No AC'97 device found.\n");
		return 1;
	}
	fprintf(tty, "AC'97 audio device is at 0x%x.\n", _device.pci_device);

	return 0;
}

DEFINE_SHELL_FUNCTION(ac97_bar, "[debug] AC'97 requested BAR sizes") {
	uint32_t orig = pci_read_field(_device.pci_device, PCI_BAR0, 4);
	pci_write_field(_device.pci_device, PCI_BAR0, 4, -1);
	uint32_t space = ~(pci_read_field(_device.pci_device, PCI_BAR0, 4) >> 1) + 1;
	fprintf(tty, "AC'97 BAR space is %x.\n", space);
	pci_write_field(_device.pci_device, PCI_BAR0, 4, orig);
	uint32_t irq = pci_read_field(_device.pci_device, PCI_INTERRUPT_LINE, 1);
	fprintf(tty, "irq: %d.\n", irq);

	return 0;
}

static int init(void) {
	BIND_SHELL_FUNCTION(ac97_test);
	BIND_SHELL_FUNCTION(ac97_bar);
	pci_scan(&find_ac97, -1, &_device);
	if (!_device.pci_device) {
		return 1;
	}
	return 0;
}

static int fini(void) {
	return 0;
}

MODULE_DEF(ac97, init, fini);
MODULE_DEPENDS(debugshell);
