# Rising knob firmware (ESP32-S3)

Firmware for the **VIEWE UEDX46460015-MD50ET** knob display in the [rising knob panel](../).
When a hand comes within ~15 cm, an Actuonix **PQ12-R** linear actuator glides the knob 12 mm out of the wood and
the screen fades in. About 2.5 s after the hand leaves (8 s after the ring was last turned) it sinks flush again
and the screen goes dark.

**Status:** it compiles for the ESP32-S3 with no warnings, and the motion, alignment, button and presence logic
passes 22 unit tests on a PC. **It has not been run on the real board, actuator or sensor yet.**

## Wiring

The board's FPC connector brings out only **GPIO39, GPIO40, GPIO43, GPIO44**, 5 V and GND. There is no 3.3 V.
That's from [VIEWE's pinout](https://github.com/VIEWESMART/UEDX46460015-MD50ESP32-1.5inch-Touch-Knob-Display#22-gpio-definition-pinout).

| FPC pin | Signal | Connect to |
|:---:|---|---|
| 1 | 5V | 5 V 2 A supply (+). This also powers the board |
| 3 | GND | Supply (−), actuator black, sensor GND, regulator GND |
| 4 | GPIO40 | Actuator **white** (RC signal), plus a 10 kΩ resistor to GND |
| 5 | GPIO39 | VL53L1X **SDA** |
| 6 | GPIO44 | VL53L1X **SCL** |
| 7 | GPIO43 | Leave free (it prints the boot log) |

- **Actuator power:** run its **red** wire from the 5 V supply directly, not through the board, with a
  470–1000 µF capacitor nearby. If it moves erratically, Actuonix suggests a 1–4 Ω resistor in series with red.
- **Sensor power: 3.3 V, never 5 V.** Feed the VL53L1X from a small 3.3 V regulator running off the 5 V supply.
  Many breakouts pull SDA/SCL up to their supply pin, and the ESP32-S3 isn't 5 V tolerant.
- The sensor looks out through a hole in the wood next to the knob. It sees a 27° cone, so a narrow hole through
  19 mm of wood would block it. Bore a pocket from the back (e.g. 13 mm Forstner) to within ~3 mm of the front,
  drill ~8 mm through, and seat the sensor at the bottom of the pocket.

## Build and flash

Install [PlatformIO](https://platformio.org/install/cli), then from this folder:

```bash
pio run -e knob -t upload        # full firmware; the first build downloads the ESP32 toolchain (~1 GB)
pio device monitor               # USB serial console, 115200
```

| Environment | What it's for |
|---|---|
| `knob` | Everything: hand sensing, actuator, ring encoder, AMOLED with a placeholder dial UI |
| `knob_no_display` | Mechanism only (no LVGL or display). Use it for first bring-up of the actuator and sensor |
| `native` | `pio test -e native` runs the unit tests on your computer |

It uses [pioarduino](https://github.com/pioarduino/platform-espressif32) 55.03.35 (Arduino-ESP32 3.3.5),
LVGL 8.4 and Pololu's VL53L1X library, all pinned in `platformio.ini`.

## First power-up

1. **Before fitting the knob,** type `down` then `up` and watch the actuator. It should retract and extend as
   expected (2.0 ms pulse = retracted, 1.0 ms = extended).
2. **Align** the flush and raised positions with `align` (see [the panel README](../README.md#aligning-the-stop-and-start)).
3. **Check the sensor.** `status` shows `sensor=ok`. `log` prints the distance 5× a second; wave a hand and check
   the numbers. Adjust `kNearMm` / `kFarMm` in `include/config.h` to suit your panel.
4. **Check the ring.** Turn it one click and watch the dial on screen. If it moves 2 per click, set
   `kEncoderCountsPerDetent = 4`. If it moves backwards, set `kEncoderReverse = true`.
5. If the dial looks off-centre on the round glass, nudge `kUiCenterOffsetX` (the controller frame is 472 px wide
   for a 466 px display).

## Console commands

| Command | Does |
|---|---|
| `status` | Mode, sensor, distance, whether it wants up, commanded position and pulse, saved flush/raised |
| `up` / `down` / `auto` | Force the knob up or down, or go back to hand detection |
| `log` | Toggle a 5 Hz distance printout for tuning |
| `align` | Interactive alignment: `+`/`-`/`+N`/`-N` nudge, `next` moves on and saves, `cancel` |
| `flush <mm>` / `height <mm>` | Set and save a position directly |

## How it works

```
VL53L1X ──► PresenceDetector ──► target ──► MotionPlanner ──► pulse 2.0–1.0 ms ──► PQ12-R ──► carriage ──► knob
(50 ms)     hysteresis, delays     ▲          smooth ramp       (LEDC 50 Hz)         own position control
ring encoder ── activity ──────────┤                                            └──► knob height ──► AMOLED brightness
BOOT button / console ── align ────┘
```

- **Smooth motion** (`lib/lift_core/src/motion.*`): every move is planned from the current commanded position and
  speed: accelerate, cruise, brake, stop exactly on target. Reversing mid-move never jumps. The planned speed stays
  under the actuator's top speed, so the PQ12's own controller follows the ramp closely.
- **Quiet when parked:** 1 s after a move the pulses stop. The actuator holds by itself and goes silent.
- **Alignment** (`lib/lift_core/src/align.*`): flush and height are adjusted live and saved to flash (NVS).
- **Hand detection** (`lib/lift_core/src/presence.*`):
  - A hand must stay within 15 cm for 120 ms before the knob rises.
  - Once it's up, anything within 25 cm keeps it up (hysteresis).
  - Turning the ring raises it, or keeps it up, for 8 s.
  - Anything parked in front of the sensor for 2 minutes with nobody turning the ring is ignored until it moves away.
- **Display:** brightness follows the knob's height, so the screen fades in as it emerges and switches off when
  flush. AMOLED black is unlit, so the flush knob reads as a dark disc in the wood. The panel is driven directly
  through ESP-IDF's `esp_lcd`. Espressif's ESP32_Display_Panel library isn't used because it links ESP-IDF's
  legacy I²C driver, which aborts at boot next to the newer one `Wire` uses.
- **Tasks:** a 100 Hz control task on core 0 runs the sensor, logic, actuator and console. Arduino's `loop()` on
  core 1 runs LVGL, so screen redraws never stutter the motion.

## Customising the screen

`src/ui.cpp` is a placeholder dial (0–100 on the ring). Replace it with your own LVGL v8 UI; the only hook is
`ui::addDetents(n)`, called with each batch of ring clicks. The touch controller isn't started, because its I²C
driver would clash with the sensor's bus.

## Known limits

- The PQ12 has no position output, so the firmware assumes it reached the commanded position. On power-up it
  commands flush; if power was cut while the knob was up, that first move runs at the actuator's full speed
  instead of the gentle ramp.
- The datasheet gives ±0.1 mm repeatability, 0.25 mm backlash and a 20% maximum duty cycle. A move every few seconds
  is well inside that, but don't leave it in a test loop going up and down continuously.
- Encoder counts per click, screen centring and sensor thresholds are best guesses until tested on the real
  hardware.
