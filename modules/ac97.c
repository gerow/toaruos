/* vim: tabstop=4 shiftwidth=4 noexpandtab
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2015 Michael Gerow
 * Copyright (C) 2015 Kevin Lange
 *
 * Driver for the Intel AC'97.
 *
 * See <http://www.intel.com/design/chipsets/manuals/29802801.pdf>.
 */

#include <logging.h>
#include <mem.h>
#include <module.h>
#include <mod/shell.h>
#include <mod/snd.h>
#include <printf.h>
#include <pci.h>
#include <system.h>

/* Utility macros */
#define N_ELEMENTS(arr) (sizeof(arr) / sizeof((arr)[0]))

/* BARs! */
#define AC97_NAMBAR  0x10  /* Native Audio Mixer Base Address Register */
#define AC97_NABMBAR 0x14  /* Native Audio Bus Mastering Base Address Register */

/* Bus mastering IO port offsets */
#define AC97_PO_BDBAR 0x10  /* PCM out buffer descriptor BAR */
#define AC97_PO_CIV   0x14  /* PCM out current index value */
#define AC97_PO_LVI   0x15  /* PCM out last valid index */
#define AC97_PO_SR    0x16  /* PCM out status register */
#define AC97_PO_PICB  0x18  /* PCM out position in current buffer register */
#define AC97_PO_CR    0x1B  /* PCM out control register */

/* Bus mastering misc */
/* Buffer descriptor list constants */
#define AC97_BDL_LEN              32                    /* Buffer descriptor list length */
#define AC97_BDL_BUFFER_LEN       0x1000                /* Length of buffer in BDL */
#define AC97_CL_GET_LENGTH(cl)    ((cl) & 0xFFFF)       /* Decode length from cl */
#define AC97_CL_SET_LENGTH(cl, v) ((cl) = (v) & 0xFFFF) /* Encode length to cl */
#define AC97_CL_BUP               (1 << 30)             /* Buffer underrun policy in cl */
#define AC97_CL_IOC               (1 << 31)             /* Interrupt on completion flag in cl */

/* PCM out control register flags */
#define AC97_X_CR_RPBM  (1 << 0)  /* Run/pause bus master */
#define AC97_X_CR_RR    (1 << 1)  /* Reset registers */
#define AC97_X_CR_LVBIE (1 << 2)  /* Last valid buffer interrupt enable */
#define AC97_X_CR_FEIE  (1 << 3)  /* FIFO error interrupt enable */
#define AC97_X_CR_IOCE  (1 << 4)  /* Interrupt on completion enable */

/* Status register flags */
#define AC97_X_SR_DCH   (1 << 0)  /* DMA controller halted */
#define AC97_X_SR_CELV  (1 << 1)  /* Current equals last valid */
#define AC97_X_SR_LVBCI (1 << 2)  /* Last valid buffer completion interrupt */
#define AC97_X_SR_BCIS  (1 << 3)  /* Buffer completion interrupt status */
#define AC97_X_SR_FIFOE (1 << 3)  /* FIFO error */

/* Mixer IO port offsets */
#define AC97_RESET          0x00
#define AC97_MASTER_VOLUME  0x02
#define AC97_AUX_OUT_VOLUME 0x04
#define AC97_MONO_VOLUME    0x06
#define AC97_PCM_OUT_VOLUME 0x18

/* snd values */
#define AC97_SND_NAME "Intel AC'97"
#define AC97_PLAYBACK_SPEED 48000
#define AC97_PLAYBACK_FORMAT SND_FORMAT_L16LE

/* An entry in a buffer dscriptor list */
typedef struct {
	uint32_t pointer;  /* Pointer to buffer */
	uint32_t cl;       /* Control values and buffer length */
} __attribute__((packed)) ac97_bdl_entry_t;

