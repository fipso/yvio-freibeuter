/* elf.c — minimal ELF32 ARM LE program loader (PT_LOAD only). */
#include "elf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define ET_EXEC   2
#define EM_ARM    40
#define PT_LOAD   1

#define PF_X 1
#define PF_W 2
#define PF_R 4

#pragma pack(push, 1)
typedef struct {
    uint8_t  ident[16];
    uint16_t type, machine;
    uint32_t version, entry, phoff, shoff, flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} Elf32_Ehdr;

typedef struct {
    uint32_t type, offset, vaddr, paddr, filesz, memsz, flags, align;
} Elf32_Phdr;
#pragma pack(pop)

static uint8_t *read_file(const char *path, long *len_out)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "elf: cannot open %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(len);
    if (!buf || fread(buf, 1, len, f) != (size_t)len) {
        fprintf(stderr, "elf: read failed %s\n", path);
        free(buf); fclose(f); return NULL;
    }
    fclose(f);
    *len_out = len;
    return buf;
}

int elf_load(cpu_t *cpu, const char *path, uint32_t *entry_out)
{
    long flen = 0;
    uint8_t *file = read_file(path, &flen);
    if (!file) return -1;

    if (flen < (long)sizeof(Elf32_Ehdr) ||
        memcmp(file, "\x7f""ELF", 4) != 0 || file[4] != 1 /*ELFCLASS32*/) {
        fprintf(stderr, "elf: not an ELF32: %s\n", path);
        free(file); return -1;
    }
    const Elf32_Ehdr *eh = (const Elf32_Ehdr *)file;
    if (eh->type != ET_EXEC || eh->machine != EM_ARM) {
        fprintf(stderr, "elf: expected ET_EXEC/EM_ARM (got type=%u machine=%u)\n",
                eh->type, eh->machine);
        free(file); return -1;
    }

    for (int i = 0; i < eh->phnum; i++) {
        const Elf32_Phdr *ph =
            (const Elf32_Phdr *)(file + eh->phoff + (size_t)i * eh->phentsize);
        if (ph->type != PT_LOAD || ph->memsz == 0) continue;

        int prot = (ph->flags & PF_R ? CPU_PROT_READ : 0) |
                   (ph->flags & PF_W ? CPU_PROT_WRITE : 0) |
                   (ph->flags & PF_X ? CPU_PROT_EXEC : 0);

        /* Runtime region (where memsz lives, e.g. .data/.bss at 0x40000000). */
        if (!cpu_is_mapped(cpu, ph->vaddr, ph->memsz) &&
            cpu_map(cpu, ph->vaddr, ph->memsz, prot) != 0) {
            fprintf(stderr, "elf: map failed for vaddr 0x%08x\n", ph->vaddr);
            free(file); return -1;
        }

        /* Init image goes to the LOAD address (p_paddr/LMA). For split LMA!=VMA
         * segments (.data here: LMA 0x2BC0C, VMA 0x40000000) the crt0 copies
         * LMA->VMA itself, so we must place the bytes at the LMA. */
        if (ph->filesz) {
            if (!cpu_is_mapped(cpu, ph->paddr, ph->filesz) &&
                cpu_map(cpu, ph->paddr, ph->filesz, prot) != 0) {
                fprintf(stderr, "elf: map failed for paddr 0x%08x\n", ph->paddr);
                free(file); return -1;
            }
            cpu_write(cpu, ph->paddr, file + ph->offset, ph->filesz);
        }
        /* Zero the .bss tail of in-place segments (vaddr==paddr). */
        if (ph->vaddr == ph->paddr && ph->memsz > ph->filesz)
            cpu_zero(cpu, ph->vaddr + ph->filesz, ph->memsz - ph->filesz);

        printf("elf: LOAD vaddr=0x%08x paddr=0x%08x filesz=0x%x memsz=0x%x flags=%c%c%c\n",
               ph->vaddr, ph->paddr, ph->filesz, ph->memsz,
               ph->flags & PF_R ? 'r' : '-',
               ph->flags & PF_W ? 'w' : '-',
               ph->flags & PF_X ? 'x' : '-');
    }

    *entry_out = eh->entry;
    free(file);
    return 0;
}
