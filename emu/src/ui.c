/* ui.c — raylib board for the yvio HLE emulator.
 *
 * Layout (responsive, logical coords so the mouse aligns under Wayland scaling):
 *   - title
 *   - 4 color JOIN slots  -> RFID figures on fields 17/16/15/14 (blue/green/red/yellow)
 *   - 8 ports             -> input-mask bits 0..7 (menu nav + gameplay)
 *   - 4 console buttons    -> OK / NO / SKIP / REPEAT
 *   - live voice-key HUD + status + a software cursor dot
 *
 * The game reads the 0x1058 mask with TWO conventions (sub_1FB28(mask) only ever
 * inspects bits inside the caller's mask, so asserting extra bits is harmless):
 *   - intro/setup flow (sub_10E68): bit 5=NO, bit 6=YES/OK, bit 7=SKIP, bit 8=repeat
 *   - general menu (sub_221F0):      bit 12=select, bit 13=forward, bit 14=back
 * Buttons -> bits:  OK -> 6   NO -> 5|14   SKIP -> 7|13   REPEAT -> 8
 *   OK is bit 6 ONLY: bit 12 is the game's "restart step" gesture (sub_10D08),
 *   which wipes bit 6's confirm edge, so it is kept off OK. Menu SELECT (bit 12)
 *   is deferred until in-game menus are reached. Bits 13/14 are read only by the
 *   menu loop (harmless elsewhere), so they stay on SKIP/NO.
 *
 * Setup: SKIP = skip intro; OK = beginner game, NO = reject -> normal/profi;
 * click a JOIN slot to register a player; OK = start. */
#include "emu.h"
#include "raylib.h"
#include <stdio.h>

static const char *PORTS[8] = {
    "Camarco", "Spring Point", "Puerto Plata", "San Juan",
    "Kingstown", "Curacao", "Cartagena", "Prinzapolca"
};
static const char *COLNAME[4] = { "BLUE", "GREEN", "RED", "YELLOW" };
static const Color COLVAL[4]  = { {60,120,230,255}, {50,180,90,255}, {220,70,70,255}, {230,200,60,255} };
static const int   REG_FIELD[4] = { 17, 16, 15, 14 };   /* blue,green,red,yellow */

/* In gameplay the 4 top slots become the cargo types (sel_cargo 1..4), matching
 * the cargo fields 0x11/0x10/0x0f/0x0e the game scans (rfid.c CARGO_FIELD). */
static const char *CARGONAME[4] = { "Getreide", "Holz", "Tabak", "Rum" };
static const Color CARGOVAL[4]  = { {220,190,90,255}, {150,110,60,255}, {120,150,70,255}, {170,70,60,255} };

static bool g_present[12];   /* input-mask figure presence (ports) */
static int  g_pulse[12];     /* keyboard momentary-tap timers (ports) */
static int  g_btn_pulse[9];  /* OK/NO/SKIP/REPEAT/KAUFEN/FEILSCHEN + KANONEN/ENTERN/SEGELN */

static float SW(void) { return (float)GetScreenWidth(); }
static float SH(void) { return (float)GetScreenHeight(); }
static int   fs(float frac) { int s = (int)(SH() * frac); return s < 8 ? 8 : s; }

