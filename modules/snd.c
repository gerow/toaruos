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
#include <system.h>

list_t _devices;
uint8_t  _devices_lock;
uint8_t whatever[4096];

int snd_register(snd_device_t * device) {
	int rv = 0;
	whatever[0] = 5;

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

static int init(void) {
	debug_print(WARNING, "[snd] _devices lock: %d", _devices_lock);
	return 0;
}

static int fini(void) {
	return 0;
}

MODULE_DEF(snd, init, fini);
MODULE_DEPENDS(debugshell);
