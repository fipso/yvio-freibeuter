<script setup lang="ts">
import { computed } from 'vue'
import type { GameState } from '../types'
import { PORTS, CARGONAME, PORT_HOTSPOTS, CARGO_HOTSPOTS, COLNAME, COLCSS } from '../lib/consts'
import { api } from '../lib/socket'

const props = defineProps<{ state: GameState }>()
const inGame = computed(() => props.state.in_game)

// Map markers derived from the serialized state. Port numbers are display indices
// 0..7 (PORTS / PORT_HOTSPOTS order). Markers are non-interactive (clicks pass
// through to the port hotspots beneath them).
const playerMarks = computed(() =>
  (props.state.player_ports ?? [])
    .map((port, ci) => ({ port, ci }))
    .filter(m => m.port >= 0 && m.port <= 7)
)
function bitsToPorts(mask: number): number[] {
  const out: number[] = []
  for (let i = 0; i < 8; i++) if (mask & (1 << i)) out.push(i)
  return out
}
const pirateMarks = computed(() => bitsToPorts(props.state.pirate_ports ?? 0))
const silverMarks = computed(() => bitsToPorts(props.state.silver_ports ?? 0))

// Dev-only calibration aid: makes hotspots visible + numbered and logs the
// click position as board-percentages, so coords in consts.ts can be tuned
// against the live render. Tree-shaken out of production builds.
const DEBUG = import.meta.env.DEV

// Ports — same logic as the old PortGrid: in-game toggles destination,
// setup sends a momentary port press.
function isDest(i: number): boolean { return props.state.sel_dest === i }
function clickPort(i: number) {
  if (inGame.value) api.dest(isDest(i) ? -1 : i)
  else api.port(i)
}

// Cargo — same logic as the old CargoBar in-game branch.
function cargoSel(i: number): boolean { return props.state.sel_cargo === i + 1 }
function cargoOffered(i: number): boolean { return props.state.offered.includes(i + 1) }
function clickCargo(i: number) { api.cargo(cargoSel(i) ? 0 : i + 1) }

function logPos(e: MouseEvent) {
  if (!DEBUG) return
  const el = e.currentTarget as HTMLElement
  const r = el.getBoundingClientRect()
  const x = ((e.clientX - r.left) / r.width) * 100
  const y = ((e.clientY - r.top) / r.height) * 100
  console.log(`board click: { x: ${x.toFixed(1)}, y: ${y.toFixed(1)} }`)
}
</script>

<template>
  <div class="board-map" :class="{ dbg: DEBUG }" @click="logPos">
    <img src="/board.webp" alt="Spielbrett" draggable="false" />

    <!-- Ports: always interactive (setup -> api.port, in-game -> api.dest) -->
    <button
      v-for="(h, i) in PORT_HOTSPOTS" :key="'p' + i"
      class="hot hot--port"
      :class="{ 'is-dest': inGame && isDest(i) }"
      :style="{ left: h.x + '%', top: h.y + '%' }"
      :title="PORTS[i]"
      :aria-label="PORTS[i]"
      @click="clickPort(i)"
    >
      <span v-if="DEBUG" class="idx">P{{ i }}</span>
    </button>

    <!-- Cargo: in-game only, 4 symbols on the left edge -->
    <template v-if="inGame">
      <button
        v-for="(h, i) in CARGO_HOTSPOTS" :key="'c' + i"
        class="hot hot--cargo"
        :class="{ 'is-sel': cargoSel(i), 'is-offered': cargoOffered(i) }"
        :style="{ left: h.x + '%', top: h.y + '%' }"
        :title="CARGONAME[i]"
        :aria-label="CARGONAME[i]"
        @click="clickCargo(i)"
      >
        <span v-if="cargoSel(i)" class="mark" />
        <span v-if="DEBUG" class="idx">C{{ i }}</span>
      </button>
    </template>

    <!-- Live entity markers (non-interactive; clicks pass through to the hotspots).
         Pirate = dark skull badge, silver = gold star badge, player = colored dot. -->
    <div
      v-for="m in pirateMarks" :key="'pir' + m"
      class="marker marker--pirate"
      :style="{ left: PORT_HOTSPOTS[m].x + '%', top: PORT_HOTSPOTS[m].y + '%' }"
      :title="'Pirat: ' + PORTS[m]"
    >☠</div>

    <div
      v-for="m in silverMarks" :key="'slv' + m"
      class="marker marker--silver"
      :style="{ left: PORT_HOTSPOTS[m].x + '%', top: PORT_HOTSPOTS[m].y + '%' }"
      :title="'Silberflotte (Gerücht): ' + PORTS[m]"
    >✦</div>

    <div
      v-for="m in playerMarks" :key="'ply' + m.ci"
      class="marker marker--player"
      :style="{
        left: PORT_HOTSPOTS[m.port].x + '%',
        top: PORT_HOTSPOTS[m.port].y + '%',
        background: COLCSS[m.ci],
        transform: `translate(calc(-50% + ${(m.ci - 1.5) * 16}px), calc(-50% + 18px))`,
      }"
      :title="'Spieler ' + COLNAME[m.ci] + ': ' + PORTS[m.port]"
    />
  </div>
