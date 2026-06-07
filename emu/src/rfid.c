/* rfid.c — host model of the board's ISO15693 RFID reader. Each board field may
 * hold a figure whose transponder identifies its player color. The game reads
 * these during registration (sub_1F250 -> sub_1E230 -> 0x1088) and cargo.
 *
 * Phase 1 (current): LOG every call with args so we can learn the exact contract
 * the game expects, then implement the real responses. */
#include "emu.h"
#include <string.h>

/* The board RFID model: 0x1088 selects a field/antenna; 0x1074 inventories the
 * selected field and returns the UID(s) of present figure(s). A figure's 12-byte
 * UID must match an off_2B134 identity key {key,0xFF,0,0,0,0xFF,...}; the game's
 * registration scans fields 14..17 (keys 0x0e..0x11). */
static int g_sel_field;

static int trace_on(void) { return g_emu.trace; }

/* The registered-figures table dword_400004F4 (8 x 16-byte entries) is matched by
 * sub_1F250 (live-read-dead path) against a searched field key via sub_1F184 (a
 * byte0 compare, byte1=0xFF terminator). sub_1F250 returns (table index + 1) of
 * the match, which the game checks == the active player's colour. We seed it. */
#define G_TABLE_PTR  0x400004F4u   /* guest global: pointer to the table */
#define TABLE_ADDR   0x50000800u   /* host-controlled buffer in scratch RAM */

/* Board field keys (byte0 of each field's antenna address), read off a --trace of
 * sub_1F250 during the cargo turn (sub_11BE8):
 *   cargo fields (off_2B134, cargo 1..4 = wheat/wood/tobacco/rum): 0x11/0x10/0x0f/0x0e
 *   port  fields (off_2B0B4, ports 0..7): 0x12 0x13 0x00 0x04 0x05 0x07 0x0a 0x0c   */
static const uint8_t CARGO_FIELD[4] = { 0x11, 0x10, 0x0f, 0x0e };
static const uint8_t PORT_FIELD[8]  = { 0x12, 0x13, 0x00, 0x04, 0x05, 0x07, 0x0a, 0x0c };

#define G_PLAYER_HEAD 0x40000444u   /* dword_40000444: head of player record list */
#define G_TURN_CTR    0x4000225Cu   /* dword_4000225C: turn counter */

/* Read the active player's state into g_emu: colour (1..4) and the two cargo
 * types its current port offers (1..4). Walk the player list for the record whose
 * turn marker (+12) == the turn counter; colour at +2, ship-pos ptr at +8 whose
 * bytes [1],[2] are the two offered good indices (0..3 -> cargo type +1). (sub_11BE8.) */
static int active_player_color(void)
{
    int found = 0;   /* 0 = active player not resolvable this call (between turns) */
    uint32_t head = 0, turn = 0;
    cpu_read(g_emu.cpu, G_PLAYER_HEAD, &head, 4);
    cpu_read(g_emu.cpu, G_TURN_CTR, &turn, 4);
    uint32_t rec = head;
    for (int i = 0; i < 8 && rec >= 0x40000000u && rec < 0x40100000u; i++) {
        uint32_t t12 = 0; cpu_read(g_emu.cpu, rec + 12, &t12, 4);
        if (t12 == turn) {
            uint8_t c = 0; cpu_read(g_emu.cpu, rec + 2, &c, 1);
            if (c >= 1 && c <= 4) {
                found = c;
                g_emu.active_color = c;
                g_emu.offered[0] = g_emu.offered[1] = 0;
                uint32_t shippos = 0; cpu_read(g_emu.cpu, rec + 8, &shippos, 4);
                if (shippos >= 0x40000000u && shippos < 0x40100000u) {
                    uint8_t g1 = 0, g2 = 0;
                    cpu_read(g_emu.cpu, shippos + 1, &g1, 1);
                    cpu_read(g_emu.cpu, shippos + 2, &g2, 1);
                    if (g1 < 4) g_emu.offered[0] = g1 + 1;   /* good index -> cargo type */
                    if (g2 < 4) g_emu.offered[1] = g2 + 1;
                }
                /* equipment + money for the equipment panel (ship-upgrade doc:
                 * rec[25..34] = sail/sailor/cannon/cargo/residence L1/L2; total
                 * points = L1 + 2*L2. money = rec+20. combat vals = rec[37/38]. */
                int32_t money = 0; cpu_read(g_emu.cpu, rec + 20, &money, 4);
                g_emu.money = money;
                uint8_t e[10]; cpu_read(g_emu.cpu, rec + 25, e, 10);
                g_emu.eq_sails   = e[0] + 2 * e[1];   /* rec[25],[26] sails   -> total rec[35] */
                g_emu.eq_cannons = e[2] + 2 * e[3];   /* rec[27],[28] cannons -> total rec[37] (firing) */
                g_emu.eq_sailors = e[4] + 2 * e[5];   /* rec[29],[30] sailors -> total rec[38] (boarding) */
                g_emu.eq_cargo   = e[6] + 2 * e[7];   /* rec[31],[32] cargo   -> total rec[36] */
                uint8_t cv[2]; cpu_read(g_emu.cpu, rec + 37, cv, 2);
                g_emu.my_cannons = cv[0];             /* rec[37] */
                g_emu.my_sailors = cv[1];             /* rec[38] */
            }
            break;
        }
        cpu_read(g_emu.cpu, rec + 88, &rec, 4);
    }
    return found;
}

