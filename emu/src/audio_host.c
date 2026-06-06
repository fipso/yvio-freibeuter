/* audio_host.c — raylib audio output. The emulator pushes 128-sample (mono,
 * 16-bit, 22050 Hz) blocks via the audio sink into an SPSC ring; raylib's audio
 * callback (on its own thread) drains it. Underrun -> silence. */
#include "emu.h"
#include "raylib.h"
#include <string.h>
#include <stdatomic.h>

#define RING_SAMPLES (1u << 16)   /* 64Ki int16 mono */
#define RING_MASK    (RING_SAMPLES - 1)
#define FILL_TARGET  8820u        /* ~400 ms @ 22050 Hz: keep audio ~in sync */

static int16_t        g_ring[RING_SAMPLES];
static _Atomic unsigned g_wr, g_rd;
static AudioStream    g_stream;

/* producer: emulator thread. Returns 0 (full) when buffered >= target, so the
 * caller stops draining the guest ring and the mixer is paced to real playback. */
static int push_block(const int16_t *pcm, void *user)
{
    (void)user;
    unsigned wr = atomic_load_explicit(&g_wr, memory_order_relaxed);
    unsigned rd = atomic_load_explicit(&g_rd, memory_order_acquire);
    if ((unsigned)(wr - rd) >= FILL_TARGET) return 0;       /* backpressure */
    for (int i = 0; i < 128; i++) { g_ring[wr & RING_MASK] = pcm[i]; wr++; }
    atomic_store_explicit(&g_wr, wr, memory_order_release);
    return 1;
}

/* consumer: raylib audio thread */
static void audio_cb(void *buffer, unsigned int frames)
{
    int16_t *out = (int16_t *)buffer;
    unsigned rd = atomic_load_explicit(&g_rd, memory_order_relaxed);
    unsigned wr = atomic_load_explicit(&g_wr, memory_order_acquire);
    for (unsigned i = 0; i < frames; i++) {
        if ((rd & RING_MASK) == (wr & RING_MASK)) out[i] = 0;      /* underrun */
        else out[i] = g_ring[rd++ & RING_MASK];
    }
    atomic_store_explicit(&g_rd, rd, memory_order_release);
}

void audiohost_init(void)
{
    InitAudioDevice();
    SetAudioStreamBufferSizeDefault(1024);
    g_stream = LoadAudioStream(22050, 16, 1);
    SetAudioStreamCallback(g_stream, audio_cb);
    PlayAudioStream(g_stream);
    audio_set_sink(push_block, NULL);
}

void audiohost_shutdown(void)
{
    UnloadAudioStream(g_stream);
    CloseAudioDevice();
}
