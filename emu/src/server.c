/* server.c — WebSocket server + lobby manager (parent) and the forked per-lobby
 * emulator loop (child). See server.h and ../docs for the design.
 *
 * Two halves live in this one translation unit:
 *   - PARENT: mongoose event loop, lobby registry, fork/reap/sweep, message
 *     routing. No Unicorn, no g_emu use.
 *   - CHILD:  a single-threaded cooperative loop that drives the emulator and
 *     bridges input/state/audio over its socketpair. Created by fork() inside
 *     the parent's create-lobby handler (so Unicorn is created post-fork).
 *
 * Wire protocol parent<->child (length-prefixed frames):
 *     uint8 type | uint32 length (host LE) | payload[length]
 * Types: FR_INPUT_JSON (parent->child), FR_STATE_JSON / FR_AUDIO_PCM (child->parent).
 *
 * WebSocket protocol client<->parent: JSON text control + gameplay messages,
 * binary frames for raw int16 mono 22050 Hz PCM audio. See README/plan. */
#include "emu.h"
#include "input.h"
#include "boot.h"
#include "server.h"
#include "vendor/mongoose.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* ---- framed protocol ---- */

enum {
    FR_INPUT_JSON = 1,   /* parent -> child: a gameplay/input WS message (verbatim JSON) */
    FR_STATE_JSON = 2,   /* child -> parent: periodic state snapshot JSON */
    FR_AUDIO_PCM  = 3,   /* child -> parent: N * 256-byte raw PCM blocks */
};
#define FRAME_HDR    5
#define MAX_FRAME    (2u * 1024 * 1024)

typedef void (*frame_cb)(uint8_t type, const uint8_t *payload, uint32_t len, void *user);

/* Parse complete frames out of buf; returns bytes consumed (whole frames only). */
static size_t parse_frames(const uint8_t *buf, size_t len, frame_cb cb, void *user)
{
    size_t off = 0;
    for (;;) {
        if (len - off < FRAME_HDR) break;
        uint8_t type = buf[off];
        uint32_t plen;
        memcpy(&plen, buf + off + 1, 4);
        if (plen > MAX_FRAME) break;                 /* corrupt; stop (trusted channel) */
        if (len - off < FRAME_HDR + (size_t)plen) break;
        cb(type, buf + off + FRAME_HDR, plen, user);
        off += FRAME_HDR + plen;
    }
    return off;
}

static void set_nonblocking(int fd)
{
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl >= 0) fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

/* ===================================================================== */
/* ============================  CHILD  ================================ */
/* ===================================================================== */

/* Outgoing (child->parent) buffer: framed bytes, flushed nonblocking. */
static uint8_t *c_out;
static size_t   c_out_len, c_out_cap, c_out_off;
#define AUDIO_BACKPRESSURE (256u * 1024)   /* hold the mixer when this much is pending */

/* Coalesced audio for one loop iteration (128 samples = 256 bytes per block). */
#define AUDIO_MAX_BLOCKS 96
static int16_t c_audio[128 * AUDIO_MAX_BLOCKS];
static int     c_audio_blocks;

/* Realtime audio pacing. The raylib sink is throttled by a realtime consumer
 * thread draining its ring; we have no such consumer, so without pacing
 * audio_pump would drain the guest mixer's ring unbounded (the mixer never
 * blocks -> produces audio as fast as the CPU can mix, and starves virtual
 * time). We model a playhead that advances with wall-clock at 22050 Hz and
 * refuse PCM once we are AUDIO_FILL_SAMPLES ahead of it — which both bounds the
 * stream to real time and lets the mixer block on its free semaphore (yielding
 * so the cooperative scheduler advances virtual time). */
#define AUDIO_FILL_SAMPLES 11025u    /* ~0.5 s lookahead @ 22050 Hz */
static uint64_t a_produced;          /* PCM samples accepted so far */
static double   a_play;              /* estimated samples consumed (playhead) */
static uint64_t a_last_ms;

static int  c_eof;                 /* parent closed the socketpair -> exit */
static uint8_t c_in[65536];        /* input accumulation (FR_INPUT_JSON only, tiny) */
static size_t  c_in_len;

