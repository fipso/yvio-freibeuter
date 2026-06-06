// Board labels + colors, mirroring emu/src/ui.c.

export const PORTS = [
  'Camarco', 'Spring Point', 'Puerto Plata', 'San Juan',
  'Kingstown', 'Curacao', 'Cartagena', 'Prinzapolca',
]

export const COLNAME = ['BLAU', 'GRÜN', 'ROT', 'GELB']
export const COLCSS  = ['#3c78e6', '#32b45a', '#dc4646', '#e6c83c']

export const CARGONAME = ['Getreide', 'Holz', 'Tabak', 'Rum']
export const CARGOCSS  = ['#dcbe5a', '#966e3c', '#789646', '#aa463c']

// Registration JOIN colors (1..4 = blue/green/red/yellow) -> RFID figure fields.
export const REG_FIELD = [17, 16, 15, 14]

export const BATTLE_BUTTONS = [
  { name: 'KANONEN', label: 'KANONEN', hint: 'feuern' },
  { name: 'ENTERN',  label: 'ENTERN',  hint: 'entern' },
  { name: 'SEGELN',  label: 'SEGELN',  hint: 'fliehen' },
]
