/* syscall.c — dispatch table + scheduler/timing/stub handlers. */
#include "emu.h"
#include "trace.h"
#include <string.h>

/* ---- simple handlers ---- */

static void sys_get_ticks(void) { ret(g_emu.virtual_ms); }

static uint32_t rng_next(void)
{
    uint32_t x = g_emu.rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return g_emu.rng_state = x;
}
static void sys_rng(void) { ret(rng_next()); }

static void sys_get_boot_args(void)  /* 0x1070: &a,&b -> 0 */
{
    uint32_t a = arg(0), b = arg(1);
    if (a) cpu_write(g_emu.cpu, a, &g_emu.boot_arg0, 4);
    if (b) cpu_write(g_emu.cpu, b, &g_emu.boot_arg1, 4);
    ret(0);
}

static void sys_input_read(void)     /* 0x1058: &out -> 0 */
{
    uint32_t mask = g_emu.input_mask;
    if (g_emu.script) {   /* headless self-test / scenario driver */
        uint32_t t = g_emu.virtual_ms;
        const char *v = g_emu.last_voice_key;
        if (t > 3000 && t < 3400) mask |= 0x0080;            /* bit7 skip intro */
        /* Advance #welintro/#join etc. with YES (bit6). At #choose_game the battle
         * scenario presses NO (bit5) twice (reject beginner+profi -> NORMAL); the
         * regression keeps pressing YES (bit6) at #choose_game_1 -> BEGINNER. */
        if (g_emu.scenario == 1 && strstr(v, "choose_game")) {
            if ((t % 3000) < 300) mask |= 0x0020;            /* NO -> normal */
        } else if (t > 6000 && (t % 4000) < 300) {
            mask |= 0x0040;                                  /* YES -> advance / beginner */
        }
        if (t > 22000) { g_emu.figures[17].present = 1; g_emu.figures[16].present = 1; }

        if (g_emu.in_game) {
            bool putfig = strstr(v, "put_figure") != 0;
            if (g_emu.scenario == 1) {
                /* BATTLE scenario: dump the pirate map once, then deliver an offered
                 * cargo to each port in turn (lift-on-#wrong_goods retries) until we
                 * land on a pirate-held port -> sea battle. Decline merchant offers. */
                if (g_emu.trace) {   /* dump pirate locations whenever they change */
                    static uint8_t last_locs[5] = { 0,0,0,0,0 };
                    static int have = 0;
                    uint8_t locs[5];
                    for (int i = 0; i < 5; i++) cpu_read(g_emu.cpu, 0x400002F2u + 12u * i, &locs[i], 1);
                    if (!have || memcmp(locs, last_locs, 5)) {
                        have = 1; memcpy(last_locs, locs, 5);
                        printf("  SCENARIO pirate-locs: %02x %02x %02x %02x %02x @vms=%u\n",
                               locs[0], locs[1], locs[2], locs[3], locs[4], t);
                    }
                }
                /* Deliver to a known-good port (P1->2 Puerto Plata, P2->0 Camarco),
                 * but FIRST host-seed a pirate at that port so the arrival check
                 * (app.elf.c:14444, runs before the quest code) triggers a battle. */
                int dest = -1;
                if (g_emu.active_color == 1) {
                    if (g_emu.sel_cargo == 0) g_emu.sel_cargo = 1;
                    else if (g_emu.sel_cargo == 1 && putfig && g_emu.sel_dest < 0) dest = 2;
                } else if (g_emu.active_color == 2) {
                    if (g_emu.sel_cargo == 0) g_emu.sel_cargo = 1;
                    else if (g_emu.sel_cargo == 1 && putfig && g_emu.sel_dest < 0) dest = 0;
                }
                if (dest >= 0) {
                    /* Seed a pirate in a PER-PLAYER slot (so two ships in flight don't
                     * clobber each other). 12-byte record: +0 active flag, +2 location
                     * (= port identity byte_40000168[16*dest]), +4 dword ship value. */
                    uint32_t slot = (uint32_t)(g_emu.active_color - 1);   /* P1->0, P2->1 */
                    uint32_t base = 0x400002F0u + 12u * slot;
                    uint8_t pid = 0; cpu_read(g_emu.cpu, 0x40000168u + 16u * dest, &pid, 1);
                    uint8_t one = 1, zero = 0;
                    cpu_write(g_emu.cpu, base + 0, &one, 1);    /* active */
                    cpu_write(g_emu.cpu, base + 1, &zero, 1);   /* type 0 -> main battle (sub_E970) */
                    cpu_write(g_emu.cpu, base + 2, &pid, 1);    /* location */
                    uint32_t shipval = 2; cpu_write(g_emu.cpu, base + 4, &shipval, 4);  /* ship value -> stats */
                    /* Populate the overlay's pirate stats directly from the ship
                     * template (dword_40000000 = sails 6 / cannons 2 / sailors 3 at
                     * level 0): seeded quest-battle paths don't call sub_E970, so the
                     * cpu_watch can't capture them. (Organic battles still use sub_E970.) */
                    g_emu.pirate_idx = (int)slot;
                    g_emu.pirate_cannons = 2;
                    g_emu.pirate_sailors = 3;
                    g_emu.pirate_sails   = 6;
                    if (g_emu.trace) printf("  SCENARIO seeded pirate@slot%u loc=0x%02x (port %d) @vms=%u\n",
                                            slot, pid, dest, t);
                    g_emu.sel_dest = dest;
                }
                if (strstr(v, "equip_offer") && (t % 3000) < 200) mask |= 0x0020; /* decline merchant */
                /* Headless only: auto-attack with KANONEN (bit 9) to resolve the
                 * battle. Interactively, control is left to the user's buttons. */
                if (g_emu.headless && g_emu.in_battle && (t % 4000) < 300) mask |= 0x0200;
            } else {
                /* Regression: P1 grain->Puerto Plata; P2 grain->Curacao(bad)->valid; then buy. */
                if (g_emu.active_color == 1) {
                    if (g_emu.sel_cargo == 0 && t > 100000) g_emu.sel_cargo = 1;
                    else if (g_emu.sel_cargo == 1 && putfig) g_emu.sel_dest = 2;
                } else if (g_emu.active_color == 2) {
                    static int p2_tries = 0;
                    if (g_emu.sel_cargo == 0) g_emu.sel_cargo = 1;
                    else if (g_emu.sel_cargo == 1 && putfig && g_emu.sel_dest < 0)
                        g_emu.sel_dest = (p2_tries++ == 0) ? 5 : 0;
                }
                if (strstr(v, "equip_offer")) {
                    static uint32_t m0 = 0; if (!m0) m0 = t;
                    if (t > m0 + 6000 && t < m0 + 6400) mask |= 0x0001;   /* KAUFEN (buy) */
                }
            }
        }
    }
    uint32_t out = arg(0);
    if (out) cpu_write(g_emu.cpu, out, &mask, 4);
    ret(0);
}

