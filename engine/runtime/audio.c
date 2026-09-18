/* engine/audio.c — the game-side software mixer + WAV loader (the audio analog of scene.c's
 * drawing). Mixes THIS process's own voices (fire-and-forget SFX + one looping music bed) into
 * interleaved stereo int16 at ENG_MIX_RATE. That mixed stream is what the audio transport
 * (libsound) submits to soundd, which sums all clients' streams and writes to the device — so
 * this file does the per-GAME mix; soundd does the cross-PROCESS mix. Pure CPU, no I/O.
 *
 * Sounds are 16-bit PCM WAV (mono or stereo) already at ENG_MIX_RATE (our gen_sfx.py output), so
 * there is no resampling — a mismatched rate just plays at the wrong pitch (logged).
 */
#include "engine.h"
#include "engine_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct eng_sound { int16_t *pcm; int frames; int ch; };   /* ch = 1 mono | 2 stereo, at ENG_MIX_RATE */

#define VOICES 16
struct voice { const struct eng_sound *s; int pos; float vol; int loop, active; };
static struct voice g_voices[VOICES];   /* one-shot SFX */
static struct voice g_music;            /* the looping bed */

/* Resolve a relative asset path under $CANVAS_ASSETS (else /usr/share/canvas); absolute as-is.
 * Mirrors eng_image_from_png's resolution so audio + art share one asset root. */
static void resolve(const char *path, char *out, size_t cap)
{
	if (path[0] == '/') { snprintf(out, cap, "%s", path); return; }
	const char *base = getenv("CANVAS_ASSETS");
	snprintf(out, cap, "%s/%s", base && *base ? base : "/usr/share/canvas", path);
}

static uint32_t rd_le32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd_le16(const unsigned char *p) { return (uint16_t)(p[0] | p[1] << 8); }

eng_sound *eng_sound_load(const char *path)
{
	char full[512]; resolve(path, full, sizeof full);
	FILE *f = fopen(full, "rb");
	if (!f) { fprintf(stderr, "audio: open %s failed\n", full); return NULL; }
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	if (n < 44) { fclose(f); return NULL; }
	unsigned char *b = malloc((size_t)n);
	if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
	fclose(f);

	if (memcmp(b, "RIFF", 4) || memcmp(b + 8, "WAVE", 4)) { free(b); fprintf(stderr, "audio: %s not WAV\n", full); return NULL; }
	int ch = 0, bits = 0; uint32_t rate = 0; const unsigned char *data = NULL; uint32_t data_len = 0;
	long o = 12;
	while (o + 8 <= n) {                                        /* walk RIFF chunks */
		const unsigned char *id = b + o; uint32_t sz = rd_le32(b + o + 4); o += 8;
		if (o + (long)sz > n) break;
		if (!memcmp(id, "fmt ", 4) && sz >= 16) {
			uint16_t fmt = rd_le16(b + o);
			ch = rd_le16(b + o + 2); rate = rd_le32(b + o + 4); bits = rd_le16(b + o + 14);
			if (fmt != 1) { free(b); fprintf(stderr, "audio: %s not PCM\n", full); return NULL; }
		} else if (!memcmp(id, "data", 4)) {
			data = b + o; data_len = sz;
		}
		o += sz + (sz & 1);                                    /* chunks are word-aligned */
	}
	if (!data || bits != 16 || (ch != 1 && ch != 2)) { free(b); fprintf(stderr, "audio: %s unsupported (bits=%d ch=%d)\n", full, bits, ch); return NULL; }
	if (rate != ENG_MIX_RATE) fprintf(stderr, "audio: %s is %u Hz, expected %d — will play off-pitch\n", full, rate, ENG_MIX_RATE);

	int frames = (int)(data_len / (uint32_t)(ch * 2));
	eng_sound *s = malloc(sizeof *s);
	s->pcm = malloc((size_t)frames * ch * sizeof(int16_t));
	s->frames = frames; s->ch = ch;
	for (int i = 0; i < frames * ch; i++) s->pcm[i] = (int16_t)rd_le16(data + i * 2);
	free(b);
	return s;
}

void eng_sound_free(eng_sound *s) { if (s) { free(s->pcm); free(s); } }

void eng_sound_play(eng_sound *s, float vol)
{
	if (!s) return;
	for (int i = 0; i < VOICES; i++)
		if (!g_voices[i].active) { g_voices[i] = (struct voice){ s, 0, vol, 0, 1 }; return; }
	/* all voices busy: steal voice 0 (oldest-ish) so a new hit still sounds */
	g_voices[0] = (struct voice){ s, 0, vol, 0, 1 };
}

void eng_music_play(eng_sound *s, float vol) { g_music = (struct voice){ s, 0, vol, 1, s ? 1 : 0 }; }
void eng_music_stop(void)                    { g_music.active = 0; }

/* Playback position of the music voice, in seconds (0 if no music). This is the PRODUCED
 * position — it runs ahead of what's audible by the output buffer latency, so a rhythm game
 * subtracts a small calibration offset. Advances as eng__mix() consumes the music (i.e. while the
 * audio transport is feeding soundd). */
double eng_music_pos(void)
{
	return (g_music.active && g_music.s) ? (double)g_music.pos / (double)ENG_MIX_RATE : 0.0;
}

/* Add one voice's next `frames` into the int32 accumulator (mono duplicated to both channels). */
static void mix_voice(struct voice *v, int32_t *acc, int frames)
{
	if (!v->active || !v->s) return;
	const struct eng_sound *s = v->s;
	for (int f = 0; f < frames; f++) {
		if (v->pos >= s->frames) { if (v->loop) v->pos = 0; else { v->active = 0; return; } }
		int32_t l, r;
		if (s->ch == 1) { l = r = s->pcm[v->pos]; }
		else            { l = s->pcm[v->pos * 2]; r = s->pcm[v->pos * 2 + 1]; }
		acc[f * 2]     += (int32_t)(l * v->vol);
		acc[f * 2 + 1] += (int32_t)(r * v->vol);
		v->pos++;
	}
}

#define MIX_MAX 4096                                   /* max frames per eng__mix call */
void eng__mix(int16_t *out, int frames)
{
	if (frames > MIX_MAX) frames = MIX_MAX;
	static int32_t acc[MIX_MAX * ENG_MIX_CH];
	memset(acc, 0, (size_t)frames * ENG_MIX_CH * sizeof(int32_t));
	mix_voice(&g_music, acc, frames);
	for (int i = 0; i < VOICES; i++) mix_voice(&g_voices[i], acc, frames);
	for (int j = 0; j < frames * ENG_MIX_CH; j++) {
		int32_t v = acc[j];
		out[j] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);   /* sum + clamp */
	}
}

/* The audio TRANSPORT (connect to a sink, feed the mixed stream each frame, disconnect) is the
 * PLATFORM's job now — it pulls eng__mix() into its sink. The canvas platform's transport (libsound
 * -> soundd) lives in the canvas platform impl; a host platform (editor) can feed an OS device or
 * stay silent. This file is pure mixing + WAV, no I/O. */
