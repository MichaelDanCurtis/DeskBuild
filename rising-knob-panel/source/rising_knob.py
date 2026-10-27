"""Rising knob panel module for the VIEWE UEDX46460015-MD50E(T) 1.5" knob display.

v3: an Actuonix PQ12-R micro linear actuator (20 mm stroke, built-in position
control) pushes the carriage, which rides on two 6 mm rods in LM6UU bearings.
The actuator resists back-driving, so the knob holds position with no signal.

Units mm. Z points out of the wood panel; the wood's front face is z = 0.
`h` is the knob height above flush: 0 = knob face level with the wood.
"""
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

# ---- knob (VIEWE drawing) ----
HUB = -21.9          # knob rear-hub screw face, relative to the knob face
KNOB_SCREWS = [(6.0, 0.0), (-3.0, 5.196), (-3.0, -5.196)]
KNOB_PINS = [(2.57, 7.05), (2.57, -7.05)]

# ---- actuator: Actuonix PQ12 (datasheet rev D) ----
PQ_STROKE = 20.0
PQ_CLOSED_HH = 42.0               # hole to hole, fully retracted
PQ_BODY_L, PQ_BODY_W, PQ_BODY_T = 36.5, 21.5, 15.0
PQ_AXIS_FROM_EDGE = 5.5           # rod axis sits 5.5 mm from one long edge
PQ_TAB_W, PQ_ROD_D = 5.0, 5.8
E_FLUSH = 3.0                     # actuator extension when the knob is flush (firmware setting)
TRAVEL = 12.0                     # nominal raised height (firmware setting)
ACT_X, ACT_Y = -5.0, -26.0        # actuator axis, under the knob

# ---- supplied U-bracket (Actuonix): pin 7.2 mm from the base, 8 mm across, 5 mm wide ----
BR_PIN, BR_OUT, BR_W, BR_SHEET = 7.2, 8.0, 5.0, 1.0

# ---- guides: 2x 6 mm rods, LM6UU (12 x 19) ----
ROD_D, ROD_X, ROD_Y = 6.0, 35.0, -5.0
LM_D, LM_L = 12.0, 19.0
HOUSING_R = 9.5

# ---- layout (at h = 0) ----
BOSS_BACK = -27.0                       # rod sockets stick out behind the plate to here
FRONT_STOP_H = 16.0                     # carriage lands on the rod bosses
H_MIN = -E_FLUSH                        # actuator fully retracted
FRAME_FRONT = BOSS_BACK - FRONT_STOP_H
FRAME_L = LM_L + 1.0                    # bearing + 1 mm retaining lip
FRAME_BACK = FRAME_FRONT - FRAME_L
LUG_FRONT = PLATE_BACK - 0.45 - FRONT_STOP_H   # carriage lug clears the plate at the front stop
Z_LUG = LUG_FRONT - BR_W / 2            # front pin
Z_REAR = Z_LUG - PQ_CLOSED_HH - E_FLUSH  # rear pin
RP_FRONT = FRAME_BACK + H_MIN - 1.0     # rear plate front face
RP_T = 5.0
RP_BACK = RP_FRONT - RP_T
WALL_X0, WALL_T = 46.0, 6.0             # side walls at |x| = 46..52
Y_TOP, Y_BOT = 33.0, -40.0
LUG_T, POST_T = 6.0, 6.0

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


def wall_screws():
    xs = WALL_X0 + WALL_T / 2
    return [(s * xs, y) for s in (1, -1) for y in (24.0, -30.0)] + [(20.0, Y_BOT + 2.5), (-20.0, Y_BOT + 2.5)]


# ---------- printed parts ----------

def base():
    """Glued/screwed to the back of the wood: plate, rod sockets and a U-shaped cage."""
    zf, zb = PLATE_FRONT, PLATE_BACK
    xo = WALL_X0 + WALL_T
    b = fuse(box(-xo, xo, Y_BOT, Y_TOP, zb, zf),
             box(WALL_X0, xo, Y_BOT, Y_TOP, RP_FRONT, zb + 0.5),
             box(-xo, -WALL_X0, Y_BOT, Y_TOP, RP_FRONT, zb + 0.5),
             box(-WALL_X0, WALL_X0, Y_BOT, Y_BOT + 5, RP_FRONT, zb + 0.5),            # bottom wall
             *[cylz(s * ROD_X, ROD_Y, BOSS_BACK, zb + 0.5, 14) for s in (1, -1)])     # rod bosses
    cuts = [cylz(0, 0, zb - 1, zf + 1, HOLE + 1.0)]
    for s in (1, -1):
        cuts.append(cylz(s * ROD_X, ROD_Y, BOSS_BACK - 0.1, zf - 1.0, ROD_D + 0.15))  # 1 mm skin at the glue face
    for x, y in wall_screws():
        cuts.append(cylz(x, y, RP_FRONT - 0.1, RP_FRONT + 12, 2.6))                    # rear plate screws
    for x, y in ((38, 26), (-38, 26), (38, -28), (-38, -28)):
        cuts.append(cylz(x, y, zb - 0.1, zf + 0.1, 3.6))                 # #4 wood screws
        cuts.append(cone_z(x, y, zb - 0.01, zb + 2.0, 7.5, 3.6))         # countersunk from the back
    return cut(b, *cuts)