typedef struct {
	uint32_t pci_device;
	uint16_t nabmbar;               /* Native audio bus mastring BAR */
	uint16_t nambar;                /* Native audio mixing BAR */
	size_t irq;                     /* This ac97's irq */
	uint8_t lvi;                    /* The currently set last valid index */
	ac97_bdl_entry_t * bdl;         /* Buffer descriptor list */
	uint16_t * bufs[AC97_BDL_LEN];  /* Virtual addresses for buffers in BDL */
	uint32_t bdl_p;
} ac97_device_t;

static ac97_device_t _device;
static snd_device_t _snd = {
	AC97_SND_NAME,
	&_device,
	AC97_PLAYBACK_SPEED,
	AC97_PLAYBACK_FORMAT,
};

/* 
 * This could be unnecessary if we instead allocate just two buffers and make
 * the ac97 think there are more.
 */

static void find_ac97(uint32_t device, uint16_t vendorid, uint16_t deviceid, void * extra) {

	ac97_device_t * ac97 = extra;

	if ((vendorid == 0x8086) && (deviceid == 0x2415)) {
		ac97->pci_device = device;
	}

}

DEFINE_SHELL_FUNCTION(ac97_status, "[debug] AC'97 status values") {
	if (!_device.pci_device) {
		fprintf(tty, "No AC'97 device found.\n");
		return 1;
	}
	fprintf(tty, "AC'97 audio device is at 0x%x.\n", _device.pci_device);
	size_t irq = pci_read_field(_device.pci_device, PCI_INTERRUPT_LINE, 1);
	fprintf(tty, "IRQ: %d\n", irq);
	uint16_t command_register = pci_read_field(_device.pci_device, PCI_COMMAND, 2);
	fprintf(tty, "COMMAND: 0x%04x\n", command_register);
	fprintf(tty, "NABMBAR: 0x%04x\n", pci_read_field(_device.pci_device, AC97_NABMBAR, 2));
	fprintf(tty, "PO_BDBAR: 0x%08x\n", inportl(_device.nabmbar + AC97_PO_BDBAR));
	if (_device.bdl) {
		for (int i = 0; i < AC97_BDL_LEN; i++) {
			fprintf(tty, "bdl[%d].pointer: 0x%x\n", i, _device.bdl[i].pointer);
			fprintf(tty, "bdl[%d].cl: 0x%x\n", i, _device.bdl[i].cl);
		}
	}
	fprintf(tty, "PO_CIV: %d\n", inportb(_device.nabmbar + AC97_PO_CIV));
	fprintf(tty, "PO_PICB: 0x%04x\n", inportb(_device.nabmbar + AC97_PO_PICB));
	fprintf(tty, "PO_CR: 0x%02x\n", inportb(_device.nabmbar + AC97_PO_CR));
	fprintf(tty, "PO_LVI: 0x%02x\n", inportb(_device.nabmbar + AC97_PO_LVI));

	return 0;
}

static void irq_handler(struct regs * regs) {
	debug_print(NOTICE, "AC97 IRQ called");
	uint16_t sr = inports(_device.nabmbar + AC97_PO_SR);
	debug_print(NOTICE, "sr: 0x%04x", sr);
	if (sr & AC97_X_SR_LVBCI) {
		outports(_device.nabmbar + AC97_PO_SR, AC97_X_SR_LVBCI);
		debug_print(NOTICE, "Last valid buffer completion interrupt handled");
	} else if (sr & AC97_X_SR_BCIS) {
		debug_print(NOTICE, "Buffer completion interrupt status start...");
		size_t start;
		if (_device.lvi == AC97_BDL_LEN / 2 - 1) {
			_device.lvi = AC97_BDL_LEN - 1;
			start = AC97_BDL_LEN / 2;
		} else {
			_device.lvi = AC97_BDL_LEN / 2 - 1;
			start = 0;
		}

		for (int i = start; i <= _device.lvi; i++) {
			snd_request_buf(&_snd, AC97_BDL_BUFFER_LEN * sizeof(*_device.bufs[0]), (uint8_t *)_device.bufs[i]);
		}
		outportb(_device.nabmbar + AC97_PO_LVI, _device.lvi);
		outports(_device.nabmbar + AC97_PO_SR, AC97_X_SR_BCIS);
		debug_print(NOTICE, "Buffer completion interrupt status handled");
	} else if (sr & AC97_X_SR_FIFOE) {
		outports(_device.nabmbar + AC97_PO_SR, AC97_X_SR_FIFOE);
		debug_print(NOTICE, "FIFO error handled");
	}

	irq_ack(_device.irq);
}

