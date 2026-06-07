/* emu.h — global emulator state: cooperative scheduler, IPC, syscall wiring. */
#ifndef YVIO_EMU_H
#define YVIO_EMU_H

#include "cpu.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define MAX_THREADS 16
#define MAX_IPC     64
#define QUEUE_CAP   32
#define NEVER       0xFFFFFFFFu

/* Trampoline addresses the host fabricates / recognises. */
#define TRAMP_THREAD_EXIT 0x1144u

typedef enum { T_FREE, T_READY, T_RUN, T_BLOCKED, T_SLEEP, T_DEAD } tstate_t;
typedef enum { WK_NONE, WK_SEM, WK_QRECV, WK_QSEND } waitkind_t;

typedef struct thread {
    cpu_ctx_t  ctx;        /* saved ARM regs (valid when this thread isn't running) */
    int        used;
    int        id;
    uint8_t    prio;       /* 1..30, higher = preferred */
    tstate_t   state;
    uint32_t   wake_ms;    /* T_SLEEP / timed-block deadline (NEVER = none) */
    void      *wait_obj;   /* hsem_t* or hqueue_t* when blocked */
    waitkind_t wait_kind;
    uint32_t   recv_out;   /* WK_QRECV: guest ptr to write the received item */
    uint32_t   send_item;  /* WK_QSEND: item value waiting to be enqueued */
    uint32_t   guest_tcb;  /* app's 124-byte TCB addr (opaque handle) */
} thread_t;

typedef struct {
    int      used;
    uint32_t gaddr;
    uint32_t items[QUEUE_CAP];
    int      head, tail, count, cap;
} hqueue_t;

typedef struct {
    int      used;
    uint32_t gaddr;
    int      count, max;
} hsem_t;

typedef struct emu {
    cpu_t   *cpu;
    const char *root;
    const char *lang;

    /* scheduler */
    thread_t threads[MAX_THREADS];
    thread_t *cur;
    int      next_id;
    bool     switched;   /* a handler context-switched: don't PC=LR */
    bool     idle;       /* nothing runnable and no timer pending */
    bool     running;
    uint64_t switch_count;

    /* time / rng */
    uint32_t virtual_ms;
    uint32_t rng_state;
    uint32_t boot_arg0, boot_arg1;

    /* ipc */
    hqueue_t queues[MAX_IPC];
    hsem_t   sems[MAX_IPC];

    /* input: 15-bit board+button mask returned by 0x1058 */
    uint32_t input_mask;

    /* RFID figures: one optional transponder per board field (rfid.c) */
    struct { int present; uint8_t color; uint8_t mem[320]; } figures[21];

    /* gameplay phase + cargo-turn selection (set by main.c watch / ui.c) */
    bool     in_game;       /* true once registration completes (cargo turn reachable) */
    int      sel_cargo;     /* 1..4 = wheat/wood/tobacco/rum; 0 = none (ui.c) */
    int      sel_dest;      /* 0..7 = chosen target port; -1 = none (ui.c) */
    int      offered[2];    /* cargo types (1..4) the active player's port offers; 0 = unknown */
    int      active_color;  /* active player's color 1..4 (read from guest state) */

    /* active player's equipment + money (rfid.c reads from the player record) */
    int      money;         /* dukaten (rec+20) */
    int      eq_cannons;    /* total cannon points = rec[27] + 2*rec[28] (combat total rec[37]) */
    int      eq_sailors;    /* rec[29] + 2*rec[30] (combat total rec[38]) */
    int      eq_sails;      /* rec[25] + 2*rec[26] */
    int      eq_cargo;      /* rec[31] + 2*rec[32] (storage) */
    int      my_cannons, my_sailors;   /* combat values rec[37]/rec[38] */

    /* sea battle (main.c watches set these; ui.c renders the overlay) */
    bool     in_battle;
    uint32_t battle_ms;     /* virtual_ms of the last battle/maneuver voice (timeout) */
    int      pirate_idx;    /* opponent pirate index 0..4 */
    int      pirate_cannons, pirate_sailors, pirate_sails;   /* PvE: from sub_E970; PvP: opponent record */
    bool     pvp_battle;    /* true during a player-vs-player capture battle */
    int      enemy_color;   /* PvP opponent's colour 1..4 (0 = NPC/none) */
    uint32_t pvp_attacker_rec; /* captured attacker player-record ptr (0 = none), set at sub_C6C0 */
    bool     pvp_pending;   /* sub_C6C0 fired: a PvP capture battle is live */

    /* map markers (boot.c silver watch + rfid map_refresh_markers fill; server.c serializes) */
    int8_t   player_ports[4];   /* [colour-1] -> current port 0..7, -1 = in transit/unknown */
    uint8_t  pirate_ports;      /* bitmask: bit i => port i occupied by a pirate (0x400002F0) */
    uint8_t  silver_ports;      /* bitmask: bit i => silverfleet rumor names port i */

    /* audio bridge state */
    bool     audio_running;
    bool     audio_realtime;   /* true when a real (raylib) sink is attached */
    uint32_t audio_cons_buf;   /* guest addr of current consume buffer */
    uint32_t audio_next_ms;    /* virtual-time meter (headless null sink only) */
    uint64_t audio_bytes;      /* total PCM bytes drained */

    /* HUD / debug: the voice-line key the game is currently speaking */
    char     last_voice_key[64];
    bool     fast;        /* fast-forward (skip intro): run fast + mute audio */

    /* unrecoverable guest fault (e.g. the quest/news wild-read @0x1da6c): set
     * once; the host loops stop stepping and surface it instead of re-running. */
    bool     crashed;
    uint32_t fault_pc;
    uint32_t fault_addr;

    /* trace + headless run caps (0 = unlimited) */
    bool     trace;
    bool     script;     /* headless: auto-drive skip/choose/register for testing */
    bool     headless;   /* no UI: auto-resolve battles in scenarios */
    int      scenario;   /* 0 = regression, 1 = "battle" (normal game -> sea battle) */
    uint64_t sc_count, sc_limit;
    uint32_t stop_vms;
} emu_t;

