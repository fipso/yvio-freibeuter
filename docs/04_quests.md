# Quest System (Aufträge)

## Overview

The game features multiple quest types that provide gold rewards and contribute to player progression.

## Quest Categories

### 1. Letter Quests (Brief)
Deliver a letter to a destination port.

| State | Voice Line | Audio |
|-------|------------|-------|
| quest_letter_pt_0 | Start: go to %port | p0143.wav + %port + p0144.wav + p0005.wav |
| quest_letter_pt_1 | Completed | p0145.wav |
| quest_miss_letter | Failed/Missed | p0146.wav |

### 2. Passenger Quests (Passagier)
Transport a passenger to their destination.

| State | Voice Line | Audio |
|-------|------------|-------|
| quest_passenger_0 | Start: passenger to %port | p0148.wav + p0708.wav + %port + p0176.wav |
| quest_passenger_1 | Completed | p0149.wav |
| quest_miss_passenger | Failed | p0150.wav + %port + p0203.wav |

### 3. Treasure Hunt Quests (Schatzsuche)

#### Man Variant
| State | Voice Line | Audio |
|-------|------------|-------|
| quest_treasure_hunt_man_0 | Start | p0151.wav + p0152.wav + %port + p0153.wav + p0154.wav |
| quest_treasure_hunt_man_1 | Reward: ~z gold | p0155.wav + ~z + p0078.wav |
| quest_min_money_man | Not enough gold | p0151.wav + p0197.wav |

#### Brother Variant
| State | Voice Line | Audio |
|-------|------------|-------|
| quest_treasure_hunt_brother_0 | Start | p0157.wav + %port + p0158.wav + p0178.wav |
| quest_treasure_hunt_brother_1 | Continue | p0159.wav + p0179.wav + p0152.wav + %port + p0153.wav + p0154.wav |
| quest_treasure_hunt_brother_2 | Reward: ~z gold | p0181.wav + ~z + p0078.wav |
| quest_min_money_brother | Not enough gold | p0198.wav + p0199.wav |

#### Passenger Variant (4 stages)
| State | Voice Line | Audio |
|-------|------------|-------|
| quest_treasure_hunt_passenger_0 | Start | p0182.wav + p0183.wav + %port + p0184.wav + p0178.wav |
| quest_treasure_hunt_passenger_1 | Stage 2 | p0185.wav + p0186.wav + %port + p0295.wav |
| quest_treasure_hunt_passenger_2 | Stage 3 | p0188.wav + p0189.wav + %port + p0296.wav |
| quest_treasure_hunt_passenger_3 | Reward: ~z gold | p0191.wav + ~z + p0078.wav |
| quest_min_money_passenger_0 | Not enough gold | p0200.wav + p0199.wav |
| quest_min_money_passenger_1 | Not enough gold | p0201.wav + p0199.wav |

### 4. Pirate Quests (Piraten)
Hunt down pirates.

| State | Voice Line | Audio |
|-------|------------|-------|
| quest_pirate_0 | Start: pirate at %port | p0163.wav + %port + p0164.wav + p0192.wav |
| quest_pirate_1 | Reward announcement | p0193.wav + ~z + p0194.wav + p0195.wav + p0196.wav |
| quest_pirate_2 | Completed | p0170.wav |
| quest_pirate_next | Next pirate | p0168.wav + %port + p0175.wav |
| quest_miss_pirate | Failed | p0207.wav + %port + p0208.wav |
| quest_min_money_pirate_0 | Need money | p0004.wav |
| quest_min_money_pirate_1 | Need money | p0171.wav |

### 5. Mayor Quests (Bürgermeister)
Missions from the town mayor.

| State | Voice Line | Audio |
|-------|------------|-------|
| quest_mayor_pt_0 | Start | p0448.wav + %port + p0449.wav + p0005.wav |
| quest_mayor_pt_1 | Progress: ~z gold | p0450.wav + ~z + p0451.wav |
| quest_mayor_pt_2 | Reward | p0452.wav + p0453.wav + ~z + p0078.wav |
| quest_mayor_pt_3 | Alternative | p0454.wav |
| quest_mayor_pt_4 | Reward | p0455.wav + p0453.wav + ~z + p0078.wav |
| quest_miss_mayor | Failed | p0004.wav |

