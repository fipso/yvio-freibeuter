/* input.c — raylib-free input model (extracted from ui_poll). See input.h.
 *
 * The game reads the 0x1058 mask with TWO conventions (sub_1FB28(mask) only ever
 * inspects bits inside the caller's mask, so asserting extra bits is harmless):
 *   - intro/setup flow (sub_10E68): bit 5=NO, bit 6=YES/OK, bit 7=SKIP, bit 8=repeat
 *   - general menu (sub_221F0):      bit 12=select, bit 13=forward, bit 14=back
 * Buttons -> bits:  OK -> 6   NO -> 5|14   SKIP -> 7|13   REPEAT -> 8
 *   OK is bit 6 ONLY: bit 12 is the game's "restart step" gesture (sub_10D08),
 *   which wipes bit 6's confirm edge, so it is kept off OK. */
#include "emu.h"
#include "input.h"

/* Registration JOIN colors -> RFID figure fields (blue/green/red/yellow). */
static const int REG_FIELD[4] = { 17, 16, 15, 14 };

static const uint32_t BTN_BITS[BTN_COUNT] = {
    (1u << 6),               /* OK        = YES / confirm (universal: setup, join, start) */
    (1u << 5) | (1u << 14),  /* NO        = reject (5) + menu BACK (14) + merchant DECLINE (5) */
    (1u << 7) | (1u << 13),  /* SKIP      = skip intro (7) + menu FORWARD (13) */
    (1u << 8),               /* REPEAT    = re-ask the current spoken prompt (8) */
    (1u << 0),               /* KAUFEN    = accept/buy the merchant's price (bit 0) */
    (1u << 1),               /* FEILSCHEN = haggle the merchant's price (bit 1) */
    (1u << 9),               /* KANONEN   = battle: attack with cannons (bit 9) */
    (1u << 10),              /* ENTERN    = battle: board with sailors (bit 10) */
    (1u << 11),              /* SEGELN    = battle: sail away/flee (bit 11; quest/PvP only) */
};

static bool s_present[12];            /* input-mask figure presence (ports, menu nav) */
static int  s_port_pulse[12];         /* momentary port-tap timers */
static int  s_btn_pulse[BTN_COUNT];   /* momentary console/maneuver button timers */

void input_reset(void)
{
    for (int i = 0; i < 12; i++) { s_present[i] = false; s_port_pulse[i] = 0; }
    for (int i = 0; i < BTN_COUNT; i++) s_btn_pulse[i] = 0;
}

void input_button_press(input_btn_t b)
{
    if (b >= 0 && b < BTN_COUNT) s_btn_pulse[b] = 6;
}

void input_port_pulse(int port)
{
    if (port >= 0 && port < 12) s_port_pulse[port] = 4;
}

void input_port_set(int port, bool present)
{
    if (port >= 0 && port < 12) s_present[port] = present;
}

bool input_port_get(int port)
{
    return (port >= 0 && port < 12) ? s_present[port] : false;
}

void input_set_register(int color, bool present)
{
    if (color < 1 || color > 4) return;
    int f = REG_FIELD[color - 1];
    g_emu.figures[f].present = present;
    g_emu.figures[f].color = (uint8_t)color;
}

void input_set_cargo(int v)
{
    if (v < 0 || v > 4) return;
    g_emu.sel_cargo = v;
    if (v != 0) g_emu.sel_dest = -1;   /* moving to a cargo field leaves any port */
}

void input_set_dest(int v)
{
    if (v < -1 || v > 7) return;
    g_emu.sel_dest = v;
}

void input_set_fast(bool on) { g_emu.fast = on; }

void input_build_mask(void)
{
    uint32_t mask = 0;
    for (int i = 0; i < 12; i++) {
        if (s_present[i]) mask |= (1u << i);
        if (s_port_pulse[i] > 0) { mask |= (1u << i); s_port_pulse[i]--; }
    }
    for (int i = 0; i < BTN_COUNT; i++)
        if (s_btn_pulse[i] > 0) { mask |= BTN_BITS[i]; s_btn_pulse[i]--; }
    g_emu.input_mask = mask;
}

bool input_btn_from_name(const char *name, input_btn_t *out)
{
    static const struct { const char *n; input_btn_t b; } MAP[] = {
        { "OK", BTN_OK }, { "NO", BTN_NO }, { "SKIP", BTN_SKIP }, { "REPEAT", BTN_REPEAT },
        { "KAUFEN", BTN_KAUFEN }, { "FEILSCHEN", BTN_FEILSCHEN },
        { "KANONEN", BTN_KANONEN }, { "ENTERN", BTN_ENTERN }, { "SEGELN", BTN_SEGELN },
    };
    for (size_t i = 0; i < sizeof MAP / sizeof MAP[0]; i++) {
        const char *a = name, *b = MAP[i].n;
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a == 0 && *b == 0) { *out = MAP[i].b; return true; }
    }
    return false;
}
