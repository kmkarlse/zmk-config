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

## RGB (custom controller — replaces ZMK's rgb_underglow)

ZMK's built-in `rgb_underglow.c` is **disabled** on this shield (`CONFIG_ZMK_RGB_UNDERGLOW=n` in both `Corne_{L,R}.conf`). It owns the pixel buffer and re-renders the strip every 50ms regardless of effect, which makes layering per-key flash writes on top impossible. We replace it with a custom controller that runs on both halves.

### Files

- **`rgb_reactive.c`** — strip controller. Owns `led_strip`, ticks at 50ms, composites a base layer color + flash overlays in ACTIVE mode and a swirl in IDLE mode. Public API: `rgb_reactive_set_layer/_set_mode/_flash`. Compiles on both halves.
- **`behavior_rgb_reactive.c`** — global-locality behavior shim. Decodes `(param1=cmd, param2=arg)` and calls into the controller. `compatible = "zmk,behavior-rgb-reactive"`, binding at `dts/bindings/behaviors/`. The behavior is invoked programmatically only, never bound in the keymap.
- **`layer_color.c`** — central-side coordinator. Subscribes to `zmk_layer_state_changed`, `zmk_activity_state_changed`, `zmk_position_state_changed` (all central-only in ZMK) and invokes the global behavior so commands auto-propagate to peripheral over split BLE.
- **`rgb_reactive.h`** — protocol constants (`RGB_RX_CMD_*`, `RGB_RX_MODE_*`, `RGB_RX_FLASH_*`).

### Behavior protocol (param1, param2)

| Command (param1) | param2 | Effect |
|---|---|---|
| `RGB_RX_CMD_SET_LAYER` (0) | layer index 0..3 | Each half sets resting color to `layer_colors[layer]` |
| `RGB_RX_CMD_SET_MODE` (1) | `RGB_RX_MODE_ACTIVE`/`_IDLE` | Each half switches between layer-color and swirl |
| `RGB_RX_CMD_FLASH` (2) | `RGB_RX_FLASH_PACK(half, chain_led)` | Half ignores if `half` ≠ its compiled role; else adds a 300ms additive overlay on that chain index |

Per-key reactive flashes are gated to **BASE layer + ACTIVE only** in `layer_color.c::position_listener` to keep BLE traffic down.

### Position → LED mapping

`layer_color.c::resolve_position` maps the 0..41 global position into `(half, chain_led)`. Half is determined from the column range (cols 0..5 = central / left, 6..11 = peripheral / right). The local 0..20 index inside a half goes through the `LOCAL_TO_CHAIN[]` table which currently assumes simple monotonic chain order (LEDs 6..26 in row-major from top-left). If lit LEDs don't match the pressed key, edit that table — the chain routing on PandaKB Corne v3 MX is not documented.

### Layer colors

| Layer | Name | Color | HSB |
|---|---|---|---|
| 0 | BASE | cyan (reactive) | 182, 73, 60 |
| 1 | SYM  | green | 120, 100, 60 |
| 2 | NUM  | red (placeholder bindings) | 0, 100, 60 |
| 3 | L3   | orange (placeholder bindings) | 30, 100, 60 |

Brightness V is fixed at 60 (≈24%) across all layers; was previously controlled by `CONFIG_ZMK_RGB_UNDERGLOW_BRT_START`. Edit the `BASE_BRIGHTNESS` macro in `rgb_reactive.c` to change.

### Build gates / boot

`CMakeLists.txt` gates the controller + behavior on `CONFIG_WS2812_STRIP` (both halves). `layer_color.c` is additionally gated on central. Per `zmk/app/CMakeLists.txt:47`, ZMK gates `keymap.c`, `events/layer_state_changed.c`, and `events/position_state_changed.c` on central, so peripheral has no layer/position symbols — that's why the coordinator is central-only.

Controller init runs at `SYS_INIT(APPLICATION, 90)` and starts ticking 500ms after boot. The central coordinator broadcasts initial `SET_MODE=ACTIVE` + `SET_LAYER=current` 3s after boot so the first broadcast lands after the peripheral split connection is up (global behavior invocations only reach already-connected peripherals).

### DT wiring

Custom DT binding lives at `dts/bindings/behaviors/zmk,behavior-rgb-reactive.yaml` (repo-root location, picked up via `zephyr/module.yml`'s `dts_root: .`). The behavior node is declared in `Corne.dtsi`:

```
behaviors {
    rgb_reactive: behavior_rgb_reactive {
        compatible = "zmk,behavior-rgb-reactive";
        #binding-cells = <2>;
        display-name = "RGB Reactive";
    };
};
```

Behavior dev name (`"rgb_reactive"`) is derived from the DT alias label — same convention upstream uses for `rgb_ug`.

## OLED status screens

OLEDs are mounted **vertically** on the PandaKB Corne v3 MX (long axis up/down from the user's view). Status screen is provided by the [`mctechnology17/zmk-nice-oled`](https://github.com/mctechnology17/zmk-nice-oled) module, pulled in via `config/west.yml` and composed into the build via `build.yaml` shield list (`Corne_L nice_oled` / `Corne_R nice_oled`). Module assets are pre-rotated for vertical mounting — do NOT reuse with horizontally-mounted OLEDs.

- Module supplies its own `custom_status_screen.c`, layer/output/battery/WPM/HID widgets, and peripheral animations (cat, spaceman, pokemon, etc).
- Per-half differentiation is automatic: central shows status info, peripheral shows animations. Toggle widgets via `CONFIG_NICE_OLED_WIDGET_*` Kconfig in the shield's Kconfig.defconfig (or override per-half in `Corne_{L,R}.conf`).
- `Corne_{L,R}.conf` only set `CONFIG_ZMK_DISPLAY=y`; the rest (status screen mode, LVGL features, pool size, work queue, etc.) is set by `boards/shields/nice_oled/Kconfig.defconfig` in the module.
- Tested with ZMK v0.3 per the module README — matches our `west.yml` revision.

## Open / TODO

- User had ~4 LEDs working last we synced; was working through chain to fix bad solder joints.
- NUM layer (index 2) defined in keymap with `&trans` placeholders. No key bound to `&mo 2` yet — user must add activation.
- `BASE_BRIGHTNESS=60` in `rgb_reactive.c` (≈24% V). Low enough to stay within battery's 1C rating when batteries arrive.
- `CONFIG_BT_CTLR_TX_PWR_PLUS_8=y` → high BLE power, drains battery faster. User may want to drop for runtime.
- `LOCAL_TO_CHAIN[]` in `layer_color.c` is a row-major guess; verify against actual chain routing when more LEDs come online.

## Build / flash

- GitHub Actions build on push to `main`. Three artifacts: `Corne_L`, `Corne_R`, `settings_reset`.
- Each half flashed separately via UF2 drag-drop after double-tap reset.
- `settings_reset` UF2 wipes saved RGB/BLE/etc state — flash that, then re-flash normal firmware.