extern emu_t g_emu;

/* ---- syscall dispatch (syscall.c) ---- */
void syscall_init(void);
void syscall_dispatch(cpu_t *cpu, uint32_t addr, uint32_t idx, void *user);

/* ---- scheduler (sched.c) ---- */
void      sched_init(uint32_t boot_pc, uint32_t boot_sp);
void      sched_run(void);                 /* the host run loop */
void      schedule(void);                  /* save cur, pick next, load it */
thread_t *thread_alloc(void);
/* helper used by blocking handlers: advance PC past the syscall before parking */
void      thread_park_pc(void);
/* wake a blocked/sleeping thread with a return value already set in its ctx */
void      thread_wake(thread_t *t, uint32_t r0);

/* handler return helpers (operate on g_emu.cur via g_emu.cpu) */
uint32_t  arg(int i);                      /* r0..r3 then stack */
void      ret(uint32_t v);                 /* set r0 */

/* ---- ipc (ipc.c) ---- */
void sys_queue_create(void);
void sys_queue_recv(bool timed);
void sys_queue_send(bool timed);
void sys_sem_create(void);
void sys_sem_take(bool try_only);
void sys_sem_give(void);
hsem_t *ipc_sem_find(uint32_t gaddr);
void    ipc_sem_post(hsem_t *s);   /* wake highest waiter, else count++ */

/* ---- audio bridge (audio.c) ---- */
void sys_audio_start(void);        /* 0x1098 */
void sys_audio_stop(void);         /* 0x1094 */
void audio_pump(void);             /* drain ready buffers, metered to vtime */
/* Sink returns 1 if it accepted the block, 0 if its buffer is full (backpressure). */
typedef int (*audio_sink_fn)(const int16_t *pcm128, void *user);
void audio_set_sink(audio_sink_fn fn, void *user);

/* ---- raylib frontend (audio_host.c, ui.c) ---- */
void audiohost_init(void);
void audiohost_shutdown(void);
void ui_init(void);
void ui_poll(void);        /* read mouse/keys -> g_emu.input_mask */
void ui_draw(void);
bool ui_should_close(void);
void ui_shutdown(void);
uint32_t ui_frame_ms(void);

/* ---- rfid (rfid.c) ---- */
void rfid_refresh_stats(void);    /* refresh active player's money/equipment into g_emu */
void map_refresh_markers(void);   /* fill player_ports[] + pirate_ports from guest state */
void sys_rfid_inventory(void);    /* 0x1074 */
void sys_rfid_read_block(void);   /* 0x1078 */
void sys_rfid_write_block(void);  /* 0x107C */
void sys_rfid_release(void);      /* 0x1080 */
void sys_rfid_read_cached(void);  /* 0x1088 */

/* ---- file io (file.c) ---- */
void sys_fopen(void);
void sys_fclose(void);
void sys_fread(void);
void sys_fwrite(void);
void sys_fseek(void);
void sys_opendir(void);
void sys_readdir(void);
void sys_closedir(void);

#endif /* YVIO_EMU_H */