/* In a player-vs-player battle the on-screen enemy is the OTHER player sharing the
 * active player's board field. The firmware pairs them with the collision test at
 * 0x12740 (same ship-pos rec+8, different colour rec+2) and runs sub_C6C0, which
 * never calls sub_E970 — so pirate_* would otherwise show stale NPC stats. We detect
 * PvP structurally: while the battle overlay is up (in_battle), if another player is
 * co-located on the active player's field, this is PvP — show that player's real
 * combat totals (rec+0x23 sails, +0x25 cannons, +0x26 sailors = the eq_* totals) and
 * its colour. Otherwise it's an NPC pirate and pirate_* (from sub_E970) is left as-is.
 * A one-time-per-battle --trace dump prints the player list + firmware battle flag so
 * the contract can be verified on real PvP fights. */
static void resolve_pvp_opponent(void)
{
    static int dumped = 0;
    static int last_log = -1;
    g_emu.enemy_color = 0;
    g_emu.pvp_battle  = false;
    if (!g_emu.in_battle) {
        dumped = 0; last_log = -1;
        g_emu.pvp_attacker_rec = 0; g_emu.pvp_pending = false;   /* clear capture at battle end */
        return;
    }

    uint32_t head = 0, turn = 0;
    cpu_read(g_emu.cpu, G_PLAYER_HEAD, &head, 4);
    cpu_read(g_emu.cpu, G_TURN_CTR, &turn, 4);

    if (trace_on() && !dumped) {
        dumped = 1;
        uint32_t bs = 0; uint8_t bf = 0;
        cpu_read(g_emu.cpu, 0x4000041Cu, &bs, 4);   /* -> current-battle struct (sub_C6C0) */
        if (bs >= 0x40000000u && bs < 0x40100000u) cpu_read(g_emu.cpu, bs, &bf, 1);
        printf("  >>> BATTLE list (turn=%u battleflag@%08x=%d voice=%s):\n",
               turn, bs, bf, g_emu.last_voice_key);
        uint32_t r = head;
        for (int i = 0; i < 8 && r >= 0x40000000u && r < 0x40100000u; i++) {
            uint8_t c = 0, kan = 0, mat = 0, seg = 0, bf2 = 0; uint32_t sp = 0, t12 = 0;
            cpu_read(g_emu.cpu, r + 2, &c, 1);   cpu_read(g_emu.cpu, r + 8, &sp, 4);
            cpu_read(g_emu.cpu, r + 12, &t12, 4);
            cpu_read(g_emu.cpu, r + 0x25, &kan, 1); cpu_read(g_emu.cpu, r + 0x26, &mat, 1);
            cpu_read(g_emu.cpu, r + 0x23, &seg, 1); cpu_read(g_emu.cpu, r + 0x28, &bf2, 1);
            printf("      rec[%d]@%08x color=%d field=%08x active=%d kan=%d mat=%d seg=%d bflag=%d\n",
                   i, r, c, sp, (int)(t12 == turn), kan, mat, seg, bf2);
            cpu_read(g_emu.cpu, r + 88, &r, 4);
        }
    }

    /* The active player's board field + colour (the resolvable "Du" combatant). */
    uint32_t active_field = 0; uint8_t active_color = 0;
    uint32_t rec = head;
    for (int i = 0; i < 8 && rec >= 0x40000000u && rec < 0x40100000u; i++) {
        uint32_t t12 = 0; cpu_read(g_emu.cpu, rec + 12, &t12, 4);
        if (t12 == turn) {
            cpu_read(g_emu.cpu, rec + 2, &active_color, 1);
            cpu_read(g_emu.cpu, rec + 8, &active_field, 4);
            break;
        }
        cpu_read(g_emu.cpu, rec + 88, &rec, 4);
    }
    if (!active_color || !active_field) return;

    /* Primary: the attacker captured at sub_C6C0 (boot.c). The firmware switches the
     * turn to the DEFENDER for the maneuver prompt, so "Du" (active_color above) is the
     * defender and the enemy must be the ATTACKER. Robust because the firmware matches
     * combatants by PORT BYTE, not by rec+8 pointer — so the old pointer scan failed. */
    uint32_t a = g_emu.pvp_attacker_rec;
    if (g_emu.pvp_pending && a >= 0x40000000u && a < 0x40100000u) {
        uint8_t ac = 0, v[3];
        cpu_read(g_emu.cpu, a + 2, &ac, 1);
        cpu_read(g_emu.cpu, a + 0x23, &v[0], 1);   /* sails total   */
        cpu_read(g_emu.cpu, a + 0x25, &v[1], 1);   /* cannons total */
        cpu_read(g_emu.cpu, a + 0x26, &v[2], 1);   /* sailors total */
        if (ac >= 1 && ac <= 4 && ac != active_color) {
            g_emu.pvp_battle     = true;
            g_emu.enemy_color    = ac;
            g_emu.pirate_sails   = v[0];
            g_emu.pirate_cannons = v[1];
            g_emu.pirate_sailors = v[2];
            int sig = ac * 1000 + v[1] * 100 + v[2] * 10 + v[0];
            if (trace_on() && sig != last_log) {
                last_log = sig;
                printf("  >>> PVP enemy(attacker) color=%d cannons=%d sailors=%d sails=%d "
                       "| Du(defender) color=%d @vms=%u\n",
                       ac, v[1], v[2], v[0], active_color, g_emu.virtual_ms);
            }
            return;
        }
    }

    /* Fallback (best effort): firmware-style co-location by PORT BYTE (byte0 at the
     * ship-pos pointer), NOT rec+8 pointer equality. */
    uint8_t active_port = 0xFF;
    if (active_field >= 0x40000000u && active_field < 0x40100000u)
        cpu_read(g_emu.cpu, active_field, &active_port, 1);
    rec = head;
    for (int i = 0; i < 8 && active_port != 0xFF && rec >= 0x40000000u && rec < 0x40100000u; i++) {
        uint8_t c = 0, spb = 0xFF; uint32_t sp = 0;
        cpu_read(g_emu.cpu, rec + 2, &c, 1);
        cpu_read(g_emu.cpu, rec + 8, &sp, 4);
        if (sp >= 0x40000000u && sp < 0x40100000u) cpu_read(g_emu.cpu, sp, &spb, 1);
        if (c >= 1 && c <= 4 && c != active_color && spb == active_port) {
            uint8_t v[3];
            cpu_read(g_emu.cpu, rec + 0x23, &v[0], 1);
            cpu_read(g_emu.cpu, rec + 0x25, &v[1], 1);
            cpu_read(g_emu.cpu, rec + 0x26, &v[2], 1);
            g_emu.pvp_battle     = true;
            g_emu.enemy_color    = c;
            g_emu.pirate_sails   = v[0];
            g_emu.pirate_cannons = v[1];
            g_emu.pirate_sailors = v[2];
            return;
        }
        cpu_read(g_emu.cpu, rec + 88, &rec, 4);
    }
    /* No PvP opponent resolvable -> NPC pirate battle; leave pirate_* (sub_E970). */
}

