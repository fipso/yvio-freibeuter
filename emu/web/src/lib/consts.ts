// Board labels + colors, mirroring emu/src/ui.c.

export const PORTS = [
  'Camarco', 'Spring Point', 'Puerto Plata', 'San Juan',
  'Kingstown', 'Curacao', 'Cartagena', 'Prinzapolca',
]

export const COLNAME = ['BLAU', 'GRÜN', 'ROT', 'GELB']
export const COLCSS  = ['#3c78e6', '#32b45a', '#dc4646', '#e6c83c']

export const CARGONAME = ['Getreide', 'Holz', 'Tabak', 'Rum']
export const CARGOCSS  = ['#dcbe5a', '#966e3c', '#789646', '#aa463c']

// Interactive-map hotspots: center point of each clickable area, as a percentage
// of the (square) board box. Indices align with PORTS / CARGONAME above.
// These are eyeballed from public/board.webp and meant to be fine-tuned against
// the live render (see the DEBUG overlay in BoardMap.vue).
export interface Hotspot { x: number; y: number }

// PORT_HOTSPOTS[i] <-> PORTS[i] <-> api.port(i) / api.dest(i)
export const PORT_HOTSPOTS: Hotspot[] = [
  { x: 15,   y: 13.5 }, // 0 Camarco
  { x: 48.5, y: 5    }, // 1 Spring Point
  { x: 75,   y: 14   }, // 2 Puerto Plata
  { x: 89,   y: 37   }, // 3 San Juan
  { x: 84,   y: 61   }, // 4 Kingstown
  { x: 71,   y: 83   }, // 5 Curacao
  { x: 33,   y: 81   }, // 6 Cartagena
  { x: 19,   y: 65   }, // 7 Prinzapolca
]

// CARGO_HOTSPOTS[i] <-> CARGONAME[i] <-> cargo value (i + 1); left-edge symbols.
export const CARGO_HOTSPOTS: Hotspot[] = [
  { x: 6, y: 24 }, // 0 Getreide -> 1
  { x: 6, y: 34 }, // 1 Holz     -> 2
  { x: 6, y: 45 }, // 2 Tabak    -> 3
  { x: 6, y: 54 }, // 3 Rum      -> 4
]

// Registration JOIN colors (1..4 = blue/green/red/yellow) -> RFID figure fields.
export const REG_FIELD = [17, 16, 15, 14]

export const BATTLE_BUTTONS = [
  { name: 'KANONEN', label: 'KANONEN', hint: 'feuern' },
  { name: 'ENTERN',  label: 'ENTERN',  hint: 'entern' },
  { name: 'SEGELN',  label: 'SEGELN',  hint: 'fliehen' },
]
