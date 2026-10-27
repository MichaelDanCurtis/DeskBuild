"""Rising knob panel module for the VIEWE UEDX46460015-MD50E(T) 1.5" knob display.

Units mm. Z points out of the wood panel; the wood's front face is z = 0.
The knob is flush at h = 0 and rises TRAVEL mm when the servo turns 180 degrees.
"""
import math
import os
import sys

import cadquery as cq
from cadquery import Solid, Vector

# ---- parameters ----
T = 19.05          # wood panel thickness (3/4")
PT = 3.0           # mount plate thickness
TRAVEL = 12.0      # how far the knob face rises above the wood
HOLE = 53.0        # hole drilled through the wood
ROD_X = 32.0       # guide rods at x = +/-ROD_X
FW = 4.0           # carriage front wall (the knob screws to it)
HUB = -21.9        # knob rear-hub screw face, relative to the knob face
PLATE_BACK = -T - PT
COL_BACK = PLATE_BACK - 5 - 1.85 - TRAVEL - 12   # carriage rear face when flush
R_CRANK = TRAVEL / 2
L_LINK = 18.0
LINE_Y = -28.0                                   # slider-crank line of action
PIN_Z0 = COL_BACK + 6                            # carriage pin height when flush
Z_CRANK = PIN_Z0 + R_CRANK - L_LINK              # servo shaft axis height

KNOB_SCREWS = [(6.0, 0.0), (-3.0, 5.196), (-3.0, -5.196)]
KNOB_PINS = [(2.57, 7.05), (2.57, -7.05)]


def box(x0, x1, y0, y1, z0, z1):
    return Solid.makeBox(x1 - x0, y1 - y0, z1 - z0, pnt=Vector(x0, y0, z0))


def obox(c, ldir, wdir, L, Wd, H):
    l = Vector(*ldir).normalized()
    w = Vector(*wdir).normalized()
    plane = cq.Plane(origin=Vector(*c), xDir=l, normal=l.cross(w))
    return Solid.makeBox(L, Wd, H, pnt=Vector(-L / 2, -Wd / 2, -H / 2)).moved(cq.Location(plane))


def cyl(p1, p2, d, d2=None):
    a, b = Vector(*p1), Vector(*p2)
    h = (b - a).Length
    if d2 is None:
        return Solid.makeCylinder(d / 2, h, a, (b - a).normalized())
    return Solid.makeCone(d / 2, d2 / 2, h, a, (b - a).normalized())


def cylz(x, y, z0, z1, d, d2=None):
    return cyl((x, y, z0), (x, y, z1), d, d2)


def cylx(x0, x1, y, z, d):
    return cyl((x0, y, z), (x1, y, z), d)


def fuse(a, *rest):
    return a.fuse(*rest).clean()


def cut(a, *rest):
    return a.cut(*rest).clean()


def rrect(x0, x1, y0, y1, z0, z1, r):
    parts = [box(x0, x1, y0 + r, y1 - r, z0, z1)]
    for x in (x0 + r, x1 - r):
        for y in (y0 + r, y1 - r):
            parts.append(cylz(x, y, z0, z1, 2 * r))
    return fuse(box(x0 + r, x1 - r, y0, y1, z0, z1), *parts)


# ---------- printed parts ----------

def mount_base():
    zb, zf = PLATE_BACK, -T
    base = fuse(rrect(-48, 48, -54, 31, zb, zf, 6),
                cylz(ROD_X, 0, zb - 5, zb + 0.5, 12), cylz(-ROD_X, 0, zb - 5, zb + 0.5, 12),
                box(40, 46, -14, 14, -54, zb + 0.5), box(-46, -40, -14, 14, -54, zb + 0.5),
                box(-23.3, -19.3, -52, -16.5, -70, zb + 0.5),         # servo wall
                box(-46, -19.3, -18, -14, -40, zb + 0.5))             # rib to the side wall
    cuts = [cylz(0, 0, zb - 6, zf + 1, 54)]
    for x in (ROD_X, -ROD_X):
        cuts.append(cylz(x, 0, zb - 5.1, zf - 1, 4.15))         # rod sockets, 1 mm skin in front
    for x in (43, -43):
        for y in (9, -9):
            cuts.append(cylz(x, y, -54.1, -40, 2.5))            # bridge screw pilots
    cuts.append(box(-24, -18.5, -45.1, -21.6, Z_CRANK - 6.5, Z_CRANK + 6.5))    # servo window
    for y in (-47.1, -19.6):
        cuts.append(cylx(-24, -18.5, y, Z_CRANK, 1.8))          # servo screw pilots
    for x, y in ((42, 25), (-42, 25), (42, -48), (-42, -48)):
        cuts.append(cylz(x, y, zb - 0.1, zf + 0.1, 3.6))        # wood screws
        cuts.append(cylz(x, y, zb - 0.01, zb + 1.95, 7.5, 3.6))  # countersink from the back
    return cut(base, *cuts)


