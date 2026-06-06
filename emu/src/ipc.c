/* ipc.c — host-side message queues + counting semaphores/mutexes.
 * Objects are keyed by the guest address the app passes (r0). Blocking parks
 * the running thread and reschedules; waking sets the waiter's return value. */
#include "emu.h"
#include <string.h>

/* ---- object tables (keyed by guest address) ---- */

static hsem_t *sem_find(uint32_t g)
{
    for (int i = 0; i < MAX_IPC; i++)
        if (g_emu.sems[i].used && g_emu.sems[i].gaddr == g) return &g_emu.sems[i];
    return NULL;
}
hsem_t *ipc_sem_find(uint32_t g) { return sem_find(g); }
static hsem_t *sem_new(uint32_t g)
{
    for (int i = 0; i < MAX_IPC; i++)
        if (!g_emu.sems[i].used) {
            hsem_t *s = &g_emu.sems[i];
            memset(s, 0, sizeof *s); s->used = 1; s->gaddr = g; return s;
        }
    return NULL;
}
static hqueue_t *q_find(uint32_t g)
{
    for (int i = 0; i < MAX_IPC; i++)
        if (g_emu.queues[i].used && g_emu.queues[i].gaddr == g) return &g_emu.queues[i];
    return NULL;
}
static hqueue_t *q_new(uint32_t g)
{
    for (int i = 0; i < MAX_IPC; i++)
        if (!g_emu.queues[i].used) {
            hqueue_t *q = &g_emu.queues[i];
            memset(q, 0, sizeof *q); q->used = 1; q->gaddr = g; return q;
        }
    return NULL;
}

/* highest-priority thread blocked on obj with the given wait kind */
static thread_t *waiter_for(void *obj, waitkind_t kind)
{
    thread_t *best = NULL;
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_t *t = &g_emu.threads[i];
        if (t->used && t->state == T_BLOCKED && t->wait_obj == obj && t->wait_kind == kind)
            if (!best || t->prio > best->prio) best = t;
    }
    return best;
}

/* If a just-woken thread outranks the running one, yield to it now. */
static void maybe_yield_to(thread_t *woken)
{
    if (woken && woken->prio > g_emu.cur->prio) {
        thread_park_pc();
        g_emu.cur->state = T_READY;
        schedule();
    }
}

static void block_cur(void *obj, waitkind_t kind, int timeout_ms)
{
    thread_park_pc();
    g_emu.cur->state = T_BLOCKED;
    g_emu.cur->wait_obj = obj;
    g_emu.cur->wait_kind = kind;
    g_emu.cur->wake_ms = (timeout_ms < 0) ? NEVER : g_emu.virtual_ms + (uint32_t)timeout_ms;
    schedule();
}

/* ---- semaphores / mutexes ---- */

void sys_sem_create(void)            /* 0x111C: r0=obj, r1=init, r2=max */
{
    hsem_t *s = sem_find(arg(0));
    if (!s) s = sem_new(arg(0));
    if (s) { s->count = (int)arg(1); s->max = (int)arg(2); }
    ret(0);
}

void sys_sem_take(bool try_only)     /* 0x1118 take(r1=timeout) / 0x112C try */
{
    hsem_t *s = sem_find(arg(0));
    if (!s) { ret((uint32_t)-4); return; }
    if (s->count > 0) { s->count--; ret(0); return; }

    int timeout = try_only ? 0 : (int)arg(1);
    if (timeout == 0) { ret((uint32_t)-4); return; }   /* would block */
    block_cur(s, WK_SEM, timeout);                      /* r0 set on wake */
}

void sys_sem_give(void)              /* 0x1130: r0=obj */
{
    hsem_t *s = sem_find(arg(0));
    if (!s) { ret(0); return; }
    thread_t *w = waiter_for(s, WK_SEM);
    if (w) thread_wake(w, 0);                           /* token handed to waiter */
    else if (s->count < s->max) s->count++;
    ret(0);
    maybe_yield_to(w);
}

/* Post a token without rescheduling (used by the host audio drain). */
void ipc_sem_post(hsem_t *s)
{
    if (!s) return;
    thread_t *w = waiter_for(s, WK_SEM);
    if (w) thread_wake(w, 0);
    else if (s->count < s->max) s->count++;
}

/* ---- message queues ---- */

void sys_queue_create(void)          /* 0x10F8: r0=obj, r1=storage, r2=cap */
{
    hqueue_t *q = q_find(arg(0));
    if (!q) q = q_new(arg(0));
    if (q) { int c = (int)arg(2); q->cap = (c > 0 && c <= QUEUE_CAP) ? c : QUEUE_CAP; }
    ret(0);
}

static void q_push(hqueue_t *q, uint32_t item)
{
    q->items[q->tail] = item;
    q->tail = (q->tail + 1) % q->cap;
    q->count++;
}
static uint32_t q_pop(hqueue_t *q)
{
    uint32_t v = q->items[q->head];
    q->head = (q->head + 1) % q->cap;
    q->count--;
    return v;
}

void sys_queue_send(bool timed)      /* 0x1110 timed(r2=to) / 0x1114 try */
{
    hqueue_t *q = q_find(arg(0));
    if (!q) { ret((uint32_t)-4); return; }
    uint32_t item = arg(1);

    thread_t *rw = waiter_for(q, WK_QRECV);
    if (rw) {                                   /* deliver straight to a receiver */
        cpu_write(g_emu.cpu, rw->recv_out, &item, 4);
        thread_wake(rw, 0);
        ret(0);
        maybe_yield_to(rw);
        return;
    }
    if (q->count < q->cap) { q_push(q, item); ret(0); return; }

    int timeout = timed ? (int)arg(2) : 0;
    if (timeout == 0) { ret((uint32_t)-4); return; }
    g_emu.cur->send_item = item;
    block_cur(q, WK_QSEND, timeout);
}

void sys_queue_recv(bool timed)      /* 0x1108 timed(r2=to) / 0x110C block */
{
    hqueue_t *q = q_find(arg(0));
    uint32_t out = arg(1);
    if (!q) { ret((uint32_t)-4); return; }

    if (q->count > 0) {
        uint32_t item = q_pop(q);
        cpu_write(g_emu.cpu, out, &item, 4);
        thread_t *sw = waiter_for(q, WK_QSEND);     /* a slot freed: admit a sender */
        if (sw) { q_push(q, sw->send_item); thread_wake(sw, 0); }
        ret(0);
        return;
    }
    /* empty but a sender is parked (cap could be 0): hand its item over */
    thread_t *sw = waiter_for(q, WK_QSEND);
    if (sw) {
        cpu_write(g_emu.cpu, out, &sw->send_item, 4);
        thread_wake(sw, 0);
        ret(0);
        maybe_yield_to(sw);
        return;
    }
    int timeout = timed ? (int)arg(2) : -1;         /* 0x110C blocks forever */
    g_emu.cur->recv_out = out;
    block_cur(q, WK_QRECV, timeout);
}