static Rectangle reg_rect(int i)
{
    float W = SW(), H = SH(), mx = W * 0.03f, w = (W - 5 * mx) / 4.0f;
    return (Rectangle){ mx + i * (w + mx), H * 0.105f, w, H * 0.10f };
}
static Rectangle port_rect(int i)
{
    float W = SW(), H = SH(), mx = W * 0.03f, top = H * 0.26f, bottom = H * 0.58f;
    float pw = (W - 5 * mx) / 4.0f, gap = (bottom - top) * 0.10f;
    float ph = ((bottom - top) - gap) / 2.0f;
    int col = i % 4, row = i / 4;
    return (Rectangle){ mx + col * (pw + mx), top + row * (ph + gap), pw, ph };
}
/* Setup: OK/NO/SKIP/REPEAT. Gameplay adds KAUFEN (buy) + FEILSCHEN (haggle). */
static int n_btn(void) { return g_emu.in_game ? 6 : 4; }
static Rectangle btn_rect(int i)   /* 0=OK 1=NO 2=SKIP 3=REPEAT 4=KAUFEN 5=FEILSCHEN */
{
    int n = n_btn();
    float W = SW(), H = SH(), mx = W * 0.03f, w = (W - (n + 1) * mx) / (float)n;
    return (Rectangle){ mx + i * (w + mx), H * 0.62f, w, H * 0.09f };
}
/* Sea-battle maneuver buttons (3, centered low in the battle overlay). */
static Rectangle bat_rect(int i)
{
    float W = SW(), H = SH(), w = W * 0.26f, gap = W * 0.03f;
    float total = 3 * w + 2 * gap, x0 = (W - total) / 2.0f;
    return (Rectangle){ x0 + i * (w + gap), H * 0.70f, w, H * 0.10f };
}

void ui_init(void)
{
    SetTraceLogLevel(LOG_WARNING);
    /* No HIGHDPI: layout + mouse share one logical space (Wayland #4908/#4931). */
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(980, 720, "yvio - Freibeuter der Karibik (emulator)");
    SetWindowMinSize(620, 480);
    SetTargetFPS(60);
}

bool ui_should_close(void) { return WindowShouldClose(); }
void ui_shutdown(void) { CloseWindow(); }

uint32_t ui_frame_ms(void)
{
    float dt = GetFrameTime() * 1000.0f;
    if (dt < 1.0f) dt = 1.0f;
    if (dt > 50.0f) dt = 50.0f;
    return (uint32_t)dt;
}

void ui_poll(void)
{
    Vector2 m = GetMousePosition();
    bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    /* While the sea-battle overlay is up it owns ALL input: the board's cargo/port/
     * console buttons are disabled (and their hover outline suppressed in ui_draw)
     * so only the KANONEN/ENTERN/SEGELN maneuver buttons respond. */
    bool board_live = !g_emu.in_battle;

    /* Top 4 slots: registration = JOIN colors; gameplay = cargo selectors.
     * In gameplay, clicking a cargo moves the (single) ship figure onto that
     * cargo field (rfid.c presents it); clicking it again removes the figure. */
    if (board_live) for (int i = 0; i < 4; i++) {
        bool hit = (click && CheckCollisionPointRec(m, reg_rect(i))) || IsKeyPressed(KEY_F1 + i);
        if (!hit) continue;
        if (g_emu.in_game) {
            g_emu.sel_cargo = (g_emu.sel_cargo == i + 1) ? 0 : (i + 1);
            g_emu.sel_dest  = -1;        /* moving to a cargo field leaves any port */
        } else {
            int f = REG_FIELD[i];
            g_emu.figures[f].present = !g_emu.figures[f].present;
            g_emu.figures[f].color = (uint8_t)(i + 1);
        }
    }

    /* Ports: registration/menu = input-mask presence; gameplay = destination
     * (move the ship figure onto a port field -> rfid.c confirms the target). */
    if (board_live) for (int i = 0; i < 8; i++) {
        bool hit = (click && CheckCollisionPointRec(m, port_rect(i))) || IsKeyPressed(KEY_ONE + i);
        if (g_emu.in_game) {
            if (hit) g_emu.sel_dest = (g_emu.sel_dest == i) ? -1 : i;
        } else {
            if (click && CheckCollisionPointRec(m, port_rect(i))) g_present[i] = !g_present[i];
            if (IsKeyPressed(KEY_ONE + i)) g_pulse[i] = 4;
        }
    }

    uint32_t mask = 0;
    for (int i = 0; i < 12; i++) {
        if (g_present[i]) mask |= (1u << i);
        if (g_pulse[i] > 0) { mask |= (1u << i); g_pulse[i]--; }
    }
    /* Console buttons: one-shot pulses (press-edge -> fixed window -> release),
     * mirroring the reliable port g_pulse model, so each click is exactly one
     * clean rising edge with a guaranteed low gap (no double-click).
     *   OK = bit 6 ONLY. Bit 12 is the game's "restart this step" gesture
     *   (sub_10D08) which disables/re-enables input bits 0-11 and would wipe
     *   bit 6's confirm edge in the same press -> it must NOT be on OK. Menu
     *   SELECT (bit 12) is deferred until in-game menus are reached/tested. */
    if (board_live) {
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || (click && CheckCollisionPointRec(m, btn_rect(0)))) g_btn_pulse[0] = 6;
        if (IsKeyPressed(KEY_N)                                || (click && CheckCollisionPointRec(m, btn_rect(1)))) g_btn_pulse[1] = 6;
        if (IsKeyPressed(KEY_S)                                || (click && CheckCollisionPointRec(m, btn_rect(2)))) g_btn_pulse[2] = 6;
        if (IsKeyPressed(KEY_R)                                || (click && CheckCollisionPointRec(m, btn_rect(3)))) g_btn_pulse[3] = 6;
        if (g_emu.in_game) {   /* merchant trading: read by sub_13868 (bits 0/1/5) at a port */
            if (IsKeyPressed(KEY_K) || (click && CheckCollisionPointRec(m, btn_rect(4)))) g_btn_pulse[4] = 6;
            if (IsKeyPressed(KEY_F) || (click && CheckCollisionPointRec(m, btn_rect(5)))) g_btn_pulse[5] = 6;
        }
    }
    if (g_emu.in_battle) {   /* sea-battle maneuver: read by sub_13858/60 (bits 9/10/11) */
        if (IsKeyPressed(KEY_C) || (click && CheckCollisionPointRec(m, bat_rect(0)))) g_btn_pulse[6] = 6;
        if (IsKeyPressed(KEY_E) || (click && CheckCollisionPointRec(m, bat_rect(1)))) g_btn_pulse[7] = 6;
        if (IsKeyPressed(KEY_G) || (click && CheckCollisionPointRec(m, bat_rect(2)))) g_btn_pulse[8] = 6;
    }
    static const uint32_t BTN_BITS[9] = {
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
    for (int i = 0; i < 9; i++) if (g_btn_pulse[i] > 0) { mask |= BTN_BITS[i]; g_btn_pulse[i]--; }

    g_emu.input_mask = mask;
    g_emu.fast = IsKeyDown(KEY_TAB);
}

