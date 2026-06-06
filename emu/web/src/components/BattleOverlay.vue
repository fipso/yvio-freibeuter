<script setup lang="ts">
import { computed } from 'vue'
import type { GameState } from '../types'
import { COLNAME, COLCSS, BATTLE_BUTTONS } from '../lib/consts'
import { api } from '../lib/socket'

const props = defineProps<{ state: GameState }>()
const c = computed(() => props.state.active_color)
const colName = computed(() => (c.value >= 1 && c.value <= 4 ? COLNAME[c.value - 1] : '–'))
const colCss = computed(() => (c.value >= 1 && c.value <= 4 ? COLCSS[c.value - 1] : '#aab'))
</script>

<template>
  <div class="overlay">
    <div class="dialog">
      <h2>SEEGEFECHT</h2>

      <div class="sides">
        <div class="side enemy">
          <h3>Feindliches Piratenschiff (#{{ state.pirate_idx }})</h3>
          <div class="row">
            <span>Kanonen {{ state.pirate_cannons }}</span>
            <span>Matrosen {{ state.pirate_sailors }}</span>
            <span>Segel {{ state.pirate_sails }}</span>
          </div>
        </div>
        <div class="vs">⚔</div>
        <div class="side me" :style="{ borderColor: colCss }">
          <h3 :style="{ color: colCss }">Du ({{ colName }})</h3>
          <div class="row">
            <span>Kanonen {{ state.eq_cannons }}</span>
            <span>Matrosen {{ state.eq_sailors }}</span>
            <span>Segel {{ state.eq_sails }}</span>
          </div>
        </div>
      </div>

      <p class="prompt">Wähle ein Manöver:</p>
      <div class="moves">
        <button v-for="b in BATTLE_BUTTONS" :key="b.name" class="move" @click="api.button(b.name)">
          <span class="ml">{{ b.label }}</span>
          <span class="mh">{{ b.hint }}</span>
        </button>
      </div>
    </div>
  </div>
</template>

<style scoped>
.overlay {
  position: absolute; inset: 0; z-index: 5;
  background: rgba(4, 8, 16, .78);
  display: grid; place-items: center; padding: 20px;
}
.dialog {
  width: min(720px, 94vw);
  background: #14213a; border: 1px solid #34507c; border-radius: 16px;
  padding: 24px; box-shadow: 0 20px 60px rgba(0,0,0,.5);
}
.dialog h2 { color: #f0b45a; font-size: 26px; letter-spacing: 2px; margin-bottom: 18px; }
.sides { display: grid; grid-template-columns: 1fr auto 1fr; align-items: center; gap: 16px; }
.side { background: #0f1a2e; border: 2px solid #34507c; border-radius: 12px; padding: 14px; }
.side h3 { font-size: 15px; margin-bottom: 8px; }
.side.enemy h3 { color: #ef6a6a; }
.row { display: flex; gap: 14px; flex-wrap: wrap; color: var(--text); font-size: 14px; }
.vs { font-size: 28px; opacity: .8; }
.prompt { color: var(--gold); margin: 20px 0 10px; }
.moves { display: grid; grid-template-columns: repeat(3, 1fr); gap: 12px; }
.move {
  display: flex; flex-direction: column; align-items: center; gap: 2px;
  padding: 16px; border-radius: 12px; background: #2c2848; border-color: #5a5a9a;
}
.move .ml { font-weight: 700; font-size: 16px; }
.move .mh { font-size: 12px; color: var(--muted); }
</style>
