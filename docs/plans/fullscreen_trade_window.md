# Plan: Fullscreen Trade Window

## Status: Complete

## Summary

Implemented a fullscreen split-screen trade interface for buying from and selling to merchant NPCs. Merchants now have generated inventories. The trade window supports browsing, buying, selling, and persists merchant inventory through save/load.

## Changes

| File | Change |
|------|--------|
| `include/astra/interaction.h` | Added `vector<Item> inventory` to ShopTrait |
| `include/astra/item_defs.h` | Declared 3 stock generator functions |
| `src/item_defs.cpp` | Implemented `generate_merchant_stock`, `generate_arms_dealer_stock`, `generate_food_merchant_stock` |
| `src/npcs/merchant.cpp` | Populated merchant stock on build |
| `src/npcs/hub_npcs.cpp` | Populated arms dealer + food merchant stock on build |
| `src/save_file.cpp` | Serialize/deserialize shop inventory, bumped read_npc to accept version |
| `include/astra/save_file.h` | Bumped version 10 -> 11 |
| `include/astra/trade_window.h` | New TradeWindow class |
| `src/trade_window.cpp` | Full implementation: input, buy/sell, rendering |
| `include/astra/game.h` | Added `trade_window_` member |
| `src/game.cpp` | Wired input intercept, shop interaction, render call |
| `CMakeLists.txt` | Added `src/trade_window.cpp` |

## Layout

- Left pane: merchant items with buy prices
- Right pane: player inventory with sell prices
- Active side has white header, inactive has dark gray
- Selected item row: yellow `>` cursor
- Credits and weight shown at bottom
- Status messages after buy/sell

## Controls

- ESC: close trade window
- TAB: switch between merchant/player sides
- Up/Down: navigate items
- SPACE: buy (merchant side) or sell (player side)
