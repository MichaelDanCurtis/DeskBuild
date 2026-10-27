"""Rising knob panel module for the VIEWE UEDX46460015-MD50E(T) 1.5" knob display.

v2: lead-screw drive. A NEMA 17 stepper with an integrated Tr8x2 lead screw pushes the
carriage through a brass nut; the carriage rides on two 8 mm rods in LM8UU bearings.
The screw is self-locking, so the knob holds position with the motor switched off.

Units mm. Z points out of the wood panel; the wood's front face is z = 0.
`h` is the knob height above flush: 0 = knob face level with the wood.
"""
import math
import os
import sys

import cadquery as cq
from cadquery import Solid, Vector

# ---- panel ----
T = 19.05            # wood panel thickness (3/4")
HOLE = 53.0          # hole through the wood
PT = 4.0             # base plate thickness
PLATE_FRONT = -T
PLATE_BACK = -T - PT

# ---- stroke (h values) ----
TRAVEL = 12.0        # nominal raised height (firmware setting, adjustable)
HOME_H = -1.0        # nominal home-sensor trip point (the sensor slides +/-2 mm)
REAR_STOP_H = -2.2   # carriage lands on the rear bumpers
FRONT_STOP_H = 17.0  # carriage lands on the rod bosses

# ---- knob (VIEWE drawing) ----
HUB = -21.9          # knob rear-hub screw face, relative to the knob face
KNOB_SCREWS = [(6.0, 0.0), (-3.0, 5.196), (-3.0, -5.196)]
KNOB_PINS = [(2.57, 7.05), (2.57, -7.05)]

# ---- guides: 2x 8 mm rods, LM8UU (15 x 24) ----
ROD_D, ROD_X, ROD_Y = 8.0, 36.0, -8.0
LM_D, LM_L = 15.0, 24.0
HOUSING_R = 11.5

# ---- drive: Tr8x2 screw, flanged brass nut, NEMA 17 ----
SCREW_Y = -38.0
NUT_FLANGE_D, NUT_FLANGE_T, NUT_BODY_D, NUT_L, NUT_PCD = 22.0, 3.5, 10.2, 15.0, 16.0
MOTOR_W, MOTOR_HOLES, MOTOR_PILOT, MOTOR_BODY = 42.3, 31.0, 22.0, 40.0

# ---- frame layout (at h = 0) ----
BOSS_BACK = -27.0                          # rod sockets stick out behind the plate to here
FRAME_FRONT = BOSS_BACK - (FRONT_STOP_H)   # carriage frame front face
FRAME_L = LM_L + 1.0                       # bearing + 1 mm retaining lip
FRAME_BACK = FRAME_FRONT - FRAME_L
RP_FRONT = -77.0                           # rear plate front face
RP_T = 6.0
RP_BACK = RP_FRONT - RP_T                  # motor face
WALL_X0, WALL_T = 49.0, 5.0                # side walls at |x| = 49..54
Y_TOP, Y_BOT = 36.0, -62.0

# ---- home sensor: slotted optical endstop board (TCST2103 type, 33 x 10.5 mm PCB) ----
# A fin on the carriage passes through the sensor's slot, which is open top and
# bottom, so overtravel can't hit anything. The board slides on two slots in the
# tab to move the trip point +/-2 mm.
OPT_Y = -36.0                        # slot centre line
OPT_X_TIP = 15.0                     # prong tips face the carriage (-x)
OPT_ZC = FRAME_BACK + HOME_H         # beam height: the fin's bottom edge trips at h = HOME_H
OPT_L, OPT_H, OPT_W = 24.5, 10.8, 6.3          # body length (y), prong height (x), width (z)
OPT_SLOT_W, OPT_SLOT_D = 3.1, 7.2
PCB_L, PCB_W, PCB_T = 33.0, 10.5, 1.6
PCB_HOLE_SPACING = 19.0
TAB_T = 4.0
FIN_T, FIN_REACH = 1.6, 6.5

TUBE_R, BORE_D, FW = 16.0, 20.0, 5.0


def box(x0, x1, y0, y1, z0, z1):
    return Solid.makeBox(x1 - x0, y1 - y0, z1 - z0, pnt=Vector(x0, y0, z0))


def cyl(p1, p2, d):
    a, b = Vector(*p1), Vector(*p2)
    return Solid.makeCylinder(d / 2, (b - a).Length, a, (b - a).normalized())


def cylz(x, y, z0, z1, d):
    return cyl((x, y, z0), (x, y, z1), d)