static int init(void) {
	debug_print(NOTICE, "Initializing AC97");
	pci_scan(&find_ac97, -1, &_device);
	if (!_device.pci_device) {
		return 1;
	}
	BIND_SHELL_FUNCTION(ac97_status);
	_device.nabmbar = pci_read_field(_device.pci_device, AC97_NABMBAR, 2) & ((uint32_t) -1) << 1;
	_device.nambar = pci_read_field(_device.pci_device, PCI_BAR0, 4) & ((uint32_t) -1) << 1;
	_device.irq = pci_read_field(_device.pci_device, PCI_INTERRUPT_LINE, 1);
	if (!irq_is_handler_free(_device.irq)) {
		debug_print(ERROR, "AC97 IRQ conflict: IRQ %d already in use", _device.irq);
		return 1;
	}
	irq_install_handler(_device.irq, irq_handler);
	/* Enable all matter of interrupts */
	outportb(_device.nabmbar + AC97_PO_CR, AC97_X_CR_FEIE | AC97_X_CR_IOCE);

	/* Enable bus mastering and disable memory mapped space */
	pci_write_field(_device.pci_device, PCI_COMMAND, 2, 0x5);
	/* Put ourselves at a reasonable volume. */
	uint16_t volume = 0x03 | (0x03 << 8);
	outports(_device.nambar + AC97_MASTER_VOLUME, volume);
	outports(_device.nambar + AC97_PCM_OUT_VOLUME, volume);

	/* Allocate our BDL and our buffers */
	_device.bdl = (void *)kmalloc_p(AC97_BDL_LEN * sizeof(*_device.bdl), &_device.bdl_p);
	memset(_device.bdl, 0, AC97_BDL_LEN * sizeof(*_device.bdl));
	for (int i = 0; i < AC97_BDL_LEN; i++) {
		_device.bufs[i] = (uint16_t *)kmalloc_p(AC97_BDL_BUFFER_LEN * sizeof(*_device.bufs[0]),
												&_device.bdl[i].pointer);
		memset(_device.bufs[i], 0, AC97_BDL_BUFFER_LEN * sizeof(*_device.bufs[0]));
		AC97_CL_SET_LENGTH(_device.bdl[i].cl, AC97_BDL_BUFFER_LEN);
	}
	/* Set the midway buffer and the last buffer to interrupt */
	_device.bdl[AC97_BDL_LEN / 2 - 1].cl |= AC97_CL_IOC;
	_device.bdl[AC97_BDL_LEN - 1].cl |= AC97_CL_IOC;
	/* Tell the ac97 where our BDL is */
	outportl(_device.nabmbar + AC97_PO_BDBAR, _device.bdl_p);
	/* Set the LVI to be the last index */
	_device.lvi = AC97_BDL_LEN - 1;
	outportb(_device.nabmbar + AC97_PO_LVI, _device.lvi);

	snd_register(&_snd);


	/* Start things playing */
	outportb(_device.nabmbar + AC97_PO_CR, inportb(_device.nabmbar + AC97_PO_CR) | AC97_X_CR_RPBM);
	debug_print(NOTICE, "AC97 initialized successfully");

	return 0;
}

static int fini(void) {
	snd_unregister(&_snd);

	free(_device.bdl);
	for (int i = 0; i < AC97_BDL_LEN; i++) {
		free(_device.bufs[i]);
	}
	return 0;
}

MODULE_DEF(ac97, init, fini);
MODULE_DEPENDS(debugshell);
MODULE_DEPENDS(snd);
