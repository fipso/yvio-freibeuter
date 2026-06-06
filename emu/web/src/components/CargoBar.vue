<script setup lang="ts">
import type { GameState } from '../types'
import { COLNAME, COLCSS, REG_FIELD } from '../lib/consts'
import { api } from '../lib/socket'

const props = defineProps<{ state: GameState }>()

// Setup only: is the JOIN figure for color (i+1) present? (figures lists present ones.)
function joined(i: number): boolean {
  const field = REG_FIELD[i]
  return props.state.figures.some(f => f.f === field && f.present)
}
function toggleJoin(i: number) { api.register(i + 1, !joined(i)) }
</script>

<template>
  <!-- Setup phase only: pick a player color to join. In-game cargo selection
       now lives on the interactive board (see BoardMap.vue). -->
  <div v-if="!state.in_game" class="cargo-bar">
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
.title { font-weight: 700; font-size: 16px; }
.sub { font-size: 12px; color: var(--muted); }
.mark { position: absolute; top: 12px; right: 12px; width: 12px; height: 12px; border-radius: 50%; background: #fff; }
</style>