/* Active player's equipment + dukaten — one line under the title (gameplay). */
static void draw_equip(int xs)
{
    int c = g_emu.active_color;
    Color col = (c >= 1 && c <= 4) ? COLVAL[c - 1] : (Color){ 200,200,200,255 };
    DrawText(TextFormat("Spieler %s   Kanonen %d   Matrosen %d   Segel %d   Laderaum %d   Dukaten %d",
                        (c >= 1 && c <= 4) ? COLNAME[c - 1] : "-",
                        g_emu.eq_cannons, g_emu.eq_sailors, g_emu.eq_sails, g_emu.eq_cargo, g_emu.money),
             xs, (int)(SH() * 0.072f), fs(0.022f), col);
}

/* Sea-battle overlay: dims the board and shows pirate vs us + 3 maneuver buttons. */
static void draw_battle(void)
{
    DrawRectangle(0, 0, (int)SW(), (int)SH(), (Color){ 0,0,0,190 });
    int xs = (int)(SW() * 0.06f), c = g_emu.active_color;
    DrawText("SEEGEFECHT", xs, (int)(SH() * 0.10f), fs(0.052f), (Color){ 240,180,90,255 });

    DrawText(TextFormat("Feindliches Piratenschiff (#%d)", g_emu.pirate_idx),
             xs, (int)(SH() * 0.21f), fs(0.034f), (Color){ 235,90,90,255 });
    DrawText(TextFormat("Kanonen %d    Matrosen %d    Segel %d",
                        g_emu.pirate_cannons, g_emu.pirate_sailors, g_emu.pirate_sails),
             xs, (int)(SH() * 0.28f), fs(0.030f), RAYWHITE);

    DrawText(TextFormat("Du (%s)", (c >= 1 && c <= 4) ? COLNAME[c - 1] : "-"),
             xs, (int)(SH() * 0.40f), fs(0.034f), (c >= 1 && c <= 4) ? COLVAL[c - 1] : (Color){ 200,200,200,255 });
    /* Use the equipment totals (same source as the panel) so this matches the
     * game's spoken/displayed values — 0 at start, not the stale rec[37]/[38]. */
    DrawText(TextFormat("Kanonen %d    Matrosen %d    Segel %d",
                        g_emu.eq_cannons, g_emu.eq_sailors, g_emu.eq_sails),
             xs, (int)(SH() * 0.47f), fs(0.030f), RAYWHITE);

    DrawText("Waehle ein Manoever:  KANONEN (C) feuern  |  ENTERN (E) entern  |  SEGELN (G) fliehen",
             xs, (int)(SH() * 0.60f), fs(0.024f), (Color){ 240,220,120,255 });
    const char *bl[3] = { "KANONEN (C)", "ENTERN (E)", "SEGELN (G)" };
    Vector2 mp = GetMousePosition();
    for (int i = 0; i < 3; i++) {
        Rectangle b = bat_rect(i);
        bool hov = CheckCollisionPointRec(mp, b);
        DrawRectangleRec(b, (Color){ 50,45,70,255 });
        DrawRectangleLinesEx(b, hov ? 4 : 2, hov ? (Color){ 255,220,120,255 } : (Color){ 140,140,200,255 });
        DrawText(bl[i], (int)b.x + 12, (int)(b.y + b.height * 0.32f), fs(0.026f), RAYWHITE);
    }
}

