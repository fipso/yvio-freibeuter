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

/* Public: refresh the active player's money/equipment into g_emu (for the HUD).
 * Called every frame so the panel + battle overlay are never stale (the RFID-
 * inventory path only runs during cargo selection). Read-only — no game state. */
void rfid_refresh_stats(void)
{
    if (g_emu.in_game) active_player_color();
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
        /* Registration: one entry per present player figure (fields 14..17). */
        int n = 0;
        for (int f = 17; f >= 14 && n < 8; f--) {   /* fields 17..14 -> colors 1..4 */
            if (!g_emu.figures[f].present) continue;
            put_entry(tbl, n, (uint8_t)f);           /* key = field idx (0x0e..0x11) */
            n++;
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
