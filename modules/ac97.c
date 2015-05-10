/*
 * Driver for the Intel AC'97.
 *
 * See <http://www.intel.com/design/chipsets/manuals/29802801.pdf>.
 */

#include <logging.h>
#include <mem.h>
#include <module.h>
#include <mod/shell.h>
#include <printf.h>
#include <pci.h>
#include <system.h>

/* utility functions */
#define N_ELEMENTS(arr) (sizeof(arr) / sizeof((arr)[0]))

/* BARs! */
#define AC97_NAMBAR PCI_BAR0  /* Native Audio Mixer Base Address Register */
#define AC97_NABMBAR PCI_BAR1  /* Native Audio Bus Mastering Base Address Register */

/* Bus mastering offsets */
/* PCM out BAR */
#define AC97_POBDBAR AC97_NABMBAR + 0x10
/* PCM out last valid index */
#define AC97_POLVI AC97_NABMBAR + 0x15
/* PCM out control register */
#define AC97_PO_CR AC97_NABMBAR + 0x1b

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

#define AC97_CL_GET_LENGTH(cl) ((cl) & 0x00ff)
#define AC97_CL_SET_LENGTH(cl, v) ((cl) = (v) & 0xff)
#define AC97_CL_BUP 30
#define AC97_CL_IOC 31

struct ac97_device {
	uint32_t pci_device;
	int irq;
	/* Buffer descriptor list */
	struct ac97_bdl_entry bdl[32];
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
	uint16_t command_register = pci_read_field(_device.pci_device, PCI_COMMAND, 2);
	fprintf(tty, "COMMAND: 0x%04x\n", command_register);

	return 0;
}

DEFINE_SHELL_FUNCTION(ac97_noise, "[debug] Intel AC'97 noise test") {
	/* Allocate a buffer and fill it with noise */
	uint16_t *buf = (uint16_t *)kvmalloc_p(1 << 15, &_device.bdl[0].pointer);
	uint16_t val = 0xf179;
	for (int i = 0; i < 1 << 15; i++) {
		buf[i] = val;
		if (val & 0x8000) {
			val = (val << 1) | 1;
		} else {
			val = val << 1;
		}
	}
	AC97_CL_SET_LENGTH(_device.bdl[0].control_and_length, 1 << 15);

	/* Give it our buffer descriptor list */
	pci_write_field(_device.pci_device, AC97_POBDBAR, 4,
			(uint32_t) map_to_physical((uintptr_t) &_device.bdl));
	pci_write_field(_device.pci_device, AC97_POLVI, 1, 1);
	/* Set it to run! */
	outportb(AC97_PO_CR, 1);

	return 0;
}

static int init(void) {
	BIND_SHELL_FUNCTION(ac97_test);
	BIND_SHELL_FUNCTION(ac97_noise);
	pci_scan(&find_ac97, -1, &_device);
	if (!_device.pci_device) {
		return 1;
	}
	/* Enable bus mastering and disable memory mapped space */
	uint16_t command_register = pci_read_field(_device.pci_device, PCI_COMMAND, 2);
	command_register |= (1 << 2); 
	command_register &= ~(1 << 1);
	pci_write_field(_device.pci_device, PCI_COMMAND, 2, command_register);
	uint32_t mixer_port = pci_read_field(_device.pci_device, PCI_BAR0, 4) & ((uint32_t) -1) << 1;
	/* Turn it up! */
	outports(mixer_port + AC97_MASTER_VOLUME, 0);
	outports(mixer_port + AC97_PCM_OUT_VOLUME, 0);

	return 0;
}

static int fini(void) {
	free(_device.bdl);
	return 0;
}

MODULE_DEF(ac97, init, fini);
MODULE_DEPENDS(debugshell);