static void child_enqueue(uint8_t type, const void *payload, uint32_t len)
{
    size_t need = c_out_len + FRAME_HDR + len;
    if (need > c_out_cap) {
        size_t nc = c_out_cap ? c_out_cap : 32768;
        while (nc < need) nc *= 2;
        uint8_t *nb = realloc(c_out, nc);
        if (!nb) return;            /* drop frame on OOM */
        c_out = nb; c_out_cap = nc;
    }
    c_out[c_out_len] = type;
    memcpy(c_out + c_out_len + 1, &len, 4);
    memcpy(c_out + c_out_len + FRAME_HDR, payload, len);
    c_out_len += FRAME_HDR + len;
}

static void child_flush_out(int fd)
{
    while (c_out_off < c_out_len) {
        ssize_t w = write(fd, c_out + c_out_off, c_out_len - c_out_off);
        if (w > 0) { c_out_off += (size_t)w; continue; }
        if (w < 0 && errno == EINTR) continue;
        break;                      /* EWOULDBLOCK / error: try again next iteration */
    }
    if (c_out_off == c_out_len) { c_out_off = c_out_len = 0; }
    else if (c_out_off > 0) {
        memmove(c_out, c_out + c_out_off, c_out_len - c_out_off);
        c_out_len -= c_out_off; c_out_off = 0;
    }
}

/* Returns 1 if the coalesced audio was queued (or empty), 0 on backpressure. */
static int child_flush_audio(void)
{
    if (c_audio_blocks == 0) return 1;
    if (c_out_len - c_out_off > AUDIO_BACKPRESSURE) return 0;
    child_enqueue(FR_AUDIO_PCM, c_audio, (uint32_t)(c_audio_blocks * 256));
    c_audio_blocks = 0;
    return 1;
}

/* Audio sink: coalesce 256-byte blocks; throttle the guest mixer on backpressure
 * (returning 0 makes audio_pump stop draining the guest ring — audio.c:59). */
static int child_audio_sink(const int16_t *pcm128, void *user)
{
    (void)user;
    /* Advance the playhead by real elapsed time, clamped to what we've produced
     * (can't play unproduced audio; a long silence just lets it catch up). */
    uint64_t now = now_ms();
    if (a_last_ms == 0) a_last_ms = now;
    a_play += (double)(now - a_last_ms) * 22.05;   /* 22050 samples / 1000 ms */
    a_last_ms = now;
    if (a_play > (double)a_produced) a_play = (double)a_produced;
    if ((double)a_produced - a_play >= (double)AUDIO_FILL_SAMPLES) return 0; /* realtime backpressure */
    if (c_out_len - c_out_off > AUDIO_BACKPRESSURE) return 0;               /* socket backpressure */
    if (c_audio_blocks >= AUDIO_MAX_BLOCKS && !child_flush_audio()) return 0;
    memcpy(c_audio + c_audio_blocks * 128, pcm128, 256);
    c_audio_blocks++;
    a_produced += 128;
    return 1;
}

/* Replace JSON-unsafe chars in a known game key with '_' (keys are ASCII ids). */
static const char *json_safe(const char *s)
{
    static char out[128];
    int j = 0;
    for (int i = 0; s[i] && j < (int)sizeof out - 1; i++) {
        unsigned char c = (unsigned char)s[i];
        out[j++] = (c == '"' || c == '\\' || c < 0x20) ? '_' : (char)c;
    }
    out[j] = 0;
    return out;
}