/* Map markers for the web UI: each player's current port, pirate-occupied ports, and
 * silverfleet rumor ports. All port numbers here are the 0-based display index 0..7
 * (consts.ts PORTS order). A player's port is the byte at its ship-position pointer
 * (rec+8 -> &port_table[idx]); pirate occupation is the global slot table at
 * 0x400002F0 (gate +1==1, port +2); silver rumors are placed by boot.c's
 * on_silver_ports and a bit is dropped here once a player reaches that port. */
#define G_PORT_TABLE   0x40000168u   /* 8 x 16 bytes; byte0 == display index 0..7 */
#define G_PORT_END     0x400001E8u   /* G_PORT_TABLE + 8*16 */
#define G_PIRATE_SLOTS 0x400002F0u   /* 5 x 12 bytes; +1 active gate, +2 port-identity */

static int shippos_to_port(uint32_t sp)
{
    if (sp < G_PORT_TABLE || sp >= G_PORT_END) return -1;   /* in transit / off-board */
    uint8_t b = 0; cpu_read(g_emu.cpu, sp, &b, 1);
    return (b <= 7) ? (int)b : -1;
}

void map_refresh_markers(void)
{
    /* player_ports[]: walk the player list, derive each colour's current port. */
    for (int i = 0; i < 4; i++) g_emu.player_ports[i] = -1;
    uint32_t head = 0; cpu_read(g_emu.cpu, G_PLAYER_HEAD, &head, 4);
    uint32_t rec = head;
    for (int i = 0; i < 8 && rec >= 0x40000000u && rec < 0x40100000u; i++) {
        uint8_t c = 0; cpu_read(g_emu.cpu, rec + 2, &c, 1);
        if (c >= 1 && c <= 4) {
            uint32_t sp = 0; cpu_read(g_emu.cpu, rec + 8, &sp, 4);
            g_emu.player_ports[c - 1] = (int8_t)shippos_to_port(sp);
        }
        cpu_read(g_emu.cpu, rec + 88, &rec, 4);
    }

    /* pirate_ports: scan the global occupied-ports slot table (firmware collector
     * mirror at 0x1b4c8). Authoritative each frame -> stale bits clear automatically. */
    uint8_t pset = 0;
    for (int s = 0; s < 5; s++) {
        uint32_t base = G_PIRATE_SLOTS + 12u * (uint32_t)s;
        uint8_t active = 0, loc = 0;
        cpu_read(g_emu.cpu, base + 1, &active, 1);   /* active gate */
        cpu_read(g_emu.cpu, base + 2, &loc, 1);      /* port identity == display index */
        if (active == 1 && loc <= 7) pset |= (uint8_t)(1u << loc);
    }
    g_emu.pirate_ports = pset;

    /* silver "reached -> remove": drop a rumor bit once any player is at that port. */
    if (g_emu.silver_ports)
        for (int i = 0; i < 4; i++) {
            int pp = g_emu.player_ports[i];
            if (pp >= 0 && pp <= 7) g_emu.silver_ports &= (uint8_t)~(1u << pp);
        }
}

