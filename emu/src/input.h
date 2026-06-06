/* input.h — raylib-free input model. Owns the button->mask bit table and the
 * momentary-pulse / port-presence state extracted from ui_poll, so the desktop
 * UI (ui.c) and the WebSocket server (server.c) share identical input semantics
 * and the bit-mapping table lives in exactly one place.
 *
 * Usage each tick: record intents (input_button_press / input_set_*), then call
 * input_build_mask() once to compute g_emu.input_mask before sched_run(). */
#ifndef YVIO_INPUT_H
#define YVIO_INPUT_H

#include <stdint.h>
#include <stdbool.h>

/* Logical buttons; index into the internal BTN_BITS table (moved from ui.c). */
typedef enum {
    BTN_OK = 0, BTN_NO, BTN_SKIP, BTN_REPEAT,
    BTN_KAUFEN, BTN_FEILSCHEN,            /* in-game merchant */
    BTN_KANONEN, BTN_ENTERN, BTN_SEGELN,  /* sea-battle maneuvers */
    BTN_COUNT
} input_btn_t;

void input_reset(void);                       /* zero all pulse timers + presence */
void input_button_press(input_btn_t b);       /* arm a 6-tick momentary pulse */
void input_port_pulse(int port);              /* 0..7 momentary port press (menu nav) */
void input_port_set(int port, bool present);  /* 0..7 level-held port presence (setup) */
bool input_port_get(int port);                /* read level-held port presence (rendering) */
void input_set_register(int color, bool present); /* color 1..4 -> g_emu.figures via REG_FIELD */
void input_set_cargo(int v);                  /* 0..4 -> g_emu.sel_cargo (+ sel_dest=-1 on nonzero) */
void input_set_dest(int v);                   /* -1..7 -> g_emu.sel_dest */
void input_set_fast(bool on);                 /* g_emu.fast */

/* Build g_emu.input_mask from level-held presence + decrement all pulse timers
 * by one tick. Call exactly once per emulator step. */
void input_build_mask(void);

/* Map a WS button name ("OK","NO",...) to a button enum. Returns false if unknown. */
bool input_btn_from_name(const char *name, input_btn_t *out);

#endif /* YVIO_INPUT_H */