def rear_bridge():
    b = rrect(-46, 46, -16, 16, -62, -54, 4)
    cuts = [cylz(0, 0, -62.1, -53.9, 24)]
    for x in (ROD_X, -ROD_X):
        cuts.append(cylz(x, 0, -60.05, -53.9, 4.15))
    for x in (43, -43):
        for y in (9, -9):
            cuts.append(cylz(x, y, -62.1, -53.9, 3.4))
            cuts.append(cylz(x, y, -62.1, -59, 6.2))
    return cut(b, *cuts)


def carriage(h=0.0):
    cb, hub = COL_BACK + h, HUB + h
    c = fuse(cylz(0, 0, cb, hub, 30),
             box(13, 26, -6, 6, cb, cb + 12), box(-26, -13, -6, 6, cb, cb + 12),
             cylz(ROD_X, 0, cb, cb + 12, 14), cylz(-ROD_X, 0, cb, cb + 12, 14),
             box(-0.2, 3.8, LINE_Y, -12, cb, cb + 10), cylx(-0.2, 3.8, LINE_Y, cb + 6, 8))
    cuts = [cylz(0, 0, cb - 1, hub - FW, 21),
            cylz(ROD_X, 0, cb - 1, cb + 13, 8.15), cylz(-ROD_X, 0, cb - 1, cb + 13, 8.15),
            box(-7.3, -3.9, -3.8, 3.8, hub - FW - 0.1, hub + 0.1),     # FPC slot
            cylx(-1, 4.6, LINE_Y, cb + 6, 2.6)]                       # link pin (M3 self-tap)
    for x, y in KNOB_SCREWS:
        cuts.append(cylz(x, y, hub - FW - 0.1, hub + 0.1, 2.8))
        cuts.append(cylz(x, y, hub - FW - 0.1, hub - FW + 2, 5.0))
    for x, y in KNOB_PINS:
        cuts.append(cylz(x, y, hub - 3, hub + 0.1, 1.9))
    return cut(c, *cuts)


def crank_pin(phi):
    return (LINE_Y + R_CRANK * math.sin(phi), Z_CRANK - R_CRANK * math.cos(phi))


def link_pin(phi):
    yc, zc = crank_pin(phi)
    return (LINE_Y, zc + math.sqrt(L_LINK ** 2 - (R_CRANK * math.sin(phi)) ** 2))


def height(phi):
    return link_pin(phi)[1] - PIN_Z0


def crank(phi=0.0):
    d = (0, math.sin(phi), -math.cos(phi))
    yp, zp = crank_pin(phi)
    k = fuse(cylx(-11.5, -4.7, LINE_Y, Z_CRANK, 9),
             obox((-6.2, LINE_Y + d[1] * 3, Z_CRANK + d[2] * 3), d, (1, 0, 0), 6, 3, 7),
             cylx(-9.7, -4.7, yp, zp, 7))
    return cut(k,
               cylx(-11.6, -7.7, LINE_Y, Z_CRANK, 4.7),      # servo spline
               cylx(-7.8, -4.6, LINE_Y, Z_CRANK, 2.3),       # horn screw
               cylx(-6.5, -4.6, LINE_Y, Z_CRANK, 4.2),       # horn screw head
               cylx(-9.8, -4.6, yp, zp, 2.6))                # crank pin (M3 self-tap)


def link(phi=0.0):
    yc, zc = crank_pin(phi)
    yl, zl = link_pin(phi)
    k = fuse(obox((-2.7, (yc + yl) / 2, (zc + zl) / 2), (0, yl - yc, zl - zc), (1, 0, 0), L_LINK, 3, 6.5),
             cylx(-4.2, -1.2, yc, zc, 6.5), cylx(-4.2, -1.2, yl, zl, 6.5))
    return cut(k, cylx(-4.3, -1.1, yc, zc, 3.2), cylx(-4.3, -1.1, yl, zl, 3.2))


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


def servo():
    return fuse(box(-35.2, -12.5, -44.6, -22.1, Z_CRANK - 6.1, Z_CRANK + 6.1),
                box(-19.3, -16.8, -49.45, -17.25, Z_CRANK - 6.1, Z_CRANK + 6.1),
                cylx(-12.6, -8, LINE_Y, Z_CRANK, 4.5))