void ui_draw(void)
{
    int xs = (int)(SW() * 0.03f);
    BeginDrawing();
    ClearBackground((Color){ 18, 30, 48, 255 });
    DrawText("Freibeuter der Karibik", xs, (int)(SH() * 0.025f), fs(0.038f), RAYWHITE);
    if (g_emu.in_game) draw_equip(xs);   /* active player's equipment + dukaten */

    /* Top 4 slots: JOIN colors (registration) or cargo selectors (gameplay).
     * In gameplay the two goods the port offers are highlighted (loadable). */
    for (int i = 0; i < 4; i++) {
        Rectangle r = reg_rect(i);
        bool game = g_emu.in_game;
        bool on = game ? (g_emu.sel_cargo == i + 1) : g_emu.figures[REG_FIELD[i]].present;
        bool offer = game && (g_emu.offered[0] == i + 1 || g_emu.offered[1] == i + 1);
        Color c = game ? CARGOVAL[i] : COLVAL[i];
        DrawRectangleRec(r, on ? c : (Color){ c.r/4, c.g/4, c.b/4, 255 });
        DrawRectangleLinesEx(r, offer ? 4 : 2, offer ? (Color){ 255,235,140,255 } : RAYWHITE);
        DrawText(game ? CARGONAME[i] : TextFormat("JOIN %s", COLNAME[i]),
                 (int)r.x + 8, (int)r.y + 6, fs(0.024f), RAYWHITE);
        DrawText(game ? (offer ? (on ? "loaded" : "offered") : "-") : TextFormat("F%d", i + 1),
                 (int)r.x + 8, (int)(r.y + r.height - fs(0.020f) - 6), fs(0.018f),
                 offer ? (Color){ 255,235,140,255 } : (Color){ 150,160,180,255 });
        if (on) DrawCircle((int)(r.x + r.width - 18), (int)(r.y + r.height/2), 9, RAYWHITE);
    }

    /* Ports (gameplay: highlight the chosen destination) */
    for (int i = 0; i < 8; i++) {
        Rectangle r = port_rect(i);
        bool dest = g_emu.in_game && g_emu.sel_dest == i;
        DrawRectangleRec(r, dest ? (Color){ 200,150,40,255 }
                              : (g_present[i] ? (Color){ 40,90,60,255 } : (Color){ 30,44,66,255 }));
        DrawRectangleLinesEx(r, 2, dest ? (Color){ 255,220,120,255 } : (Color){ 90,120,160,255 });
        DrawText(PORTS[i], (int)r.x + 10, (int)r.y + 8, fs(0.026f), RAYWHITE);
        DrawText(g_emu.in_game ? "target port (sail here)" : TextFormat("port %d (key %d)", i, i + 1),
                 (int)r.x + 10, (int)(r.y + r.height - fs(0.022f) - 6), fs(0.020f), (Color){ 150,170,200,255 });
    }

    /* Console buttons (gameplay adds KAUFEN/FEILSCHEN for port merchants) */
    const char *blab[6] = { "OK / JA (Space)", "NO / NEIN (N)", "SKIP (S)", "Repeat (R)",
                            "KAUFEN (K)", "FEILSCHEN (F)" };
    int nb = n_btn();
    for (int i = 0; i < nb; i++) {
        Rectangle b = btn_rect(i);
        bool trade = i >= 4;   /* merchant buttons get a distinct tint */
        DrawRectangleRec(b, trade ? (Color){ 40,60,50,255 } : (Color){ 60,50,40,255 });
        DrawRectangleLinesEx(b, 2, trade ? (Color){ 90,160,120,255 } : (Color){ 150,120,80,255 });
        DrawText(blab[i], (int)b.x + 8, (int)(b.y + b.height*0.32f), fs(nb > 4 ? 0.018f : 0.022f), RAYWHITE);
    }

    /* HUD */
    DrawText(TextFormat("speaking: %s", g_emu.last_voice_key[0] ? g_emu.last_voice_key : "(silence)"),
             xs, (int)(SH() * 0.745f), fs(0.030f), (Color){ 240,220,120,255 });
    DrawText(TextFormat("vms=%u  audio=%.1fs  mask=0x%04x%s", g_emu.virtual_ms,
                        (double)g_emu.audio_bytes / 2.0 / 22050.0, g_emu.input_mask,
                        g_emu.fast ? "  [FAST]" : ""),
             xs, (int)(SH() * 0.80f), fs(0.024f), (Color){ 150,200,150,255 });
    if (g_emu.in_game)
        DrawText("Turn: load ONE of the highlighted cargos, then click a TARGET PORT to sail & sell.",
                 xs, (int)(SH() * 0.855f), fs(0.020f), (Color){ 240,220,120,255 });
    else
        DrawText("Setup: SKIP=skip intro | OK=beginner / NO=reject -> normal/profi | click JOIN slot | OK=start.",
                 xs, (int)(SH() * 0.855f), fs(0.020f), (Color){ 240,220,120,255 });
    DrawText(g_emu.in_game
             ? "Merchant at a port: KAUFEN(K)=buy  FEILSCHEN(F)=haggle  NO(N)=decline  R=repeat | F1-F4 cargo, 1-8 port."
             : "Keys: 1-8 ports | F1-F4 join (blue/green/red/yellow) | Space=OK  N=NO  S=SKIP  R=Repeat | TAB=fast-fwd.",
             xs, (int)(SH() * 0.905f), fs(0.020f), (Color){ 130,150,180,255 });

    /* Sea-battle overlay (drawn over the board when a fight is active). */
    if (g_emu.in_battle) draw_battle();

    /* Software cursor + hover highlight (Wayland-proof aiming). During a battle the
     * board is disabled, so suppress its hover outlines (the maneuver buttons draw
     * their own hover inside draw_battle); the cursor dot still draws. */
    Vector2 mp = GetMousePosition();
    if (!g_emu.in_battle) {
        for (int i = 0; i < 4; i++) if (CheckCollisionPointRec(mp, reg_rect(i)))  DrawRectangleLinesEx(reg_rect(i), 3, RAYWHITE);
        for (int i = 0; i < 8; i++) if (CheckCollisionPointRec(mp, port_rect(i))) DrawRectangleLinesEx(port_rect(i), 3, (Color){240,220,120,255});
        for (int i = 0; i < n_btn(); i++) if (CheckCollisionPointRec(mp, btn_rect(i)))  DrawRectangleLinesEx(btn_rect(i), 3, (Color){240,220,120,255});
    }
    DrawCircleV(mp, 6, (Color){ 255,80,80,255 });
    DrawCircleLines((int)mp.x, (int)mp.y, 6, RAYWHITE);
    EndDrawing();
}
