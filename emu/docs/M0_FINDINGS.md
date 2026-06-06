# M0 — Boot trace findings

Status: **DONE.** `pirates/app.elf` boots on the Unicorn ARM7TDMI-S core with a
log-only syscall hook. No faults, no unimplemented-syscall halts. Full trace in
`m0_boot_trace.txt`. The boot sequence confirms the research model and surfaced
two syscalls the static analysis missed.

## Two bugs fixed to get a clean boot
1. **SP=0 crash.** Writing `CPSR` (mode→SYS) switches Unicorn's *banked* SP, so the
   SP register must be written **after** the mode. (`main.c`)
2. **`.data` init.** `start()` (entry `0x3000`) copies `.data` from its **LMA
   `0x2BC0C`** (`p_paddr`) to VMA `0x40000000`. The loader must place segment bytes
   at `p_paddr`, not `p_vaddr`. (`elf.c`)

## Confirmed boot sequence (matches the design)
```
sem_create(0x4000065c,1,1)                 # a mutex
get_boot_args(&v4,&v3)
queue_create(0x40002198, 0x400021bc, 20)   # the 20-slot event queue   ✓ predicted addr
sem_create(0x4000220c,1,1)                 # event-queue lock sem        ✓
scheduler_init(.., 0x40001be0)             # ready-queue heads           ✓
thread_create(0x4000099c, tramp=0x25cd0, prio=12, stk=0x40000998)   # INPUT thread  ✓
fopen(...) ...                             # load data files from the dump
sem_create(0x40000a28, 8, 8)               # audio ring "free" sem       ✓
sem_create(0x40000a3c, 0, 8)               # audio ring "ready" sem      ✓
thread_create(0x4000127c, tramp=0x25cd0, prio=13, ...)             # AUDIO mixer  ✓
thread_create(0x400efb60, tramp=0x25cd0, prio=14, ...)            # MAIN game    ✓
... then a get_ticks() spin (waiting on time; expected — no real clock/scheduler in M0)
```
The trampoline `0x25cd0`, the priorities (12/13/14), and every shared object
address match `docs/` from the dev branch.

## ABI corrections (vs the planning ABI table)
The OS `.syscalls` pointer table (`OS.elf` @ vaddr `0x1170`, 92 entries) was decoded
in full; idx = `(addr-0x1000)/4`. Two syscalls were **missing** from the static
`MEMORY[0x10xx]` grep because the app reaches them via `JUMPOUT`/wrapper tail-calls
(`sub_23558`, `sub_2358C`, …):

| addr | idx | OS target | finding |
|---|---|---|---|
| `0x105c` | 23 | `0x5E950` → `0x6490c` | `memcpy(userbuf, &sysconst, 24)` — **read a 24-byte system object** into caller buffer |
| `0x1060` | 24 | `0x5E94C` → `0x6491c` | **write/process a 24-byte object** (board model `0x400023e8` passed as arg) |

`0x6c5b4` (the shared callee) was disassembled and is plain **`memcpy`** (ptr-equal
early-out + word-aligned `ldm/stm` block copy).

Also: **`0x1024` (idx9) is NOT `os_exec`/exit** as the planning ABI guessed — its OS
target `0x5B408` sits in the FAT/file cluster (`sub_5B340` loader area, signature
`int sub_5B408(int *a1, u8 a2)`). It was called once at boot with `(0,0,0)` and a
benign return let boot continue. Treat as a filesystem helper; revisit at M1.

`obj24_read`/`obj24_write` (`0x105c`/`0x1060`) exact purpose (RTC/date? board
snapshot? config blob?) is deferred to M1 — they are memcpy-backed get/set of a
24-byte struct and were benign (return 0) for boot. Decide their backing when a
consumer of the read data is observed in the M1 trace.

## How to reproduce
```
cd emu && nix-shell shell.nix --run "make && ./build/yvio-emu --root .."
```