def rod(x):
    return cylz(x, 0, -60.05, -20.05, 4)


def bearing(x, h=0.0):
    return cut(cylz(x, 0, COL_BACK + h, COL_BACK + 12 + h, 8), cylz(x, 0, COL_BACK + h - 1, COL_BACK + 13 + h, 4.2))


def panel():
    return cut(box(-75, 75, -75, 75, -T, 0), cylz(0, 0, -T - 1, 1, HOLE))


def static_parts():
    return [("Wood panel", panel(), "wood"), ("Mount base", mount_base(), "base"),
            ("Rear bridge", rear_bridge(), "base"), ("Servo MG90S", servo(), "servo"),
            ("Guide rod L", rod(-ROD_X), "steel"), ("Guide rod R", rod(ROD_X), "steel")]


def moving_parts(phi):
    h = height(phi)
    return [("Carriage", carriage(h), "carriage"), ("Crank", crank(phi), "link"),
            ("Link", link(phi), "link"), ("Knob display", knob(h), "knob"),
            ("Display glass", display(h), "glass"), ("FPC", fpc(h), "fpc"),
            ("LM4UU L", bearing(-ROD_X, h), "steel"), ("LM4UU R", bearing(ROD_X, h), "steel")]


COLORS = {"wood": (0.62, 0.42, 0.24), "base": (0.92, 0.92, 0.9), "carriage": (0.98, 0.78, 0.15),
          "link": (0.85, 0.2, 0.18), "knob": (0.75, 0.76, 0.78), "glass": (0.05, 0.05, 0.07),
          "fpc": (0.85, 0.5, 0.15), "servo": (0.15, 0.35, 0.85), "steel": (0.6, 0.62, 0.66)}


def overlap(a, b):
    try:
        return a.intersect(b).Volume()
    except Exception:
        return 0.0


def check():
    statics = [(n, b) for n, b, _ in static_parts()]
    clashes = []
    for i, (n1, b1) in enumerate(statics):
        for n2, b2 in statics[i + 1:]:
            v = overlap(b1, b2)
            if v > 0.1:
                clashes.append("static %s x %s %.2f mm3" % (n1, n2, v))
    skip = {frozenset(("Knob display", "Display glass"))}
    for deg in range(0, 181, 15):
        phi = math.radians(deg)
        movs = [(n, b) for n, b, _ in moving_parts(phi)]
        for mn, mb in movs:
            for sn, sb in statics:
                v = overlap(mb, sb)
                if v > 0.1:
                    clashes.append("%3d deg h=%5.2f  %s x %s %.2f mm3" % (deg, height(phi), mn, sn, v))
        for i, (n1, b1) in enumerate(movs):
            for n2, b2 in movs[i + 1:]:
                if frozenset((n1, n2)) in skip:
                    continue
                v = overlap(b1, b2)
                if v > 0.1:
                    clashes.append("%3d deg h=%5.2f  %s x %s %.2f mm3" % (deg, height(phi), n1, n2, v))
    print("heights every 30 deg:", [round(height(math.radians(d)), 2) for d in range(0, 181, 30)])
    print("max link angle: %.1f deg" % math.degrees(math.asin(R_CRANK / L_LINK)))
    print("clashes:", len(clashes))
    for c in clashes:
        print("  ", c)
    return clashes


def print_pose(name, shape):
    """Rotate/translate a printed part into its print orientation, sitting on z=0."""
    if name == "Mount base":
        shape = shape.rotate(Vector(0, 0, 0), Vector(1, 0, 0), 180)   # glue face down
    elif name in ("Crank", "Link"):
        shape = shape.rotate(Vector(0, 0, 0), Vector(0, 1, 0), -90)   # flat face down
    bb = shape.BoundingBox()
    return shape.translate(Vector(-(bb.xmin + bb.xmax) / 2, -(bb.ymin + bb.ymax) / 2, -bb.zmin))


def export(out):
    os.makedirs(out, exist_ok=True)
    printed = {"Mount base": mount_base(), "Rear bridge": rear_bridge(), "Carriage": carriage(),
               "Crank": crank(), "Link": link()}
    for n, s in printed.items():
        fn = os.path.join(out, n.lower().replace(" ", "_") + ".stl")
        cq.exporters.export(print_pose(n, s), fn, tolerance=0.01, angularTolerance=0.1)
        bb = print_pose(n, s).BoundingBox()
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
