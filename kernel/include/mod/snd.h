#ifndef KERNEL_MOD_SND_H
#define KERNEL_MOD_SND_H

#define SND_FORMAT_L16LE 0  /* Linear 16-bit little endian */

#include <logging.h>
#include <system.h>

typedef struct snd_device {
	char name[256];            /* Name of the device. */
	void * device;             /* Private data for the device. May be NULL. */
	uint32_t playback_speed;   /* Playback speed in Hz */
	uint32_t playback_format;  /* Playback format (SND_FORMAT_*) */
} snd_device_t;

int snd_register(snd_device_t * device);
int snd_unregister(snd_device_t * device);
int snd_request_buf(snd_device_t * device, uint32_t size, uint8_t *buffer);

#endif  /* KERNEL_MOD_SND_H */