### 6. Woman Quests (Frau)
Quests involving a woman NPC.

| State | Voice Line | Audio |
|-------|------------|-------|
| quest_woman_pt_0 | Start | p0456.wav + %port + p0457.wav + p0005.wav |
| quest_woman_pt_1 | Progress | p0458.wav |
| quest_woman_pt_2 | Reward: ~z gold | p0458.wav + p0453.wav + ~z + p0078.wav |
| quest_miss_woman | Failed | p0004.wav |

**Reward Range**: 700-1000 gold (100 × random(7,10))

### 7. Smuggle Quests (Schmuggel)

#### Smuggle Type 1 (2 stages)
| State | Voice Line | Audio |
|-------|------------|-------|
| quest_smuggle_1_pt_0 | Start | p0631.wav + ~z1 + p0460.wav + p0632.wav + %port + p0461.wav + ~z2 + p0462.wav |
| quest_smuggle_1_pt_1 | Progress | p0464.wav |
| quest_smuggle_1_pt_2 | Reward | p0463.wav + p0453.wav + ~z + p0078.wav |
| quest_miss_smuggle_1 | Failed | p0630.wav |

#### Smuggle Type 2 (4 stages)
| State | Voice Line | Audio |
|-------|------------|-------|
| quest_smuggle_2_pt_0 | Start | p0631.wav + ~z1 + p0460.wav + p0632.wav + %port + p0461.wav + ~z2 + p0462.wav |
| quest_smuggle_2_pt_1 | Stage 2 | p0463.wav + p0465.wav + ~z1 + p0466.wav + %port + p0467.wav + ~z2 + p0468.wav |
| quest_smuggle_2_pt_2 | Stage 3 | p0464.wav |
| quest_smuggle_2_pt_3 | Reward | p0463.wav + p0453.wav + ~z + p0078.wav |
| quest_miss_smuggle_2 | Failed | p0630.wav |

#### Smuggle Type 3 (5 stages)
| State | Voice Line | Audio |
|-------|------------|-------|
| quest_smuggle_3_0 | Start | p0752.wav + ~z1 + p0753.wav + %port + p0461.wav + ~z2 + p0462.wav |
| quest_smuggle_3_1 | Stage 2 | p0463.wav + p0754.wav + ~z1 + p0078.wav + p0755.wav + %port + p0756.wav + p0757.wav + ~z2 + p0078.wav + p0758.wav |
| quest_smuggle_3_2 | Stage 3 | p0464.wav |
| quest_smuggle_3_3 | Reward | p0191.wav + ~z + p0078.wav |
| quest_smuggle_3_4 | Alternative | p0155.wav + p0759.wav |
| quest_miss_smuggle_3 | Failed | p0004.wav |

### 8. VIP Quests
Escort VIP to destination.

| State | Voice Line | Audio |
|-------|------------|-------|
| quest_vip_pt_0 | Start | p0469.wav + %port + p0470.wav + p0005.wav |
| quest_vip_pt_1 | Progress | p0709.wav |
| quest_vip_pt_2 | Reward variant 1 | p0474.wav + p0453.wav + ~z + p0078.wav |
| quest_vip_pt_3 | Reward variant 2 | p0471.wav + p0453.wav + ~z + p0078.wav |
| quest_miss_vip | Failed | p0004.wav |

### 9. Storm Quests (Sturm)
Weather-related missions.

| State | Voice Line | Audio |
|-------|------------|-------|
| quest_storm_pt_0 | Start | p0472.wav + %port + p0473.wav + p0005.wav |
| quest_storm_pt_1 | Progress | p0474.wav + p0475.wav |
| quest_storm_pt_2 | Completed | p0709.wav |
| quest_miss_storm | Failed | p0004.wav |

### 10. Mercenary Quests (Söldner)
Hire mercenaries to help.

