import adsk.core, adsk.fusion, math

P = adsk.core.Point3D.create
V = adsk.core.Vector3D.create
UNION = adsk.fusion.BooleanTypes.UnionBooleanType
DIFF = adsk.fusion.BooleanTypes.DifferenceBooleanType
INTERSECT = adsk.fusion.BooleanTypes.IntersectionBooleanType
S = 0.1  # mm -> cm
# author in Z-up mm (z out of the panel, wood front face at z=0); the design is Y-up cm
W = lambda x, y, z: P(x * S, z * S, -y * S)
Wv = lambda x, y, z: V(x, z, -y)

# ---- parameters (mm) ----
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
SPLINE_TIP = -8.0                                # servo spline top (x)

KNOB_SCREWS = [(6.0, 0.0), (-3.0, 5.196), (-3.0, -5.196)]
KNOB_PINS = [(2.57, 7.05), (2.57, -7.05)]

tbm = adsk.fusion.TemporaryBRepManager.get()


def box(x0, x1, y0, y1, z0, z1):
    c = W((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)
    obb = adsk.core.OrientedBoundingBox3D.create(c, Wv(1, 0, 0), Wv(0, 1, 0),
                                                 (x1 - x0) * S, (y1 - y0) * S, (z1 - z0) * S)
    return tbm.createBox(obb)


def obox(c, ldir, wdir, L, Wd, H):
    obb = adsk.core.OrientedBoundingBox3D.create(W(*c), Wv(*ldir), Wv(*wdir), L * S, Wd * S, H * S)
    return tbm.createBox(obb)


def cyl(p1, p2, d, d2=None):
    return tbm.createCylinderOrCone(W(*p1), d / 2 * S, W(*p2), (d if d2 is None else d2) / 2 * S)


def cylz(x, y, z0, z1, d, d2=None):
    return cyl((x, y, z0), (x, y, z1), d, d2)


def cylx(x0, x1, y, z, d):
    return cyl((x0, y, z), (x1, y, z), d)


def merge(target, *tools, op=UNION):
    for t in tools:
        if not tbm.booleanOperation(target, t, op):
            raise RuntimeError("boolean failed")
    return target


def rrect(x0, x1, y0, y1, z0, z1, r):
    b = box(x0 + r, x1 - r, y0, y1, z0, z1)
    merge(b, box(x0, x1, y0 + r, y1 - r, z0, z1))
    for x in (x0 + r, x1 - r):
        for y in (y0 + r, y1 - r):
            merge(b, cylz(x, y, z0, z1, 2 * r))
    return b


# ---------- printed parts ----------

def mount_base():
    zb, zf = PLATE_BACK, -T
    base = rrect(-48, 48, -54, 31, zb, zf, 6)
    merge(base,
          cylz(ROD_X, 0, zb - 5, zb + 0.5, 12), cylz(-ROD_X, 0, zb - 5, zb + 0.5, 12),
          box(40, 46, -14, 14, -54, zb + 0.5), box(-46, -40, -14, 14, -54, zb + 0.5),
          box(-23.3, -19.3, -52, -15, -70, zb + 0.5),           # servo wall
          box(-46, -19.3, -18, -14, -40, zb + 0.5))             # rib to the side wall
    cuts = [cylz(0, 0, zb - 6, zf + 1, 54)]
    for x in (ROD_X, -ROD_X):
        cuts.append(cylz(x, 0, zb - 5.1, zf - 1, 4.15))         # rod sockets, 1 mm skin in front
    for x in (43, -43):
        for y in (9, -9):
            cuts.append(cylz(x, y, -54.1, -40, 2.5))            # bridge screw pilots
    cuts.append(box(-24, -18.5, -45.1, -21.6, -65.4, -52.4))    # servo window
    for y in (-47.1, -19.6):
        cuts.append(cylx(-24, -18.5, y, Z_CRANK, 1.8))          # servo screw pilots
    for x, y in ((42, 25), (-42, 25), (42, -48), (-42, -48)):
        cuts.append(cylz(x, y, zb - 0.1, zf + 0.1, 3.6))        # wood screws
        cuts.append(cylz(x, y, zb - 0.01, zb + 1.95, 7.5, 3.6))  # countersink from the back
    return merge(base, *cuts, op=DIFF)


def rear_bridge():
    b = rrect(-46, 46, -16, 16, -62, -54, 4)
    cuts = [cylz(0, 0, -62.1, -53.9, 24)]
    for x in (ROD_X, -ROD_X):
        cuts.append(cylz(x, 0, -60.05, -53.9, 4.15))
    for x in (43, -43):
        for y in (9, -9):
            cuts.append(cylz(x, y, -62.1, -53.9, 3.4))
            cuts.append(cylz(x, y, -62.1, -59, 6.2))
    return merge(b, *cuts, op=DIFF)


def carriage(h=0.0):
    cb, hub = COL_BACK + h, HUB + h
    c = cylz(0, 0, cb, hub, 30)
    merge(c,
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
    return merge(c, *cuts, op=DIFF)


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
    k = cylx(-11.5, -4.7, LINE_Y, Z_CRANK, 9)
    merge(k,
          obox((-6.2, LINE_Y + d[1] * 3, Z_CRANK + d[2] * 3), d, (1, 0, 0), 6, 3, 7),
          cylx(-9.7, -4.7, yp, zp, 7))
    return merge(k,
                 cylx(-11.6, -7.7, LINE_Y, Z_CRANK, 4.7),      # servo spline
                 cylx(-7.8, -4.6, LINE_Y, Z_CRANK, 2.3),       # horn screw
                 cylx(-6.5, -4.6, LINE_Y, Z_CRANK, 4.2),       # horn screw head
                 cylx(-9.8, -4.6, yp, zp, 2.6), op=DIFF)       # crank pin (M3 self-tap)


def link(phi=0.0):
    yc, zc = crank_pin(phi)
    yl, zl = link_pin(phi)
    d = (0, yl - yc, zl - zc)
    k = obox((-2.7, (yc + yl) / 2, (zc + zl) / 2), d, (1, 0, 0), L_LINK, 3, 6.5)
    merge(k, cylx(-4.2, -1.2, yc, zc, 6.5), cylx(-4.2, -1.2, yl, zl, 6.5))
    return merge(k, cylx(-4.3, -1.1, yc, zc, 3.2), cylx(-4.3, -1.1, yl, zl, 3.2), op=DIFF)


# ---------- reference (bought / existing) parts ----------

def knob(h=0.0):
    k = cylz(0, 0, -11.5 + h, h, 51.6)
    merge(k, cylz(0, 0, -14 + h, -11.49 + h, 51.0), cylz(0, 0, -18.9 + h, -13.99 + h, 24.3),
          cylz(0, 0, HUB + h, -18.89 + h, 21))
    for x, y in KNOB_PINS:
        merge(k, cylz(x, y, -23.95 + h, HUB + 0.5 + h, 1.5))
    for x, y in KNOB_SCREWS:
        merge(k, cylz(x, y, HUB - 0.05 + h, HUB + 5 + h, 2.5), op=DIFF)
    return k


def display(h=0.0):
    return cylz(0, 0, -0.3 + h, 0.05 + h, 32.4)


def fpc(h=0.0):
    return box(-5.75, -5.45, -2.8, 2.8, HUB - 60 + h, HUB + h)


def servo():
    s = box(-35.2, -12.5, -44.6, -22.1, Z_CRANK - 6.1, Z_CRANK + 6.1)
    merge(s, box(-19.3, -16.8, -49.45, -17.25, Z_CRANK - 6.1, Z_CRANK + 6.1),
          cylx(-12.6, -8, LINE_Y, Z_CRANK, 4.5))
    return s


def rod(x):
    return cylz(x, 0, -60.05, -20.05, 4)


def bearing(x, h=0.0):
    b = cylz(x, 0, COL_BACK + h, COL_BACK + 12 + h, 8)
    return merge(b, cylz(x, 0, COL_BACK + h - 1, COL_BACK + 13 + h, 4.2), op=DIFF)


def panel():
    p = box(-75, 75, -75, 75, -T, 0)
    return merge(p, cylz(0, 0, -T - 1, 1, HOLE), op=DIFF)


PRINTED = ["Mount base", "Rear bridge", "Carriage", "Crank", "Link"]


def static_parts():
    return [("Wood panel (ref)", panel(), "wood"), ("Mount base", mount_base(), "base"),
            ("Rear bridge", rear_bridge(), "base"), ("Servo MG90S (ref)", servo(), "servo"),
            ("Guide rod L (ref)", rod(-ROD_X), "steel"), ("Guide rod R (ref)", rod(ROD_X), "steel")]


def moving_parts(phi):
    h = height(phi)
    return [("Carriage", carriage(h), "carriage"), ("Crank", crank(phi), "link"),
            ("Link", link(phi), "link"), ("Knob display (ref)", knob(h), "knob"),
            ("Display glass (ref)", display(h), "glass"), ("FPC (ref)", fpc(h), "fpc"),
            ("LM4UU L (ref)", bearing(-ROD_X, h), "steel"), ("LM4UU R (ref)", bearing(ROD_X, h), "steel")]