static void sys_obj24_read(void)     /* 0x105C: memcpy(buf,&sys,24) -> stub zeros */
{
    if (arg(0)) cpu_zero(g_emu.cpu, arg(0), 24);
    ret(0);
}

/* ---- scheduler-facing handlers ---- */

static void sys_scheduler_init(void) { ret(0); }   /* 0x10E4: app zeroed its lists */

static void sys_yield(void)          /* 0x10EC / 0x10F4 */
{
    thread_park_pc();
    g_emu.cur->state = T_READY;
    schedule();
}

static void sys_thread_create(void)  /* 0x113C */
{
    uint32_t tramp = arg(1), prio = arg(2), stacktop = arg(3), user_entry = arg(5);
    thread_t *t = thread_alloc();
    if (!t) { ret((uint32_t)-1); return; }
    memset(&t->ctx, 0, sizeof t->ctx);
    t->ctx.r[0]  = user_entry;          /* trampoline arg = real entry */
    t->ctx.r[13] = stacktop;
    t->ctx.r[14] = TRAMP_THREAD_EXIT;
    t->ctx.r[15] = tramp;               /* 0x25cd0 */
    t->ctx.cpsr  = 0x1F;
    t->prio = (uint8_t)prio;
    t->state = T_READY;
    t->guest_tcb = arg(0);
    if (g_emu.trace) printf("  thread_create id=%d prio=%u entry=0x%08x sp=0x%08x\n",
                            t->id, t->prio, user_entry, stacktop);
    ret(0);                              /* creator continues */
}

