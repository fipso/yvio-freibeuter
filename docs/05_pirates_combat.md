# Pirates and Combat System

## Pirate Types

Pirates can appear in various contexts:
1. **Port Occupation** - Pirates occupying ports that need to be liberated
2. **Quest Pirates** - Specific pirates tied to quests
3. **Random Encounters** - Pirates encountered during travel

## Combat Types

### 1. Cannon Combat (Kanonenkampf / Firing)
Uses the player's cannon equipment.

| Officer | Attack | Win | Lose |
|---------|--------|-----|------|
| Officer 1 | of1btcn.wav | of1cnwn.wav | of1cnls.wav |
| Officer 2 | of2btcn.wav | of2cnwn.wav | of2cnls.wav |
| Officer 3 | of3btcn.wav | of3cnwn.wav | of3cnls.wav |
| Officer 4 | of4btcn.wav | of4cnwn.wav | of4cnls.wav |

### 2. Boarding Combat (Enterkampf / Boarding)
Uses the player's sailor equipment.

| Officer | Attack | Win | Lose |
|---------|--------|-----|------|
| Officer 1 | of1btent.wav | of1entls.wav | of1entwn.wav |
| Officer 2 | of2btent.wav | of2entls.wav | of2entwn.wav |
| Officer 3 | of3btent.wav | of3entls.wav | of3entwn.wav |
| Officer 4 | of4btent.wav | of4entls.wav | of4entwn.wav |

**Note**: The "won"/"lost" file names appear swapped (entls=win, entwn=lose) - verify in game.

## PvP Combat

Player vs Player combat has separate voice lines:

### Firing PvP
| Officer | Win | Lose |
|---------|-----|------|
| Officer 1 | of1pvp1.wav | of1pvp2.wav |
| Officer 2 | of2pvp1.wav | of2pvp2.wav |
| Officer 3 | of3pvp1.wav | of3pvp2.wav |
| Officer 4 | of4pvp1.wav | of4pvp2.wav |

### Boarding PvP
| Officer | Win | Lose |
|---------|-----|------|
| Officer 1 | of1pvp1.wav | of1pvp3.wav |
| Officer 2 | of2pvp1.wav | of2pvp3.wav |
| Officer 3 | of3pvp1.wav | of3pvp3.wav |
| Officer 4 | of4pvp1.wav | of4pvp3.wav |

## Combat Experience

- `info_boarding_credit` = p0423.wav + p0812.wav - Boarding experience gained
- `info_firing_credit` = p0423.wav + p0813.wav - Firing experience gained

## Battle Voice Lines

### Standard Battle
- `of_X_battle` = %ship + %pirate + #of_X_play_maneuver
- `of_X_battle_sh` = %ship + %pirate + #of_X_play_maneuver_sh (short version)
- `of_X_battle_info` = ... + p0423.wav + p0434.wav + p0479.wav (with tutorial)

### Quest Pirate Battle
- `of_X_quest_pirate_battle` = %pirate + ofX_016.wav + #of_X_play_maneuver_sh

### Multiquest Pirate Battle
- `of_X_multiquest_pirate_battle` = %pirate + #of_X_play_maneuver_sh

### VIP Quest Battle
- `of_X_quest_vip_battle` = %pirate + #of_X_play_maneuver_sh

## Pirate Quest Progression

| State | Voice Line | Audio |
|-------|------------|-------|
| quest_pirate_0 | Start | p0163.wav + %port + p0164.wav + p0192.wav |
| quest_pirate_1 | Reward | p0193.wav + ~z + p0194.wav + p0195.wav + p0196.wav |
| quest_pirate_2 | Completed | p0170.wav |
| quest_pirate_next | Next target | p0168.wav + %port + p0175.wav |
| quest_miss_pirate | Failed | p0207.wav + %port + p0208.wav |

### Pirate Introduction
- `pirate_intro_1` = p0165.wav
- `pirate_intro_2` = p0169.wav

## Port Occupation

### Multiquest Pirate Ports
Pirates can occupy multiple ports simultaneously:

| Count | Voice Line |
|-------|------------|
| 1 port | p0610.wav + %port |
| 2 ports | p0610.wav + %port1 + p0294.wav + %port2 |
| 3 ports | p0610.wav + %port1 + %port2 + p0294.wav + %port3 |
| 4 ports | p0610.wav + %port1 + %port2 + %port3 + p0294.wav + %port4 |

Info variants add tutorial hints (p0423.wav + p0435.wav/p0421.wav/p0710.wav).

## Combat Mechanics (from Notes.txt v1.1)

### Kaperfahrt (Privateering)
- Loot calculation slightly changed

### Unterwegs (Travel Combat)
- Win with sailors = always reputation increase
- Win with cannons = never reputation increase
- Random factor reduced when significantly stronger

### Hintenliegende Spieler (Trailing Players)
- Players behind get slightly easier fights (balancing element)

### Port Liberation
- When a player loses against an occupying pirate, the pirate is weakened
- Makes it easier to jointly liberate a port

## Combat Calculation

From code analysis (`app.elf.c:17738-17820`):

### Combat Resolution
```
player_strength = equipment_points * multiplier
enemy_strength = enemy_stats[2] or enemy_stats[3]

if (player_strength <= enemy_threshold):
    roll_modifier = 7  # harder
else:
    roll_modifier = 15  # easier

random_roll = sub_EC54(roll_modifier)
combat_result = complex_calculation(player_strength, random_roll)

if (player_value > combat_result ||
    (player_value == combat_result && random_coin_flip)):
    VICTORY
else:
    DEFEAT
```

### Combat Outcomes
- **Victory**: Gain loot, experience, possibly reputation
- **Defeat**: Lose some resources, journey may continue with penalty

### Defeat Gold Penalty
From earlier analysis:
- Route types 1,3,5,7,9: Lose 100 gold from potential reward
- Route types 2,4,6,8,10: Lose 200 gold from potential reward

## Pirate Strength Factors

**TODO: Needs further binary analysis**

Pirate strength appears to be influenced by:
1. Game difficulty (beginner/normal/profi)
2. Current game time/progress
3. Port location
4. Quest type (quest pirates vs random)
5. Number of occupied ports

## Code References

- Combat resolution: `app.elf.c:17738-17820`
- Pirate battle initiation: `app.elf.c:3439-3484`
- Port occupation: `app.elf.c:7625-9021`
- Combat win/lose handling: `app.elf.c:13572-13620`
