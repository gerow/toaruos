/*
 * Driver for the Intel AC'97.
 *
 * See <http://www.intel.com/design/chipsets/manuals/29802801.pdf>.
 */

#include <logging.h>
#include <module.h>
#include <mod/shell.h>
#include <printf.h>
#include <pci.h>
#include <system.h>

#define AC97_NAMBAR PCI_BAR0  /* Native Audio Mixer Base Address Register */
#define AC97_NABMBAR PCI_BAR1  /* Native Audio Bus Mastering Base Address Register */

/* Bus mastering offsets */
#define AC97_PCM_OUT_BDL_BASE AC97_NAMBAR

/* Mixer settings */
#define AC97_RESET             0x00
#define AC97_MASTER_VOLUME     0x02
#define AC97_AUX_OUT_VOLUME    0x04
#define AC97_MONO_VOLUME       0x06
#define AC97_PCM_OUT_VOLUME    0x18

struct ac97_bdl_entry {
	uint32_t pointer;
	uint32_t control_and_length;
} __attribute__((packed));

struct ac97_device {
	uint32_t pci_device;
	int irq;
	struct ac97_bdl_entry *buffer_descriptor_list[32];
};

struct buffer_descriptor_entry {
	uint32_t ioc : 1;
	uint32_t bup : 1;
	uint32_t reserved : 14;
	uint32_t buffer_length : 16;
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
	uint32_t mixer_port = pci_read_field(_device.pci_device, PCI_BAR0, 4) & ((uint32_t) -1) << 1;
	/* Turn it up! */
	outports(mixer_port + AC97_MASTER_VOLUME, 0);
	outports(mixer_port + AC97_PCM_OUT_VOLUME, 0);

	/* Allocate our buffer descriptor list */
	_device.buffer_descriptor_list = malloc(sizeof(*_device.buffer_descriptor_list));
	return 0;
}

static int fini(void) {
	free(_device.buffer_descriptor_list);
	return 0;
}

MODULE_DEF(ac97, init, fini);
MODULE_DEPENDS(debugshell);