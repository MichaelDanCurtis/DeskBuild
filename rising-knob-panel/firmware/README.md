# Rising knob firmware (ESP32-S3)

Firmware for the **VIEWE UEDX46460015-MD50ET** knob display in the [rising knob panel](../).
When a hand comes within ~15 cm, the knob glides 12 mm out of the wood and the screen fades in. About 2.5 s
after the hand leaves (8 s after the ring was last turned) it sinks flush again and the screen goes dark.

**Status:** it compiles for the ESP32-S3 with no warnings, and the motion and presence logic passes 15 unit tests
on a PC. **It has not been run on the real board, servo or sensor yet.** Expect to tune the items under
[First power-up](#first-power-up).

## Wiring

The board's FPC connector brings out only **GPIO39, GPIO40, GPIO43, GPIO44**, 5 V and GND. There is no 3.3 V.
That's from [VIEWE's pinout](https://github.com/VIEWESMART/UEDX46460015-MD50ESP32-1.5inch-Touch-Knob-Display#22-gpio-definition-pinout);
the earlier "GPIO38" note was wrong.

| FPC pin | Signal | Connect to |
|:---:|---|---|
| 1 | 5V | 5 V 2 A supply (+). This also powers the board |
| 3 | GND | Supply (−), servo GND, sensor GND, regulator GND |
| 4 | GPIO40 | Servo signal (orange), plus a 10 kΩ resistor to GND |
| 5 | GPIO39 | VL53L1X **SDA** |
| 6 | GPIO44 | VL53L1X **SCL** |
| 7 | GPIO43 | Leave free (it prints the boot log) |

- **Servo power:** run the servo's red wire straight from the 5 V supply, not through the board, and put a
  470–1000 µF capacitor across it near the servo.
- **Sensor power: 3.3 V, never 5 V.** Feed the VL53L1X breakout from a small 3.3 V regulator (an MCP1700-3302 or an
  AMS1117-3.3 module) running off the 5 V supply. Many breakouts pull SDA/SCL up to their supply pin, and 5 V on
  those lines can damage the ESP32-S3.
- The sensor looks out through a hole in the wood next to the knob. It sees a 27° cone, so a narrow hole through
  19 mm of wood would block it. Bore a pocket from the back (e.g. 13 mm Forstner) to within ~3 mm of the front,
  then drill ~8 mm through, and seat the sensor at the bottom of the pocket. Readings under 25 mm (the hole wall)
  are ignored.

## Build and flash

Install [PlatformIO](https://platformio.org/install/cli), then from this folder:

```bash
pio run -e knob -t upload        # full firmware; the first build downloads the ESP32 toolchain (~1 GB)
pio device monitor               # USB serial console, 115200
```

| Environment | What it's for |
|---|---|
| `knob` | Everything: hand sensing, motion, ring encoder, AMOLED with a placeholder dial UI |
| `knob_no_display` | Mechanism only (no LVGL or display). Use it for first bring-up of the servo and sensor |
| `native` | `pio test -e native` runs the motion and presence unit tests on your computer |

It uses [pioarduino](https://github.com/pioarduino/platform-espressif32) 55.03.35 (Arduino-ESP32 3.3.5),
LVGL 8.4, Espressif's ESP32_Display_Panel 1.0.4 and Pololu's VL53L1X library, all pinned in `platformio.ini`.

## First power-up

1. **Calibrate the servo before closing anything up.** The crank must stop exactly at its dead centres for the
   knob to sit flush and hold without power. In the serial console:

   ```text
   cal            enter calibration (the servo holds its position)
   f              go to the saved flush pulse (500 us by default)
   +10 / -10      nudge until the crank points straight down and the knob is flush
   set flush      save it
   r              go to the raised pulse (2400 us by default)
   +10 / -10      nudge until the crank points straight up
   set raised     save it
   done
   ```

   The values are kept in flash. If the servo buzzes or strains at either end, back off a few µs. If it can't reach
   both dead centres, move the crank one tooth on the servo spline.
2. **Check the sensor.** `status` shows `sensor=ok`. `log` prints the distance 5× a second; wave a hand and check
   the numbers. Adjust `kNearMm` / `kFarMm` in `include/config.h` to suit your panel.
3. **Check the ring.** Turn it one click and watch the dial on screen. If it moves 2 per click, set
   `kEncoderCountsPerDetent = 4`. If it moves backwards, set `kEncoderReverse = true`.
4. If the dial looks off-centre on the round glass, nudge `kUiCenterOffsetX` (the controller frame is 472 px wide
   for a 466 px display).

## Console commands

| Command | Does |
|---|---|
| `status` | Mode, sensor state, distance, whether it wants up, angle, knob height, servo pulse |
| `up` / `down` / `auto` | Force the knob up or down, or go back to hand detection |
| `log` | Toggle a 5 Hz distance printout for tuning |
| `cal` | Servo calibration (see above) |

## How it works

```
VL53L1X ──► PresenceDetector ──► MotionPlanner ──► servo angle ──► MG90S ─► crank ─► knob
(50 ms)     hysteresis, delays     smooth trapezoid   LEDC 50 Hz
ring encoder ─┘ (activity keeps it up)                └──► knob height ──► AMOLED brightness
```

- **Smooth motion** (`lib/lift_core/src/motion.*`): every move is planned from the knob's current position and
  speed: accelerate, cruise, brake, and stop exactly on target. If a hand returns while the knob is sinking, it
  slows, stops and rises again without a jolt. Rising takes ~1.5 s and sinking ~2 s (`kRaise*` / `kLower*`).
  The crank adds its own easing on top: the knob barely moves in the first and last few degrees of servo travel.
- **Quiet when parked:** 0.6 s after a move ends the servo stops receiving pulses. The crank sits on a dead centre,
  so the knob stays put and the servo goes silent.
- **Hand detection** (`lib/lift_core/src/presence.*`):
  - A hand must stay within 15 cm for 120 ms before the knob rises.
  - Once it's up, anything within 25 cm keeps it up (hysteresis).
  - Turning the ring raises it, or keeps it up, for 8 s.
  - Anything parked in front of the sensor for 2 minutes with nobody turning the ring is ignored until it moves away.
- **Display:** brightness follows the knob's height, so the screen fades in as it emerges and switches off when
  flush. AMOLED black is unlit, so the flush knob reads as a dark disc in the wood.
- **Tasks:** a 100 Hz control task on core 0 runs the sensor, logic, servo and console. Arduino's `loop()` on
  core 1 runs LVGL. The control loop's timing doesn't depend on screen redraws.

## Customising the screen

`src/ui.cpp` is a placeholder dial (0–100 on the ring). Replace it with your own LVGL v8 UI; the only hook is
`ui::addDetents(n)`, called with each batch of ring clicks. The touch controller isn't started, because its
I²C driver would clash with the sensor's bus.

## Known limits

- On power-up the firmware drives the servo to flush. If power was cut while the knob was up, that first move
  is a fast snap rather than a glide.
- The servo pulses, encoder counts per click, screen centring and sensor thresholds are best guesses until tested
  on the real hardware.
