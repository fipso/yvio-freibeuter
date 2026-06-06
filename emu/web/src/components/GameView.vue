<script setup lang="ts">
import { computed, ref } from 'vue'
import { store, api, toggleAudio } from '../lib/socket'
import EquipmentPanel from './EquipmentPanel.vue'
import CargoBar from './CargoBar.vue'
import PortGrid from './PortGrid.vue'
import ConsoleButtons from './ConsoleButtons.vue'
import BattleOverlay from './BattleOverlay.vue'
import VoiceHud from './VoiceHud.vue'

const fast = ref(false)
const st = computed(() => store.state)

function leave() { api.leaveLobby() }
function toggleFast() { fast.value = !fast.value; api.fast(fast.value) }
function audio() { toggleAudio() }
</script>

<template>
  <div class="game">
    <div class="board-bg" />

    <div class="hud-top">
      <div class="left">
        <button class="ghost" @click="leave">← Lobby</button>
        <span class="lobby-id">Spiel #{{ store.currentLobby }}</span>
        <span class="phase" :class="st?.in_game ? 'play' : 'setup'">
          {{ st?.in_game ? 'Im Spiel' : 'Vorbereitung' }}
        </span>
      </div>
      <div class="right">
        <button class="toggle" :class="{ on: store.audioOn }" @click="audio">
          {{ store.audioOn ? '🔊 Ton an' : '🔈 Ton aus' }}
        </button>
        <button class="toggle" :class="{ on: fast }" @click="toggleFast" title="Intro überspringen">
          ⏩ {{ fast ? 'schnell' : 'normal' }}
        </button>
      </div>
    </div>

    <div v-if="!st" class="loading">lade Spielzustand …</div>

    <div v-else class="layout">
      <EquipmentPanel v-if="st.in_game" :state="st" />
      <CargoBar :state="st" />
      <PortGrid :state="st" />
      <ConsoleButtons :state="st" />
      <VoiceHud :state="st" />
      <p class="hint">
        <template v-if="st.in_game">
          Zug: lade EINE der hervorgehobenen Waren, dann wähle einen Zielhafen zum Segeln &amp; Verkaufen.
        </template>
        <template v-else>
          Vorbereitung: ÜBERSPRINGEN = Intro · OK = Anfängerspiel / NEIN = ablehnen → Normal/Profi ·
          klicke eine Farbe zum Beitreten · OK = starten.
        </template>
      </p>
    </div>

    <BattleOverlay v-if="st && st.in_battle && !st.crashed" :state="st" />

    <div v-if="st && st.crashed" class="crash-overlay">
      <div class="crash-box">
        <h2>Spiel abgestürzt</h2>
        <p>Bekannter Quest-/Nachrichten-Bug — das Spiel hat einen Lesefehler ausgelöst
           und wurde beendet.</p>
        <p v-if="store.error" class="emsg">{{ store.error }}</p>
        <button class="primary" @click="leave">Zurück zur Lobby</button>
      </div>
    </div>
  </div>
</template>

<style scoped>
.game { position: relative; flex: 1; display: flex; flex-direction: column; overflow: hidden; }
.board-bg {
  position: absolute; inset: 0;
  background-image: url('/board.webp');
  background-size: cover; background-position: center;
  opacity: .12; filter: saturate(1.1);
  pointer-events: none;
}
.hud-top {
  position: relative; z-index: 1;
  display: flex; align-items: center; justify-content: space-between;
  padding: 12px 18px; gap: 12px;
}
.left, .right { display: flex; align-items: center; gap: 10px; }
.lobby-id { color: var(--muted); font-size: 14px; }
.phase { font-size: 12px; padding: 3px 9px; border-radius: 999px; border: 1px solid var(--line); }
.phase.play { color: #9ff0bf; border-color: #2f6e48; background: #163524; }
.phase.setup { color: var(--gold); border-color: #5a4a20; background: #2a2412; }
.ghost { background: transparent; color: var(--muted); padding: 7px 12px; }
.toggle { padding: 7px 12px; }
.toggle.on { background: #284a36; border-color: #3c7a52; }
.loading { position: relative; z-index: 1; color: var(--muted); padding: 40px; text-align: center; }
.layout {
  position: relative; z-index: 1;
  flex: 1; display: flex; flex-direction: column; gap: 14px;
  padding: 8px 18px 22px; max-width: 1100px; width: 100%; margin: 0 auto;
}
.hint { color: var(--gold); font-size: 13px; opacity: .85; margin: 2px 0 0; }

.crash-overlay {
  position: absolute; inset: 0; z-index: 6;
  background: rgba(6, 4, 8, .82); display: grid; place-items: center; padding: 20px;
}
.crash-box {
  width: min(560px, 92vw); text-align: center;
  background: #2a1620; border: 1px solid #7a3450; border-radius: 16px;
  padding: 28px; box-shadow: 0 20px 60px rgba(0,0,0,.5);
}
.crash-box h2 { color: #ff7a7a; font-size: 24px; letter-spacing: 1px; margin-bottom: 12px; }
.crash-box p { color: var(--text); opacity: .9; margin: 0 0 14px; }
.crash-box .emsg { color: var(--gold); font-family: ui-monospace, monospace; font-size: 13px; }
.crash-box .primary { background: var(--accent); border-color: var(--accent); color: #06101f; font-weight: 700; padding: 10px 18px; }
</style>
