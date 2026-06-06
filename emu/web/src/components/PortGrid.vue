<script setup lang="ts">
import { computed } from 'vue'
import type { GameState } from '../types'
import { PORTS } from '../lib/consts'
import { api } from '../lib/socket'

const props = defineProps<{ state: GameState }>()
const inGame = computed(() => props.state.in_game)

function isDest(i: number): boolean { return props.state.sel_dest === i }
function click(i: number) {
  if (inGame.value) api.dest(isDest(i) ? -1 : i)
  else api.port(i) // setup/menu: momentary port press
}
</script>

<template>
  <div class="ports">
    <button
      v-for="(name, i) in PORTS" :key="i"
      class="port"
      :class="{ dest: isDest(i) }"
      @click="click(i)"
    >
      <span class="name">{{ name }}</span>
      <span class="sub">{{ inGame ? 'Zielhafen (hierher segeln)' : `Hafen ${i + 1}` }}</span>
    </button>
  </div>
</template>

<style scoped>
.ports { display: grid; grid-template-columns: repeat(4, 1fr); gap: 12px; }
.port {
  display: flex; flex-direction: column; gap: 4px; text-align: left;
  padding: 14px 16px; min-height: 70px;
  border: 2px solid #355a8c; border-radius: 12px;
  background: #1e2e44;
}
.port.dest { background: #c89628; border-color: var(--gold); color: #11141f; }
.name { font-weight: 700; font-size: 15px; }
.sub { font-size: 12px; color: var(--muted); }
.port.dest .sub { color: #3a2e10; }
</style>
