# M1 — status

The unmodified `pirates/app.elf` boots, runs its real RTOS, loads its data, plays
its **full German startup narration**, and reacts to board input — on the host
HLE emulator. No faults, no unimplemented syscalls, no scheduler deadlock.

## Validated headlessly (with the real binary)
- **Scheduler + IPC** (`sched.c`, `ipc.c`): 3 app threads (input p12 `0x23774`,
  audio p13 `0x23d04`, main p14 `0x1ee34`) scheduled cooperatively; ~80k context
  switches over a minute of guest time, no deadlock. Queues + sems/mutexes drive
  the game's event loop (block on `queue_recv` → wake on input → board mutex →
  `obj24` board update).
- **File I/O** (`file.c`): loads `lang.txt`, `files_de.txt`, `texts_de.txt`,
  `sounds/intro.wav`, and the voice WAVs — **0 failures**. Paths are SD-root
  relative; saves redirect to `emu/saves/`.
- **Audio bridge** (`audio.c`): host plays the I2S-DMA drain role over the app's
  8×260B ring (`0x40000A50`) gated by sems A28(free)/A3C(ready); consumption is
  metered to virtual time. The game played **57.6 s of audio** in order:
  `intro → p0231..p0234 (Welcome) → p0445/p0446`. `0x1094/98` wired; `0x109c/A0` noop.
- **Timing / rng / boot-args / obj24** (`syscall.c`).

## Built (run on a desktop to see/hear it)
- **raylib frontend** (`audio_host.c` audio output, `ui.c` board). Default mode
  opens a window with the 8 ports (click to place/remove a figure), the
  SKIP/Siegpunkte/Select buttons (keys S/V/Space), and a status line; virtual time
  is paced to ~real time so audio plays at the right speed. Input folds into the
  15-bit `0x1058` mask. (A/V not testable in the headless build env — verify on
  hardware with a display + audio.)

## Input model (corrected — two conventions on the one 0x1058 mask)
`sub_1FB28(mask)` (app.elf.c:25884) returns the **earliest newly-pressed bit inside
`mask`** (edge-detected via `dword_40000588`, consumed on read). The game reads it
with **two different conventions** depending on context:
- **Intro / setup flow** (`sub_10E68`, ~14060-14380), via `sub_13850()`=`sub_1FB28(96)`
  (bits 5,6) and `sub_13870(n)`:
  - **bit 6 = YES / OK / confirm** (also confirms registration, :14324)
  - **bit 5 = NO / reject** — at `#choose_game_1`, NO rejects beginner → `#choose_game_2`
    (then bit 6 = profi, bit 5/other = normal)
  - **bit 7 = SKIP intro** (:14079) · **bit 8 = repeat/re-ask** the prompt
  - **bit 12 = restart/redo** (`sub_10D08` returns 3 unless bit 12, :13972)
- **General menu / gameplay** (`sub_221F0`, :27980 → loop :28420): **bit 12 = OK/select**,
  **bit 13 = forward/skip**, **bit 14 = back/no**. (Nothing reads bit 14 as "Siegpunkte" —
  that earlier label was wrong; bit 14 is BACK.)

The UI buttons (one-shot pulses, like the ports) map: **OK**=bit 6, **NO**=5|14,
**SKIP**=7|13, **REPEAT**=8. Keyboard: `Space`/`Enter`=OK, `N`=NO, `S`=SKIP, `R`=Repeat,
`1-8`=ports, `F1-F4`=join (blue/green/red/yellow), `TAB`=fast-forward.

**OK is bit 6 only — NOT bit 12.** Bit 12 is the game's "restart this step" gesture:
`sub_10D08` (13971, 364 call sites) runs `sub_1FC2C(0x80000FFF, 0/1)` which disables
then re-enables input bits 0-11, **wiping bit 6's freshly-latched confirm edge** in the
same press. Unioning bit 12 into OK therefore broke the join confirm
(`sub_13F9C`:16630 `dword_40000448 && sub_13870(6)==6`) and caused intermittent
double-clicks. Menu SELECT (bit 12, only valid inside the `sub_221F0` menu loop) is
deferred until in-game menus are reached and can be context-gated.

Verified flow with scripted input:
`#intro → #welintro → #choose_game_1 → #short_game → #join` (player registration).
The interactive build shows the live voice-line key as `speaking: …`.