From Notes.txt: "Söldner sind billiger geworden" (Mercenaries are cheaper in v1.1)

#### Mercenary 1
| State | Voice Line | Audio |
|-------|------------|-------|
| quest_merc_1_0 | Hire offer | p0777.wav + p0778.wav + p0737.wav + ~z + p0462.wav |
| quest_merc_1_1 | Hired | p0779.wav |

#### Mercenary 2
| State | Voice Line | Audio |
|-------|------------|-------|
| quest_merc_2_0 | Hire offer | p0780.wav + p0778.wav + p0737.wav + ~z + p0462.wav |
| quest_merc_2_1 | Hired | p0781.wav |

#### Mercenary 3
| State | Voice Line | Audio |
|-------|------------|-------|
| quest_merc_3_0 | Hire offer | p0782.wav + p0778.wav + p0737.wav + ~z + p0462.wav |
| quest_merc_3_1 | Hired | p0783.wav |

### 11. Hostage Quests (Geisel)
Rescue hostages.

| State | Voice Line | Audio |
|-------|------------|-------|
| quest_hostage_0 | Start | p0745.wav + %port + p0746.wav |
| quest_hostage_1 | Reward: ~z1 and ~z2 | p0747.wav + ~z1 + p0078.wav + p0079.wav + ~z2 + p0078.wav |
| quest_hostage_2 | Stage 2 | p0748.wav + %port + p0749.wav |
| quest_hostage_3 | Reward | p0750.wav + p0453.wav + ~z + p0078.wav |
| quest_hostage_4 | Alternative | p0751.wav |
| quest_miss_hostage | Failed | p0004.wav |

### 12. Treasure Quests (Schatz)
Find treasure.

| State | Voice Line | Audio |
|-------|------------|-------|
| quest_treasure_0 | Start | p0760.wav + p0755.wav + %port + p0761.wav + ~z + p0738.wav |
| quest_treasure_1 | Reward: dual | p0155.wav + ~z1 + p0078.wav + p0079.wav + ~z2 + p0078.wav |
| quest_treasure_2 | Two ports | p0155.wav + p0762.wav + %port1 + p0728.wav + %port2 |
| quest_treasure_3 | Reward | p0145.wav + p0079.wav + ~z + p0078.wav |
| quest_treasure_4 | Alternative | p0458.wav + p0803.wav + p0739.wav |
| quest_miss_treasure | Failed | p0156.wav |
| quest_miss_treasure_1 | Failed variant 1 | p0156.wav |
| quest_miss_treasure_2 | Failed variant 2 | p0146.wav |
| quest_miss_treasure_passenger | Failed passenger | p0202.wav + %port + p0203.wav |
| quest_miss_treasure_map | Failed map | p0204.wav + %port + p0205.wav + p0206.wav |

### 13. Help Quests (Hilfe)
Aid other ships/people.

| State | Voice Line | Audio |
|-------|------------|-------|
| quest_help_0 | Start | p0763.wav + %port + p0764.wav + ~z + p0078.wav + p0002.wav |
| quest_help_1 | Progress | p0765.wav + ~z + p0738.wav |
| quest_help_2 | Continue | p0709.wav |
| quest_help_3 | Completed | p0475.wav |
| quest_miss_help | Failed | p0004.wav |

### 14. Battle Quests (Schlacht)
Engage in naval battles.

| State | Voice Line | Audio |
|-------|------------|-------|
| quest_battle_0 | Start: two ports | p0768.wav + p0769.wav + %port1 + p0728.wav + %port2 + p0770.wav + p0771.wav |
| quest_battle_1 | At port | p0772.wav + %port + p0773.wav |
| quest_battle_2 | Progress | p0774.wav |
| quest_battle_3 | Reward | p0775.wav + p0737.wav + ~z + p0738.wav |
| quest_battle_4 | Alternative | p0776.wav + p0739.wav |
| quest_miss_battle_1 | Failed variant 1 | p0774.wav |
| quest_miss_battle_2 | Failed variant 2 | p0150.wav |

