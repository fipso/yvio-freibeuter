/* file.c — fopen/fread/fseek/fwrite/fclose/opendir/readdir/closedir served from
 * the SD-card dump. Guest paths use ? (game root) and ?? (sounds/<lang>/). */
#include "emu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define SCRATCH_BASE 0x50000000u   /* host->guest scratch (dirents) */
#define MAX_FH 32

typedef struct { int used; FILE *fp; DIR *dir; } fh_t;
static fh_t g_fh[MAX_FH];

static char *read_cstr(uint32_t gaddr, char *buf, size_t n)
{
    for (size_t i = 0; i < n - 1; i++) {
        uint8_t c = 0;
        cpu_read(g_emu.cpu, gaddr + i, &c, 1);
        buf[i] = (char)c;
        if (!c) return buf;
    }
    buf[n - 1] = 0;
    return buf;
}

/* Resolve a guest path against the dump root. */
static const char *resolve(const char *g, bool write)
{
    static char out[2048];
    if (write) {
        const char *base = g; for (const char *p = g; *p; p++) if (*p == '/') base = p + 1;
        if (*g == '?') { base = g + 1; if (*base == '?') base++; for (const char *p = base; *p; p++) if (*p == '/') base = p + 1; }
        snprintf(out, sizeof out, "%s/emu/saves/%s", g_emu.root, base);
        return out;
    }
    /* The app pre-expands ?/?? itself, so paths arrive SD-root-relative
     * (e.g. "app.elf", "pirates/lang.txt", "pirates/sounds/de/p0300.wav").
     * The ?/?? cases remain as a fallback for any un-expanded path. */
    if (g[0] == '?' && g[1] == '?')
        snprintf(out, sizeof out, "%s/pirates/sounds/%s/%s", g_emu.root, g_emu.lang, g + 2);
    else if (g[0] == '?')
        snprintf(out, sizeof out, "%s/pirates/%s", g_emu.root, g + 1);
    else
        snprintf(out, sizeof out, "%s/%s", g_emu.root, g[0] == '/' ? g + 1 : g);
    return out;
}

static int fh_alloc(void)
{
    for (int i = 1; i < MAX_FH; i++) if (!g_fh[i].used) { memset(&g_fh[i],0,sizeof g_fh[i]); g_fh[i].used=1; return i; }
    return 0;
}
static fh_t *fh_get(uint32_t handle)
{
    return (handle >= 1 && handle < MAX_FH && g_fh[handle].used) ? &g_fh[handle] : NULL;
}

void sys_fopen(void)   /* 0x102C: r0=path, r1=mode -> handle (0=fail) */
{
    char pbuf[1024], mbuf[16];
    const char *gpath = read_cstr(arg(0), pbuf, sizeof pbuf);
    const char *gmode = read_cstr(arg(1), mbuf, sizeof mbuf);
    bool write = strpbrk(gmode, "waWA+") != NULL;
    char mode[20]; snprintf(mode, sizeof mode, "%.8s%s", gmode, strchr(gmode,'b')?"":"b");

    const char *host = resolve(gpath, write);
    FILE *fp = fopen(host, mode);
    if (!fp) { if (g_emu.trace) printf("  fopen FAIL %s (%s)\n", gpath, host); ret(0); return; }
    int h = fh_alloc();
    if (!h) { fclose(fp); ret(0); return; }
    g_fh[h].fp = fp;
    if (g_emu.trace) printf("  fopen %s -> h%d\n", gpath, h);
    ret((uint32_t)h);
}

void sys_fclose(void)  /* 0x1028: r0=handle */
{
    fh_t *f = fh_get(arg(0));
    if (f) { if (f->fp) fclose(f->fp); f->used = 0; }
    ret(0);
}

void sys_fread(void)   /* 0x1030: r0=buf, r1=size, r2=count, r3=handle */
{
    uint32_t buf = arg(0), size = arg(1), count = arg(2);
    fh_t *f = fh_get(arg(3));
    if (!f || !f->fp || !size) { ret(0); return; }
    size_t total = (size_t)size * count;
    uint8_t *tmp = malloc(total ? total : 1);
    size_t n = fread(tmp, size, count, f->fp);
    cpu_write(g_emu.cpu, buf, tmp, n * size);
    free(tmp);
    ret((uint32_t)n);
}

void sys_fwrite(void)  /* 0x1038: r0=buf, r1=size, r2=count, r3=handle */
{
    uint32_t buf = arg(0), size = arg(1), count = arg(2);
    fh_t *f = fh_get(arg(3));
    if (!f || !f->fp || !size) { ret(0); return; }
    size_t total = (size_t)size * count;
    uint8_t *tmp = malloc(total ? total : 1);
    cpu_read(g_emu.cpu, buf, tmp, total);
    size_t n = fwrite(tmp, size, count, f->fp);
    free(tmp);
    ret((uint32_t)n);
}

void sys_fseek(void)   /* 0x1034: r0=handle, r1=offset, r2=whence (0=SET) */
{
    fh_t *f = fh_get(arg(0));
    int whence = (int)arg(2);
    int w = whence == 1 ? SEEK_CUR : whence == 2 ? SEEK_END : SEEK_SET;
    if (whence != 0 && g_emu.trace) printf("  fseek whence=%d\n", whence);
    ret(f && f->fp ? (uint32_t)fseek(f->fp, (long)(int32_t)arg(1), w) : (uint32_t)-1);
}

void sys_opendir(void) /* 0x1040: r0=path -> handle */
{
    char pbuf[1024];
    const char *host = resolve(read_cstr(arg(0), pbuf, sizeof pbuf), false);
    DIR *d = opendir(host);
    if (!d) { ret(0); return; }
    int h = fh_alloc();
    if (!h) { closedir(d); ret(0); return; }
    g_fh[h].dir = d;
    ret((uint32_t)h);
}

void sys_readdir(void) /* 0x1044: r0=handle -> dirent* (NULL at end) */
{
    fh_t *f = fh_get(arg(0));
    if (!f || !f->dir) { ret(0); return; }
    struct dirent *e = readdir(f->dir);
    if (!e) { ret(0); return; }
    /* Write the name into guest scratch; the app reads the name from offset 0. */
    uint8_t namebuf[256];
    size_t n = strnlen(e->d_name, sizeof namebuf - 1);
    memcpy(namebuf, e->d_name, n); namebuf[n] = 0;
    cpu_write(g_emu.cpu, SCRATCH_BASE, namebuf, n + 1);
    ret(SCRATCH_BASE);
}

void sys_closedir(void) /* 0x103C: r0=handle */
{
    fh_t *f = fh_get(arg(0));
    if (f) { if (f->dir) closedir(f->dir); f->used = 0; }
    ret(0);
}