def cone_z(x, y, z0, z1, d0, d1):
    return Solid.makeCone(d0 / 2, d1 / 2, z1 - z0, Vector(x, y, z0), Vector(0, 0, 1))


def fuse(a, *rest):
    return a.fuse(*rest).clean()


def cut(a, *rest):
    return a.cut(*rest).clean()


def slot_y(x, y0, y1, z0, z1, d):
    """Slot along y (a stadium) through z."""
    return fuse(cylz(x, y0, z0, z1, d), cylz(x, y1, z0, z1, d), box(x - d / 2, x + d / 2, y0, y1, z0, z1))


def slot_x(x0, x1, y, z0, z1, d):
    """Slot running along z, cut through x (for the sensor tab)."""
    return fuse(cyl((x0, y, z0), (x1, y, z0), d), cyl((x0, y, z1), (x1, y, z1), d),
                box(x0, x1, y - d / 2, y + d / 2, z0, z1))


def tab_x0():
    return OPT_X_TIP + OPT_H + PCB_T


def nut_holes():
    return [(NUT_PCD / 2 * math.cos(math.radians(a)), SCREW_Y + NUT_PCD / 2 * math.sin(math.radians(a)))
            for a in (45, 135, 225, 315)]


# ---------- printed parts ----------

def base():
    """Glued/screwed to the back of the wood: plate, rod sockets and a U-shaped cage."""
    zf, zb = PLATE_FRONT, PLATE_BACK
    walls = [box(s * WALL_X0 if s > 0 else -WALL_X0 - WALL_T, s * WALL_X0 + WALL_T if s > 0 else -WALL_X0,
                 Y_BOT, Y_TOP, RP_FRONT, zb + 0.5) for s in (1, -1)]
    b = fuse(box(-WALL_X0 - WALL_T, WALL_X0 + WALL_T, Y_BOT, Y_TOP, zb, zf),
             *walls,
             box(-WALL_X0, WALL_X0, Y_BOT, Y_BOT + 5, RP_FRONT, zb + 0.5),             # bottom wall
             *[cylz(s * ROD_X, ROD_Y, BOSS_BACK, zb + 0.5, 16) for s in (1, -1)])      # rod bosses
    cuts = [cylz(0, 0, zb - 1, zf + 1, HOLE + 1.0)]
    for s in (1, -1):
        cuts.append(cylz(s * ROD_X, ROD_Y, BOSS_BACK - 0.1, zf - 1.0, ROD_D + 0.15))  # 1 mm skin at the glue face
        for y in (25.0, -50.0):
            cuts.append(cylz(s * (WALL_X0 + WALL_T / 2), y, RP_FRONT - 0.1, RP_FRONT + 12, 2.6))  # rear plate screws
    for x in (20.0, -20.0):
        cuts.append(cyl((x, Y_BOT + 2.5, RP_FRONT - 0.1), (x, Y_BOT + 2.5, RP_FRONT + 12), 2.6))
    for x, y in ((42, 30), (-42, 30), (40, -50), (-40, -50)):
        cuts.append(cylz(x, y, zb - 0.1, zf + 0.1, 3.6))                 # #4 wood screws
        cuts.append(cone_z(x, y, zb - 0.01, zb + 2.0, 7.5, 3.6))         # countersunk from the back
    return cut(b, *cuts)


