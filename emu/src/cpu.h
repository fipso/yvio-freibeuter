/* cpu.h — the only thing the rest of the emulator knows about the CPU.
 *
 * Backed by Unicorn (cpu_unicorn.c) for M1; a hand-written ARM7TDMI-S
 * interpreter can implement the same interface at M3 for determinism / WASM.
 */
#ifndef YVIO_CPU_H
#define YVIO_CPU_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct cpu cpu_t;

/* Saved ARM register file = one "OS thread" context (the scheduler swaps these). */
typedef struct {
    uint32_t r[16]; /* r0..r15 (r13=SP, r14=LR, r15=PC) */
    uint32_t cpsr;
} cpu_ctx_t;

typedef enum {
    CPU_OK = 0,   /* ran out of instruction budget / stopped cleanly */
    CPU_SYSCALL,  /* hit the syscall trampoline table; handler already ran */
    CPU_FAULT,    /* unmapped access / decode error */
    CPU_HALT,     /* hit the LR sentinel (entry returned) */
} cpu_status_t;

/* Memory protections (mirror Unicorn's UC_PROT_*). */
enum { CPU_PROT_READ = 1, CPU_PROT_WRITE = 2, CPU_PROT_EXEC = 4 };

/* Called when PC enters the syscall trampoline range. The handler reads
 * r0..r3 / stack, sets r0, and may switch thread contexts. */
typedef void (*cpu_syscall_cb)(cpu_t *cpu, uint32_t trampoline_addr,
                               uint32_t idx, void *user);

cpu_t *cpu_create(void);
void   cpu_destroy(cpu_t *);

/* Map a guest region backed by a host buffer so handlers can memcpy directly.
 * addr/size are rounded to 4 KiB. Returns 0 on success. The backing memory is
 * owned by the cpu and zero-initialised. */
int   cpu_map(cpu_t *, uint32_t addr, size_t size, int prot);
bool  cpu_is_mapped(cpu_t *, uint32_t addr, size_t size);
void *cpu_host_ptr(cpu_t *, uint32_t gaddr); /* host pointer into a mapped region, or NULL */

/* Host-side memory access (bypasses guest protections; used by loaders/handlers). */
void cpu_read (cpu_t *, uint32_t gaddr, void *dst, size_t n);
void cpu_write(cpu_t *, uint32_t gaddr, const void *src, size_t n);
void cpu_zero (cpu_t *, uint32_t gaddr, size_t n);

/* Install a syscall hook over [lo, hi). */
int cpu_set_syscall_hook(cpu_t *, uint32_t lo, uint32_t hi, cpu_syscall_cb, void *user);

/* Watch a single guest code address; cb fires (with idx=0) when PC reaches it. */
int cpu_watch(cpu_t *, uint32_t addr, cpu_syscall_cb cb, void *user);

/* Run the current context until a syscall trap, fault, halt, or instruction
 * budget is exhausted. */
cpu_status_t cpu_run(cpu_t *, uint64_t max_insns);
void cpu_stop(cpu_t *);

/* Fault state: set once when the guest takes an unrecoverable fault (unmapped
 * access / decode error). Once faulted, cpu_run returns CPU_FAULT without
 * re-entering the engine, so the host loop can stop instead of re-running the
 * dead PC. cpu_fault_addr() is the offending data address (0 for a fetch/decode). */
bool     cpu_faulted(cpu_t *);
uint32_t cpu_fault_addr(cpu_t *);

/* Register / context access. */
uint32_t cpu_reg(cpu_t *, int idx);            /* idx 0..15 */
void     cpu_set_reg(cpu_t *, int idx, uint32_t val);
uint32_t cpu_cpsr(cpu_t *);
void     cpu_set_cpsr(cpu_t *, uint32_t val);
void     cpu_get_ctx(cpu_t *, cpu_ctx_t *);
void     cpu_set_ctx(cpu_t *, const cpu_ctx_t *);

#endif /* YVIO_CPU_H */
