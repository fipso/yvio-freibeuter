/* elf.h — minimal ELF32 (ARM, little-endian) program loader. */
#ifndef YVIO_ELF_H
#define YVIO_ELF_H

#include "cpu.h"

/* Load PT_LOAD segments of an ELF32 file into the cpu address space, mapping
 * any not-yet-mapped regions. On success returns 0 and writes the entry point
 * to *entry_out. Returns negative on error. */
int elf_load(cpu_t *cpu, const char *path, uint32_t *entry_out);

#endif /* YVIO_ELF_H */
