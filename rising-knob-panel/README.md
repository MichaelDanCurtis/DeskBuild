# Rising knob panel

A 3D-printed module that mounts a **VIEWE UEDX46460015-MD50ET** (ESP32-S3, 1.5" round 466×466 AMOLED knob display)
behind a wood panel. When a hand comes near, the knob glides **12 mm** out of the panel so its ring can be turned.
When the hand leaves, it sinks back **flush with the wood**.

**v2:** a lead-screw stepper on 8 mm rods replaces the first version's hobby servo, crank and 4 mm rods. That
version is in git history and in the [original chat](../chats/2026-10-24-autodesk-fusion-mcp-server-connection/).
**Nothing has been printed or tested on real hardware yet.**

![Flush, raised, from behind, and cutaways at flush, halfway and raised](renders/overview.png)

## How it moves

- **Drive:** a **NEMA 17 stepper with a built-in Tr8×2 lead screw** pushes the carriage through a brass nut.
  2 mm per turn gives 800 microsteps per mm.
- **Holds with the power off:** the 2 mm lead is self-locking, so pressing on the knob can't push it back.
  The motor switches off once parked and the module is silent.
- **Guides:** two **8 mm steel rods in LM8UU bearings**, held at both ends by the base and the rear plate. With the
  side walls, that makes a rigid box.
- **Gradual, no popping:** the firmware ramps speed up and down. Rising takes ~1.5 s and sinking ~2 s. The TMC2209
  driver in StealthChop mode is close to silent. If a hand returns while the knob is sinking, it slows, stops and
  rises again without a jolt.
- **Hard stops at both ends:** rear bumpers on the rear plate and the rod bosses on the base.
- **Contactless home sensor:** a fin on the carriage passes through a slotted optical sensor whose slot is open at
  both ends, so overtravel can't damage it.

| | v1 (servo) | v2 (this) |
|---|---|---|
| Actuator | MG90S hobby servo, plastic gears | NEMA 17 + lead screw, metal |
| Holding | Only at the crank's dead centres | Anywhere: the lead screw self-locks |
| Guides | 2× Ø4 mm rods, LM4UU | 2× Ø8 mm rods, LM8UU, supported at both ends |
| Position accuracy | Servo deadband, ~0.1–0.5 mm | 1/800 mm steps, homed to a switch |
| Noise | Gear whine | Near-silent (StealthChop), off when parked |
| Adjustable stops | Servo pulse calibration only | Sliding home sensor + firmware fine-tune |
| Depth behind the wood | ~45 mm | ~104 mm (~87 mm with a 23 mm "pancake" NEMA 17) |

## Aligning the stop and start

The **home sensor** is the reference: the firmware finds it on every power-up, and both stop positions are
measured from where it trips.

1. **Coarse, mechanical:** the optical sensor board is held by two screws in **vertical slots** on the rear plate's
   tab, so it can slide ±2 mm along the travel. Set it so it trips about 1 mm *below* flush, which is where the
   slots' centre puts it.
2. **Fine, in firmware:** the **flush** position (0.05 mm steps) and the **raised height** (0.1 mm steps) are
   adjusted live with the knob ring or the USB console, then saved to flash:
   - Run `align` in the console, or hold the board's BOOT button for 2 s.
   - **Flush step:** the knob drops to flush so you can see it against the wood. Turn the ring (or type `+`/`-`)
     and it moves in 0.05 mm steps. Because the ring can't be turned while flush, turning it lifts the knob; it
     drops back to show the new flush position after a second.
   - Press the button or type `next` for the **height** step. The knob follows the ring live.
   - Press or type `next` again to save.
   - You can also set values directly: `flush 1.20`, `height 12.5`.

## Print files

STLs are already in print orientation. Use PETG, 4 walls and 40% gyroid infill. No supports are needed.

| File | Size (mm) | Job |
|---|---|---|
| [`stl/base.stl`](stl/base.stl) | 108 × 98 × 58 | Glues/screws to the back of the wood: plate with the knob hole, front rod sockets, side walls and bottom wall |
| [`stl/rear_plate.stl`](stl/rear_plate.stl) | 108 × 98 × 20 | Bolts to the walls: rear rod sockets, motor mount (oversize holes for alignment), slotted tab for the home sensor, rear bumpers |
| [`stl/carriage.stl`](stl/carriage.stl) | 95 × 68 × 47 | The knob screws to its front; two LM8UU press-fit bores, nut seat and the home-sensor fin. The FPC runs down the hollow middle |

[`rising_knob_assembly.step`](rising_knob_assembly.step) is the whole assembly (printed parts, knob, motor, screw,
nut, rods, bearings, home sensor and a wood panel), coloured, for opening in Fusion.

### Hardware

- 1× **NEMA 17 stepper with integrated Tr8×2 lead screw** (2 mm lead) and its flanged brass nut. Cut the screw to
  **44 mm** from the motor face.
- 2× **Ø8 mm steel rod, 62 mm** long, and 2× **LM8UU** linear bearings
- 1× **slotted optical endstop board** (TCST2103 type, 33 × 10.5 mm, mounting holes 19 mm apart, as sold for
  3D printers). For a board with different hole spacing, drill the plain tab.
- 1× **TMC2209** stepper driver module (e.g. BIGTREETECH V1.3), 1× **PCF8574** I²C expander module,
  1× **VL53L1X** distance sensor
- 1× 12 V 2 A power supply, 1× 12 V → 5 V buck module (≥ 1 A), 1× 3.3 V regulator (MCP1700-3302 or AMS1117-3.3),
  100 µF 25 V capacitor, 10 kΩ resistor
- Screws:
  - 3× M2.5×6 (knob to carriage)
  - 4× M3×12 (nut flange into the carriage, self-tapping)
  - 4× M3×10 + washers (motor, through the 6 mm rear plate)
  - 6× M3×12 (rear plate to walls, self-tapping)
  - 2× M3×10 + nuts (home sensor board)
  - 4× #4 × ½" flat-head wood screws, or glue

Wiring is in the [firmware README](firmware/README.md#wiring).

### Assembly order

1. Press the LM8UU bearings into the carriage from the back, until they seat on the lip.
2. Screw the knob to the carriage (M2.5 screws from inside the tube). Feed the FPC through the slot and down the middle.
3. Press the rods into the base sockets, slide the carriage on, and bolt the rear plate on over the rod ends.
4. Thread the motor's screw into the nut. Bolt the nut flange to the carriage and the motor to the rear plate,
   leaving all screws slightly loose.
5. Run the carriage end to end by turning the screw by hand, so the nut and motor centre themselves. Then tighten.
   The oversize holes give ±0.5 mm of float; this is what keeps the screw from binding.
6. Screw the home sensor board to the tab, centred in its slots, so the carriage's fin runs through the middle of
   the sensor's slot. Then follow [Aligning the stop and start](#aligning-the-stop-and-start).

### Assumptions to check

- **Panel thickness:** 3/4" (19.05 mm). This is `T` at the top of `source/rising_knob.py`.
- **Hole in the wood:** about 53 mm. A 2-1/8" (54 mm) Forstner bit works.
- **FPC length:** the knob's 60 mm ribbon only just reaches the rear plate. Use a 10-pin, 0.5 mm FPC extension.
- **Fits:** the LM8UU bores are 15.15 mm (press fit) and the rod sockets 8.15 mm. Print the carriage first and test-fit.

## Regenerating

The model is parametric [CadQuery](https://cadquery.readthedocs.io/) code. Change the parameters at the top of
[`source/rising_knob.py`](source/rising_knob.py) (panel thickness, rod spacing, motor size, stop positions) and rebuild:

```bash
uv venv --python 3.12 .venv
uv pip install --python .venv/bin/python cadquery pillow
cd source
../.venv/bin/python rising_knob.py check export ../stl   # clash check over the stroke + STL export
mv ../stl/rising_knob_assembly.step ..
../.venv/bin/python render.py ../renders                   # offscreen VTK previews (xvfb-run -a on headless Linux)
```

`check` intersects every part with every other part at each 1 mm of the stroke, from the rear bumpers (−2.2 mm)
to the front stop (+17 mm). It also confirms the nut stays fully on the screw. The last run reported **0 clashes**,
and all three STLs are single watertight bodies. Pushing the carriage past either hard stop does show overlaps
with the stops (never with the home sensor), so the check does catch collisions.
