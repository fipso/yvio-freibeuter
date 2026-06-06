/* cpu_unicorn.c — Unicorn-backed implementation of cpu.h (ARM7TDMI-S / ARMv4T). */
#include "cpu.h"

#include <unicorn/unicorn.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_REGIONS 16
#define PAGE 0x1000u

typedef struct {
    uint32_t addr;   /* page-aligned guest base */
    size_t   size;   /* page-aligned size */
    uint8_t *host;   /* owned backing store */
} region_t;

struct cpu {
    uc_engine     *uc;
    region_t       regions[MAX_REGIONS];
    int            nregions;

    uc_hook        sc_hook;
    cpu_syscall_cb sc_cb;
    void          *sc_user;

    bool           syscall_pending; /* set by hook before uc_emu_stop */
    bool           halted;
    bool           faulted;
    uint32_t       fault_addr;      /* offending data address on the fault */
};

static const int kArmReg[16] = {
    UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2,  UC_ARM_REG_R3,
    UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6,  UC_ARM_REG_R7,
    UC_ARM_REG_R8, UC_ARM_REG_R9, UC_ARM_REG_R10, UC_ARM_REG_R11,
    UC_ARM_REG_R12, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC,
};

/* ---- region helpers ---- */

static region_t *region_for(cpu_t *c, uint32_t gaddr)
{
    for (int i = 0; i < c->nregions; i++) {
        region_t *r = &c->regions[i];
        if (gaddr >= r->addr && gaddr < r->addr + r->size)
            return r;
    }
    return NULL;
}

bool cpu_is_mapped(cpu_t *c, uint32_t addr, size_t size)
{
    region_t *r = region_for(c, addr);
    return r && (uint64_t)addr + size <= (uint64_t)r->addr + r->size;
}

void *cpu_host_ptr(cpu_t *c, uint32_t gaddr)
{
    region_t *r = region_for(c, gaddr);
    return r ? r->host + (gaddr - r->addr) : NULL;
}

int cpu_map(cpu_t *c, uint32_t addr, size_t size, int prot)
{
    if (c->nregions >= MAX_REGIONS) return -1;
    uint32_t base = addr & ~(PAGE - 1);
    size_t   end  = (addr + size + PAGE - 1) & ~(PAGE - 1);
    size_t   len  = end - base;

    uint8_t *host = calloc(1, len);
    if (!host) return -1;

    uc_err err = uc_mem_map_ptr(c->uc, base, len, (uint32_t)prot, host);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "cpu_map(0x%08x,0x%zx): %s\n", base, len, uc_strerror(err));
        free(host);
        return -1;
    }
    c->regions[c->nregions++] = (region_t){ base, len, host };
    return 0;
}

void cpu_read(cpu_t *c, uint32_t gaddr, void *dst, size_t n)
{
    void *p = cpu_host_ptr(c, gaddr);
    if (p) memcpy(dst, p, n);
    else   uc_mem_read(c->uc, gaddr, dst, n);
}

void cpu_write(cpu_t *c, uint32_t gaddr, const void *src, size_t n)
{
    void *p = cpu_host_ptr(c, gaddr);
    if (p) memcpy(p, src, n);
    else   uc_mem_write(c->uc, gaddr, src, n);
}

void cpu_zero(cpu_t *c, uint32_t gaddr, size_t n)
{
    void *p = cpu_host_ptr(c, gaddr);
    if (p) memset(p, 0, n);
    else { uint8_t z = 0; for (size_t i = 0; i < n; i++) uc_mem_write(c->uc, gaddr + i, &z, 1); }
}

/* ---- registers ---- */

uint32_t cpu_reg(cpu_t *c, int idx)
{
    uint32_t v = 0;
    uc_reg_read(c->uc, kArmReg[idx & 15], &v);
    return v;
}

void cpu_set_reg(cpu_t *c, int idx, uint32_t val)
{
    uc_reg_write(c->uc, kArmReg[idx & 15], &val);
}

uint32_t cpu_cpsr(cpu_t *c)
{
    uint32_t v = 0;
    uc_reg_read(c->uc, UC_ARM_REG_CPSR, &v);
    return v;
}

void cpu_set_cpsr(cpu_t *c, uint32_t val)
{
    uc_reg_write(c->uc, UC_ARM_REG_CPSR, &val);
}

void cpu_get_ctx(cpu_t *c, cpu_ctx_t *ctx)
{
    for (int i = 0; i < 16; i++) uc_reg_read(c->uc, kArmReg[i], &ctx->r[i]);
    uc_reg_read(c->uc, UC_ARM_REG_CPSR, &ctx->cpsr);
}

void cpu_set_ctx(cpu_t *c, const cpu_ctx_t *ctx)
{
    for (int i = 0; i < 16; i++) uc_reg_write(c->uc, kArmReg[i], (void *)&ctx->r[i]);
    uc_reg_write(c->uc, UC_ARM_REG_CPSR, (void *)&ctx->cpsr);
}

/* ---- syscall hook ---- */

