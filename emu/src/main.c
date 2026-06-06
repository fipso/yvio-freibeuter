/* main.c — entry: parse args, then run the WebSocket server, a headless session,
 * or the desktop raylib UI. Emulator bring-up + gameplay watch hooks live in
 * boot.c (emu_boot); the server lives in server.c. */
#include "cpu.h"
#include "emu.h"
#include "boot.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *root = ".";
    bool headless = false;
    bool server = false;
    int port = 8080;
    g_emu.lang = "de";
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--root")  && i + 1 < argc) root = argv[++i];
        else if (!strcmp(argv[i], "--lang") && i + 1 < argc) g_emu.lang = argv[++i];
        else if (!strcmp(argv[i], "--trace")) g_emu.trace = true;
        else if (!strcmp(argv[i], "--headless")) headless = g_emu.headless = true;
        else if (!strcmp(argv[i], "--script")) g_emu.script = true;
        else if (!strcmp(argv[i], "--server")) server = true;
        else if (!strcmp(argv[i], "--port") && i + 1 < argc) port = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--scenario") && i + 1 < argc) {
            g_emu.script = true;                       /* shares the setup driving */
            const char *s = argv[++i];
            if (!strcmp(s, "battle")) g_emu.scenario = 1;
            else if (!strcmp(s, "news") || !strcmp(s, "quest")) g_emu.scenario = 2;
        }
        else if (!strcmp(argv[i], "--max")   && i + 1 < argc) g_emu.sc_limit = strtoull(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--vms")   && i + 1 < argc) g_emu.stop_vms = (uint32_t)strtoul(argv[++i], 0, 0);
    }
    g_emu.root = root;

    /* Server mode: the parent process never creates Unicorn (fork-safety); each
     * lobby's forked child boots its own emulator instance. */
    if (server)
        return server_main(root, g_emu.lang, port);

    cpu_t *cpu = NULL;
    if (emu_boot(&cpu) != 0) return 1;

    if (headless) {
        printf("---- run (headless) ----\n");
        sched_run();
    } else {
        audiohost_init();
        ui_init();
        /* Pace virtual time to ~real time: ~16 ms of guest time per 60 fps frame,
         * so the game's audio plays back at the right speed. */
        while (!ui_should_close()) {
            ui_poll();                       /* mouse/keys -> input_mask, fast flag */
            if (!g_emu.crashed) {            /* stop stepping a faulted guest (no spam) */
                g_emu.stop_vms += g_emu.fast ? 300u : ui_frame_ms();  /* FF skips intro */
                g_emu.running = true;
                sched_run();                 /* run guest until vms hits the cap */
                rfid_refresh_stats();        /* keep the HUD money/equipment current */
                /* Battle-overlay safety net: close it if no battle/maneuver voice for
                 * ~10 s (covers outcome paths whose follow-up voice we don't watch). */
                if (g_emu.in_battle && g_emu.virtual_ms - g_emu.battle_ms > 10000u)
                    g_emu.in_battle = false;
            }
            ui_draw();                       /* keep drawing (shows the crash banner) */
        }
        ui_shutdown();
        audiohost_shutdown();
    }

    printf("---- done: syscalls=%llu switches=%llu vms=%u audio=%llu bytes (%.2fs) ----\n",
           (unsigned long long)g_emu.sc_count, (unsigned long long)g_emu.switch_count,
           g_emu.virtual_ms, (unsigned long long)g_emu.audio_bytes,
           (double)g_emu.audio_bytes / 2.0 / 22050.0);
    cpu_destroy(cpu);
    return 0;
}
