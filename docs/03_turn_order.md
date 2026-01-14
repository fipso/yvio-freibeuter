# Player Turn Order

## Player Colors

The game supports up to 4 players, each with a unique color:

| ID | Color Code | Color (EN) | Color (DE) | Audio File |
|----|------------|------------|------------|------------|
| 1 | bl | Blue | Blau | p0289.wav |
| 2 | gr | Green | Grün | p0292.wav |
| 3 | re | Red | Rot | p0290.wav |
| 4 | ye | Yellow | Gelb | p0291.wav |

## Player Count Announcements

- `players_1` = p0241.wav - 1 player announcement
- `players_2` = p0242.wav - 2 players announcement
- `players_3` = p0243.wav - 3 players announcement
- `players_4` = p0244.wav - 4 players announcement

## Game Setup

### Start Port Selection
- `set_ship_to_start_port` = p0240.wav - Instruction to place ship at starting port

### Ship Names (%ship variable)
The %ship placeholder is replaced with the player's ship name based on their color.

## Active Player Announcements

When a player's turn begins, the system announces based on game difficulty:

### Beginner Mode
- Active player: p0300.wav + %ship + p0221.wav (`player_active_beginner`)
- Inactive player: p0300.wav + %ship + p0220.wav (`player_inactive_beginner`)

### Normal Mode
- Active player: %ship + p0221.wav (`player_active_normal`)
- Inactive player: %ship + p0220.wav (`player_inactive`)

### Profi Mode
- Active player: %ship only (`player_active_profi`)
- Inactive player: %ship + p0220.wav (`player_inactive`)

## Turn Order Determination

Based on code analysis (`app.elf.c`), turn order appears to be determined by:

1. **Initial Setup**: Players place ships at starting ports
2. **Turn Sequence**: Clockwise or based on game state
3. **Decision System**: Uses `decision_new` (p0627.wav) for new decisions

### Tie-Breaking

When two values are equal, a coin flip (`sub_1E5F8(0, 1)`) determines the outcome:
- Common pattern: `if ( v37 > v36 || v37 == v36 && !sub_1E5F8(0, 1) )`
- Returns 0 or 1 randomly when tied

## Player State Variables

From code analysis:
- `byte_40002260` = Game difficulty mode (1=beginner, 2=normal, 3=profi)
- Player structure contains position, inventory, score, etc.

## Destination Arrival

When a player reaches their destination port:
- Full: `player_reached_destination_port` = p0300.wav + %ship + p0297.wav + %port + p0223.wav
- Short: `player_reached_destination_port_sh` = %ship + p0605.wav + %port

## Code References

- Player color handling: `app.elf.c:5900-5912`, `5940-5956`
- Active player announcement: `app.elf.c:5958-5970`
- Inactive player announcement: `app.elf.c:5916-5920`
- Player count announcement: `app.elf.c:5982-5990`
- Random tie-breaking: Uses `sub_1E5F8(0, 1)` throughout

## Notes

**TODO: Further investigation needed**
- Exact algorithm for determining initial player order
- How player order changes during game (if at all)
- Special rules for solo play vs multiplayer
