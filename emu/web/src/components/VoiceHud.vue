<script setup lang="ts">
import { computed } from 'vue'
import type { GameState } from '../types'

const props = defineProps<{ state: GameState }>()
const voice = computed(() => props.state.last_voice_key || '(Stille)')
const seconds = computed(() => (props.state.virtual_ms / 1000).toFixed(1))
</script>

<template>
  <div class="hud">
    <span class="speaking">🗣 {{ voice }}</span>
    <span class="time">t = {{ seconds }} s</span>
  </div>
</template>

<style scoped>
.hud {
  display: flex; align-items: center; justify-content: space-between;
  background: #0e1a2b; border: 1px solid var(--line); border-radius: 10px;
  padding: 10px 14px; font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
}
.speaking { color: var(--gold); }
.time { color: var(--muted); font-size: 13px; }
</style>