static int child_serialize_state(char *buf, int cap)
{
    int n = snprintf(buf, cap,
        "{\"t\":\"state\",\"crashed\":%s,\"in_game\":%s,\"in_battle\":%s,\"active_color\":%d,"
        "\"money\":%d,\"eq_cannons\":%d,\"eq_sailors\":%d,\"eq_sails\":%d,\"eq_cargo\":%d,"
        "\"offered\":[%d,%d],\"sel_cargo\":%d,\"sel_dest\":%d,"
        "\"last_voice_key\":\"%s\",\"virtual_ms\":%u,"
        "\"pirate_idx\":%d,\"pirate_cannons\":%d,\"pirate_sailors\":%d,\"pirate_sails\":%d,"
        "\"pvp_battle\":%s,\"enemy_color\":%d,"
        "\"figures\":[",
        g_emu.crashed ? "true" : "false",
        g_emu.in_game ? "true" : "false", g_emu.in_battle ? "true" : "false", g_emu.active_color,
        g_emu.money, g_emu.eq_cannons, g_emu.eq_sailors, g_emu.eq_sails, g_emu.eq_cargo,
        g_emu.offered[0], g_emu.offered[1], g_emu.sel_cargo, g_emu.sel_dest,
        json_safe(g_emu.last_voice_key), g_emu.virtual_ms,
        g_emu.pirate_idx, g_emu.pirate_cannons, g_emu.pirate_sailors, g_emu.pirate_sails,
        g_emu.pvp_battle ? "true" : "false", g_emu.enemy_color);
    if (n < 0) return 0;
    if (n >= cap) n = cap - 1;
    int first = 1;
    for (int f = 0; f < 21 && n < cap - 2; f++) {
        if (!g_emu.figures[f].present) continue;
        int m = snprintf(buf + n, cap - n, "%s{\"f\":%d,\"present\":true,\"color\":%d}",
                         first ? "" : ",", f, g_emu.figures[f].color);
        if (m < 0) break;
        n += (m >= cap - n) ? cap - n - 1 : m;
        first = 0;
    }
    n += snprintf(buf + n, cap - n, "]}");
    if (n >= cap) n = cap - 1;
    return n;
}

/* Parse one FR_INPUT_JSON message and apply it to the input model. */
static void child_on_input(uint8_t type, const uint8_t *p, uint32_t n, void *user)
{
    (void)user;
    if (type != FR_INPUT_JSON) return;
    struct mg_str j = mg_str_n((const char *)p, n);
    char *t = mg_json_get_str(j, "$.t");
    if (!t) return;

    if (!strcmp(t, "button")) {
        char *name = mg_json_get_str(j, "$.name");
        input_btn_t b;
        if (name && input_btn_from_name(name, &b)) input_button_press(b);
        free(name);
    } else if (!strcmp(t, "register")) {
        long color = mg_json_get_long(j, "$.color", 0);
        bool present = false; mg_json_get_bool(j, "$.present", &present);
        input_set_register((int)color, present);
    } else if (!strcmp(t, "cargo")) {
        input_set_cargo((int)mg_json_get_long(j, "$.value", 0));
    } else if (!strcmp(t, "dest")) {
        input_set_dest((int)mg_json_get_long(j, "$.value", -1));
    } else if (!strcmp(t, "port")) {
        int v = (int)mg_json_get_long(j, "$.value", -1);
        bool present = false;
        if (mg_json_get_bool(j, "$.present", &present)) input_port_set(v, present);
        else input_port_pulse(v);
    } else if (!strcmp(t, "fast")) {
        bool on = false; mg_json_get_bool(j, "$.on", &on); input_set_fast(on);
    }
    free(t);
}

static void child_drain_input(int fd)
{
    for (;;) {
        if (c_in_len == sizeof c_in) c_in_len = 0;     /* overflow guard (shouldn't happen) */
        ssize_t r = read(fd, c_in + c_in_len, sizeof c_in - c_in_len);
        if (r > 0) {
            c_in_len += (size_t)r;
            size_t used = parse_frames(c_in, c_in_len, child_on_input, NULL);
            if (used) { memmove(c_in, c_in + used, c_in_len - used); c_in_len -= used; }
            continue;
        }
        if (r == 0) { c_eof = 1; return; }             /* parent closed */
        if (errno == EINTR) continue;
        break;                                          /* EWOULDBLOCK / EAGAIN */
    }
}

