# Negotiation System (Feilschen)

## Overview

When purchasing equipment from merchants, players can choose to haggle (Feilschen) to reduce the price. The success of haggling depends on the player's rank/title.

## Haggling Flow

### 1. Initial Offer
When a merchant offers equipment, the player can:
- **Accept** - Buy at the offered price
- **Decline** - Refuse the purchase
- **Haggle** - Attempt to negotiate a lower price

### 2. Haggle Offer Voice Lines

Each merchant has unique haggle offer voice lines:

| Merchant | Voice Line Pattern |
|----------|-------------------|
| m_1 | h1_011.wav + ~z + h1_012.wav |
| m_2 | h2_011.wav + ~z + h2_012.wav |
| m_3 | h3_011.wav + ~z + h3_012.wav |
| m_4 | h4_011.wav + ~z + h4_012.wav |
| m_5 | h5_011.wav + ~z + h5_012.wav |

Voice line keys:
- `m_1_equip_offer_haggle` = h1_011.wav + ~z + h1_012.wav
- `m_2_equip_offer_haggle` = h2_011.wav + ~z + h2_012.wav
- `m_3_equip_offer_haggle` = h3_011.wav + ~z + h3_012.wav
- `m_4_equip_offer_haggle` = h4_011.wav + ~z + h4_012.wav
- `m_5_equip_offer_haggle` = h5_011.wav + ~z + h5_012.wav

The `~z` is replaced with the new (reduced) price.

## Haggling Success Calculation

From code analysis (`app.elf.c:13582-13596`):

### Success Formula

```c
// Player rank stored at offset a1+60 (value 1-10)
rank = *(char *)(a1 + 60);

// Complex floating point calculation with rank
// Higher rank = higher threshold = better success chance
threshold = complex_calculation(rank);

// Random roll 1-100
roll = sub_1E5F8(1, 100);

// Success check
if (roll > threshold) {
    // FAILURE - haggling fails
    play_no_luck_voice();
} else {
    // SUCCESS - price reduced
    reduce_price();
}
```

### Rank Effect on Haggling

From Notes.txt v1.1:
> "beim Feilschen wird nun der Rang berücksichtigt"
> (When haggling, rank is now considered)

Higher player rank provides:
- **Higher base success probability**
- The rank value (1-10) is used in a calculation that determines the success threshold
- As rank increases, the threshold increases, making rolls more likely to succeed

### 10 Player Ranks

| Rank | Code | Title | Effect |
|------|------|-------|--------|
| 1 | cap | Captain (Kapitän) | Lowest haggling bonus |
| 2 | maj | Major | Slight bonus |
| 3 | col | Colonel (Oberst) | Small bonus |
| 4 | adm | Admiral | Moderate bonus |
| 5 | bar | Baron | Good bonus |
| 6 | gba | Grand Baron | Better bonus |
| 7 | mar | Marquis | High bonus |
| 8 | gma | Grand Marquis | Very high bonus |
| 9 | duk | Duke (Herzog) | Excellent bonus |
| 10 | gdu | Grand Duke | Maximum bonus |

## Price Reduction

### On Successful Haggle

From code analysis (`app.elf.c:13603-13611`):

```c
// Equipment type determines reduction amount
v117 = equipment_type;
if (v117 == 1 || v117 == 3 || v117 == 5 || v117 == 7 || v117 == 9) {
    reduction = 100;  // Small equipment
} else {
    reduction = 200;  // Big equipment
}
new_price = current_price - reduction;
```

| Equipment Type | Type IDs | Price Reduction |
|---------------|----------|-----------------|
| Small Sail (Segel) | 1 | 100 gold |
| Big Sail | 2 | 200 gold |
| Small Cannon (Kanone) | 3 | 100 gold |
| Big Cannon | 4 | 200 gold |
| Small Sailors (Matrosen) | 5 | 100 gold |
| Big Sailors | 6 | 200 gold |
| Small Cargo (Laderaum) | 7 | 100 gold |
| Big Cargo | 8 | 200 gold |
| Small Residence | 9 | 100 gold |
| Big Residence | 10 | 200 gold |

