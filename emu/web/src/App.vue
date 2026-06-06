<script setup lang="ts">
import { onMounted } from 'vue'
import { store, connect } from './lib/socket'
import LobbyView from './components/LobbyView.vue'
import GameView from './components/GameView.vue'

onMounted(connect)
</script>

<template>
  <div class="app">
    <header class="topbar">
      <h1>Freibeuter der Karibik</h1>
      <div class="status">
        <span class="dot" :class="store.connected ? 'on' : 'off'" />
        {{ store.connected ? 'verbunden' : 'getrennt …' }}
      </div>
    </header>

    <main class="main">
      <LobbyView v-if="store.currentLobby === null" />
      <GameView v-else />
    </main>
  </div>
</template>

<style scoped>
.app { display: flex; flex-direction: column; min-height: 100%; }
.topbar {
  display: flex; align-items: center; justify-content: space-between;
  padding: 12px 20px; border-bottom: 1px solid var(--line);
  background: linear-gradient(180deg, #16243a, #0e1828);
}
.topbar h1 { font-size: 20px; letter-spacing: .5px; color: var(--gold); }
.status { display: flex; align-items: center; gap: 8px; color: var(--muted); font-size: 14px; }
.dot { width: 10px; height: 10px; border-radius: 50%; }
.dot.on { background: #46d27a; box-shadow: 0 0 8px #46d27a; }
.dot.off { background: #d25a5a; }
.main { flex: 1; display: flex; }
</style>
