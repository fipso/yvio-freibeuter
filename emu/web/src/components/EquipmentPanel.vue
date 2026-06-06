<script setup lang="ts">
import { computed } from 'vue'
import type { GameState } from '../types'
import { COLNAME, COLCSS } from '../lib/consts'

const props = defineProps<{ state: GameState }>()
const c = computed(() => props.state.active_color)
const colName = computed(() => (c.value >= 1 && c.value <= 4 ? COLNAME[c.value - 1] : '–'))
const colCss = computed(() => (c.value >= 1 && c.value <= 4 ? COLCSS[c.value - 1] : '#aab'))

const stats = computed(() => [
  { label: 'Kanonen', value: props.state.eq_cannons },
  { label: 'Matrosen', value: props.state.eq_sailors },
  { label: 'Segel', value: props.state.eq_sails },
  { label: 'Laderaum', value: props.state.eq_cargo },
  { label: 'Dukaten', value: props.state.money },
])
</script>

<template>
  <div class="equip" :style="{ borderColor: colCss }">
    <div class="player" :style="{ color: colCss }">
      <span class="chip" :style="{ background: colCss }" />
      Spieler {{ colName }}
    </div>
    <div class="stats">
      <div v-for="s in stats" :key="s.label" class="stat">
        <span class="v">{{ s.value }}</span>
        <span class="l">{{ s.label }}</span>
      </div>
    </div>
  </div>
</template>

<style scoped>
.equip {
  display: flex; align-items: center; gap: 22px; flex-wrap: wrap;
  background: var(--panel); border: 1px solid var(--line); border-left-width: 4px;
  border-radius: 12px; padding: 12px 18px;
}
.player { display: flex; align-items: center; gap: 8px; font-weight: 700; font-size: 16px; }
.chip { width: 14px; height: 14px; border-radius: 4px; }
.stats { display: flex; gap: 26px; flex-wrap: wrap; }
.stat { display: flex; flex-direction: column; align-items: center; min-width: 56px; }
.stat .v { font-size: 20px; font-weight: 700; }
.stat .l { font-size: 12px; color: var(--muted); }
</style>
