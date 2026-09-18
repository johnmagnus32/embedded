/* sound.h — the gameboy-v3 audio client SDK (libsound), the audio twin of canvas.h.
 *
 * A client connects to soundd, receives one shared PCM ring, writes interleaved int16 stereo
 * frames into it, and signals how many it has written; soundd mixes all clients to the device and
 * reports back how many it has drained (backpressure). The engine's mixer is the only caller —
 * games use eng_sound_* and never touch this.
 */
#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>
#include "sound-proto.h"

typedef struct sound_ctx sound_ctx;   /* opaque connection handle */

/* Connect to soundd. Blocks until WELCOME + the ring arrive. Returns NULL on failure (no soundd
 * running -> the caller just runs silent). */
sound_ctx *sound_connect(enum sound_role role);
void       sound_disconnect(sound_ctx *c);

uint32_t sound_rate(const sound_ctx *c);
uint32_t sound_channels(const sound_ctx *c);

/* Free ring space, in frames — the most you may sound_write() right now without overrunning
 * PCM soundd hasn't consumed yet. Call sound_poll() first to refresh it. */
int  sound_avail(sound_ctx *c);
void sound_poll(sound_ctx *c);        /* drain SOUND_CONSUMED to advance the free-space estimate */

/* Copy up to `frames` interleaved stereo int16 frames into the ring (clamped to sound_avail),
 * advance the write cursor, and signal soundd. Returns frames actually written (< frames if the
 * ring was nearly full — the caller should keep the rest and retry next tick). */
int sound_write(sound_ctx *c, const int16_t *pcm, int frames);

#endif /* SOUND_H */