/* The forked child: own g_emu + Unicorn, headless, bridged over fd. Never returns. */
static void child_main(int fd, const char *root, const char *lang)
{
    /* Reset signal disposition inherited from the parent. */
    signal(SIGCHLD, SIG_DFL);

    memset(&g_emu, 0, sizeof g_emu);
    g_emu.root = root;
    g_emu.lang = lang;

    cpu_t *cpu = NULL;
    if (emu_boot(&cpu) != 0) { fprintf(stderr, "child: emu_boot failed\n"); _exit(1); }

    audio_set_sink(child_audio_sink, NULL);     /* sets g_emu.audio_realtime = true */
    input_reset();

    /* Reseed the RNG per child (syscall_init set a fixed seed; otherwise every
     * lobby would play an identical sequence). */
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    g_emu.rng_state = (uint32_t)((unsigned)getpid() * 2654435761u) ^ (uint32_t)ts.tv_nsec;
    if (g_emu.rng_state == 0) g_emu.rng_state = 0x2545F491u;

    set_nonblocking(fd);

    uint64_t last = now_ms();
    uint32_t last_bcast = 0;
    int first = 1;

    for (;;) {
        uint64_t loop_start = now_ms();

        /* 1) drain input from parent -> input model */
        child_drain_input(fd);
        if (c_eof) _exit(0);

        /* 2) real elapsed -> small virtual-time slice (mirrors ui_frame_ms clamp) */
        uint32_t dms = (uint32_t)(loop_start - last);
        last = loop_start;
        if (dms < 1) dms = 1;
        if (dms > 50) dms = 50;

        /* 3) finalize this tick's input mask before the guest reads it */
        input_build_mask();

        /* 4) advance + run one slice (audio_pump runs inside sched_run) */
        g_emu.stop_vms += g_emu.fast ? 300u : dms;
        g_emu.running = true;
        sched_run();
        rfid_refresh_stats();
        if (g_emu.in_battle && g_emu.virtual_ms - g_emu.battle_ms > 10000u)
            g_emu.in_battle = false;            /* battle-overlay safety net (main.c:196) */

        /* Unrecoverable guest fault: push a final crashed state so clients can
         * show it, then exit. The parent reaps us (exit code 2) and ends the
         * lobby immediately. */
        if (g_emu.crashed) {
            char j[4096];
            int n = child_serialize_state(j, sizeof j);
            child_enqueue(FR_STATE_JSON, j, (uint32_t)n);
            child_flush_out(fd);
            _exit(2);
        }

        /* 5) flush audio + outgoing buffer */
        child_flush_audio();
        child_flush_out(fd);

        /* 6) broadcast state ~25 Hz */
        if (first || g_emu.virtual_ms - last_bcast >= 40u) {
            first = 0;
            last_bcast = g_emu.virtual_ms;
            char json[4096];
            int n = child_serialize_state(json, sizeof json);
            child_enqueue(FR_STATE_JSON, json, (uint32_t)n);
            child_flush_out(fd);
        }

        /* 7) sleep the remainder of a ~16 ms budget; wake early on input */
        uint64_t spent = now_ms() - loop_start;
        int nap = 16 - (int)spent;
        struct pollfd pfd = { .fd = fd, .events = POLLIN };
        poll(&pfd, 1, nap > 0 ? nap : 0);
    }
}

/* ===================================================================== */
/* ============================  PARENT  =============================== */
/* ===================================================================== */

#define MAX_LOBBIES          8
#define EMPTY_TIMEOUT_MS     30000u
#define AUDIO_DROP_QUEUE     (512u * 1024)   /* drop audio for a client this far behind */

typedef struct {
    int      used;
    int      id;
    char     name[32];
    pid_t    pid;
    int      fd;                    /* parent end of socketpair (owned by mongoose once wrapped) */
    struct mg_connection *conn;     /* wrapped-fd connection */
    uint64_t empty_since_ms;        /* 0 = not empty */
    int      terminating;           /* SIGTERM already sent */
    int      alive;
} lobby_t;

static lobby_t      g_lobbies[MAX_LOBBIES];
static int          g_next_lobby_id = 1;
static const char  *g_root, *g_lang;
static volatile sig_atomic_t g_sigchld;

static void on_sigchld(int sig) { (void)sig; g_sigchld = 1; }

/* WS client connections store their lobby id (or -1) in c->data (memcpy-safe). */
static int conn_lobby_get(struct mg_connection *c)
{
    int v; memcpy(&v, c->data, sizeof v); return v;
}
static void conn_lobby_set(struct mg_connection *c, int v)
{
    memcpy(c->data, &v, sizeof v);
}

static lobby_t *lobby_by_id(int id)
{
    if (id <= 0) return NULL;
    for (int i = 0; i < MAX_LOBBIES; i++)
        if (g_lobbies[i].used && g_lobbies[i].id == id) return &g_lobbies[i];
    return NULL;
}

