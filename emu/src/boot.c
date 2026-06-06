/* boot.c — shared emulator bring-up + gameplay watch hooks (extracted from
 * main.c so both the desktop UI and the server child reuse identical setup). */
#include "cpu.h"
#include "elf.h"
#include "emu.h"
#include "boot.h"

#include <stdio.h>
#include <string.h>

#define LOW_BASE     0x00000000u   /* syscall trampoline table at 0x1000 */
#define LOW_SIZE     0x00002000u
#define RAM_BASE     0x40000000u
#define RAM_SIZE     0x00100000u
#define SCRATCH_BASE 0x50000000u   /* host->guest scratch (dirents) */
#define SCRATCH_SIZE 0x00001000u
#define BOOT_SP      0x400F0000u
#define SC_LO 0x1000u
#define SC_HI 0x1170u

/* Voice-line resolvers: r0 = key string (e.g. "#set_ship_to_start_port"). */
#define ADDR_VOICE_SIMPLE 0x0000461Cu   /* sub_461C(key, flags) */
#define ADDR_VOICE_VARS   0x0002155Cu   /* sub_2155C(key, nvars, vars) */
#define ADDR_VOICE_QUEUE  0x0000468Cu   /* sub_468C(key): queues a voice line */
#define ADDR_TURN_CARGO   0x00011BE8u   /* sub_11BE8: take-new-goods turn handler */
#define ADDR_RFID_MATCH   0x0001F250u   /* sub_1F250(keyptr, out, &status): RFID match */
#define ADDR_BATTLE       0x0001BD1Cu   /* sub_1BD1C(player, pirate_idx): per-pirate battle check */
#define ADDR_PIRATE_STATS 0x0000E970u   /* sub_E970(level, cannons, sailors, sails): set enemy stats */

static void on_voice(cpu_t *cpu, uint32_t addr, uint32_t idx, void *user)
{
    (void)addr; (void)idx; (void)user;
    uint32_t p = cpu_reg(cpu, 0);
    char *d = g_emu.last_voice_key;
    for (int i = 0; i < 63; i++) {
        uint8_t ch = 0; cpu_read(cpu, p + i, &ch, 1);
        d[i] = (char)ch;
        if (!ch) break;
    }
    d[63] = 0;
    if (g_emu.trace) printf("  >>> voice: %s @vms=%u\n", d, g_emu.virtual_ms);

    /* Destination rejected (wrong goods, or you're already at that port): "lift"
     * the ship figure off the port (reset sel_dest) so the player can choose a
     * different one. Otherwise the figure stays latched on the bad port and the
     * turn loops forever re-prompting -> the game appears frozen. */
    if (g_emu.in_game && g_emu.sel_dest >= 0 &&
        (strstr(d, "wrong_goods") || strstr(d, "take_new_port")))
        g_emu.sel_dest = -1;

    /* Cargo rejected (#no_goods_at_port): the chosen cargo type isn't one of the
     * two goods this port produces. Same latch-freeze as above — the cargo scan
     * (sub_11BE8 ~14903) replays #no_goods_at_port forever while the figure stays
     * on the wrong cargo field. "Lift" it (reset sel_cargo) so the player can pick
     * one of the offered cargos instead. */
    if (g_emu.in_game && g_emu.sel_cargo > 0 && strstr(d, "no_goods_at_port"))
        g_emu.sel_cargo = 0;

    /* Sea battle: OPEN the overlay on a battle / maneuver prompt; CLOSE it on the
     * first watched voice that is neither a battle nor a maneuver line. The win/lose
     * outcome voices play via sub_3394 (unwatched), but the next watched voice after
     * a fight (#credit / #title / #goods_reward / #..._success / #take_new_goods) is
     * always non-battle, so this closes promptly without hooking the hot sub_3394.
     * (A 10 s no-voice timeout in the frame loop is the final safety net.) */
    /* Real interactive sea-battle prompts are always officer lines ("of_<N>_...")
     * that end in a #..._play_maneuver choice (of_N_battle*, of_N_quest_pirate_battle,
     * of_N_multiquest_pirate_battle, of_N_quest_vip_battle). Quest mission briefings
     * such as #quest_battle_N, #quest_miss_battle_N and #multiquest_silver_battle just
     * mention "battle" with no maneuver choice and must NOT pop the overlay. */
    const char *k = (d[0] == '#') ? d + 1 : d;       /* skip optional leading '#' */
    bool officer = (strncmp(k, "of_", 3) == 0);
    bool is_battle_voice = officer &&
        (strstr(d, "maneuver") ||
         (strstr(d, "_battle") && !strstr(d, "battle_won") && !strstr(d, "battle_lost")));
    if (is_battle_voice) {
        g_emu.in_battle = true;
        g_emu.battle_ms = g_emu.virtual_ms;
    } else if (g_emu.in_battle) {
        g_emu.in_battle = false;
    }
}

/* Sea battle: sub_1BD1C runs once per pirate to test the player's port; the one
 * that matches computes the enemy stats via sub_E970. Record the candidate
 * pirate index from each check; confirm the battle (and capture stats) when
 * sub_E970 fires. */
