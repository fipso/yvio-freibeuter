/* trace.h — syscall index/name table and boot-trace logging. */
#ifndef YVIO_TRACE_H
#define YVIO_TRACE_H

#include <stdint.h>

/* Human-readable name for a syscall trampoline address (0x1000..0x116C).
 * Returns "?" for indices the game is not known to use. */
const char *syscall_name(uint32_t trampoline_addr);

#endif /* YVIO_TRACE_H */
