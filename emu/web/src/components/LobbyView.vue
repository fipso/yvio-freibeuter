<script setup lang="ts">
import { ref } from 'vue'
import { store, api } from '../lib/socket'

const name = ref('')

function create() {
  const n = name.value.trim() || 'Crew'
  api.createLobby(n)
  name.value = ''
}
</script>

<template>
  <div class="lobby">
    <div class="card">
      <div class="row head">
        <h2>Spiele</h2>
        <button class="ghost" @click="api.listLobbies()">aktualisieren</button>
      </div>

      <p v-if="store.lobbies.length === 0" class="empty">
        Keine laufenden Spiele. Erstelle eines!
      </p>

      <ul v-else class="list">
        <li v-for="l in store.lobbies" :key="l.id">
          <div class="meta">
            <span class="lname">{{ l.name }}</span>
            <span class="players">{{ l.players }} Spieler</span>
          </div>
          <button class="join" @click="api.joinLobby(l.id)">beitreten</button>
        </li>
      </ul>

      <div class="row create">
        <input v-model="name" placeholder="Name des Spiels" @keyup.enter="create" />
        <button class="primary" @click="create">Neues Spiel</button>
      </div>

      <p v-if="store.error" class="err">{{ store.error }}</p>
    </div>
  </div>
</template>

<style scoped>
.lobby { flex: 1; display: grid; place-items: center; padding: 24px; }
.card {
  width: min(560px, 92vw);
  background: var(--panel); border: 1px solid var(--line); border-radius: 14px;
  padding: 20px; box-shadow: 0 12px 40px rgba(0,0,0,.35);
}
.row { display: flex; align-items: center; gap: 10px; }
.head { justify-content: space-between; margin-bottom: 14px; }
.head h2 { color: var(--gold); font-size: 18px; }
.empty { color: var(--muted); padding: 18px 4px; }
.list { list-style: none; margin: 0 0 16px; padding: 0; display: flex; flex-direction: column; gap: 8px; }
.list li {
  display: flex; align-items: center; justify-content: space-between;
  background: var(--panel-2); border: 1px solid var(--line); border-radius: 10px; padding: 10px 14px;
}
.meta { display: flex; flex-direction: column; }
.lname { font-weight: 600; }
.players { color: var(--muted); font-size: 13px; }
.create { margin-top: 6px; }
.create input { flex: 1; }
.primary { background: var(--accent); border-color: var(--accent); color: #06101f; font-weight: 700; padding: 9px 16px; }
.join { background: #2a4a36; border-color: #3c7a52; padding: 8px 14px; }
.ghost { background: transparent; padding: 6px 12px; color: var(--muted); }
.err { color: #ff9a9a; margin: 10px 0 0; }
</style>