static lobby_t *lobby_by_pid(pid_t pid)
{
    for (int i = 0; i < MAX_LOBBIES; i++)
        if (g_lobbies[i].used && g_lobbies[i].pid == pid) return &g_lobbies[i];
    return NULL;
}

static lobby_t *lobby_alloc(void)
{
    for (int i = 0; i < MAX_LOBBIES; i++)
        if (!g_lobbies[i].used) {
            memset(&g_lobbies[i], 0, sizeof g_lobbies[i]);
            g_lobbies[i].used = 1;
            return &g_lobbies[i];
        }
    return NULL;
}

static int lobby_count_clients(struct mg_mgr *mgr, int id)
{
    int n = 0;
    for (struct mg_connection *c = mgr->conns; c; c = c->next)
        if (c->is_websocket && conn_lobby_get(c) == id) n++;
    return n;
}

static void ws_send_str(struct mg_connection *c, const char *s)
{
    mg_ws_send(c, s, strlen(s), WEBSOCKET_OP_TEXT);
}

static void send_error(struct mg_connection *c, const char *msg)
{
    char buf[160];
    snprintf(buf, sizeof buf, "{\"t\":\"error\",\"msg\":\"%s\"}", msg);
    ws_send_str(c, buf);
}

static void build_lobby_list(struct mg_mgr *mgr, char *buf, size_t cap)
{
    int n = snprintf(buf, cap, "{\"t\":\"lobbies\",\"items\":[");
    int first = 1;
    for (int i = 0; i < MAX_LOBBIES && n < (int)cap - 2; i++) {
        if (!g_lobbies[i].used || !g_lobbies[i].alive) continue;
        int players = lobby_count_clients(mgr, g_lobbies[i].id);
        int m = snprintf(buf + n, cap - n, "%s{\"id\":%d,\"name\":\"%s\",\"players\":%d}",
                         first ? "" : ",", g_lobbies[i].id, g_lobbies[i].name, players);
        if (m < 0) break;
        n += (m >= (int)cap - n) ? (int)cap - n - 1 : m;
        first = 0;
    }
    n += snprintf(buf + n, cap - n, "]}");
    (void)n;
}

static void send_lobby_list(struct mg_connection *c)
{
    char buf[1024];
    build_lobby_list(c->mgr, buf, sizeof buf);
    ws_send_str(c, buf);
}

static void broadcast_lobby_list(struct mg_mgr *mgr)
{
    char buf[1024];
    build_lobby_list(mgr, buf, sizeof buf);
    for (struct mg_connection *c = mgr->conns; c; c = c->next)
        if (c->is_websocket) ws_send_str(c, buf);
}

static void broadcast_to_lobby(struct mg_mgr *mgr, int id, const void *data, size_t len, int op)
{
    for (struct mg_connection *c = mgr->conns; c; c = c->next) {
        if (!c->is_websocket || conn_lobby_get(c) != id) continue;
        if (op == WEBSOCKET_OP_BINARY && c->send.len > AUDIO_DROP_QUEUE) continue; /* slow client: skip audio */
        mg_ws_send(c, data, len, op);
    }
}

static void lobby_check_empty(struct mg_mgr *mgr, int id)
{
    lobby_t *lob = lobby_by_id(id);
    if (!lob) return;
    lob->empty_since_ms = (lobby_count_clients(mgr, id) == 0) ? now_ms() : 0;
}

/* Free a lobby slot and notify/detach its remaining clients. Idempotent. */
static void lobby_cleanup(struct mg_mgr *mgr, lobby_t *lob, const char *reason)
{
    if (!lob || !lob->used) return;
    int id = lob->id;
    for (struct mg_connection *c = mgr->conns; c; c = c->next) {
        if (c->is_websocket && conn_lobby_get(c) == id) {
            conn_lobby_set(c, -1);
            if (reason) send_error(c, reason);
        }
    }
    if (lob->conn) { lob->conn->is_closing = 1; lob->conn = NULL; }
    lob->used = 0;
    lob->alive = 0;
    broadcast_lobby_list(mgr);
}

