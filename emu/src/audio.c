/* audio.c — host side of the I2S DMA. The app mixes 16-bit/mono/22050 PCM into
 * an 8x260-byte ring (256B payload + 4B link) gated by sems A28 (free) / A3C
 * (ready). We play the OS drain role: take ready buffers, hand the PCM to a sink
 * (raylib on desktop; null when headless), and give A28 back so the mixer
 * refills. Consumption is metered to virtual time so it also works headless. */
#include "emu.h"
#include <string.h>

#define RING_BASE   0x40000A50u
#define SEM_FREE    0x40000A28u    /* A28: free buffers to fill */
#define SEM_READY   0x40000A3Cu    /* A3C: buffers ready to play */
#define BUF_BYTES   256
#define LINK_OFF    256
#define SAMPLES_PER_BUF 128        /* 256 bytes / 2 (int16 mono) */
/* 128 samples @ 22050 Hz = 5.805 ms; meter ~6 ms per buffer in virtual time. */
#define MS_PER_BUF  6

static audio_sink_fn g_sink;
static void         *g_sink_user;

void audio_set_sink(audio_sink_fn fn, void *user)
{
    g_sink = fn; g_sink_user = user;
    g_emu.audio_realtime = (fn != NULL);
}

void sys_audio_start(void)   /* 0x1098(&desc): begin/resume DMA from the ring */
{
    if (!g_emu.audio_running) {
        g_emu.audio_running = true;
        if (!g_emu.audio_cons_buf) g_emu.audio_cons_buf = RING_BASE;
        g_emu.audio_next_ms = g_emu.virtual_ms;
    }
    ret(0);
}

void sys_audio_stop(void)    /* 0x1094: pause DMA */
{
    g_emu.audio_running = false;
    ret(0);
}

void audio_pump(void)
{
    if (!g_emu.audio_running) return;
    hsem_t *ready = ipc_sem_find(SEM_READY);
    hsem_t *freeb = ipc_sem_find(SEM_FREE);
    if (!ready || !freeb) return;

    if (g_emu.audio_next_ms < g_emu.virtual_ms) g_emu.audio_next_ms = g_emu.virtual_ms;

    while (ready->count > 0) {
        int16_t pcm[SAMPLES_PER_BUF];
        cpu_read(g_emu.cpu, g_emu.audio_cons_buf, pcm, BUF_BYTES);

        if (g_emu.audio_realtime && !g_emu.fast) {
            /* Real output: keep the host ring filled; stop on backpressure so the
             * mixer is throttled to real playback (no underrun, ~bounded lookahead). */
            if (!g_sink(pcm, g_sink_user)) break;
        } else {
            /* Headless OR fast-forward: meter consumption to virtual time and mute.
             * Metering is essential even in FF: it makes the mixer block on A28 so
             * the cooperative scheduler isn't starved (otherwise the frame hangs). */
            if (g_emu.virtual_ms < g_emu.audio_next_ms) break;
            g_emu.audio_next_ms += MS_PER_BUF;
        }
        g_emu.audio_bytes += BUF_BYTES;

        uint32_t next = 0;
        cpu_read(g_emu.cpu, g_emu.audio_cons_buf + LINK_OFF, &next, 4);
        if (next >= 0x40000000u && next < 0x40100000u) g_emu.audio_cons_buf = next;

        ready->count--;
        ipc_sem_post(freeb);                 /* free buffer -> wake mixer */
    }
}
