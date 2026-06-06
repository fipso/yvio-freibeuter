/* sched.c — cooperative scheduler over saved ARM contexts + virtual clock.
 *
 * One guest thread runs in the CPU at a time. Blocking syscalls park the
 * running thread (after advancing its PC past the syscall) and switch to the
 * highest-priority ready thread. Virtual time idle-skips to the next deadline
 * when nothing is runnable now, so the input thread's sleep(15) loop is free.
 */
#include "emu.h"
#include <string.h>

emu_t g_emu;

/* ---- argument / return helpers (AAPCS) ---- */

uint32_t arg(int i)
{
    if (i < 4) return cpu_reg(g_emu.cpu, i);
    uint32_t sp = cpu_reg(g_emu.cpu, 13);
    uint32_t v = 0;
    cpu_read(g_emu.cpu, sp + (uint32_t)(i - 4) * 4, &v, 4);
    return v;
}

void ret(uint32_t v) { cpu_set_reg(g_emu.cpu, 0, v); }

/* ---- thread table ---- */

thread_t *thread_alloc(void)
{
    for (int i = 0; i < MAX_THREADS; i++)
        if (!g_emu.threads[i].used) {
            thread_t *t = &g_emu.threads[i];
            memset(t, 0, sizeof *t);
            t->used = 1;
            t->id = g_emu.next_id++;
            t->wake_ms = NEVER;
            return t;
        }
    fprintf(stderr, "sched: out of thread slots\n");
    return NULL;
}

void thread_park_pc(void)
{
    /* On resume the thread must continue *after* the syscall: PC = LR. */
    cpu_set_reg(g_emu.cpu, 15, cpu_reg(g_emu.cpu, 14));
}

void thread_wake(thread_t *t, uint32_t r0)
{
    t->ctx.r[0] = r0;
    t->state = T_READY;
    t->wait_obj = NULL;
    t->wait_kind = WK_NONE;
    t->wake_ms = NEVER;
}

/* ---- scheduling policy ---- */

static thread_t *pick_ready(void)
{
    thread_t *best = NULL;
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_t *t = &g_emu.threads[i];
        if (t->used && t->state == T_READY)
            if (!best || t->prio > best->prio) best = t;
    }
    return best;
}

/* Advance the virtual clock to the earliest pending deadline, waking any
 * threads (sleepers or timed-blocked) whose deadline has passed. */
static int advance_clock(void)
{
    uint32_t earliest = NEVER;
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_t *t = &g_emu.threads[i];
        if (t->used && (t->state == T_SLEEP || t->state == T_BLOCKED) &&
            t->wake_ms != NEVER && t->wake_ms < earliest)
            earliest = t->wake_ms;
    }
    if (earliest == NEVER) return 0;
    if (earliest > g_emu.virtual_ms) g_emu.virtual_ms = earliest;

    for (int i = 0; i < MAX_THREADS; i++) {
        thread_t *t = &g_emu.threads[i];
        if (!t->used || t->wake_ms == NEVER) continue;
        if ((t->state == T_SLEEP || t->state == T_BLOCKED) &&
            t->wake_ms <= g_emu.virtual_ms) {
            /* timed-block elapsed -> -4 timeout; sleep elapsed -> echoes arg
             * (already stored in r0 at park time). */
            uint32_t r0 = (t->state == T_BLOCKED) ? (uint32_t)-4 : t->ctx.r[0];
            thread_wake(t, r0);
        }
    }
    return 1;
}

void schedule(void)
{
    cpu_get_ctx(g_emu.cpu, &g_emu.cur->ctx);
    if (g_emu.cur->state == T_RUN) g_emu.cur->state = T_READY;

    thread_t *next = pick_ready();
    while (!next) {
        if (!advance_clock()) { g_emu.idle = true; break; }
        next = pick_ready();
    }
    if (!next) {
        /* Nothing to run and no timer: keep cur loaded; the run loop stops. */
        g_emu.switched = true; /* prevent the hook from doing PC=LR */
        return;
    }
    next->state = T_RUN;
    cpu_set_ctx(g_emu.cpu, &next->ctx);
    g_emu.cur = next;
    g_emu.switched = true;
    g_emu.switch_count++;
}

/* ---- bring-up ---- */

void sched_init(uint32_t boot_pc, uint32_t boot_sp)
{
    memset(g_emu.threads, 0, sizeof g_emu.threads);
    g_emu.next_id = 0;

    thread_t *boot = thread_alloc();        /* thread 0 = boot/entry */
    boot->prio = 14;
    boot->state = T_RUN;
    g_emu.cur = boot;

    cpu_set_cpsr(g_emu.cpu, 0x1F);          /* SYS, ARM, IRQ/FIQ enabled */
    cpu_set_reg(g_emu.cpu, 15, boot_pc);
    cpu_set_reg(g_emu.cpu, 13, boot_sp);
    cpu_set_reg(g_emu.cpu, 14, 0xFFFFFFFCu);
}

/* The host run loop: drive the CPU, handle syscalls, idle-skip virtual time. */
void sched_run(void)
{
    g_emu.running = true;
    const uint64_t BUDGET = 50000000ull;

    while (g_emu.running) {
        cpu_status_t st = cpu_run(g_emu.cpu, BUDGET);
        audio_pump();   /* drain ready audio buffers (metered to virtual time) */

        if (st == CPU_SYSCALL) {
            if (g_emu.switched) {
                g_emu.switched = false;     /* new thread already loaded */
            } else {
                /* non-blocking syscall: return to caller (PC = LR) */
                cpu_set_reg(g_emu.cpu, 15, cpu_reg(g_emu.cpu, 14));
            }
            if (g_emu.idle) {
                printf("sched: idle (no runnable thread, no pending timer) @vms=%u\n",
                       g_emu.virtual_ms);
                break;
            }
            if (g_emu.sc_limit && g_emu.sc_count >= g_emu.sc_limit) {
                printf("sched: syscall cap %llu reached (vms=%u switches=%llu)\n",
                       (unsigned long long)g_emu.sc_limit, g_emu.virtual_ms,
                       (unsigned long long)g_emu.switch_count);
                break;
            }
            if (g_emu.stop_vms && g_emu.virtual_ms >= g_emu.stop_vms)
                break;   /* per-frame pacing cap (interactive): silent */
            continue;
        }
        if (st == CPU_OK) continue;          /* budget exhausted; keep going */
        if (st == CPU_HALT) { printf("sched: halt\n"); break; }
        printf("sched: fault @0x%08x\n", cpu_reg(g_emu.cpu, 15));
        break;
    }
}