/* child -> parent frame: relay to the lobby's WS clients. */
static void parent_on_child_frame(uint8_t type, const uint8_t *p, uint32_t n, void *user)
{
    lobby_t *lob = user;
    if (!lob || !lob->conn) return;
    struct mg_mgr *mgr = lob->conn->mgr;
    if (type == FR_STATE_JSON) broadcast_to_lobby(mgr, lob->id, p, n, WEBSOCKET_OP_TEXT);
    else if (type == FR_AUDIO_PCM) broadcast_to_lobby(mgr, lob->id, p, n, WEBSOCKET_OP_BINARY);
}

/* Handler for the wrapped child socketpair connection. */
static void child_conn_fn(struct mg_connection *c, int ev, void *ev_data)
{
    (void)ev_data;
    lobby_t *lob = (lobby_t *)c->fn_data;
    if (ev == MG_EV_READ) {
        size_t used = parse_frames(c->recv.buf, c->recv.len, parent_on_child_frame, lob);
        if (used) mg_iobuf_del(&c->recv, 0, used);
    } else if (ev == MG_EV_CLOSE) {
        if (lob && lob->conn == c) { lob->conn = NULL; lob->alive = 0; }
    }
}

/* Close every fd mongoose currently owns (listener + clients + other lobbies)
 * in the freshly forked child, except the one socketpair end it keeps. */
static void child_close_inherited_fds(struct mg_mgr *mgr, int keep)
{
    for (struct mg_connection *c = mgr->conns; c; c = c->next) {
        int s = (int)(size_t)c->fd;
        if (s >= 0 && s != keep) close(s);
    }
}

static void parent_forward_input(lobby_t *lob, const char *json, size_t len)
{
    uint8_t hdr[FRAME_HDR];
    uint32_t l = (uint32_t)len;
    hdr[0] = FR_INPUT_JSON;
    memcpy(hdr + 1, &l, 4);
    mg_send(lob->conn, hdr, FRAME_HDR);
    mg_send(lob->conn, json, len);
}

static void create_lobby(struct mg_connection *c, const char *name)
{
    lobby_t *lob = lobby_alloc();
    if (!lob) { send_error(c, "too many lobbies"); return; }

    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        lob->used = 0; send_error(c, "socketpair failed"); return;
    }
    pid_t pid = fork();
    if (pid < 0) {
        close(sv[0]); close(sv[1]); lob->used = 0; send_error(c, "fork failed"); return;
    }
    if (pid == 0) {
        /* CHILD */
        close(sv[0]);
        child_close_inherited_fds(c->mgr, sv[1]);
        child_main(sv[1], g_root, g_lang);   /* never returns */
        _exit(0);
    }

    /* PARENT */
    close(sv[1]);
    set_nonblocking(sv[0]);
    lob->pid = pid;
    lob->fd = sv[0];
    lob->id = g_next_lobby_id++;
    lob->alive = 1;
    lob->empty_since_ms = 0;
    snprintf(lob->name, sizeof lob->name, "%s", name && *name ? name : "game");
    lob->conn = mg_wrapfd(c->mgr, sv[0], child_conn_fn, lob);
    if (!lob->conn) {
        kill(pid, SIGTERM);
        close(sv[0]);
        lob->used = 0;
        send_error(c, "wrapfd failed");
        return;
    }

    /* auto-join the creator */
    conn_lobby_set(c, lob->id);
    char buf[64];
    snprintf(buf, sizeof buf, "{\"t\":\"joined\",\"id\":%d}", lob->id);
    ws_send_str(c, buf);
    broadcast_lobby_list(c->mgr);
    printf("lobby %d \"%s\" created (pid %d)\n", lob->id, lob->name, (int)pid);
}

static void join_lobby(struct mg_connection *c, int id)
{
    lobby_t *lob = lobby_by_id(id);
    if (!lob || !lob->alive) { send_error(c, "no such lobby"); return; }
    int old = conn_lobby_get(c);
    conn_lobby_set(c, id);
    lob->empty_since_ms = 0;
    char buf[64];
    snprintf(buf, sizeof buf, "{\"t\":\"joined\",\"id\":%d}", id);
    ws_send_str(c, buf);
    if (old > 0 && old != id) lobby_check_empty(c->mgr, old);
    broadcast_lobby_list(c->mgr);
}

static void leave_lobby(struct mg_connection *c)
{
    int old = conn_lobby_get(c);
    conn_lobby_set(c, -1);
    ws_send_str(c, "{\"t\":\"left\"}");
    if (old > 0) { lobby_check_empty(c->mgr, old); broadcast_lobby_list(c->mgr); }
}