static int g_cand_pirate;
static void on_battle_check(cpu_t *cpu, uint32_t addr, uint32_t idx, void *user)
{
    (void)addr; (void)idx; (void)user;
    g_cand_pirate = (int)cpu_reg(cpu, 1);
}
static void on_pirate_stats(cpu_t *cpu, uint32_t addr, uint32_t idx, void *user)
{
    (void)addr; (void)idx; (void)user;
    g_emu.pirate_cannons = (int)cpu_reg(cpu, 1);   /* sub_E970(level, cannons, sailors, sails) */
    g_emu.pirate_sailors = (int)cpu_reg(cpu, 2);
    g_emu.pirate_sails   = (int)cpu_reg(cpu, 3);
    g_emu.pirate_idx     = g_cand_pirate;
    /* in_battle is set by on_voice when the player is actually asked to choose a
     * maneuver (#play_maneuver) — sub_E970 just primes the captured stats, so an
     * AI/background combat doesn't pop the overlay. */
    if (g_emu.trace)
        printf("  >>> PIRATE STATS pirate=%d cannons=%d sailors=%d sails=%d @vms=%u\n",
               g_emu.pirate_idx, g_emu.pirate_cannons, g_emu.pirate_sailors,
               g_emu.pirate_sails, g_emu.virtual_ms);
}

/* Gameplay-phase signal: the first take-new-goods turn means registration is
 * done, so the UI can swap JOIN slots -> cargo buttons. */
static void on_turn_cargo(cpu_t *cpu, uint32_t addr, uint32_t idx, void *user)
{
    (void)cpu; (void)addr; (void)idx; (void)user;
    if (!g_emu.in_game && g_emu.trace)
        printf("  >>> sub_11BE8 cargo-turn entry @vms=%u\n", g_emu.virtual_ms);
    g_emu.in_game = true;
    /* New turn: clear the previous turn's cargo/destination selection. */
    g_emu.sel_cargo = 0;
    g_emu.sel_dest  = -1;
}

/* Trace-first instrumentation: log the 12-byte key sub_1F250 searches for during
 * the cargo turn, so we learn the exact cargo/port UIDs the game expects. */
static int g_rfid_log_budget;
static void on_rfid_match(cpu_t *cpu, uint32_t addr, uint32_t idx, void *user)
{
    (void)addr; (void)idx; (void)user;
    if (!g_emu.trace || !g_emu.in_game || g_rfid_log_budget <= 0) return;
    g_rfid_log_budget--;
    uint32_t p = cpu_reg(cpu, 0);
    uint8_t k[12];
    for (int i = 0; i < 12; i++) { uint8_t b = 0; cpu_read(cpu, p + i, &b, 1); k[i] = b; }
    printf("  >>> 1F250 key=%02x%02x%02x%02x%02x%02x%02x%02x lr=%08x voice=%s\n",
           k[0], k[1], k[2], k[3], k[4], k[5], k[6], k[7],
           cpu_reg(cpu, 14), g_emu.last_voice_key);
}

int emu_boot(cpu_t **cpu_out)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/pirates/app.elf", g_emu.root);
    printf("yvio-emu: loading %s (lang=%s)\n", path, g_emu.lang);

    cpu_t *cpu = cpu_create();
    if (!cpu) return 1;
    g_emu.cpu = cpu;

    if (cpu_map(cpu, LOW_BASE, LOW_SIZE, CPU_PROT_READ | CPU_PROT_WRITE | CPU_PROT_EXEC) ||
        cpu_map(cpu, RAM_BASE, RAM_SIZE, CPU_PROT_READ | CPU_PROT_WRITE) ||
        cpu_map(cpu, SCRATCH_BASE, SCRATCH_SIZE, CPU_PROT_READ | CPU_PROT_WRITE)) {
        fprintf(stderr, "memory map failed\n");
        return 1;
    }

    uint32_t entry = 0;
    if (elf_load(cpu, path, &entry) != 0) return 1;

    syscall_init();
    cpu_set_syscall_hook(cpu, SC_LO, SC_HI, syscall_dispatch, NULL);
    cpu_watch(cpu, ADDR_VOICE_SIMPLE, on_voice, NULL);
    cpu_watch(cpu, ADDR_VOICE_VARS,   on_voice, NULL);
    cpu_watch(cpu, ADDR_VOICE_QUEUE,  on_voice, NULL);
    cpu_watch(cpu, ADDR_TURN_CARGO,   on_turn_cargo, NULL);
    cpu_watch(cpu, ADDR_BATTLE,       on_battle_check, NULL);
    cpu_watch(cpu, ADDR_PIRATE_STATS, on_pirate_stats, NULL);
    cpu_watch(cpu, ADDR_RFID_MATCH,   on_rfid_match, NULL);
    g_rfid_log_budget = 120;
    g_emu.sel_dest = -1;
    sched_init(entry, BOOT_SP);

    if (cpu_out) *cpu_out = cpu;
    return 0;
}