def rear_plate():
    """Holds the rods' back ends, the motor, the home sensor and the rear bumpers."""
    p = fuse(box(-WALL_X0 - WALL_T, WALL_X0 + WALL_T, Y_BOT, Y_TOP, RP_BACK, RP_FRONT),
             *[cylz(s * 22.0, ROD_Y, RP_FRONT - 0.5, FRAME_BACK + REAR_STOP_H, 9) for s in (1, -1)],  # bumpers
             box(tab_x0(), tab_x0() + TAB_T, OPT_Y - PCB_L / 2, OPT_Y + PCB_L / 2,
                 RP_FRONT - 0.5, OPT_ZC + PCB_W / 2 + 2.0))                                          # sensor tab
    cuts = [box(-12, 12, -10, 14, RP_BACK - 1, RP_FRONT + 1),                   # FPC opening
            cylz(0, SCREW_Y, RP_BACK - 1, RP_FRONT + 1, MOTOR_PILOT + 0.6)]      # motor pilot
    for s in (1, -1):
        cuts.append(cylz(s * ROD_X, ROD_Y, RP_BACK + 1.0, RP_FRONT + 0.1, ROD_D + 0.15))
        for y in (25.0, -50.0):
            x = s * (WALL_X0 + WALL_T / 2)
            cuts.append(cylz(x, y, RP_BACK - 0.1, RP_FRONT + 0.1, 3.4))
            cuts.append(cylz(x, y, RP_BACK - 0.1, RP_BACK + 3.0, 6.2))
    for x in (20.0, -20.0):
        cuts.append(cylz(x, Y_BOT + 2.5, RP_BACK - 0.1, RP_FRONT + 0.1, 3.4))
        cuts.append(cylz(x, Y_BOT + 2.5, RP_BACK - 0.1, RP_BACK + 3.0, 6.2))
    for sx in (1, -1):
        for sy in (1, -1):  # motor screws, oversize for +/-0.5 mm alignment float
            cuts.append(cylz(sx * MOTOR_HOLES / 2, SCREW_Y + sy * MOTOR_HOLES / 2, RP_BACK - 0.1, RP_FRONT + 0.1, 4.2))
    for dy in (-PCB_HOLE_SPACING / 2, PCB_HOLE_SPACING / 2):  # slide the sensor +/-2 mm to set the trip point
        cuts.append(slot_x(tab_x0() - 0.1, tab_x0() + TAB_T + 0.1, OPT_Y + dy, OPT_ZC - 2.0, OPT_ZC + 2.0, 3.4))
    return cut(p, *cuts)


def carriage(h=0.0):
    """Knob mount, bearing housings and lead-screw nut seat in one part."""
    ff, fb, hub = FRAME_FRONT + h, FRAME_BACK + h, HUB + h
    c = fuse(cylz(0, 0, fb, hub, 2 * TUBE_R),                                     # tube to the knob hub
             box(-ROD_X, ROD_X, ROD_Y - 9, ROD_Y + 9, fb, ff),                    # beam between bearings
             *[cylz(s * ROD_X, ROD_Y, fb, ff, 2 * HOUSING_R) for s in (1, -1)],    # bearing housings
             box(-14, 14, SCREW_Y, ROD_Y, fb, ff), cylz(0, SCREW_Y, fb, ff, 28),   # nut lobe
             box(11, OPT_X_TIP + FIN_REACH, OPT_Y - FIN_T / 2, OPT_Y + FIN_T / 2, fb, ff))  # home-sensor fin
    cuts = [cylz(0, 0, fb - 1, hub - FW, BORE_D),                                 # FPC passes down the middle
            box(-7.3, -3.9, -3.8, 3.8, hub - FW - 0.1, hub + 0.1),                # FPC slot in the front wall
            cylz(0, SCREW_Y, fb - 1, ff + 1, NUT_BODY_D + 0.4)]                   # nut body / screw clearance
    for s in (1, -1):
        cuts.append(cylz(s * ROD_X, ROD_Y, fb - 0.1, fb + LM_L, LM_D + 0.15))    # LM8UU press fit
        cuts.append(cylz(s * ROD_X, ROD_Y, fb + LM_L - 0.1, ff + 0.1, 12.0))     # lip, rod clearance
    for x, y in nut_holes():
        cuts.append(cylz(x, y, fb - 0.1, fb + 12, 2.6))                           # M3 self-tapping, nut flange
    for x, y in KNOB_SCREWS:
        cuts.append(cylz(x, y, hub - FW - 0.1, hub + 0.1, 2.8))
        cuts.append(cylz(x, y, hub - FW - 0.1, hub - FW + 2, 5.0))
    for x, y in KNOB_PINS:
        cuts.append(cylz(x, y, hub - 3, hub + 0.1, 1.9))
    return cut(c, *cuts)


# ---------- reference (bought / existing) parts ----------

def knob(h=0.0):
    k = fuse(cylz(0, 0, -11.5 + h, h, 51.6), cylz(0, 0, -14 + h, -11.49 + h, 51.0),
             cylz(0, 0, -18.9 + h, -13.99 + h, 24.3), cylz(0, 0, HUB + h, -18.89 + h, 21),
             *[cylz(x, y, -23.95 + h, HUB + 0.5 + h, 1.5) for x, y in KNOB_PINS])
    return cut(k, *[cylz(x, y, HUB - 0.05 + h, HUB + 5 + h, 2.5) for x, y in KNOB_SCREWS])


def display(h=0.0):
    return cylz(0, 0, -0.3 + h, 0.05 + h, 32.4)


def fpc(h=0.0):
    return box(-5.75, -5.45, -2.8, 2.8, HUB - 60 + h, HUB + h)


def rod(x):
    return cylz(x, ROD_Y, RP_FRONT - 5.0, PLATE_FRONT - 1.0, ROD_D)