### 15. Amulet Quests (Amulett)
Find/deliver magical amulets.

| State | Voice Line | Audio |
|-------|------------|-------|
| quest_amulett_0 | Start | p0784.wav + p0737.wav + ~z + p0738.wav |
| quest_amulett_1 | Two ports | p0785.wav + %port1 + p0728.wav + %port2 + p0786.wav |
| quest_amulett_2 | Progress | p0787.wav |
| quest_amulett_3 | Continue | p0788.wav |
| quest_amulett_4 | Reward | p0789.wav + p0079.wav + ~z + p0078.wav |
| quest_miss_amulett | Failed | p0204.wav + %port + p0205.wav |

### 16. Special Quests

| Quest | Voice Line | Audio |
|-------|------------|-------|
| quest_cheat | Cheat reward | p0766.wav + p0331.wav + ~z + p0078.wav |
| quest_game | Game reward | p0767.wav + p0079.wav + ~z + p0078.wav |
| quest_miss_journey | Journey failed | p0204.wav + %port + p0205.wav |

## Multiquest System

### Storm Event
- `multiquest_storm` = p0324.wav + %port + p0326.wav

### Silver Quest
| State | Voice Line | Audio |
|-------|------------|-------|
| multiquest_silver | Announcement | p0727.wav + %port_1 + %port_2 + p0728.wav + %port_3 + p0729.wav |
| multiquest_silver_battle | Battle info | p0730.wav + p0423.wav + p0731.wav |
| multiquest_silver_lost | Lost | p0733.wav + p0734.wav |
| multiquest_silver_success | Won: ~z gold | p0732.wav + p0079.wav + ~z + p0078.wav |
| multiquest_silver_cannon | Cannon reward | p0735.wav + ~z1 + p0078.wav + p0736.wav + p0739.wav + p0737.wav + ~z2 + p0738.wav |
| multiquest_silver_sailors | Sailor reward | p0740.wav + ~z1 + p0741.wav + p0739.wav + p0737.wav + ~z2 + p0738.wav |
| multiquest_silver_bought | Purchased | p0723.wav |

### Pirate Ports
| State | Voice Line |
|-------|------------|
| multiquest_pirate_port | Single port: p0610.wav + %port |
| multiquest_pirate_ports_2 | Two ports: p0610.wav + %port1 + p0294.wav + %port2 |
| multiquest_pirate_ports_3 | Three ports |
| multiquest_pirate_ports_4 | Four ports |

Info variants add tutorial hints (p0423.wav + p0435.wav/p0421.wav/p0710.wav).

### Multiquest Timing
- `multiquest_long` = rost.wav + p0802.wav + %month
- `multiquest_short` = rost.wav
- `multiquests_over` = p0422.wav

## Quest Reward Ranges

Based on code analysis (`app.elf.c`), quest rewards use `100 * sub_1E5F8(min, max)`:

| Gold Range | Used In |
|------------|---------|
| 100-300 | Various small rewards |
| 200-400 | Small quests |
| 300-500 | Medium quests |
| 400-600 | Medium quests |
| 500-700 | Larger quests |
| 500-800 | Quest variations |
| 600-900 | Large quests |
| 600-1000 | Large quests |
| 700-1000 | Woman quest, some others |
| 700-1500 | Special quests |
| 800-1500 | Large quests |
| 900-1500 | Special quests |
| 1000-1200 | High-tier quests |
| 1000-1500 | High-tier quests |
| 1100-2000 | Major quests |
| 1300-1500 | Major quests |
| 3600-4500 | (Special subtraction calculation) |

## Quest Time Limits

From Notes.txt: "Zeitdauer verlängert: Man hat mehr Zeit um eine Quest zu erfüllen" (Time extended in v1.1 - more time to complete quests)

## Code References

- Quest state handling: `app.elf.c:3918-3915` (quest voice lines)
- Reward calculations: `app.elf.c:16865-23662`
- Quest miss handlers: `app.elf.c:3392` (multiquest_silver_lost)