def rear_plate():
    """Holds the rods' back ends; the actuator passes through and pins to the post."""
    xo = WALL_X0 + WALL_T
    post_x1 = ACT_X - BR_PIN
    p = fuse(box(-xo, xo, Y_BOT, Y_TOP, RP_BACK, RP_FRONT),
             box(post_x1 - POST_T, post_x1, ACT_Y - 7, ACT_Y + 7, Z_REAR - 3.5, RP_BACK + 0.5))  # bracket post
    cuts = [box(-12, 12, -10, 14, RP_BACK - 1, RP_FRONT + 1),                                    # FPC opening
            box(ACT_X - PQ_AXIS_FROM_EDGE - 1, ACT_X - PQ_AXIS_FROM_EDGE + PQ_BODY_W + 1,          # actuator window
                ACT_Y - PQ_BODY_T / 2 - 1, ACT_Y + PQ_BODY_T / 2 + 1, RP_BACK - 1, RP_FRONT + 1),
            cyl((post_x1 - POST_T - 0.1, ACT_Y, Z_REAR), (post_x1 + 0.1, ACT_Y, Z_REAR), 2.6)]      # M3 bracket screw
    for s in (1, -1):
        cuts.append(cylz(s * ROD_X, ROD_Y, RP_BACK + 1.0, RP_FRONT + 0.1, ROD_D + 0.15))
    for x, y in wall_screws():
        cuts.append(cylz(x, y, RP_BACK - 0.1, RP_FRONT + 0.1, 3.4))
        cuts.append(cylz(x, y, RP_BACK - 0.1, RP_BACK + 2.5, 5.8))
    return cut(p, *cuts)


def carriage(h=0.0):
    """Knob mount, bearing housings and the actuator lug in one part."""
    ff, fb, hub = FRAME_FRONT + h, FRAME_BACK + h, HUB + h
    lug_x1 = ACT_X - BR_PIN
    c = fuse(cylz(0, 0, fb, hub, 2 * TUBE_R),                                     # tube to the knob hub
             box(-ROD_X, ROD_X, ROD_Y - 6, ROD_Y + 6, fb, ff),                    # beam between bearings
             *[cylz(s * ROD_X, ROD_Y, fb, ff, 2 * HOUSING_R) for s in (1, -1)],    # bearing housings
             box(lug_x1 - LUG_T, lug_x1, ACT_Y - 6, ROD_Y, fb, LUG_FRONT + h))    # actuator lug
    cuts = [cylz(0, 0, fb - 1, hub - FW, BORE_D),                                 # FPC passes down the middle
            box(-7.3, -3.9, -3.8, 3.8, hub - FW - 0.1, hub + 0.1),                # FPC slot in the front wall
            cyl((lug_x1 - LUG_T - 0.1, ACT_Y, Z_LUG + h), (lug_x1 + 0.1, ACT_Y, Z_LUG + h), 2.6)]  # M3 bracket screw
    for s in (1, -1):
        cuts.append(cylz(s * ROD_X, ROD_Y, fb - 0.1, fb + LM_L, LM_D + 0.15))    # LM6UU press fit
        cuts.append(cylz(s * ROD_X, ROD_Y, fb + LM_L - 0.1, ff + 0.1, 9.0))      # lip, rod clearance
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
    return cylz(x, ROD_Y, RP_BACK + 1.0, PLATE_FRONT - 1.0, ROD_D)


def rod_length():
    return (PLATE_FRONT - 1.0) - (RP_BACK + 1.0)


def bearing(x, h=0.0):
    z0 = FRAME_BACK + h
    return cut(cylz(x, ROD_Y, z0, z0 + LM_L, LM_D), cylz(x, ROD_Y, z0 - 1, z0 + LM_L + 1, ROD_D + 0.05))


def pq12_body():
    """Static part of the actuator: housing plus the rear mounting tab."""
    x0 = ACT_X - PQ_AXIS_FROM_EDGE
    zb = Z_REAR + 3.0
    body = box(x0, x0 + PQ_BODY_W, ACT_Y - PQ_BODY_T / 2, ACT_Y + PQ_BODY_T / 2, zb, zb + PQ_BODY_L)
    tab = fuse(box(ACT_X - 3, ACT_X + 3, ACT_Y - PQ_TAB_W / 2, ACT_Y + PQ_TAB_W / 2, Z_REAR, zb + 0.01),
               cyl((ACT_X, ACT_Y - PQ_TAB_W / 2, Z_REAR), (ACT_X, ACT_Y + PQ_TAB_W / 2, Z_REAR), 6.0))
    tab = cut(tab, cyl((ACT_X, ACT_Y - 3, Z_REAR), (ACT_X, ACT_Y + 3, Z_REAR), 3.0))
    return fuse(body, tab)