def rod_length():
    return (PLATE_FRONT - 1.0) - (RP_FRONT - 5.0)


def bearing(x, h=0.0):
    z0 = FRAME_BACK + h
    return cut(cylz(x, ROD_Y, z0, z0 + LM_L, LM_D), cylz(x, ROD_Y, z0 - 1, z0 + LM_L + 1, ROD_D + 0.05))


def nut(h=0.0):
    z0 = FRAME_BACK + h
    n = fuse(cylz(0, SCREW_Y, z0 - NUT_FLANGE_T, z0, NUT_FLANGE_D),
             cylz(0, SCREW_Y, z0 - 0.01, z0 - NUT_FLANGE_T + NUT_L, NUT_BODY_D))
    return cut(n, cylz(0, SCREW_Y, z0 - 5, z0 + NUT_L, 8.0),
               *[cylz(x, y, z0 - NUT_FLANGE_T - 0.1, z0 + 0.1, 3.5) for x, y in nut_holes()])


def screw_tip_z():
    return FRAME_BACK - NUT_FLANGE_T + NUT_L + FRONT_STOP_H + 1.0


def lead_screw():
    return cylz(0, SCREW_Y, RP_BACK + 2.0, screw_tip_z(), 8.0)


def motor():
    m = fuse(box(-MOTOR_W / 2, MOTOR_W / 2, SCREW_Y - MOTOR_W / 2, SCREW_Y + MOTOR_W / 2, RP_BACK - MOTOR_BODY, RP_BACK),
             cylz(0, SCREW_Y, RP_BACK - 0.01, RP_BACK + 2.0, MOTOR_PILOT))
    return m


def opto_sensor():
    """U-shaped slot sensor (prongs toward the carriage) on its board."""
    body = box(OPT_X_TIP, OPT_X_TIP + OPT_H, OPT_Y - OPT_L / 2, OPT_Y + OPT_L / 2, OPT_ZC - OPT_W / 2, OPT_ZC + OPT_W / 2)
    body = cut(body, box(OPT_X_TIP - 0.1, OPT_X_TIP + OPT_SLOT_D, OPT_Y - OPT_SLOT_W / 2, OPT_Y + OPT_SLOT_W / 2,
                         OPT_ZC - OPT_W, OPT_ZC + OPT_W))
    pcb = box(OPT_X_TIP + OPT_H, tab_x0(), OPT_Y - PCB_L / 2, OPT_Y + PCB_L / 2, OPT_ZC - PCB_W / 2, OPT_ZC + PCB_W / 2)
    return fuse(body, pcb)


def panel():
    return cut(box(-75, 75, -75, 75, -T, 0), cylz(0, 0, -T - 1, 1, HOLE))


def static_parts():
    return [("Wood panel", panel(), "wood"), ("Base", base(), "base"), ("Rear plate", rear_plate(), "base"),
            ("Rod L", rod(-ROD_X), "steel"), ("Rod R", rod(ROD_X), "steel"), ("Motor NEMA17", motor(), "motor"),
            ("Lead screw", lead_screw(), "steel"), ("Home sensor", opto_sensor(), "switch")]


def moving_parts(h):
    return [("Carriage", carriage(h), "carriage"), ("Knob display", knob(h), "knob"),
            ("Display glass", display(h), "glass"), ("FPC", fpc(h), "fpc"),
            ("LM8UU L", bearing(-ROD_X, h), "steel"), ("LM8UU R", bearing(ROD_X, h), "steel"),
            ("Nut", nut(h), "brass")]


COLORS = {"wood": (0.62, 0.42, 0.24), "base": (0.92, 0.92, 0.9), "carriage": (0.98, 0.78, 0.15),
          "knob": (0.75, 0.76, 0.78), "glass": (0.05, 0.05, 0.07), "fpc": (0.85, 0.5, 0.15),
          "motor": (0.2, 0.22, 0.25), "steel": (0.6, 0.62, 0.66), "brass": (0.8, 0.62, 0.25),
          "switch": (0.15, 0.35, 0.85)}

# Parts that touch or interpenetrate by design.
EXPECTED = {frozenset(p) for p in [("Knob display", "Display glass"), ("Nut", "Lead screw"),
                                   ("LM8UU L", "Rod L"), ("LM8UU R", "Rod R"),
                                   ("Nut", "Carriage"), ("LM8UU L", "Carriage"), ("LM8UU R", "Carriage"),
                                   ("Knob display", "Carriage"),
                                   ("Rod L", "Base"), ("Rod R", "Base"), ("Rod L", "Rear plate"),
                                   ("Rod R", "Rear plate"), ("Motor NEMA17", "Rear plate"),
                                   ("Lead screw", "Motor NEMA17"), ("Home sensor", "Rear plate")]}


