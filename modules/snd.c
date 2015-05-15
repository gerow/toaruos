/* vim: tabstop=4 shiftwidth=4 noexpandtab
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2015 Mike Gerow
 *
 * Sound subsystem.
 */

#include <mod/snd.h>

#include <list.h>
#include <module.h>
#include <ringbuffer.h>
#include <system.h>

/* Utility macros */
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

#define SND_BUF_SIZE 0x1000 * 32 * 2

static uint32_t snd_write(fs_node_t * node, uint32_t offset, uint32_t size, uint8_t *buffer);
static int snd_ioctl(fs_node_t * node, int request, void * argp);

static uint8_t  _devices_lock;
static list_t _devices;
static fs_node_t _main_fnode = {
	.name = "dsp",
	.device = &_devices,
	.flags = FS_CHARDEVICE,
	.ioctl = snd_ioctl,
	.write = snd_write,
};
static ring_buffer_t *_buf;

int snd_register(snd_device_t * device) {
	int rv = 0;

	debug_print(WARNING, "[snd] _devices lock: %d", _devices_lock);
	spin_lock(&_devices_lock);
	if (list_find(&_devices, device)) {
		debug_print(WARNING, "[snd] attempt to register duplicate %s", device->name);
		rv = -1;
		goto snd_register_cleanup;
	}
	debug_print(NOTICE, "[snd] %s registered", device->name);

snd_register_cleanup:
	spin_unlock(&_devices_lock);
	return rv;
}

int snd_unregister(snd_device_t * device) {
	int rv = 0;

	node_t * node = list_find(&_devices, device);
	if (!node) {
		debug_print(WARNING, "[snd] attempted to unregister %s, "
				"but it was never registered", device->name);
		goto snd_unregister_cleanup;
	}
	list_delete(&_devices, node);
	debug_print(NOTICE, "[snd] %s unregistered", device->name);

snd_unregister_cleanup:
	spin_unlock(&_devices_lock);
	return rv;
}

static uint32_t snd_write(fs_node_t * node, uint32_t offset, uint32_t size, uint8_t *buffer) {
	return ring_buffer_write(_buf, size, buffer);
}

static int snd_ioctl(fs_node_t * node, int request, void * argp) {
	return -1;
}

int snd_request_buf(snd_device_t * device, uint32_t size, uint8_t *buffer) {
	debug_print(NOTICE, "[snd] got request for %d bytes from %s", size, device->name);
	/* Add check to make sure this is the first device, otherwise ignore */
	size_t read_size = MIN(ring_buffer_unread(_buf), size);
	debug_print(NOTICE, "[snd] reading %d bytes from buffer", read_size);
	if (read_size) {
		ring_buffer_read(_buf, read_size, buffer);
	}
	if (read_size != size) {
		buffer += read_size;
		debug_print(NOTICE, "[snd] filling in %d zeroes", size - read_size);
		memset(buffer, 0, size - read_size);
	}

	return read_size;
}

static int init(void) {
	_buf = ring_buffer_create(SND_BUF_SIZE);
	vfs_mount("/dev/dsp", &_main_fnode);
	return 0;
}

static int fini(void) {
	/* umount? */
	ring_buffer_destroy(_buf);
	return 0;
}

MODULE_DEF(snd, init, fini);
MODULE_DEPENDS(debugshell);