static void sys_thread_exit(void)    /* 0x1144 */
{
    g_emu.cur->state = T_DEAD;
    schedule();
}

static void sys_sleep_ms(void)       /* 0x115C */
{
    uint32_t ms = arg(0);
    if (ms == 0) { ret(0); return; }
    thread_park_pc();
    ret(ms);                            /* echo (saved in ctx) */
    g_emu.cur->state = T_SLEEP;
    g_emu.cur->wait_obj = NULL;
    g_emu.cur->wake_ms = g_emu.virtual_ms + ms;
    schedule();
}

static void sys_noop0(void) { ret(0); }   /* benign stub */

/* ---- dispatch ---- */

void syscall_init(void)
{
    g_emu.rng_state = 0x2545F491u;
    g_emu.boot_arg0 = g_emu.boot_arg1 = 0;
    g_emu.input_mask = 0;
}

void syscall_dispatch(cpu_t *cpu, uint32_t addr, uint32_t idx, void *user)
{
    (void)cpu; (void)idx; (void)user;
    g_emu.sc_count++;
    if (g_emu.trace)
        printf("  [t%d vms=%u] @0x%04x %-15s r0=%08x r1=%08x r2=%08x r3=%08x\n",
               g_emu.cur->id, g_emu.virtual_ms, addr, syscall_name(addr),
               arg(0), arg(1), arg(2), arg(3));

    switch (addr) {
    /* file io */
    case 0x102C: sys_fopen();   break;
    case 0x1028: sys_fclose();  break;
    case 0x1030: sys_fread();   break;
    case 0x1038: sys_fwrite();  break;
    case 0x1034: sys_fseek();   break;
    case 0x1040: sys_opendir(); break;
    case 0x1044: sys_readdir(); break;
    case 0x103C: sys_closedir();break;
    case 0x1024: sys_noop0();   break;   /* fs_op */
    case 0x1050: sys_noop0();   break;   /* fflush/reset */

    /* timing / rng / input / misc */
    case 0x1054: sys_get_ticks();    break;
    case 0x1064: sys_rng();          break;
    case 0x1070: sys_get_boot_args();break;
    case 0x1058: sys_input_read();   break;
    case 0x105C: sys_obj24_read();   break;
    case 0x1060: sys_noop0();        break;   /* obj24_write */

    /* rfid */
    case 0x1074: sys_rfid_inventory();   break;
    case 0x1078: sys_rfid_read_block();  break;
    case 0x107C: sys_rfid_write_block(); break;
    case 0x1080: sys_rfid_release();     break;
    case 0x1088: sys_rfid_read_cached(); break;

    /* audio */
    case 0x1094: sys_audio_stop();  break;
    case 0x1098: sys_audio_start(); break;
    case 0x109C: case 0x10A0: sys_noop0(); break;   /* init / mode-vol */

    /* scheduler / ipc */
    case 0x10E4: sys_scheduler_init(); break;
    case 0x10EC: sys_yield(); break;
    case 0x10F4: sys_yield(); break;
    case 0x10F8: sys_queue_create(); break;
    case 0x1108: sys_queue_recv(true);  break;
    case 0x110C: sys_queue_recv(false); break;
    case 0x1110: sys_queue_send(true);  break;
    case 0x1114: sys_queue_send(false); break;
    case 0x1118: sys_sem_take(false); break;
    case 0x111C: sys_sem_create();    break;
    case 0x112C: sys_sem_take(true);  break;
    case 0x1130: sys_sem_give();      break;
    case 0x113C: sys_thread_create(); break;
    case 0x1144: sys_thread_exit();   break;
    case 0x115C: sys_sleep_ms();      break;

    default:
        printf("  *** UNIMPL syscall @0x%04x (idx %u) lr=0x%08x — returning 0\n",
               addr, idx, cpu_reg(g_emu.cpu, 14));
        sys_noop0();
        break;
    }
}
