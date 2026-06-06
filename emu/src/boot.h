/* boot.h — emulator bring-up shared by the desktop (main.c) and server (server.c)
 * paths. Maps guest memory, loads <root>/pirates/app.elf, installs the syscall
 * dispatch hook plus the gameplay watch hooks, and initialises the scheduler.
 *
 * The caller must set g_emu.root (and g_emu.lang) before calling. On success
 * returns 0 and stores the ready-to-run cpu in *cpu_out (also g_emu.cpu);
 * returns non-zero on failure. */
#ifndef YVIO_BOOT_H
#define YVIO_BOOT_H

#include "cpu.h"

int emu_boot(cpu_t **cpu_out);

#endif /* YVIO_BOOT_H */
