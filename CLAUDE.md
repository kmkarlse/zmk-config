# Project context for Claude Code

## Hardware

- **PCB:** PandaKB Corne v3 MX (https://pandakb.com/guides/corne-v3-mx-build-guide/)
- **MCU:** generic AliExpress nRF52840 ProMicro clones — **NOT real Nice!Nano v2**. Pinout-compatible on the 24 standard Pro Micro castellations. **Does NOT expose P1.07** (no back pads — clone has top-side through-holes labeled `107`, `106`, `102`, `101`, plus a 4-pin QSPI header `VDD/DO/CLK/GND`. None of these connect to PCB when MCU is mounted normally).
- **Switches:** Choc.
- **LEDs per side:** 21 SK6812MINI-E (in-switch) + 6 WS2812B-5050 (underglow) = **27 daisy-chained**.
- **Power:** USB-C, no battery yet. 1200 mAh Luxorparts LiPo on order — has **JST-PH 2.0mm**, PCB connector is 1.25mm → user splice with kit pigtail.
- **Host:** Norwegian Windows layout.
- **Build target:** `nice_nano_v2` (works because clone shares standard Pro Micro D-pins).

## Critical config: RGB data pin is P0.06, not P1.07

The PandaKB shield (`boards/shields/Corne/Corne.dtsi`) was originally written for a real Nice!Nano v2 whose RGB back-pad is wired to **P1.07**. PandaKB v3 MX PCB supports BOTH `ProMicro RP2040` and `ProMicro nRF52840` per build guide — meaning RGB MUST route through a pin common to both, i.e. a standard Pro Micro D-pin. On nRF52840 ProMicro that's **P0.06** (D1/TX position).

Verified by user via multimeter: continuity from first underglow LED's DIN ↔ D1 socket pad on PCB.

`Corne.dtsi` `spi3_default` and `spi3_sleep` both pinned to `NRF_PSEL(SPIM_MOSI, 0, 6)`.

**Never revert to P1.07** — clone doesn't expose it, RGB data goes nowhere.

## RGB chain notes

- Single serial chain. One bad LED kills all downstream.
- Order: **6 underglow first, then 21 per-key**. First LED is the underglow whose DIN traces to MCU's D1 (P0.06).
- `chain-length = <27>`, `color-mapping = GRB`, `compatible = "worldsemi,ws2812-spi"`.
- LED diode test (red on GND, black on VDD reads ~0.4V one direction, OL the other = chip alive).
- Reverse-mount in-switch SK6812 pinout: chamfer = pin 1 = VDD; opposite chamfer = pin 4 = DIN; counter-clockwise from chamfer.

## Layer colors (custom C)

`boards/shields/Corne/layer_color.c` — listens to `zmk_layer_state_changed`, sets RGB HSB based on highest active layer.

| Layer | Name | Color | HSB |
|---|---|---|---|
| 0 | BASE | cyan (RGB 66,239,245) | 182, 73, 96 |
| 1 | SYM  | green | 120, 100, 60 |
| 2 | NUM  | red (placeholder bindings) | 0, 100, 60 |

Boot init at SYS_INIT(APPLICATION, 90) schedules a delayed work 1s after boot to apply BASE color (otherwise RGB stays at ZMK default until first layer event).

`CMakeLists.txt` in shield folder gates compilation on `CONFIG_ZMK_RGB_UNDERGLOW`. Compiles on **both halves** — ZMK propagates layer state over split BLE, so `zmk_layer_state_changed` fires on the peripheral too and `zmk_keymap_highest_layer_active()` is unconditional in v0.3. Each half reacts independently and sets its own underglow.

## OLED status screens

- **Left (central):** ZMK's default built-in status screen — layer + output widgets (battery widget disabled).
- **Right (peripheral):** custom bongo cat screen, full 128x32. Set up via `CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM=y` in `Corne_R.conf` so ZMK skips its built-in `status_screen.c` and our `custom_status_screen.c` provides `zmk_display_status_screen()`.
- **Bongo cat widget** in `boards/shields/Corne/widgets/`: bitmap data (5 idle + 2 tap frames, 1-bit indexed) in `bongo_cat_images.c`, animation logic in `bongo_cat.c`. Subscribes to `zmk_position_state_changed` (not keycode) so it works on peripheral. Animates on right-hand keypresses; left-hand keypresses don't propagate to peripheral so cat stays idle for those.
- `CMakeLists.txt` gates bongo cat sources on `CONFIG_ZMK_DISPLAY AND CONFIG_ZMK_SPLIT AND NOT CONFIG_ZMK_SPLIT_ROLE_CENTRAL` — built only for peripheral.

## Open / TODO

- User had ~4 LEDs working last we synced; was working through chain to fix bad solder joints.
- NUM layer (index 2) defined in keymap with `&trans` placeholders. No key bound to `&mo 2` yet — user must add activation.
- `BRT_START=60` (24% brightness). Low enough to stay within battery's 1C rating when batteries arrive.
- `CONFIG_BT_CTLR_TX_PWR_PLUS_8=y` → high BLE power, drains battery faster. User may want to drop for runtime.

## Build / flash

- GitHub Actions build on push to `main`. Three artifacts: `Corne_L`, `Corne_R`, `settings_reset`.
- Each half flashed separately via UF2 drag-drop after double-tap reset.
- `settings_reset` UF2 wipes saved RGB/BLE/etc state — flash that, then re-flash normal firmware.