## Cargo trading turn (in progress)
The take-new-goods turn is `sub_11BE8` (14589), RFID-driven: it scans the 4 **cargo
field** keys `off_2B134` (0x11/0x10/0x0f/0x0e = wheat/wood/tobacco/rum) and the 8
**port field** keys `off_2B0B4` (0x12 0x13 0x00 0x04 0x05 0x07 0x0a 0x0c) via
`sub_1F250` -> (dead live read) -> match vs the seeded `dword_400004F4` table
(`sub_1F184`: byte0 compare, 0xFF terminator). A match returns (table-index+1), which
the game checks == the active player's colour (`*(v2+2)`); the cargo must be one of
the 2 goods the port offers (`*(v2+8)`[1],[2]).
- **Model** (`rfid.c`): once `in_game` (set by a `cpu_watch` on `sub_11BE8` in `main.c`),
  `rfid_sync_table` keeps each registered figure at index colour-1 and moves the **active
  player's** figure onto its current field — a cargo field (`sel_cargo`) then a port
  field (`sel_dest`). Active colour + the 2 offered goods are read from the guest player
  record (`dword_40000444` list, turn marker +12, colour +2, ship-pos +8).
- **UI** (`ui.c`): after registration the top-4 slots become cargo buttons
  Getreide/Holz/Tabak/Rum (the 2 offered goods are highlighted "load me"); the 8 ports
  become the destination selector.
- **Single-figure model:** the RFID reader only sees a figure *placed on an action field*,
  so `rfid_sync_table` presents **only the active player's figure, only once placed** (on a
  cargo field via `sel_cargo`, then a port field via `sel_dest`), at table index colour-1.
  The 4 cargo fields ARE the colour home fields, so parking idle figures there used to
  (a) auto-select wheat and (b) spam the wrong-player `#error` beep (`sub_4B80`,
  app.elf.c:14896) — both gone with the single-figure model.
- **Full turn works** (verified `--script --trace`, watching `sub_468C` too): load cargo
  (`#put_figure_to_destination_port`) → sail to a port that buys it → port announced
  (`#puerto_plata_lg`) → `#of_*_goods_reward` (money) → merchant equipment offer
  (`#m_*_equip_offer`). Active player switches correctly turn-to-turn (`active_player_color`
  reads colour 1→2 from the guest list). `#wrong_goods_*` is correct — that port doesn't
  buy that cargo; pick another.
