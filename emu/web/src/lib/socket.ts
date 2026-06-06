// Reactive WebSocket store: one connection to the backend, shared game state,
// lobby list, and the input API. Free-for-all control — any client may send any
// input; the backend applies it to the shared g_emu for its lobby.
import { reactive } from 'vue'
import type { GameState, LobbyInfo } from '../types'
import { PcmPlayer } from './audio'

const WS_URL: string =
  (import.meta.env.VITE_WS_URL as string | undefined) || `ws://${location.hostname}:8080/`

export const player = new PcmPlayer()

export const store = reactive({
  connected: false,
  lobbies: [] as LobbyInfo[],
  currentLobby: null as number | null,
  state: null as GameState | null,
  error: '',
  audioOn: false,
})

let ws: WebSocket | null = null

function send(obj: unknown) {
  if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(obj))
}

export function connect() {
  if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) return
  ws = new WebSocket(WS_URL)
  ws.binaryType = 'arraybuffer'
  ws.onopen = () => { store.connected = true; api.listLobbies() }
  ws.onclose = () => {
    store.connected = false
    store.state = null
    setTimeout(connect, 1000) // auto-reconnect
  }
  ws.onerror = () => { /* onclose follows */ }
  ws.onmessage = (ev: MessageEvent) => {
    if (ev.data instanceof ArrayBuffer) { player.push(ev.data); return }
    const m = JSON.parse(ev.data as string)
    switch (m.t) {
      case 'lobbies': store.lobbies = m.items as LobbyInfo[]; break
      case 'joined':  store.currentLobby = m.id; store.error = ''; break
      case 'left':    store.currentLobby = null; store.state = null; break
      case 'error':   store.error = m.msg; break
      case 'state':   store.state = m as GameState; break
    }
  }
}

export const api = {
  listLobbies: () => send({ t: 'list_lobbies' }),
  createLobby: (name: string) => send({ t: 'create_lobby', name }),
  joinLobby: (id: number) => send({ t: 'join_lobby', id }),
  leaveLobby: () => { send({ t: 'leave_lobby' }); store.currentLobby = null; store.state = null },
  button: (name: string) => send({ t: 'button', name }),
  register: (color: number, present: boolean) => send({ t: 'register', color, present }),
  cargo: (value: number) => send({ t: 'cargo', value }),
  dest: (value: number) => send({ t: 'dest', value }),
  port: (value: number) => send({ t: 'port', value }),
  fast: (on: boolean) => send({ t: 'fast', on }),
}

export function toggleAudio() {
  store.audioOn = player.toggle()
}
