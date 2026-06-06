<script setup lang="ts">
import { computed } from 'vue'
import type { GameState } from '../types'
import { api } from '../lib/socket'

const props = defineProps<{ state: GameState }>()

const base = [
  { name: 'OK',     label: 'OK / JA',  kind: 'ok' },
  { name: 'NO',     label: 'NEIN',     kind: 'no' },
  { name: 'SKIP',   label: 'ÜBERSPR.', kind: 'neutral' },
  { name: 'REPEAT', label: 'WIEDERH.', kind: 'neutral' },
]
const trade = [
  { name: 'KAUFEN',    label: 'KAUFEN',    kind: 'buy' },
  { name: 'FEILSCHEN', label: 'FEILSCHEN', kind: 'buy' },
]
const buttons = computed(() => (props.state.in_game ? [...base, ...trade] : base))
</script>

<template>
  <div class="console" :style="{ gridTemplateColumns: `repeat(${buttons.length}, 1fr)` }">
    <button
      v-for="b in buttons" :key="b.name"
      class="cbtn" :class="b.kind"
      @click="api.button(b.name)"
    >
      {{ b.label }}
    </button>
  </div>
</template>

<style scoped>
.console { display: grid; gap: 12px; }
.cbtn { padding: 16px 10px; font-weight: 700; font-size: 15px; border-radius: 12px; }
.cbtn.ok { background: #2c5a38; border-color: #3f8a55; }
.cbtn.no { background: #5a2c2c; border-color: #8a3f3f; }
.cbtn.neutral { background: #3a3320; border-color: #6a5a30; }
.cbtn.buy { background: #20405a; border-color: #3f7aa0; }
</style>