def pq12_rod(h=0.0):
    pin = Z_LUG + h
    r = fuse(cylz(ACT_X, ACT_Y, Z_REAR + 3.0 + PQ_BODY_L, pin, PQ_ROD_D),
             cyl((ACT_X, ACT_Y - PQ_ROD_D / 2, pin), (ACT_X, ACT_Y + PQ_ROD_D / 2, pin), PQ_ROD_D))
    return cut(r, cyl((ACT_X, ACT_Y - 3, pin), (ACT_X, ACT_Y + 3, pin), 3.0))


def u_bracket(pin_z):
    """The bent-sheet bracket Actuonix supplies, base against a face at x = ACT_X - BR_PIN."""
    x0 = ACT_X - BR_PIN
    half = BR_OUT / 2
    b = fuse(box(x0, x0 + BR_SHEET, ACT_Y - half, ACT_Y + half, pin_z - BR_W / 2, pin_z + BR_W / 2),
             *[box(x0, ACT_X, y0, y0 + BR_SHEET, pin_z - BR_W / 2, pin_z + BR_W / 2)
               for y0 in (ACT_Y - half, ACT_Y + half - BR_SHEET)],
             *[cyl((ACT_X, y0, pin_z), (ACT_X, y0 + BR_SHEET, pin_z), BR_W)
               for y0 in (ACT_Y - half, ACT_Y + half - BR_SHEET)])
    return cut(b, cyl((ACT_X, ACT_Y - half - 1, pin_z), (ACT_X, ACT_Y + half + 1, pin_z), 3.2),
               cyl((x0 - 1, ACT_Y, pin_z), (x0 + 2, ACT_Y, pin_z), 3.2))


def panel():
    return cut(box(-75, 75, -75, 75, -T, 0), cylz(0, 0, -T - 1, 1, HOLE))


def static_parts():
    return [("Wood panel", panel(), "wood"), ("Base", base(), "base"), ("Rear plate", rear_plate(), "base"),
            ("Rod L", rod(-ROD_X), "steel"), ("Rod R", rod(ROD_X), "steel"),
            ("PQ12 body", pq12_body(), "motor"), ("Rear bracket", u_bracket(Z_REAR), "steel")]


def moving_parts(h):
    return [("Carriage", carriage(h), "carriage"), ("Knob display", knob(h), "knob"),
            ("Display glass", display(h), "glass"), ("FPC", fpc(h), "fpc"),
            ("LM6UU L", bearing(-ROD_X, h), "steel"), ("LM6UU R", bearing(ROD_X, h), "steel"),
            ("PQ12 rod", pq12_rod(h), "steel"), ("Front bracket", u_bracket(Z_LUG + h), "steel")]


COLORS = {"wood": (0.62, 0.42, 0.24), "base": (0.92, 0.92, 0.9), "carriage": (0.98, 0.78, 0.15),
          "knob": (0.75, 0.76, 0.78), "glass": (0.05, 0.05, 0.07), "fpc": (0.85, 0.5, 0.15),
          "motor": (0.2, 0.22, 0.25), "steel": (0.6, 0.62, 0.66)}

# Parts that touch or interpenetrate by design.
EXPECTED = {frozenset(p) for p in [("Knob display", "Display glass"), ("Knob display", "Carriage"),
                                   ("LM6UU L", "Rod L"), ("LM6UU R", "Rod R"),
                                   ("LM6UU L", "Carriage"), ("LM6UU R", "Carriage"),
                                   ("Rod L", "Base"), ("Rod R", "Base"), ("Rod L", "Rear plate"),
                                   ("Rod R", "Rear plate"), ("PQ12 rod", "PQ12 body"),
                                   ("PQ12 rod", "Front bracket"), ("Front bracket", "Carriage"),
                                   ("Rear bracket", "Rear plate"), ("Rear bracket", "PQ12 body")]}


def overlap(a, b):
    try:
        return a.intersect(b).Volume()
    except Exception:
        return 0.0


def heights():
    return [H_MIN + i * 1.0 for i in range(int(FRONT_STOP_H - H_MIN) + 1)]


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

    print("stroke: full retract h=%.1f, flush h=0 (extension %.1f mm), raised h=%.1f, front stop h=%.1f, "
          "full extension h=%.1f" % (H_MIN, E_FLUSH, TRAVEL, FRONT_STOP_H, PQ_STROKE - E_FLUSH))
    print("rods: %.1f mm; depth behind the wood: %.1f mm" % (rod_length(), -(Z_REAR - 3.5) - T))
    print("clashes:", len(clashes))
    for c in clashes:
        print("  ", c)
    return clashes


def print_pose(name, shape):
    """Rotate/translate a printed part into its print orientation, sitting on z=0."""
    if name in ("Base", "Rear plate"):
        shape = shape.rotate(Vector(0, 0, 0), Vector(1, 0, 0), 180)   # flat face down, walls/post up
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
