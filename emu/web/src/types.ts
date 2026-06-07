// Wire types mirroring the backend g_emu state snapshot (see emu/src/server.c).

export interface Figure {
  f: number        // board field index (14..17 = registration colors)
  present: boolean
  color: number    // 1..4
}

export interface GameState {
  crashed?: boolean   // guest took an unrecoverable fault (quest/news bug); lobby ends
  in_game: boolean
  in_battle: boolean
  active_color: number   // 1..4, 0 = none
  money: number
  eq_cannons: number
  eq_sailors: number
  eq_sails: number
  eq_cargo: number
  offered: [number, number] // cargo types (1..4) the active port offers; 0 = unknown
  sel_cargo: number         // 0..4
  sel_dest: number          // -1..7
  figures: Figure[]
  last_voice_key: string
  virtual_ms: number
  pirate_idx: number
  pirate_cannons: number
  pirate_sailors: number
  pirate_sails: number
  pvp_battle: boolean    // true during a player-vs-player capture battle
  enemy_color: number    // PvP opponent's color 1..4 (0 = NPC/none)
}

export interface LobbyInfo {
  id: number
  name: string
  players: number
}