</template>

<style scoped>
.board-map {
  position: relative;
  width: 100%;
  max-width: 900px;
  margin: 0 auto;
  aspect-ratio: 1600 / 1594; /* matches public/board.webp; keeps overlays aligned */
  border-radius: 12px;
  overflow: hidden;
  box-shadow: 0 6px 30px rgba(0, 0, 0, .35);
}
.board-map > img {
  display: block;
  width: 100%;
  height: 100%;
  object-fit: cover;
  user-select: none;
  -webkit-user-drag: none;
}

/* Hotspots are invisible at rest; affordance appears on hover / for state. */
.hot {
  position: absolute;
  transform: translate(-50%, -50%);
  padding: 0;
  border: 0;
  background: transparent;
  box-shadow: none;
  border-radius: 12px;
  cursor: pointer;
  transition: background .1s ease, box-shadow .1s ease;
}
.hot:hover {
  background: rgba(240, 220, 120, .18);
  box-shadow: 0 0 0 2px var(--gold), 0 0 14px rgba(240, 220, 120, .35);
}
/* Keep centering on press (global button:active sets translateY, which would
   otherwise un-center the hotspot); add a small scale for press feedback. */
.hot:active:not(:disabled) { transform: translate(-50%, -50%) scale(.95); }

/* Port hotspots: rounded-rect covering the painted label card. */
.hot--port {
  width: max(44px, 15%);
  height: max(32px, 9%);
}
.hot--port.is-dest {
  background: rgba(240, 220, 120, .28);
  box-shadow: 0 0 0 3px var(--gold), 0 0 18px rgba(240, 220, 120, .55);
}

/* Cargo hotspots: circular, over the compact left-edge symbols. */
.hot--cargo {
  width: max(40px, 8%);
  height: max(40px, 8%);
  border-radius: 50%;
}
/* "offered" = goods the active port wants — accent ring, distinct from gold. */
.hot--cargo.is-offered {
  box-shadow: 0 0 0 2px var(--accent), 0 0 12px rgba(90, 160, 255, .45);
}
.hot--cargo.is-sel {
  background: rgba(240, 220, 120, .3);
  box-shadow: 0 0 0 3px var(--gold), 0 0 16px rgba(240, 220, 120, .5);
}
.mark {
  position: absolute; top: 50%; left: 50%;
  transform: translate(-50%, -50%);
  width: 12px; height: 12px; border-radius: 50%; background: #fff;
}

/* Dev calibration: outline + index labels so coords can be tuned. */
.board-map.dbg .hot {
  background: rgba(90, 160, 255, .18);
  box-shadow: 0 0 0 1px rgba(255, 255, 255, .5);
}
.idx {
  position: absolute; top: 50%; left: 50%;
  transform: translate(-50%, -50%);
  font-size: 11px; font-weight: 700; color: #fff;
  text-shadow: 0 1px 2px #000; pointer-events: none;
}

/* Live entity markers — non-interactive overlays anchored on a port hotspot. */
.marker {
  position: absolute;
  pointer-events: none;
  display: grid; place-items: center;
  font-size: 13px; line-height: 1;
  user-select: none;
  z-index: 2;
}
.marker--player {
  width: 14px; height: 14px;
  border-radius: 50%;
  border: 2px solid rgba(255, 255, 255, .9);
  box-shadow: 0 1px 4px rgba(0, 0, 0, .6);
  /* transform set inline (fans out by player color so co-located ships don't stack) */
}
.marker--pirate {
  width: 22px; height: 22px;
  border-radius: 50%;
  background: #1a1320; color: #fff;
  border: 2px solid #dc4646;
  box-shadow: 0 1px 6px rgba(0, 0, 0, .6);
  transform: translate(calc(-50% - 24px), calc(-50% - 14px));   /* up-left of port */
}
.marker--silver {
  width: 22px; height: 22px;
  border-radius: 50%;
  background: var(--gold); color: #2a230a;
  border: 2px solid #fff3c4;
  transform: translate(calc(-50% + 24px), calc(-50% - 14px));   /* up-right of port */
  animation: silverpulse 1.6s ease-in-out infinite;
}
@keyframes silverpulse {
  0%, 100% { box-shadow: 0 0 6px rgba(240, 220, 120, .5); }
  50%      { box-shadow: 0 0 16px rgba(240, 220, 120, .95); }
}
</style>