/* Public: refresh the active player's money/equipment into g_emu (for the HUD).
 * Called every frame so the panel + battle overlay are never stale (the RFID-
 * inventory path only runs during cargo selection). Read-only — no game state. */
void rfid_refresh_stats(void)
{
    if (g_emu.in_game) { active_player_color(); resolve_pvp_opponent(); }
    map_refresh_markers();
}

static void put_entry(uint8_t *tbl, int idx, uint8_t field_key)
{
    tbl[idx * 16 + 0]  = field_key;   /* byte0 = field key (matched by sub_1F184) */
    tbl[idx * 16 + 1]  = 0xFF;        /* byte1 = terminator */
    tbl[idx * 16 + 5]  = 0xFF;
    tbl[idx * 16 + 12] = 0;           /* match counter (kept <= 0x0E) */
    tbl[idx * 16 + 13] = 0;
}

static void rfid_sync_table(void)
{
    uint8_t tbl[8 * 16], prev[8 * 16];
    cpu_read(g_emu.cpu, TABLE_ADDR, prev, sizeof prev);   /* for counter carry-over */
    memset(tbl, 0xFF, sizeof tbl);     /* 0xFF = empty entry */

    if (!g_emu.in_game) {
        /* Registration: seed each present figure at index colour-1, so sub_1F250
         * (returns table-index+1) reports the figure's true colour even when lower
         * colours are absent. Fields 17..14 -> colours 1..4 = 18-f. A running
         * counter would collapse colours when a lower one is skipped (the 2nd
         * present figure always landing at index 1 = green). */
        for (int f = 17; f >= 14; f--) {
            if (!g_emu.figures[f].present) continue;
            int color = 18 - f;                       /* 17->1 16->2 15->3 14->4 */
            put_entry(tbl, color - 1, (uint8_t)f);     /* index colour-1; key 0x0e..0x11 */
        }
    } else {
        /* Cargo turn — single-figure model: the RFID reader only sees a figure
         * that is PLACED on an action field, never the idle ships. So present
         * ONLY the active player's figure, and ONLY once it has been placed on a
         * cargo field (pick cargo) or a port field (set destination). The cargo
         * fields ARE the colour home fields (0x11/0x10/0x0f/0x0e), so parking
         * idle figures there would read as "cargo loaded" and trip wrong-player
         * errors — hence nothing is presented until the user selects something.
         * The entry sits at index colour-1 so sub_1F250 returns the colour. */
        int color = active_player_color();   /* 0 if not resolvable this call */
        /* Reset the cargo/destination selection when the active player CHANGES
         * (a new player's turn) so player N never inherits player N-1's figure
         * placement. Only act on a *resolved* colour, so a transient "unknown"
         * between turns never wipes a live selection mid-turn. */
        static int last_turn_color = 0;
        if (color >= 1 && color <= 4) {
            if (last_turn_color != 0 && last_turn_color != color) {
                g_emu.sel_cargo = 0;
                g_emu.sel_dest  = -1;
            }
            last_turn_color = color;
        }
        int active_idx = (g_emu.active_color >= 1 && g_emu.active_color <= 4) ? g_emu.active_color - 1 : 0;
        int field = -1;
        if (g_emu.sel_dest >= 0 && g_emu.sel_dest < 8)
            field = PORT_FIELD[g_emu.sel_dest];          /* figure on a port field */
        else if (g_emu.sel_cargo >= 1 && g_emu.sel_cargo <= 4)
            field = CARGO_FIELD[g_emu.sel_cargo - 1];    /* figure on a cargo field */
        if (field >= 0) {
            put_entry(tbl, active_idx, (uint8_t)field);
            /* Carry the match counter while the figure stays on the same field so
             * sub_1F250 can accumulate it (the figure-lift / good-taken signal). */
            if (prev[active_idx * 16] == (uint8_t)field && prev[active_idx * 16 + 12] <= 0x0E)
                tbl[active_idx * 16 + 12] = prev[active_idx * 16 + 12];
        }
        if (trace_on()) {
            static int last_cargo = -99, last_dest = -99;
            if (g_emu.sel_cargo != last_cargo || g_emu.sel_dest != last_dest) {
                last_cargo = g_emu.sel_cargo; last_dest = g_emu.sel_dest;
                printf("  RFID turn: color=%d idx=%d cargo=%d dest=%d field=0x%02x  "
                       "offered=%d,%d  EQ kan=%d mat=%d seg=%d lad=%d duk=%d @vms=%u\n",
                       color, active_idx, g_emu.sel_cargo, g_emu.sel_dest, field & 0xFF,
                       g_emu.offered[0], g_emu.offered[1],
                       g_emu.eq_cannons, g_emu.eq_sailors, g_emu.eq_sails, g_emu.eq_cargo,
                       g_emu.money, g_emu.virtual_ms);
            }
        }
    }
    cpu_write(g_emu.cpu, TABLE_ADDR, tbl, sizeof tbl);
    uint32_t ptr = TABLE_ADDR;
    cpu_write(g_emu.cpu, G_TABLE_PTR, &ptr, 4);   /* dword_400004F4 = &table */
}