### Minimum Price

From code (`app.elf.c:13612-13613`):

```c
if (new_price <= 100) {
    // Cannot haggle below 100 gold
    play_no_luck_voice();
    return;
}
```

**Minimum price after haggling: 100 gold**

Players cannot haggle below this threshold - attempts will trigger the "no luck" response.

## Failed Haggle (No Luck)

When haggling fails, each merchant has a unique "no luck" voice line:

| Merchant | Voice Line |
|----------|------------|
| m_1 | h1_010.wav |
| m_2 | h2_010.wav |
| m_3 | h3_010.wav |
| m_4 | h4_010.wav |
| m_5 | h5_010.wav |

Voice line keys:
- `m_1_no_luck` = h1_010.wav
- `m_2_no_luck` = h2_010.wav
- `m_3_no_luck` = h3_010.wav
- `m_4_no_luck` = h4_010.wav
- `m_5_no_luck` = h5_010.wav

## Multiple Haggle Attempts

Players can attempt to haggle multiple times on the same purchase:

1. **First successful haggle**: Price reduced by 100/200 gold
2. **Subsequent haggles**: Each success reduces price by another 100/200 gold
3. **Continue until**:
   - Price reaches minimum (100 gold)
   - Haggling fails (random roll fails)
   - Player accepts current price
   - Player declines purchase

### Haggle Attempt Tracking

From code (`app.elf.c:13615-13616`):

```c
if (v72 == 1)
    v72 = 0;  // Mark that first haggle attempt has been made
```

After the first haggle attempt, subsequent offers use the short format voice lines.

## Merchant-Equipment Specialization

Each merchant (m_1 through m_5) is associated with specific ports and equipment:

| Merchant | Case ID | Equipment Info Files |
|----------|---------|---------------------|
| m_1 | 6 | p0417.wav (small), p0715.wav (big) |
| m_2 | 2 | p0416.wav (small), p0714.wav (big) |
| m_3 | 4 | p0414.wav (small), p0712.wav (big) |
| m_4 | 0 | p0418.wav (small/big) |
| m_5 | 7 | p0415.wav (small), p0713.wav (big) |

## Profi Mode Haggling

In Profi (expert) mode, haggling uses compact voice lines:

- `buy_profi` = p0723.wav - Quick purchase confirmation
- `not_bought_profi` = p0722.wav - Quick decline

The haggling mechanics remain the same, but voice feedback is shorter.

## Honor/Rank Calculation

Player rank is determined by accumulated honor points:

From code (`app.elf.c:16306-16317`):

```c
// byte_2B2E4/byte_2B2E5 contains rank threshold table
// 10 ranks with 8-byte entries each
for (i = 0; i < 10; i++) {
    if (player_honor == threshold[i]) {
        player_rank = rank_value[i];
        break;
    }
}
```

### Rank Threshold Table

Stored in `byte_2B2E4` with 8-byte entries:
- Entry format: [rank_value, ..., honor_threshold, ...]
- 10 entries for ranks 1-10

**TODO: Extract exact honor thresholds from binary**

## Code References

- Haggle voice line selection: `app.elf.c:5368-5388` (`sub_764C`)
- No luck voice line: `app.elf.c:5278-5290` (`sub_74A8`)
- Success calculation: `app.elf.c:13582-13596`
- Price reduction: `app.elf.c:13603-13614`
- Rank storage: Player structure offset `a1 + 60`
- Rank determination: `app.elf.c:16306-16317`
- Rank data table: `byte_2B2E4`, `byte_2B2E5`

## Summary

1. Player initiates haggle at merchant
2. System calculates success threshold based on player rank
3. Random roll 1-100 determines success
4. If successful: Price reduced by 100 (small) or 200 (big) gold
5. Player can continue haggling until price hits 100 gold minimum
6. If roll fails: "No luck" voice plays, haggling ends
7. Higher rank = better haggling success rate
