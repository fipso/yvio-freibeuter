# Ship Upgrades (Ausrüstung)

## Overview

The game features 5 types of ship equipment, each available in two levels:

| Equipment Type | Level 1 (sma) | Level 2 (big) | Effect |
|---------------|---------------|---------------|--------|
| Sails (Segel) | 1 point | 2 points | Affects travel speed |
| Cannons (Kanonen) | 1 point | 2 points | Used in firing combat |
| Sailors (Matrosen) | 1 point | 2 points | Used in boarding combat |
| Cargo (Ladung) | 1 point | 2 points | Cargo capacity |
| Residence (Wohnsitz) | 1 point | 2 points | Home port benefits |

## Player Structure Storage

Equipment is stored as byte values in the player structure:

| Offset | Equipment |
|--------|-----------|
| a1[25] | Sail Level 1 count |
| a1[26] | Sail Level 2 count |
| a1[27] | Sailor Level 1 count |
| a1[28] | Sailor Level 2 count |
| a1[29] | Cannon Level 1 count |
| a1[30] | Cannon Level 2 count |
| a1[31] | Cargo Level 1 count |
| a1[32] | Cargo Level 2 count |
| a1[33] | Residence Level 1 count |
| a1[34] | Residence Level 2 count |
| a1[35] | Rank/Level (affects bonuses) |

## Equipment Point System

Total equipment points = (Level 1 count × 1) + (Level 2 count × 2)

Route type requirements (from `app.elf.c:12915-12918`, `13412-13415`):
- **Route types 1-2**: Require total sail points > 2
- **Route types 3-4**: Require total sailor points > 2
- **Route types 5-6**: Require total cannon points > 2
- **Route types 7-8**: Require total cargo points > 2

## Equipment Voice Lines

### Equipment Names
| Equipment | Level 1 Audio | Level 2 Audio |
|-----------|--------------|---------------|
| Sail | p0275.wav (sma_sail) | p0276.wav (big_sail) |
| Cannon | p0277.wav (sma_cannon) | p0278.wav (big_cannon) |
| Sailor | p0279.wav (sma_sailor) | p0280.wav (big_sailor) |
| Cargo | p0281.wav (sma_cargo) | p0282.wav (big_cargo) |
| Residence | sma_residence | big_residence |

### Full Equipment Announcements
When you already have maximum of an equipment type:

| Equipment | Level 1 | Level 2 |
|-----------|---------|---------|
| Sail | p0261.wav + p0018.wav + p0259.wav | p0261.wav + p0019.wav + p0259.wav |
| Cannon | p0261.wav + p0020.wav + p0259.wav | p0261.wav + p0021.wav + p0259.wav |
| Sailor | p0814.wav | p0814.wav |
| Cargo | p0261.wav + p0024.wav + p0259.wav | p0261.wav + p0025.wav + p0259.wav |
| Residence | p0815.wav | p0815.wav |

### No More Equipment
- `no_more_equipment` = p0212.wav

## Merchant Offers (5 Merchants)

Each of the 5 merchants (m_1 through m_5) has unique voice lines:

### Long Offer Format (first time)
- `m_X_equip_offer_long_sma` - Small equipment offer: hX_001.wav + hX_002.wav + ~z + hX_003.wav + p0095.wav
- `m_X_equip_offer_long_big` - Big equipment offer: hX_001.wav + hX_006.wav + ~z + hX_007.wav + p0095.wav

### Short Offer Format (repeat)
- `m_X_equip_offer_short_sma` - Small: hX_004.wav + ~z + hX_005.wav + p0095.wav
- `m_X_equip_offer_short_big` - Big: hX_008.wav + ~z + hX_009.wav + p0095.wav

### Haggle Offer (Feilschen)
- `m_X_equip_offer_haggle` - After haggling: hX_011.wav + ~z + hX_012.wav

### Info Variants (Beginner Mode)
All offers have `_info` variants that add tutorial hints:
- Small: adds p0423.wav + p041X.wav
- Big: adds p0423.wav + p071X.wav

Where X corresponds to merchant number (p0414-p0418 for small, p0712-p0715 for big).

### Residence Offers
- `m_X_equip_offer_long_resi` - hX_001.wav + hX_016.wav + ~z + hX_019.wav + p0095.wav
- `m_X_equip_offer_short_resi` - hX_016.wav + ~z + hX_019.wav + p0095.wav

### Profi Mode Offers (Compact)
- `equip_offer_profi_cannon` = ~z + p0721.wav + p0480.wav + p0704.wav
- `equip_offer_profi_sail` = ~z + p0721.wav + p0613.wav + p0270.wav
- `equip_offer_profi_sailors` = ~z + p0721.wav + p0614.wav + p0269.wav
- `equip_offer_profi_cargo` = ~z + p0721.wav + p0613.wav + p0271.wav
- `equip_offer_profi_residence` = ~z + p0721.wav + p0743.wav

## Equipment Effects

### Sails - Travel Speed
From `Notes.txt`: "Segel geben weniger Bonus auf die Reisegeschwindigkeit" (Sails give less bonus to travel speed - reduced in v1.1)

**Effect**: Better sails reduce the number of turns required to travel between ports.

### Cannons - Firing Combat
Used in ship-to-ship cannon battles (Kanonenkampf).
- Total cannon points affect damage dealt
- Higher cannons increase win probability

### Sailors - Boarding Combat
Used in boarding actions (Entern).
- Total sailor points affect boarding success
- Higher sailors increase win probability

### Cargo - Capacity
Affects how much cargo you can carry.
- Each cargo point increases max cargo
- Level 2 cargo boxes hold more than Level 1

### Residence - Home Port
Provides benefits when visiting your home port.
- Calculation uses `word_400000FE` and `word_4000010E` multipliers
- Benefits may include: discount on purchases, bonus gold, etc.

## Combat Experience

From `files_de.txt`:
- `info_boarding_credit` = p0423.wav + p0812.wav - Boarding experience info
- `info_firing_credit` = p0423.wav + p0813.wav - Firing experience info

Combat experience contributes to player rank advancement.

## Equipment Assignment to Ports

Equipment pieces are distributed to specific ports:
- `equips_to_port_1` = p0213.wav + %equip1 + p0215.wav + %port1 + p0216.wav + %equip2 + p0215.wav + %port2 + p0446.wav
- `equips_to_port_2` = (same pattern)
- `equip_to_port_init` = p0213.wav + %equip + p0215.wav + %port + p0429.wav + p0446.wav

## Info Voice Lines

- `info_no_equip` = p0423.wav + p0420.wav - When no equipment available
- `info_price` = p0423.wav + p0419.wav - Price information

## Code References

- Equipment check: `app.elf.c:12915-12918`
- Equipment increment: `app.elf.c:13093-13142`, `13662-13711`
- Residence calculation: `app.elf.c:10056-10057`
- Rank bonus: `app.elf.c:10170` (uses sub_26464(30, rank))
