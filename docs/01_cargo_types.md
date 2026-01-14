# Cargo Types (Waren)

## Overview

The game features 4 cargo types that can be traded between ports:

| ID | Name (DE) | Name (EN) | Audio File |
|----|-----------|-----------|------------|
| 0 | Getreide | Grain/Wheat | p0050.wav |
| 1 | Holz | Wood/Planks | p0051.wav |
| 2 | Tabak | Tobacco | p0052.wav |
| 3 | Rum | Rum | p0053.wav |

## Port Demand System

Each port has demand for specific cargo types. When a port has high demand for a cargo type, selling that cargo there yields higher profit.

### High Demand Voice Lines
- `high_demand_grain` = p0138.wav + p0050.wav
- `high_demand_wood` = p0138.wav + p0051.wav
- `high_demand_tobacco` = p0138.wav + p0052.wav
- `high_demand_rum` = p0138.wav + p0053.wav

### Wrong Cargo Voice Lines
When delivering wrong cargo to a port:
- `wrong_goods_grain` = p0130.wav + p0225.wav + p0131.wav
- `wrong_goods_wood` = p0130.wav + p0226.wav + p0131.wav
- `wrong_goods_tobacco` = p0130.wav + p0227.wav + p0131.wav
- `wrong_goods_rum` = p0130.wav + p0228.wav + p0131.wav

### No Goods at Port
- `no_goods_at_port` = p0211.wav

## Officer Cargo Voice Lines

Each of the 4 officers (of_1 through of_4) has specific voice lines for cargo:

### First Cargo Announcement
| Officer | Grain | Wood | Tobacco | Rum |
|---------|-------|------|---------|-----|
| Officer 1 | of1corn1.wav | of1wd1.wav | of1tbk1.wav | of1rum1.wav |
| Officer 2 | of2corn1.wav | of2wd1.wav | of2tbk1.wav | of2rum1.wav |
| Officer 3 | of3corn1.wav | of3wd1.wav | of3tbk1.wav | of3rum1.wav |
| Officer 4 | of4corn1.wav | of4wd1.wav | of4tbk1.wav | of4rum1.wav |

### Last Cargo Announcement
| Officer | Grain | Wood | Tobacco | Rum |
|---------|-------|------|---------|-----|
| Officer 1 | of1corn2.wav | of1wd2.wav | of1tbk2.wav | of1rum2.wav |
| Officer 2 | of2corn2.wav | of2wd2.wav | of2tbk2.wav | of2rum2.wav |
| Officer 3 | of3corn2.wav | of3wd2.wav | of3tbk2.wav | of3rum2.wav |
| Officer 4 | of4corn2.wav | of4wd2.wav | of4tbk2.wav | of4rum2.wav |

## Cargo Rewards

### Standard Reward Voice Lines
- `of_1_goods_reward` through `of_4_goods_reward` - Full reward announcement
- `of_1_goods_reward_sh` through `of_4_goods_reward_sh` - Short reward announcement
- `of_1_goods_reward_profi` through `of_4_goods_reward_profi` - Profi mode reward

### Reward Info Voice Lines (with tutorial hints)
- `of_1_goods_reward_info` through `of_4_goods_reward_info`
- `of_1_goods_reward_info_sh` through `of_4_goods_reward_info_sh`
- `of_1_goods_reward_info_2` through `of_4_goods_reward_info_2`

### No Reward
- `of_1_goods_no_reward` through `of_4_goods_no_reward`

## Info Voice Lines (Tutorial)

- `info_goods_grain` = p0423.wav + p0430.wav - Explains grain trading
- `info_goods_tobacco` = p0423.wav + p0431.wav - Explains tobacco trading
- `info_goods_rum` = p0423.wav + p0432.wav - Explains rum trading

## Cargo Effect on Pirates

**TODO: Needs further investigation from binary analysis**

The cargo you carry may affect:
- Pirate encounter probability
- Pirate aggressiveness/strength
- Loot dropped by pirates

## Cargo Effect on Profit

Profit from selling cargo depends on:
1. **Base cargo value** - Each cargo type has a base price
2. **Port demand** - High demand = higher prices
3. **Player rank/honor** - Higher rank may get better prices
4. **Negotiation (Feilschen)** - Successful haggling increases profit

### Cargo Storage in Player Structure
Based on code analysis:
- `a1[37]` - Related to cannons/combat (used in cargo combat calculations)
- `a1[38]` - Related to sailors/combat (used in cargo combat calculations)

**TODO: Find exact player structure offsets for cargo inventory**

## Code References

- Cargo ID to string mapping: `app.elf.c:6491-6500`
- Cargo reward calculation: `app.elf.c:9144-9394`
- High demand check: `files_de.txt:437-440`
