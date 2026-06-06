<script setup lang="ts">
import { computed } from 'vue'
import type { GameState } from '../types'
import { CARGONAME, CARGOCSS, COLNAME, COLCSS, REG_FIELD } from '../lib/consts'
import { api } from '../lib/socket'

const props = defineProps<{ state: GameState }>()
const inGame = computed(() => props.state.in_game)

// Setup: is the JOIN figure for color (i+1) present? (figures lists present ones.)
function joined(i: number): boolean {
  const field = REG_FIELD[i]
  return props.state.figures.some(f => f.f === field && f.present)
}
function toggleJoin(i: number) { api.register(i + 1, !joined(i)) }

// In-game: cargo selection (1..4), with the port's offered goods highlighted.
function selected(i: number): boolean { return props.state.sel_cargo === i + 1 }
function offered(i: number): boolean { return props.state.offered.includes(i + 1) }
function toggleCargo(i: number) { api.cargo(selected(i) ? 0 : i + 1) }
</script>

<template>
  <div class="cargo-bar">
    <!-- Setup: JOIN colors -->
    <template v-if="!inGame">
      <button
        v-for="i in 4" :key="'c' + i"
        class="slot join"
        :class="{ on: joined(i - 1) }"
        :style="{ '--c': COLCSS[i - 1] }"
        @click="toggleJoin(i - 1)"
      >
        <span class="title">{{ COLNAME[i - 1] }}</span>
        <span class="sub">{{ joined(i - 1) ? 'beigetreten' : 'beitreten' }}</span>
        <span v-if="joined(i - 1)" class="mark" />
      </button>
    </template>

    <!-- In-game: cargo selectors -->
    <template v-else>
      <button
        v-for="i in 4" :key="'g' + i"
        class="slot cargo"
        :class="{ on: selected(i - 1), offered: offered(i - 1) }"
        :style="{ '--c': CARGOCSS[i - 1] }"
        @click="toggleCargo(i - 1)"
      >
        <span class="title">{{ CARGONAME[i - 1] }}</span>
        <span class="sub">{{ offered(i - 1) ? (selected(i - 1) ? 'geladen' : 'angeboten') : '–' }}</span>
        <span v-if="selected(i - 1)" class="mark" />
      </button>
    </template>
  </div>
</template>

<style scoped>
.cargo-bar { display: grid; grid-template-columns: repeat(4, 1fr); gap: 12px; }
.slot {
  position: relative; display: flex; flex-direction: column; gap: 4px;
  padding: 14px 16px; min-height: 74px; text-align: left;
  border: 2px solid var(--line); border-radius: 12px;
  background: color-mix(in srgb, var(--c) 16%, #0e1a2b);
}
.slot.on { background: color-mix(in srgb, var(--c) 60%, #0e1a2b); border-color: var(--c); }
.slot.offered { border-color: var(--gold); }
.title { font-weight: 700; font-size: 16px; }
.sub { font-size: 12px; color: var(--muted); }
.slot.offered .sub { color: var(--gold); }
.mark { position: absolute; top: 12px; right: 12px; width: 12px; height: 12px; border-radius: 50%; background: #fff; }
</style>
