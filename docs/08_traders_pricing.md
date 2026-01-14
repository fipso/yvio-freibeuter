# Traders and Pricing System

## The 5 Merchants

The game features 5 unique merchants (m_1 through m_5), each with their own voice and personality. They sell equipment at ports.

### Merchant Voice Files
Each merchant has dedicated voice files (h1-h5):

| Merchant | Voice Files | Equipment Info |
|----------|-------------|----------------|
| m_1 (h1) | h1_*.wav | p0417.wav (sma), p0715.wav (big) |
| m_2 (h2) | h2_*.wav | p0416.wav (sma), p0714.wav (big) |
| m_3 (h3) | h3_*.wav | p0414.wav (sma), p0712.wav (big) |
| m_4 (h4) | h4_*.wav | p0418.wav (sma/big) |
| m_5 (h5) | h5_*.wav | p0415.wav (sma), p0713.wav (big) |

## Player Rank/Title System

Players earn titles as they progress. Rank affects trading benefits.

### Titles (10 ranks)
| ID | Code | Title | Audio |
|----|------|-------|-------|
| 1 | cap | Captain (Kapitän) | p0811.wav |
| 2 | maj | Major | p0309.wav |
| 3 | col | Colonel (Oberst) | p0310.wav |
| 4 | adm | Admiral | p0311.wav |
| 5 | bar | Baron | p0312.wav |
| 6 | gba | Grand Baron (Großbaron) | p0313.wav |
| 7 | mar | Marquis | p0314.wav |
| 8 | gma | Grand Marquis (Großmarquis) | p0315.wav |
| 9 | duk | Duke (Herzog) | p0316.wav |
| 10 | gdu | Grand Duke (Großherzog) | p0317.wav |

### Title Announcement
- `title` = p0307.wav + %ship + p0079.wav + ~z + p0078.wav + p0308.wav + %title + p0318.wav
- `title_sh` = (same, short version)
- `title_info` = (same + tutorial hints)

## Equipment Pricing

### Price Range
Merchants announce prices using the hX_zYYY.wav files:
- Range: 100 - 3900 gold
- Increments of 100

### Price Offer Voice Lines

#### Long Offer (First Time)
| Type | Pattern |
|------|---------|
| Small Equipment | hX_001.wav + hX_002.wav + ~z + hX_003.wav + p0095.wav |
| Big Equipment | hX_001.wav + hX_006.wav + ~z + hX_007.wav + p0095.wav |
| Residence | hX_001.wav + hX_016.wav + ~z + hX_019.wav + p0095.wav |

#### Short Offer (Repeat)
| Type | Pattern |
|------|---------|
| Small Equipment | hX_004.wav + ~z + hX_005.wav + p0095.wav |
| Big Equipment | hX_008.wav + ~z + hX_009.wav + p0095.wav |
| Residence | hX_016.wav + ~z + hX_019.wav + p0095.wav |

### Profi Mode Offers
Compact format for experienced players:
- `equip_offer_profi_cannon` = ~z + p0721.wav + p0480.wav + p0704.wav
- `equip_offer_profi_sail` = ~z + p0721.wav + p0613.wav + p0270.wav
- `equip_offer_profi_sailors` = ~z + p0721.wav + p0614.wav + p0269.wav
- `equip_offer_profi_cargo` = ~z + p0721.wav + p0613.wav + p0271.wav
- `equip_offer_profi_residence` = ~z + p0721.wav + p0743.wav

## Purchase Outcomes

### Successful Purchase
| Type | Voice Line |
|------|------------|
| bought_s | p0122.wav + p0257.wav + ~z1 + p0258.wav + p0083.wav + ~z2 + p0085.wav |
| bought_m | p0122.wav + p0257.wav + ~z1 + p0258.wav + p0083.wav + ~z2 + p0084.wav |
| bought_sh | p0123.wav + p0257.wav + ~z + p0078.wav |
| buy_profi | p0723.wav |

### Merchant Buy Response
- `m_X_buy` = hX_013.wav + hX_014.wav

### Failed/Declined Purchase
- `m_X_not_bought` = hX_014.wav
- `not_bought_profi` = p0722.wav

### Bad Luck (Haggle Failed)
- `m_X_no_luck` = hX_010.wav

## Honor/Rank Effect on Trading

From Notes.txt v1.1:
> "beim Feilschen wird nun der Rang berücksichtigt"
> (When haggling, rank is now considered)

Higher player rank provides:
- Better initial prices
- Higher haggling success rate
- Better equipment availability

### Rank Bonus Calculation

From code analysis (`app.elf.c:10056-10057`):
```c
bonus = (word_400000FE * residence_lvl1) + (residence_lvl2 * word_4000010E)
```

The residence equipment provides additional bonuses based on multipliers.

## Equipment Availability

- `info_no_equip` = p0423.wav + p0420.wav - No equipment available
- `no_more_equipment` = p0212.wav - Player has maximum equipment

## Price Information
- `info_price` = p0423.wav + p0419.wav - Price explanation in beginner mode

## Code References

- Merchant voice selection: Uses m_X_ prefix based on merchant ID
- Price announcement: Uses hX_zYYY.wav files for numbers
- Purchase handling: `app.elf.c:5240` (buy_profi), `5205` (not_bought_profi)
- Title handling: `app.elf.c:2773-2934`