void sys_rfid_inventory(void)     /* 0x1074: r0 = out buf for UID(s) -> count */
{
    (void)g_sel_field;
    /* No LIVE tags: keeps sub_1E230 -> 0 so sub_1F250 takes the v25==0 path,
     * which matches our seeded dword_400004F4 table -> registration. */
    rfid_sync_table();
    ret(0);
}

void sys_rfid_read_block(void)    /* 0x1078: r0=out, r1=tag, r2=block, r3=count */
{
    if (trace_on())
        printf("  RFID 0x1078 read_block r0=%08x r1=%08x r2=%08x r3=%08x\n",
               arg(0), arg(1), arg(2), arg(3));
    ret((uint32_t)-1);
}

void sys_rfid_write_block(void)   /* 0x107C: r0=tag, r1=data, r2=block, r3=bsize */
{
    if (trace_on())
        printf("  RFID 0x107C write_block r0=%08x r1=%08x r2=%08x r3=%08x\n",
               arg(0), arg(1), arg(2), arg(3));
    ret(0);
}

void sys_rfid_release(void)       /* 0x1080: deselect */
{
    if (trace_on()) printf("  RFID 0x1080 release\n");
    ret(0);
}

void sys_rfid_read_cached(void)   /* 0x1088: r0=field -> selects the antenna */
{
    g_sel_field = (int)arg(0);     /* 0x1074 will inventory this field */
    ret(0);
}