static bool hook_mem_invalid(uc_engine *uc, uc_mem_type type, uint64_t addr,
                             int size, int64_t value, void *ud)
{
    (void)uc;
    cpu_t *c = (cpu_t *)ud;
    if (c) c->fault_addr = (uint32_t)addr;
    const char *t = type == UC_MEM_WRITE_UNMAPPED ? "WRITE" :
                    type == UC_MEM_READ_UNMAPPED  ? "READ"  :
                    type == UC_MEM_FETCH_UNMAPPED ? "FETCH" : "PROT";
    /* Printed once: cpu_run short-circuits once faulted, so this hook won't fire
     * again for the same dead PC (no per-frame spam). */
    fprintf(stderr, "  mem-invalid %s @0x%08x size=%d value=0x%llx\n",
            t, (uint32_t)addr, size, (unsigned long long)value);
    return false; /* don't fix up -> emulation stops with the error */
}

static void hook_code(uc_engine *uc, uint64_t addr, uint32_t size, void *ud)
{
    (void)size;
    cpu_t *c = (cpu_t *)ud;
    uint32_t a = (uint32_t)addr;
    uint32_t idx = (a - 0x1000u) / 4u;

    c->syscall_pending = true;
    if (c->sc_cb) c->sc_cb(c, a, idx, c->sc_user);
    uc_emu_stop(uc); /* re-enter the host run loop with a defined state */
}

int cpu_set_syscall_hook(cpu_t *c, uint32_t lo, uint32_t hi, cpu_syscall_cb cb, void *user)
{
    c->sc_cb = cb;
    c->sc_user = user;
    uc_err err = uc_hook_add(c->uc, &c->sc_hook, UC_HOOK_CODE, (void *)hook_code,
                             c, lo, hi - 1);
    return err == UC_ERR_OK ? 0 : -1;
}

#define MAX_WATCH 12   /* boot.c installs 7 gameplay watches; was 4 -> 3 silently dropped */
static struct { cpu_t *c; cpu_syscall_cb cb; void *user; uc_hook h; } g_watch[MAX_WATCH];
static int g_nwatch;

static void hook_watch(uc_engine *uc, uint64_t addr, uint32_t size, void *ud)
{
    (void)uc; (void)size;
    int i = (int)(intptr_t)ud;
    if (g_watch[i].cb) g_watch[i].cb(g_watch[i].c, (uint32_t)addr, 0, g_watch[i].user);
}

int cpu_watch(cpu_t *c, uint32_t addr, cpu_syscall_cb cb, void *user)
{
    if (g_nwatch >= MAX_WATCH) return -1;
    int i = g_nwatch++;
    g_watch[i].c = c; g_watch[i].cb = cb; g_watch[i].user = user;
    uc_err err = uc_hook_add(c->uc, &g_watch[i].h, UC_HOOK_CODE, (void *)hook_watch,
                             (void *)(intptr_t)i, addr, addr);
    return err == UC_ERR_OK ? 0 : -1;
}

/* ---- run loop ---- */

cpu_status_t cpu_run(cpu_t *c, uint64_t max_insns)
{
    if (c->halted) return CPU_HALT;
    if (c->faulted) return CPU_FAULT;   /* don't re-run a dead PC -> no fault spam */

    uint32_t pc = cpu_reg(c, 15);
    uint32_t cpsr = cpu_cpsr(c);
    uint64_t begin = pc;
    if (cpsr & (1u << 5)) begin |= 1u; /* Thumb: pass T-state via bit0 to uc_emu_start */

    c->syscall_pending = false;
    uc_err err = uc_emu_start(c->uc, begin, 0xFFFFFFFFull, 0, max_insns);

    if (c->syscall_pending) return CPU_SYSCALL;
    if (err != UC_ERR_OK) {
        c->faulted = true;
        fprintf(stderr, "cpu_run: uc error @0x%08x: %s\n", cpu_reg(c, 15), uc_strerror(err));
        return CPU_FAULT;
    }
    return CPU_OK; /* instruction budget exhausted */
}

void cpu_stop(cpu_t *c) { uc_emu_stop(c->uc); }

bool     cpu_faulted(cpu_t *c)    { return c->faulted; }
uint32_t cpu_fault_addr(cpu_t *c) { return c->fault_addr; }

/* ---- lifecycle ---- */

cpu_t *cpu_create(void)
{
    cpu_t *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    uc_err err = uc_open(UC_ARCH_ARM, UC_MODE_ARM | UC_MODE_LITTLE_ENDIAN, &c->uc);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "uc_open: %s\n", uc_strerror(err));
        free(c);
        return NULL;
    }
    uc_hook mh;
    uc_hook_add(c->uc, &mh, UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED |
                UC_HOOK_MEM_FETCH_UNMAPPED, (void *)hook_mem_invalid, c, 1, 0);
    return c;
}

void cpu_destroy(cpu_t *c)
{
    if (!c) return;
    if (c->uc) uc_close(c->uc);
    for (int i = 0; i < c->nregions; i++) free(c->regions[i].host);
    free(c);
}
