/*
 * Intel AC'97 Driver
 */

#include <logging.h>
#include <module.h>
#include <mod/shell.h>
#include <printf.h>
#include <pci.h>
#include <system.h>

struct ac97_device {
	uint32_t pci_device;
	int irq;
};

static struct ac97_device _device;

static void find_ac97(uint32_t device, uint16_t vendorid, uint16_t deviceid, void * extra) {

	struct ac97_device * ac97 = extra;

	if ((vendorid == 0x8086) && (deviceid == 0x2415)) {
		ac97->pci_device = device;
	}

}

DEFINE_SHELL_FUNCTION(ac97_test, "[debug] Intel AC'97 experiments") {

	if (!_device.pci_device) {
		fprintf(tty, "No AC'97 device found.\n");
		return 1;
	}
	size_t irq = pci_read_field(_device.pci_device, PCI_INTERRUPT_LINE, 1);
	fprintf(tty, "AC'97 audio device is at 0x%x.\n", _device.pci_device);
	fprintf(tty, "IRQ: %d\n", irq);

	return 0;
}

static int init(void) {
	BIND_SHELL_FUNCTION(ac97_test);
	pci_scan(&find_ac97, -1, &_device);
	if (!_device.pci_device) {
		return 1;
	}
	/*
	 * Look for an IRQ to use. Only 9, 10, and 11 are technically free for anyone to use.
	 * Ideally we should actually have routines that can share IRQs.
	 */
	_device.irq = -1;
	/* XXX: This is _NOT_ thread safe. */
	for (int i = 9; i <= 11; i++) {
		if (irq_is_handler_free(i)) {
			_device.irq = i;
			break;
		}
	}
	if (_device.irq == -1) {
		debug_print(WARNING, "Could not find a free IRQ for AC'97 device.");
		return 1;
	}
	pci_write_field(_device.pci_device, PCI_INTERRUPT_LINE, 1, _device.irq);
	debug_print(INFO, "IRQ for AC'97 set to %d.", _device.irq);
	return 0;
}

static int fini(void) {
	return 0;
}

MODULE_DEF(ac97, init, fini);
MODULE_DEPENDS(debugshell);