static void handle_ws_msg(struct mg_connection *c, struct mg_str d)
{
    char *t = mg_json_get_str(d, "$.t");
    if (!t) return;

    if (!strcmp(t, "list_lobbies")) {
        send_lobby_list(c);
    } else if (!strcmp(t, "create_lobby")) {
        char *name = mg_json_get_str(d, "$.name");
        create_lobby(c, name);
        free(name);
    } else if (!strcmp(t, "join_lobby")) {
        join_lobby(c, (int)mg_json_get_long(d, "$.id", -1));
    } else if (!strcmp(t, "leave_lobby")) {
        leave_lobby(c);
    } else {
        /* gameplay message: route to the conn's lobby child verbatim */
        lobby_t *lob = lobby_by_id(conn_lobby_get(c));
        if (lob && lob->conn) parent_forward_input(lob, d.buf, d.len);
        else send_error(c, "not in a lobby");
    }
    free(t);
}

static void ws_fn(struct mg_connection *c, int ev, void *ev_data)
{
    if (ev == MG_EV_OPEN) {
        if (!c->is_listening) conn_lobby_set(c, -1);
    } else if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = ev_data;
        struct mg_str *upg = mg_http_get_header(hm, "Upgrade");
        if (upg && mg_strcasecmp(*upg, mg_str("websocket")) == 0) {
            mg_ws_upgrade(c, hm, NULL);
        } else {
            mg_http_reply(c, 200, "Content-Type: text/plain\r\n", "yvio-emu server\n");
        }
    } else if (ev == MG_EV_WS_OPEN) {
        send_lobby_list(c);
    } else if (ev == MG_EV_WS_MSG) {
        struct mg_ws_message *wm = ev_data;
        handle_ws_msg(c, wm->data);
    } else if (ev == MG_EV_CLOSE) {
        if (c->is_websocket) {
            int old = conn_lobby_get(c);
            conn_lobby_set(c, -1);
            if (old > 0) { lobby_check_empty(c->mgr, old); broadcast_lobby_list(c->mgr); }
        }
    }
}

static void reap_children(struct mg_mgr *mgr)
{
    int st;
    pid_t pid;
    while ((pid = waitpid(-1, &st, WNOHANG)) > 0) {
        lobby_t *lob = lobby_by_pid(pid);
        if (lob) {
            const char *reason = (WIFEXITED(st) && WEXITSTATUS(st) == 2)
                                 ? "game crashed (quest/news bug)" : "lobby ended";
            printf("lobby %d (pid %d) exited: %s\n", lob->id, (int)pid, reason);
            lobby_cleanup(mgr, lob, reason);
        }
    }
}

static void sweep_lobbies(void)
{
    uint64_t t = now_ms();
    for (int i = 0; i < MAX_LOBBIES; i++) {
        lobby_t *lob = &g_lobbies[i];
        if (!lob->used || lob->terminating) continue;
        if (lob->empty_since_ms && t - lob->empty_since_ms > EMPTY_TIMEOUT_MS) {
            printf("lobby %d empty -> terminating (pid %d)\n", lob->id, (int)lob->pid);
            lob->terminating = 1;
            kill(lob->pid, SIGTERM);   /* child exits -> EOF + SIGCHLD -> reaped */
        }
    }
}

int server_main(const char *root, const char *lang, int port)
{
    g_root = root;
    g_lang = lang;

    signal(SIGPIPE, SIG_IGN);
    struct sigaction sa = { 0 };
    sa.sa_handler = on_sigchld;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    struct mg_mgr mgr;
    mg_mgr_init(&mgr);

    char url[64];
    snprintf(url, sizeof url, "http://0.0.0.0:%d", port);
    if (!mg_http_listen(&mgr, url, ws_fn, NULL)) {
        fprintf(stderr, "server: failed to listen on %s\n", url);
        return 1;
    }
    printf("yvio-emu server listening on ws://0.0.0.0:%d (root=%s lang=%s)\n", port, root, lang);

    for (;;) {
        mg_mgr_poll(&mgr, 20);
        if (g_sigchld) { g_sigchld = 0; reap_children(&mgr); }
        sweep_lobbies();
    }
    /* not reached */
}