def overlap(a, b):
    try:
        return a.intersect(b).Volume()
    except Exception:
        return 0.0


def heights():
    hs = [REAR_STOP_H, HOME_H, 0.0]
    hs += [x * 1.0 for x in range(1, int(FRONT_STOP_H))] + [FRONT_STOP_H]
    return hs


def check():
    statics = [(n, b) for n, b, _ in static_parts()]
    clashes = []
    for i, (n1, b1) in enumerate(statics):
        for n2, b2 in statics[i + 1:]:
            if frozenset((n1, n2)) in EXPECTED:
                continue
            v = overlap(b1, b2)
            if v > 0.1:
                clashes.append("static %s x %s %.2f mm3" % (n1, n2, v))
    for h in heights():
        movs = [(n, b) for n, b, _ in moving_parts(h)]
        for mn, mb in movs:
            for sn, sb in statics:
                if frozenset((mn, sn)) in EXPECTED:
                    continue
                v = overlap(mb, sb)
                if v > 0.1:
                    clashes.append("h=%5.1f  %s x %s %.2f mm3" % (h, mn, sn, v))
        for i, (n1, b1) in enumerate(movs):
            for n2, b2 in movs[i + 1:]:
                if frozenset((n1, n2)) in EXPECTED:
                    continue
                v = overlap(b1, b2)
                if v > 0.1:
                    clashes.append("h=%5.1f  %s x %s %.2f mm3" % (h, n1, n2, v))

    # Screw engagement: the nut must stay fully on the screw over the whole stroke.
    nut_front_max = FRAME_BACK - NUT_FLANGE_T + NUT_L + FRONT_STOP_H
    engaged = screw_tip_z() >= nut_front_max and RP_BACK + 2.0 <= FRAME_BACK - NUT_FLANGE_T + REAR_STOP_H
    print("stroke: rear stop h=%.1f, home trip h=%.1f, flush h=0, raised h=%.1f, front stop h=%.1f"
          % (REAR_STOP_H, HOME_H, TRAVEL, FRONT_STOP_H))
    fin_clear = OPT_SLOT_W / 2 - FIN_T / 2
    print("home sensor: trips at h=%.1f, fin clears the slot walls by %.2f mm, open slot so overtravel is free"
          % (HOME_H, fin_clear))
    print("lead screw from motor face: %.1f mm, rods: %.1f mm, nut engaged over stroke: %s"
          % (screw_tip_z() - RP_BACK, rod_length(), engaged))
    print("depth behind the wood: %.1f mm (to motor back, %g mm motor)" % (-(RP_BACK - MOTOR_BODY) - T, MOTOR_BODY))
    print("clashes:", len(clashes))
    for c in clashes:
        print("  ", c)
    return clashes


def print_pose(name, shape):
    """Rotate/translate a printed part into its print orientation, sitting on z=0."""
    if name == "Base":
        shape = shape.rotate(Vector(0, 0, 0), Vector(1, 0, 0), 180)   # glue face down
    bb = shape.BoundingBox()
    return shape.translate(Vector(-(bb.xmin + bb.xmax) / 2, -(bb.ymin + bb.ymax) / 2, -bb.zmin))


def export(out):
    os.makedirs(out, exist_ok=True)
    printed = {"Base": base(), "Rear plate": rear_plate(), "Carriage": carriage()}
    for n, s in printed.items():
        fn = os.path.join(out, n.lower().replace(" ", "_") + ".stl")
        posed = print_pose(n, s)
        cq.exporters.export(posed, fn, tolerance=0.01, angularTolerance=0.1)
        bb = posed.BoundingBox()
        print("STL %-12s %5.1f x %5.1f x %5.1f mm  %6.1f cm3" % (n, bb.xlen, bb.ylen, bb.zlen, s.Volume() / 1000))
    assy = cq.Assembly(name="Rising knob")
    for n, s, k in static_parts() + moving_parts(0.0):
        assy.add(s, name=n, color=cq.Color(*COLORS[k]))
    assy.save(os.path.join(out, "rising_knob_assembly.step"))
    print("STEP saved")


if __name__ == "__main__":
    if "check" in sys.argv:
        check()
    if "export" in sys.argv:
        export(sys.argv[sys.argv.index("export") + 1])
