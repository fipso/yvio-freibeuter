# Freibeuter der Karibik — Web Client

A Vue 3 + Vite frontend for playing the emulated game in the browser, with a
**lobby system** (multiple concurrent games) and **multiple players per game**
sharing one live state. It talks to the C emulator backend over **WebSocket**.

## Architecture

```
Browser(s) ──WS(JSON + binary PCM)──▶ yvio-emu --server   (mongoose, parent)
                                          └─ forks one emulator child per lobby
```

- **Lobby**: one game = one forked emulator process (own `g_emu` + Unicorn).
- **Control**: free-for-all — any connected client can press any button / place
  any figure (a shared remote for the single original device).
- **Audio**: the backend streams 22050 Hz mono int16 PCM as binary WS frames;
  the client plays it back gap-free via the Web Audio API (click **🔊 Ton an**
  first — browsers require a user gesture before audio can start).

## Run

1. **Backend** (from `emu/`, built via `nix-shell shell.nix --run make`):
   ```bash
   ./build/yvio-emu --server --port 8080 --root ..
   ```
   `--root` points at the SD-card dump (the repo root, which holds `pirates/`).

2. **Frontend** (from `emu/web/`):
   ```bash
   bun install
   bun run dev          # http://localhost:5173
   ```

By default the client connects to `ws://<current-host>:8080/`. Override with
`VITE_WS_URL` in `.env` (e.g. when the backend runs on another machine):
```
VITE_WS_URL=ws://192.168.1.50:8080/
```

Build for production with `bun run build` (outputs `dist/`), preview with
`bun run preview`.

## Board backdrop

`public/board.webp` (≈190 KB) is generated from the original `board.jpg`
(≈6 MB). Regenerate it with `bun run board` (uses `sharp`).

## WebSocket protocol

Client → server (control): `{"t":"list_lobbies"}`, `{"t":"create_lobby","name":…}`,
`{"t":"join_lobby","id":…}`, `{"t":"leave_lobby"}`.

Client → server (gameplay): `{"t":"button","name":"OK|NO|SKIP|REPEAT|KAUFEN|FEILSCHEN|KANONEN|ENTERN|SEGELN"}`,
`{"t":"register","color":1..4,"present":bool}`, `{"t":"cargo","value":0..4}`,
`{"t":"dest","value":-1..7}`, `{"t":"port","value":0..7}`, `{"t":"fast","on":bool}`.

Server → client: `{"t":"lobbies","items":[…]}`, `{"t":"joined","id":…}`,
`{"t":"left"}`, `{"t":"error","msg":…}`, periodic `{"t":"state",…}` (~25 Hz),
plus **binary** frames of raw int16 mono 22050 Hz PCM audio.

## Layout

Functional panels (mirroring the native raylib UI) over a dimmed board photo:
equipment, the 4 top slots (JOIN colors in setup / cargo selectors in-game),
the 8 ports (sail destination in-game), the console buttons, a voice/time HUD,
and a sea-battle overlay.

## Project structure

```
src/
  main.ts, App.vue, types.ts
  style.css
  lib/    socket.ts (WS store + input API)  audio.ts (PCM player)  consts.ts
  components/  LobbyView  GameView  EquipmentPanel  CargoBar  PortGrid
               ConsoleButtons  BattleOverlay  VoiceHud
public/board.webp           # generated backdrop
scripts/make-board.mjs      # board.jpg -> public/board.webp
```
