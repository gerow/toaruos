/* vim: tabstop=4 shiftwidth=4 noexpandtab
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2011-2014 Kevin Lange
 *
 * Interrupt Requests
 *
 */
#include <system.h>
#include <logging.h>

extern void _irq0(void);
extern void _irq1(void);
extern void _irq2(void);
extern void _irq3(void);
extern void _irq4(void);
extern void _irq5(void);
extern void _irq6(void);
extern void _irq7(void);
extern void _irq8(void);
extern void _irq9(void);
extern void _irq10(void);
extern void _irq11(void);
extern void _irq12(void);
extern void _irq13(void);
extern void _irq14(void);
extern void _irq15(void);

static irq_handler_t irq_routines[16] = { NULL };
static list_t irq_entries[16] = {};

/*
 * Install an interupt handler for a hardware device.
 *
 * In order for an IRQ to be shared device must be provided and it must be
 * unique to the service installing the handler. In order to allocate an IRQ
 * for just one handler pass NULL as the device.
 */
void irq_install_handler(size_t irq, irq_handler_t handler, void * device) {
	node_t * node = malloc(*node);
	node->owner = &entries[irq];
	irq_entry_t * irq_entry = malloc(*irq_entry);
	irq_entry->handler = handler;
	irq_entry->device = device;
	node->value = irq_entry;
	
	list_append(&irq_entries[irq], node);

	/* Make sure the IRQ entries are consistent for properly sharing. */
	bool shared = false;
	bool exclusive = false;
	foreach(node, (&irq_entries[irq])) {
		irq_entry = node->value;
		if (irq_entry->device == NULL) {
			exclusive = true;
		} else {
			shared = true;
		}
	}	
	assert(!(shared && exclusive));
}

/*
 * Remove an interrupt handler for a hardware device.
 */
void irq_uninstall_handler(size_t irq, void * device) {
	irq_entry_t * irq_entry;
	foreach(node, (&irq_entries[irq])) {
		irq_entry = node->value;
		if (irq_entry->device == device) {
			list_delete(&irq_entries[irq], node);
			free(irq_entry);
			free(node);
			return;
		}
	}

	/* TODO(gerow): if we get here something has gone wrong */
}

/*
 * Remap interrupt handlers
 */
void irq_remap(void) {
	outportb(0x20, 0x11);
	outportb(0xA0, 0x11);
	outportb(0x21, 0x20);
	outportb(0xA1, 0x28);
	outportb(0x21, 0x04);
	outportb(0xA1, 0x02);
	outportb(0x21, 0x01);
	outportb(0xA1, 0x01);
	outportb(0x21, 0x0);
	outportb(0xA1, 0x0);
}

void irq_gates(void) {
	idt_set_gate(32, _irq0, 0x08, 0x8E);
	idt_set_gate(33, _irq1, 0x08, 0x8E);
	idt_set_gate(34, _irq2, 0x08, 0x8E);
	idt_set_gate(35, _irq3, 0x08, 0x8E);
	idt_set_gate(36, _irq4, 0x08, 0x8E);
	idt_set_gate(37, _irq5, 0x08, 0x8E);
	idt_set_gate(38, _irq6, 0x08, 0x8E);
	idt_set_gate(39, _irq7, 0x08, 0x8E);
	idt_set_gate(40, _irq8, 0x08, 0x8E);
	idt_set_gate(41, _irq9, 0x08, 0x8E);
	idt_set_gate(42, _irq10, 0x08, 0x8E);
	idt_set_gate(43, _irq11, 0x08, 0x8E);
	idt_set_gate(44, _irq12, 0x08, 0x8E);
	idt_set_gate(45, _irq13, 0x08, 0x8E);
	idt_set_gate(46, _irq14, 0x08, 0x8E);
	idt_set_gate(47, _irq15, 0x08, 0x8E);
}

/*
 * Set up interrupt handler for hardware devices.
 */
void irq_install(void) {
	irq_remap();
	irq_gates();
	IRQ_RES;
}

void irq_ack(size_t irq_no) {
	if (irq_no >= 8) {
		outportb(0xA0, 0x20);
	}
	outportb(0x20, 0x20);
}

int irq_call_handlers(size_t irq_no, struct regs *r) {
	irq_entry_t * irq_entry = NULL;
	int rv = IRQ_NOT_HANDLED;
	foreach(node, (&irq_routines[irq])) {
		irq_entry = node;
		int new_rv = node->handler(r, node->device);
		if (rv == IRQ_HANDLED && newrv == IRQ_HANDLED) {
			/* TODO(gerow): uh oh */
		}
		rv = new_rv;
	}
	return rv;
}

void irq_handler(struct regs *r) {
	IRQ_OFF;
	irq_t irq = r->int_no - 32;
	if (r->int_no > 47 || r->int_no < 32) {
		irq_ack(irq);
	} else {
		if (irq_call_handlers(irq, r) != IRQ_HANDLED) {
			/* TODO(gerow): we should probably print that something bad happened */
			irq_ack(irq);
		}
	}
	IRQ_RES;
}