- **Per-player reset** (`rfid.c`): `rfid_sync_table` now clears `sel_cargo`/`sel_dest` whenever
  `active_player_color()` reports a *different* colour (a new player's turn), so player N never
  inherits player N-1's figure placement. Verified: P2 starts `cargo=0 dest=-1 field=0xff`, and
  P1→P2 both complete full turns with no fault. This stale state was the likely cause of the
  "player 2 selects wheat + Curaçao → crash" report (player 2's figure was already latched on
  player 1's destination port before the turn was set up). One cargo per turn is enforced by the
  single `sel_cargo` (selecting a new cargo replaces the old).
- **Freeze fix — lift-on-rejection** (`main.c`): the reported "crash" at P2 grain+Curaçao was a *freeze* —
  an invalid destination (your own current port → `#take_new_port`, or a non-buying port → `#wrong_goods`)
  makes `sub_11BE8` re-prompt forever while our figure stays latched on that port. `on_voice` now resets
  `sel_dest = -1` on `#wrong_goods`/`#take_new_port`, "lifting" the figure so the player picks another port.
  Verified: `dest=Curaçao → #wrong_goods → dest lifted → valid port → reward`; no loop.
- **Freeze fix #2 — wrong-cargo lift** (`main.c`): clicking a cargo type the current port does NOT produce
  has the *same* latch-freeze. The cargo scan in `sub_11BE8` (~app.elf.c:14903) checks the selected cargo's
  value against the port's two offered goods (`*(ship+8)[1]/[2]`); on a mismatch it plays `#no_goods_at_port`
  (`sub_8ABC`→`sub_468C`) and `continue`s the scan forever while our figure stays on the wrong cargo field →
  freeze. `on_voice` now resets `sel_cargo = 0` on `#no_goods_at_port`, lifting the figure so the player can
  pick one of the offered cargos. Verified (`--scenario battle --trace`): `#no_goods_at_port` plays exactly
  once, then the RFID trace shows `cargo=0 field=0xff` (lifted) — no replay loop. (`#no_goods_at_port` is the
  cargo-only reject; the destination rejects `#wrong_goods`/`#take_new_port` are distinct strings.)
- **Merchant trading buttons** (`ui.c`): at a port the merchant offers equipment; the negotiation loop
  (app.elf.c:13520-13580) reads `sub_13868()`=`sub_1FB28(35)` = bits 0/1/5 → **bit 0 = buy/accept**,
  **bit 1 = haggle (Feilschen)**, **bit 5 = decline** (=NO), bit 8 = repeat (=REPEAT). Added gameplay-only
  **KAUFEN (K → bit 0)** and **FEILSCHEN (F → bit 1)** buttons (one-shot pulses); console row is 6 buttons
  in-game, 4 in setup. HUD corrected to "load ONE of the highlighted cargos" (one cargo per turn).
- **Equipment panel** (`rfid.c`+`ui.c`): `active_player_color()` also reads the active player record's money
  (`rec+20`) and equipment (`rec+25..34`, totals = L1+2·L2) into `g_emu`; the UI shows a line under the title:
  *Spieler <colour> · Kanonen · Matrosen · Segel · Laderaum · Dukaten*. **Verified**: start = 1500 Dukaten, all
  equipment 0; after a merchant buy → the matching stat increments, Dukaten drops by the price.
  **Equipment level offsets** (anchored to combat via the upgrade-purchase fn app.elf.c:13079-13144 + the
  maneuver combat fns): rec[25/26]=**sails** (→rec[35]), rec[27/28]=**cannons** (→rec[37], firing/`#music_cannon`),
  rec[29/30]=**sailors** (→rec[38], boarding/`#music_sword`), rec[31/32]=**cargo** (→rec[36]), rec[33/34]=**residence**.
  (Earlier rev had cannons/sailors swapped → cannons rendered under "Matrosen"; fixed in `rfid.c`.)
- **Sea-battle screen** (`main.c`+`ui.c`): `cpu_watch` on `sub_E970` captures the enemy pirate's
  cannons/sailors/sails (r1/r2/r3); `sub_1BD1C` records the candidate pirate index; `in_battle` opens on the
  `#of_*_play_maneuver` voice and closes on the outcome (`#of_*_firing/boarding_won/lost`, `#info_battle_lost`,
  next turn). While `in_battle` the UI draws an overlay (pirate vs us stats) + 3 one-shot buttons **KANONEN (C →
  bit 9 firing) · ENTERN (E → bit 10 boarding) · SEGELN (G → bit 11 flee)** — main battle reads 9/10, quest/PvP
  reads 9/10/11. Combat refs: `sub_1BD1C`:22418, `sub_E970`:11981, `sub_57E4`:3397, win iff `rec[37]>pirateCannons`
  / `rec[38]>pirateSailors` + roll. **Needs interactive verification** (a pirate encounter isn't reachable headlessly).
- **CLI battle scenario** (`--scenario battle`, `main.c`+`syscall.c`): drives a **normal** 2-player game
  (NO at `#choose_game_1`+`_2` → normal difficulty), registers 2 players, and delivers cargo. Because the game
  only places pirates much later, the scenario **host-seeds a pirate** at the active player's destination port
  (per-player slot in `byte_400002F0`: +0 active, +1 type 0, +2 location = `byte_40000168[16*dest]`, +4 ship
  value) so the arrival check (app.elf.c:14444, *before* the quest-distribution code that faults in normal mode)
  triggers a sea battle. Verified headless: `#puerto_plata_lg → #…_battle → #of_1_play_maneuver_sh → (KANONEN) →
  success → reward`, no fault. Headless auto-resolves with KANONEN (bit 9); interactive (`--scenario battle` w/o
  `--headless`) drives to the battle and hands the fight to the user's KANONEN/ENTERN/SEGELN buttons. The seeded
  quest-battle path skips `sub_E970`, so the scenario sets the overlay pirate stats directly from the ship template
  (cannons 2 / sailors 3 / sails 6); organic battles still use the `sub_E970` watch.
- **Battle/equipment UI fixes** (`ui.c`/`rfid.c`/`main.c`): (1) the battle "Du" line now shows the player's
  **equipment totals** (`eq_cannons`/`eq_sailors`/`eq_sails`, same source as the panel) — 0 at start, matching the
  game's spoken value — instead of the stale `rec[37]/[38]`. (2) `rfid_refresh_stats()` (wraps `active_player_color`)
  is called **every frame** in the interactive loop, so money/equipment are never stale (the RFID path only ran
  during cargo selection → the panel could show 0 / Dukaten 0). (3) the battle overlay now **closes** on the first
  watched non-battle/non-maneuver voice after a fight (`#credit`/`#goods_reward`/etc.) plus a 10 s no-voice timeout —
  the win/lose outcomes play via the unwatched `sub_3394`, so the old close-on-outcome list never fired interactively.
- **FIXED — quest/news wild-read fault** (`sub_1DF54`→`sub_1D754`, read @`0x1da6c`), seen "reading the
  news for January" and in normal/long mode after bad deliveries. Root cause: `sys_rng` (0x1064) returned a
  full 32-bit value, but the game treats it as **signed** — `sub_1E5F8` does `asr #10` then a modulo to pick
  an array index. A sign-bit-set RNG → negative shift → negative modulo → `sub_1BC10` reads `free_list[neg]`
  (a stack pointer) and the caller indexes `port_table[that*16]` → unmapped read. Fix: `sys_rng` masks the
  sign bit (`& 0x7FFFFFFF`) in `syscall.c` (the device RNG is non-negative). Repro: `--scenario news`
  (normal mode, no pirate seeding, cycles deliveries); `--trace` dumps the port/pirate tables at any fault
  (`diag_dump_fault` in `sched.c`). Also hardened: a guest fault is now non-fatal — `cpu_run` won't re-run a
  dead PC, the host loops stop stepping (`g_emu.crashed`), the desktop shows a crash banner, and a server
  lobby ends immediately with a "game crashed" message instead of spamming the console forever.
- **Deferred:** `0x1120` (a blocking sem-wait at `sub_24944`) is still stubbed to return 0
  (tolerated so far); the in-game menu SELECT (bit 12) still context-gated for later.

## Registration cracked — reaches gameplay (update)
- **Mouse:** dropped `FLAG_WINDOW_HIGHDPI`; layout + mouse share one logical space and a
  software cursor dot is drawn at the hit-test point (Wayland-proof aiming).
- **RFID registration** (`rfid.c`): the game identifies player figures via the RFID
  reader. `0x1088` selects a field; `0x1074` inventories it; `sub_1F250` matches against the
  `dword_400004F4` table. That table is never populated by the game (live path is dead:
  `sub_20BA0` returns 0, `sub_1F238` has no callers), so we **seed it from the host**: one
  16-byte entry per present figure on fields 14-17, UID `{fieldKey,0xFF,0,0,0,0xFF,...}`
  (keys 0x11/0x10/0x0f/0x0e = colors 1-4), and point `dword_400004F4` at it. Inventory
  returns 0 (no live tag) so `sub_1F250` takes the `v25==0` table-match path → registers.
- **Verified flow** (`--headless --script`): `#welintro → #choose_game_1 → #short_game →
  #join → #set_ship_to_start_port → #equip_ready → #equips_to_port → #of_1_take_new_goods_info`
  — i.e. **the first player's cargo turn**. Registration ends on `dword_40000448>0 &&
  sub_13870(6)==6` (a player joined + bit 6 pressed).
- **UI** (`ui.c`): 4 colored JOIN slots (click / F1-F4 → figures on fields 17/16/15/14) +
  the 8 ports + console buttons + voice-key HUD. Interactive flow: TAB/key8 skip intro →
  key7 choose game → click a JOIN slot → key7 to start.

## Deferred / next
- **RFID figure memory** (`0x1074/78/7C/80/88`) still noop — the game has not
  called them yet (intro/lobby), so per the trace-first approach they'll be
  implemented when the cargo flow actually exercises them.
- **obj24_read/write** (`0x105c/0x1060`) still stubbed (zero/no-op); revisit if a
  consumer needs real data.
- **Verify the 7 M1 done-criteria interactively** on hardware: register player →
  color voice; move ship → port voice; cargo round-trip; clean audio.
- Real-time/audio-clock coupling currently paces by frame delta; refine if audio
  drifts under load.

## Build / run
```
cd emu
nix-shell shell.nix --run "make"
# interactive (desktop, needs display + audio):
nix-shell shell.nix --run "./build/yvio-emu --root .."
# headless logic test / trace:
nix-shell shell.nix --run "./build/yvio-emu --root .. --headless --trace --max 60000 --vms 60000"
```
