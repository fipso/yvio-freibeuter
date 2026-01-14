# Difficulty Levels (Spielmodi)

## Overview

The game has three difficulty levels stored in `byte_40002260`:

| Value | Mode | German |
|-------|------|--------|
| 1 | Beginner | Anfänger |
| 2 | Normal | Normal |
| 3 | Profi | Profi |

## Game Mode Selection Voice Lines

- `game_modus_beg` = p0808.wav - Beginner mode selected
- `game_modus_nor` = p0809.wav - Normal mode selected
- `game_modus_pro` = p0810.wav - Profi mode selected

## Difficulty Effects

### Voice Line Verbosity

The difficulty level affects how verbose voice announcements are:

#### Player Active Announcement
| Mode | Voice Line |
|------|------------|
| Beginner | p0300.wav + %ship + p0221.wav (full) |
| Normal | %ship + p0221.wav (medium) |
| Profi | %ship only (minimal) |

#### Player Inactive Announcement
| Mode | Voice Line |
|------|------------|
| Beginner | p0300.wav + %ship + p0220.wav |
| Normal/Profi | %ship + p0220.wav |

### Info/Tutorial Hints

Many voice lines have `_info` variants that only play in Beginner mode:
- Equipment offers add tutorial explanation
- Battle announcements include hints
- Quest dialogues are more explanatory

### Profi Mode Shortcuts

Profi mode uses compact voice lines:
- `equip_offer_profi_cannon` = ~z + p0721.wav + p0480.wav + p0704.wav
- `equip_offer_profi_sail` = ~z + p0721.wav + p0613.wav + p0270.wav
- `equip_offer_profi_sailors` = ~z + p0721.wav + p0614.wav + p0269.wav
- `equip_offer_profi_cargo` = ~z + p0721.wav + p0613.wav + p0271.wav
- `equip_offer_profi_residence` = ~z + p0721.wav + p0743.wav

### Profi Mode Trading
- `buy_profi` = p0723.wav - Quick purchase confirmation
- `not_bought_profi` = p0722.wav - Quick decline

### Profi Mode Battle
- `of_X_battle_profi` = p0724.wav + #of_X_play_maneuver_sh

### Goods Reward Profi
- `of_X_goods_reward_profi` = ofXgw.wav + ~z + ofXdke.wav

## Probability Adjustments

From code analysis (`app.elf.c:14423-14425`):

```c
if ( byte_40002260 == 1 && sub_1E5F8(1, 100) <= 50    // Beginner: 50%
  || byte_40002260 == 2 && sub_1E5F8(1, 100) <= 30    // Normal: 30%
  || byte_40002260 == 3 && sub_1E5F8(1, 100) <= 10 )  // Profi: 10%
```

This affects certain random events - easier in Beginner mode.

| Mode | Probability |
|------|-------------|
| Beginner | 50% |
| Normal | 30% |
| Profi | 10% |

## Combat Difficulty

### Pirate Strength

From Notes.txt v1.1:
- "Bei einer Niederlage eines Spielers gegen einen besetzenden Pirat, wird dieser geschwächt"
- When a player loses against an occupying pirate, the pirate is weakened
- Makes joint port liberation easier

### Combat Adjustments

Code at `app.elf.c:14473, 14491`:
- Profi mode (byte_40002260 == 3) uses different combat calculation functions
- `sub_DB94` for Profi vs `sub_CE8C` for other modes

## Function Selection by Difficulty

```c
v80 = byte_40002260 == 3 ? sub_DB94(v2) : sub_CE8C(v2);
```

Profi mode uses a different calculation path, likely with:
- Less favorable random ranges
- Stronger enemies
- Fewer bonuses

## Tutorial System

Beginner mode includes extensive tutorial hints marked with `_info` suffix in voice line names.

### Info Tags in Voice Lines
The `??p0423.wav` file appears to be a standard "Info:" prefix used before tutorial explanations.

Common tutorial hint files:
- p0414.wav - p0418.wav: Equipment info
- p0419.wav - p0421.wav: Various hints
- p0426.wav - p0438.wav: Battle and reward info
- p0479.wav: Combat info

## Game Length

- `short_game` = p0407.wav + ~z + p0408.wav + p0446.wav
- `long_game` = p0407.wav + ~z + p0408.wav + p0446.wav

## Code References

- Difficulty mode storage: `byte_40002260`
- Mode selection: `app.elf.c:14136, 14220`
- Probability checks: `app.elf.c:14423-14425`
- Combat function selection: `app.elf.c:14714`
- Voice line selection: `app.elf.c:5958-5968, 24070-24079`
