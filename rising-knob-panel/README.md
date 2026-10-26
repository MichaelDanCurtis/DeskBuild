# Rising knob panel

A 3D-printed module that mounts a **VIEWE UEDX46460015-MD50ET** (ESP32-S3, 1.5" round 466×466 AMOLED knob display)
behind a wood panel. When a hand comes near, the knob glides **12 mm** out of the panel so its ring can be turned.
When the hand leaves, it sinks back **flush with the wood**.

Designed in the chat [Autodesk Fusion MCP server connection](../chats/2026-10-24-autodesk-fusion-mcp-server-connection/)
and regenerated here from that chat's source. **Nothing has been printed or tested on real hardware yet.**

![Flush, raised, from behind, and cutaways at 0°, 90° and 180° of the crank](renders/overview.png)

## How it moves

An **MG90S micro servo** turns a small **crank**, and a **link** pushes the **carriage** the knob is screwed to.
The carriage slides on two Ø4 mm steel rods in LM4UU linear bearings. Turning the servo 180° moves the knob 12 mm.

- **Gradual, no popping.** A crank slows to a stop at both ends of its stroke, so the motion follows an S-curve:
  0 → 0.6 → 2.2 → 5.0 → 8.2 → 10.9 → 12 mm at every 30° of servo turn. Easing in firmware softens it further.
- **Holds without power.** At each end the crank and link line up straight, so pushing on the knob loads the frame rather than the
  servo gears. Once it's parked you can stop sending the servo signal, and it stays put without buzzing.
- **Forgiving at flush.** Near the ends, 5° of servo error moves the knob only about 0.03 mm.
- **One wire to the board.** The servo signal can come from a spare GPIO on the display's FPC, so no second
  microcontroller is needed. The earlier chat found 5V, GND and GPIO38/39/40 on the FPC; check this against VIEWE's pinout.

## Actuator options

You asked which actuator to use. The servo is the design that's modelled and clash-checked here. These are the alternatives:

| Actuator | Smoothness | Noise | Holds when off | Parts cost | Control | Verdict |
|---|---|---|---|---|---|---|
| **MG90S micro servo + crank/link** (built here) | Good: the crank eases in and out | Soft gear whine for ~1.5 s per move | Yes, at the dead centres | ~$5 | 1 PWM pin | Simplest and cheapest; a good first build |
| **Micro lead-screw stepper + TMC2209 driver** | Best: microstepping with acceleration ramps | Near silent (StealthChop) | Yes, the lead screw self-locks | ~$10 | STEP/DIR pins, plus homing against a hard stop or a switch | Upgrade if the servo whine bothers you |
| N20 gear motor with lead screw + DRV8833 | Good with a PWM ramp | Quiet to moderate | Yes | ~$8 | 2 PWM pins + 2 limit switches | Works, but needs end-stop switches to know where it is |
| Micro linear servo (Actuonix PQ12-R / L12-R) | Good | Moderate | Mostly | $70–90 | 1 PWM pin | Self-contained, but pricey and slow |
| Solenoid | ✗ snaps | Loud clack | — | — | — | Doesn't meet "gradual" |

## Hand sensing (not modelled yet)

- **Recommended:** a **VL53L1X** time-of-flight distance sensor on I²C. It looks through a small hole in the wood next to the knob and
  reliably detects a hand at about 5–30 cm. Use hysteresis in firmware, for example rise at < 15 cm and fall after 2 s with nothing within 20 cm.
- **Invisible option:** a copper-foil pad glued behind the wood with an **MPR121** capacitive sensor. It senses a hand through the wood
  with no visible hole, but takes more tuning.

## Print files

All STLs are already in print orientation and need no supports. Use PETG or PLA, 4 walls and about 30% infill.

| File | Size (mm) | Job |
|---|---|---|
| [`stl/mount_base.stl`](stl/mount_base.stl) | 96 × 85 × 51 | Plate that glues or screws to the back of the wood, with rod sockets, side walls and the servo wall |
| [`stl/carriage.stl`](stl/carriage.stl) | 78 × 47 × 31 | The knob screws to its front; holds the bearings and the link pin. The FPC passes through the middle |
| [`stl/rear_bridge.stl`](stl/rear_bridge.stl) | 92 × 32 × 8 | Holds the back ends of the rods, with a hole for the FPC |
| [`stl/crank.stl`](stl/crank.stl) | 14 × 9 × 6.8 | Presses onto the servo spline and is held by the horn screw |
| [`stl/link.stl`](stl/link.stl) | 24.5 × 6.5 × 3 | Connects the crank to the carriage, 18 mm between holes |

[`rising_knob_assembly.step`](rising_knob_assembly.step) is the whole assembly (printed parts, knob, servo, rods, bearings and a
wood panel), coloured, for opening in Fusion.

### Hardware

- 1× MG90S servo
- 2× Ø4 × 40 mm steel rods and 2× LM4UU linear bearings
- 3× M2.5×6 screws (knob to carriage)
- 2× M3×8 screws (the crank and link pivot pins, self-tapping into plastic)
- 4× M3×16 screws (rear bridge to base)
- 4× #4 × ½" flat-head wood screws, or glue
- a 5 V 2 A supply and a 470–1000 µF capacitor across the servo power

### Assumptions to check before printing

- **Panel thickness:** 3/4" (19.05 mm). This is `T` at the top of `source/rising_knob.py`.
- **Hole in the wood:** about 53 mm. A 2-1/8" (54 mm) Forstner bit is easier to find and align.
- **Fits:** the rod and bearing holes are sized as press fits, and the crank's spline bore is Ø4.7 mm. Print the carriage first and test-fit it.
- **FPC slack:** the ribbon needs a slack loop behind the rear bridge for the 12 mm of travel. A 10-pin, 0.5 mm FPC extension makes this easier.

## Regenerating

The model is parametric [CadQuery](https://cadquery.readthedocs.io/) code. Change the parameters at the top of
[`source/rising_knob.py`](source/rising_knob.py) (panel thickness, travel, hole size, rod spacing) and rebuild:

```bash
uv venv --python 3.12 .venv
uv pip install --python .venv/bin/python cadquery pillow
cd source
../.venv/bin/python rising_knob.py check export ../stl   # clash check through the stroke + STL export
mv ../stl/rising_knob_assembly.step ..
../.venv/bin/python render.py ../renders                   # offscreen VTK previews
```

On a headless Linux machine, run the render step under `xvfb-run -a`.

`check` intersects every part with every other part at 13 crank angles (every 15° from 0° to 180°) and lists any overlap above
0.1 mm³. The last run reported **0 clashes**, and all five STLs are single watertight bodies.
