"""
Proximity-lift mount for the VIEWE UEDX46460015-MD50ET 1.5" round knob display.

A micro servo turns a crank pin inside a horizontal slot on the carriage
(Scotch yoke).  Half a turn lifts the board by 2 * crank_r with a natural
sine ease-in/ease-out, and both ends are dead centres, so pressing the knob
cannot push it down and the servo can be unpowered at rest.

Generates every printable part plus reference hardware (board, servo, horn,
guide rods, wood panel) in a new Fusion design.  Edit P and re-run to
regenerate, e.g. for a different panel thickness.

Coordinates (mm): Z = 0 is the back face of the wood panel (the glue face),
+Z points out of the panel front.  The board is centred on the Z axis.
"""
import math
import os

import adsk.core
import adsk.fusion

P = {
    'panel_t': 19.05,   # wood panel thickness (3/4")
    'crank_r': 8.0,     # radius of the horn hole used as crank pin; travel = 2 * crank_r
    'hole_d': 53.975,   # 2-1/8" Forstner bore through the wood
    'bore_d': 54.5,     # clearance bore in the mount plate
    'rod_d': 4.0,       # steel guide rods (press fit in plate and base)
    'bush_d': 4.3,      # printed bushing bore in the carriage
    'rod_x': 34.0,      # guide rod X offset
    'z_cc': -30.0,      # servo shaft (crank centre) height
    'out_dir': '/Volumes/Main External/Development/Desk/board-lift',
}

# VIEWE UEDX46460015-MD50ET V2.0 spec, mechanical drawing p.9
PUCK_D = 51.7
PUCK_H = 15.23
BOARD_DEPTH = 25.15                       # front face -> bottom of rear mounting boss
BOSS_STEPS = [(24.0, 4.85), (20.9, 3.0), (17.4, 2.07)]

# MG90S / SG90 micro servo: shaft along -Y, ears seated on the wall's -Y face
SERVO_L, SERVO_W = 22.8, 12.2
SERVO_SHAFT_FROM_END = 6.0
SERVO_EAR_PITCH = 27.8
SERVO_EAR_LEN = 32.4
SERVO_EAR_TO_TOP = 6.8                    # ear underside -> case top
SERVO_EAR_TO_BOTTOM = 15.9
SERVO_TOTAL_H = 28.5
HORN_LEN = 17.0                           # untrimmed single-arm horn, centre to tip

WALL_Y = 33.0                             # -Y face of the servo wall
Z_BASE_TOP, Z_BASE_BOT = -49.0, -53.0
Z_ROD_BOSS_TOP = -45.0

PRINT_PARTS = ['mount_plate', 'base', 'carriage', 'board_adapter', 'pin_sleeve', 'glue_jig']
LIFT_GROUP = ['carriage', 'board_adapter', 'REF_board']
CRANK_GROUP = ['REF_horn', 'pin_sleeve']


# ---------------------------------------------------------------- geometry helpers

def tbm():
    return adsk.fusion.TemporaryBRepManager.get()


def p3(x, y, z):
    return adsk.core.Point3D.create(x / 10, y / 10, z / 10)


def v3(x, y, z):
    return adsk.core.Vector3D.create(x, y, z)


def _bool(a, b, kind):
    if not tbm().booleanOperation(a, b, kind):
        raise RuntimeError('boolean operation failed')
    return a


def U(a, *bs):
    for b in bs:
        _bool(a, b, adsk.fusion.BooleanTypes.UnionBooleanType)
    return a


def S(a, *bs):
    for b in bs:
        _bool(a, b, adsk.fusion.BooleanTypes.DifferenceBooleanType)
    return a


def obox(c, ldir, wdir, l, w, h):
    ld = v3(*ldir)
    ld.normalize()
    wd = v3(*wdir)
    wd.normalize()
    obb = adsk.core.OrientedBoundingBox3D.create(p3(*c), ld, wd, l / 10, w / 10, h / 10)
    return tbm().createBox(obb)


def box(x0, x1, y0, y1, z0, z1):
    return obox(((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), (1, 0, 0), (0, 1, 0),
                x1 - x0, y1 - y0, z1 - z0)


def cyl(a, b, r):
    return tbm().createCylinderOrCone(p3(*a), r / 10, p3(*b), r / 10)


def cone(a, ra, b, rb):
    return tbm().createCylinderOrCone(p3(*a), ra / 10, p3(*b), rb / 10)


def cylz(x, y, z0, z1, r):
    return cyl((x, y, z0), (x, y, z1), r)


def cyly(x, z, y0, y1, r):
    return cyl((x, y0, z), (x, y1, z), r)


def pol(r, ang):
    a = math.radians(ang)
    return r * math.cos(a), r * math.sin(a)


def rrect(x0, x1, y0, y1, z0, z1, r):
    b = box(x0 + r, x1 - r, y0, y1, z0, z1)
    U(b, box(x0, x1, y0 + r, y1 - r, z0, z1))
    for x in (x0 + r, x1 - r):
        for y in (y0 + r, y1 - r):
            U(b, cylz(x, y, z0, z1, r))
    return b


def gusset(x0, x1, yc, zc, dy, dz):
    """Right-triangle web in the YZ plane, right angle at (yc, zc), legs dy and dz."""
    b = box(x0, x1, min(yc, yc + dy), max(yc, yc + dy), min(zc, zc + dz), max(zc, zc + dz))
    ay, az = yc + dy, zc
    by, bz = yc, zc + dz
    hy, hz = by - ay, bz - az
    length = math.hypot(hy, hz)
    my, mz = (ay + by) / 2, (az + bz) / 2
    ny, nz = -hz / length, hy / length
    if ny * (my - yc) + nz * (mz - zc) < 0:
        ny, nz = -ny, -nz
    big = 4 * length
    c = ((x0 + x1) / 2, my + ny * big / 2, mz + nz * big / 2)
    return S(b, obox(c, (0, hy, hz), (1, 0, 0), big, (x1 - x0) + 2, big))


def slot_x(cx, cz, length, width, y0, y1):
    """Stadium slot running along X, cut through Y."""
    h = length / 2 - width / 2
    b = box(cx - h, cx + h, y0, y1, cz - width / 2, cz + width / 2)
    return U(b, cyly(cx - h, cz, y0, y1, width / 2), cyly(cx + h, cz, y0, y1, width / 2))


def slot_radial(ang, r0, r1, width, z0, z1):
    ca, sa = math.cos(math.radians(ang)), math.sin(math.radians(ang))
    rm = (r0 + r1) / 2
    b = obox((rm * ca, rm * sa, (z0 + z1) / 2), (ca, sa, 0), (-sa, ca, 0), r1 - r0, width, z1 - z0)
    return U(b, cylz(r0 * ca, r0 * sa, z0, z1, width / 2), cylz(r1 * ca, r1 * sa, z0, z1, width / 2))


# ---------------------------------------------------------------- layout

def derived():
    T, r, zcc = P['panel_t'], P['crank_r'], P['z_cc']
    d = {
        'z_board': T - BOARD_DEPTH,       # board mounting face in the lowered (flush) position
        'z_ct': zcc - r + 10.0,           # carriage top, lowered (yoke slot is 10 below it)
    }
    d['adapter_h'] = d['z_board'] - d['z_ct']
    if d['z_ct'] + 2 * r > -10.5:
        raise ValueError('crank_r too large: the raised carriage would hit the rod bosses')
    if d['z_ct'] - 14 < Z_ROD_BOSS_TOP + 0.5:
        raise ValueError('crank_r too large: the lowered carriage would hit the base bosses')
    if d['adapter_h'] < 13.0:
        raise ValueError('panel too thin for the board adapter (needs >= ~12 mm)')
    return d


# ---------------------------------------------------------------- printed parts

def mount_plate():
    """Glued to the back of the panel.  Holds the rod tops and hangs the two pillars."""
    rx = P['rod_x']
    b = rrect(-46, 46, -44, 38, -4, 0, 6)
    for sx in (-1, 1):
        U(b, cylz(sx * rx, 0, -10, -4, 5.5))
        U(b, cylz(sx * 40, 31, Z_BASE_TOP, -4, 5))
    S(b, cylz(0, 0, -5, 1, P['bore_d'] / 2))
    for sx in (-1, 1):
        S(b, cylz(sx * rx, 0, -10.01, -3, P['rod_d'] / 2))       # blind rod socket
        S(b, cylz(sx * 40, 31, Z_BASE_TOP - 0.01, Z_BASE_TOP + 10, 1.3))  # M3 self-tap pilot
    for x, y in [(-38, -36), (38, -36), (-22, 32), (22, 32)]:    # optional #4 wood screws
        S(b, cylz(x, y, -5, 1, 1.8))
        S(b, cone((x, y, -4.01), 3.76, (x, y, -2.05), 1.8))
    S(b, cylz(0, -36, -5, 1, 4))                                 # proximity sensor window
    return b


def base():
    """Bottom frame: rod feet, pillar screws, and the servo wall."""
    rx = P['rod_x']
    zb0, zb1 = Z_BASE_BOT, Z_BASE_TOP
    b = rrect(-46, -28, -8, 37, zb0, zb1, 4)
    U(b, rrect(28, 46, -8, 37, zb0, zb1, 4),
      rrect(-46, 46, 26, 37, zb0, zb1, 4),
      rrect(-16, 27, 26, 47, zb0, zb1, 4),
      box(-16, 27, WALL_Y, WALL_Y + 4, zb1, -20))
    for x0, x1 in [(-15.5, -13.5), (23.5, 25.5)]:
        U(b, gusset(x0, x1, WALL_Y + 4, zb1, 10, 27))
    for sx in (-1, 1):
        U(b, cylz(sx * rx, 0, zb1, Z_ROD_BOSS_TOP, 5.5))
        S(b, cylz(sx * rx, 0, zb0 - 0.01, Z_ROD_BOSS_TOP + 0.01, P['rod_d'] / 2))
        S(b, cylz(sx * 40, 31, zb0 - 0.01, zb1 + 0.01, 1.7))
        S(b, cylz(sx * 40, 31, zb0 - 0.01, zb0 + 2.5, 3.25))     # M3 head counterbore
    zcc = P['z_cc']
    body_cx = SERVO_L / 2 - SERVO_SHAFT_FROM_END
    S(b, box(body_cx - SERVO_L / 2 - 0.2, body_cx + SERVO_L / 2 + 0.2,
             WALL_Y - 0.1, WALL_Y + 4.1, zcc - SERVO_W / 2 - 0.2, zcc + SERVO_W / 2 + 0.2))
    for sx in (-1, 1):
        S(b, cyly(body_cx + sx * SERVO_EAR_PITCH / 2, zcc, WALL_Y - 0.1, WALL_Y + 4.1, 1.0))
    return b


def carriage(zt):
    """Rides the rods; carries the board adapter and the yoke slot."""
    rx = P['rod_x']
    b = rrect(-rx - 5.5, rx + 5.5, -16, 16, zt - 4, zt, 4)
    U(b, rrect(-22, 22, -30, -14, zt - 3, zt, 3),
      box(-19, 19, 12, 16, zt - 16, zt))   # stops above the horn screw when raised
    for sx in (-1, 1):
        U(b, cylz(sx * rx, 0, zt - 14, zt, 5.5))
    for x0, x1 in [(-19, -15), (15, 19)]:
        U(b, gusset(x0, x1, 12, zt - 4, -6, -8))
    for sx in (-1, 1):
        S(b, cylz(sx * rx, 0, zt - 14.01, zt + 0.01, P['bush_d'] / 2))
    S(b, slot_x(0, zt - 10, 27, 4.4, 11.9, 16.1))
    S(b, cylz(0, 0, zt - 4.01, zt + 0.01, 6))
    for a in (30, 150, 270):
        x, y = pol(13.5, a)
        S(b, cylz(x, y, zt - 4.01, zt + 0.01, 1.7))
    for x in (-16, -8, 8, 16):                                    # zip-tie slots
        S(b, box(x - 1.1, x + 1.1, -27, -22.5, zt - 3.01, zt + 0.01))
    return b


def board_adapter(z0, h):
    """Bolts to the board's rear boss (3x M2.5) and to the carriage (3x M3 from below)."""
    b = cylz(0, 0, z0, z0 + 5, 16)
    U(b, cylz(0, 0, z0 + 5, z0 + h, 13))
    S(b, cylz(0, 0, z0 - 0.01, z0 + h - 3, 10.5))
    zt0, zt1 = z0 + h - 3.01, z0 + h + 0.01
    S(b, cylz(0, 0, zt0, zt1, 4.5))                              # FPC pass-through
    for a in (90, 210, 330):                                     # M2.5, any PCD 11-15.6 mm
        S(b, slot_radial(a, 5.4, 7.9, 2.8, zt0, zt1))
    for a in (30, 150, 270):                                     # relief for locating pins
        for k in range(-3, 4):
            x, y = pol(6.8, a + 8 * k)
            S(b, cylz(x, y, zt0, zt1, 1.8))
    S(b, box(-4, 4, -14, -9, z0 + 5, z0 + h - 4))                 # FPC exit window
    for a in (30, 150, 270):
        x, y = pol(13.5, a)
        S(b, cylz(x, y, z0 - 0.01, z0 + 5.01, 1.3))
    return b


def pin_sleeve(x, z):
    b = cyly(x, z, 11.5, 18.0, 2.0)
    return S(b, cyly(x, z, 11.4, 18.1, 1.15))


def glue_jig(x):
    """Centres the mount plate on the wood bore while the glue cures."""
    b = cylz(x, 0, -8, -4, 31)
    U(b, cylz(x, 0, -4, 0, P['bore_d'] / 2 - 0.15),
      cylz(x, 0, 0, 10, P['hole_d'] / 2 - 0.14))
    return S(b, cylz(x, 0, -9, 11, 20))


# ---------------------------------------------------------------- reference hardware

def ref_panel():
    T = P['panel_t']
    return S(box(-85, 85, -75, 75, 0, T), cylz(0, 0, -1, T + 1, P['hole_d'] / 2))


def ref_board(z_face):
    zf = z_face + BOARD_DEPTH
    b = cylz(0, 0, zf - PUCK_H, zf, PUCK_D / 2)
    z = zf - PUCK_H
    for dia, ln in BOSS_STEPS:
        U(b, cylz(0, 0, z - ln, z, dia / 2))
        z -= ln
    return b


def ref_fpc(z_face, z_deck):
    """Board FPC routed out of the adapter window and down onto the carriage deck."""
    b = box(-2.75, 2.75, -0.15, 0.15, z_face - 9, z_face)
    return U(b, box(-2.75, 2.75, -22.3, 0.15, z_face - 9.3, z_face - 9),
             box(-2.75, 2.75, -22.3, -22.0, z_deck + 0.3, z_face - 9.3),
             box(-2.75, 2.75, -30.0, -22.0, z_deck, z_deck + 0.3))


def ref_servo():
    zcc = P['z_cc']
    cx = SERVO_L / 2 - SERVO_SHAFT_FROM_END
    top = WALL_Y - SERVO_EAR_TO_TOP
    b = box(cx - SERVO_L / 2, cx + SERVO_L / 2, top, WALL_Y + SERVO_EAR_TO_BOTTOM,
            zcc - SERVO_W / 2, zcc + SERVO_W / 2)
    U(b, box(cx - SERVO_EAR_LEN / 2, cx + SERVO_EAR_LEN / 2, WALL_Y - 2.5, WALL_Y,
             zcc - SERVO_W / 2, zcc + SERVO_W / 2))
    tip = WALL_Y + SERVO_EAR_TO_BOTTOM - SERVO_TOTAL_H
    U(b, cyly(0, zcc, top - 3.0, top, 5.9), cyly(0, zcc, tip, top - 3.0, 2.4))
    return b


def ref_horn():
    """Stock single-arm horn pointing down, with the M2 crank-pin screw and nut."""
    zcc, r = P['z_cc'], P['crank_r']
    tip = WALL_Y + SERVO_EAR_TO_BOTTOM - SERVO_TOTAL_H
    y0, y1 = tip - 0.8, tip + 1.0                                 # arm plate
    b = cyly(0, zcc, y0, tip + 2.7, 3.5)
    U(b, box(-2.5, 2.5, y0, y1, zcc - HORN_LEN, zcc),
      cyly(0, zcc - HORN_LEN, y0, y1, 2.5),
      cyly(0, zcc - r, y0 - 1.6, y0, 2.3),                        # M2 nut
      cyly(0, zcc - r, y1, y1 + 1.3, 1.9),                        # M2 head
      cyly(0, zcc - r, 11.5, y1, 1.0))                            # M2 shank
    return b


def ref_rods():
    b = cylz(-P['rod_x'], 0, Z_BASE_BOT, -3, P['rod_d'] / 2 - 0.025)
    return U(b, cylz(P['rod_x'], 0, Z_BASE_BOT, -3, P['rod_d'] / 2 - 0.025))


# ---------------------------------------------------------------- Fusion plumbing

def _design():
    return adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)


def _occ(name):
    for occ in _design().rootComponent.occurrences:
        if occ.component.name == name:
            return occ
    raise KeyError(name)


def _get_tf(occ):
    return occ.transform2 if hasattr(occ, 'transform2') else occ.transform


def _set_tf(occ, m):
    if hasattr(occ, 'transform2'):
        occ.transform2 = m
    else:
        occ.transform = m


def _appearance(design, name, rgb):
    ap = design.appearances.itemByName(name)
    if ap:
        return ap
    app = adsk.core.Application.get()
    src = None
    for li in range(app.materialLibraries.count):
        lib = app.materialLibraries.item(li)
        for nm in ('Plastic - Matte (Yellow)', 'Plastic - Glossy (White)', 'Paint - Enamel Glossy (White)'):
            try:
                src = lib.appearances.itemByName(nm)  # raises when the name is missing
            except RuntimeError:
                src = None
            if src:
                break
        if src:
            break
    ap = design.appearances.addByCopy(src, name)
    for i in range(ap.appearanceProperties.count):
        cp = adsk.core.ColorProperty.cast(ap.appearanceProperties.item(i))
        if cp:
            cp.value = adsk.core.Color.create(rgb[0], rgb[1], rgb[2], 255)
            break
    return ap


def _style(design, body, rgb, opacity):
    try:
        body.appearance = _appearance(design, 'lift_%02x%02x%02x' % rgb, rgb)
    except Exception as e:  # cosmetic only
        print('appearance skipped:', e)
    if opacity < 1:
        try:
            body.opacity = opacity
        except Exception as e:
            print('opacity skipped:', e)


def build(new_doc=True):
    app = adsk.core.Application.get()
    if new_doc:
        app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
    design = _design()
    design.designType = adsk.fusion.DesignTypes.DirectDesignType
    design.unitsManager.distanceDisplayUnits = adsk.fusion.DistanceUnits.MillimeterDistanceUnits
    root = design.rootComponent
    if root.occurrences.count or root.bRepBodies.count:
        raise RuntimeError('active design is not empty; run build() with new_doc=True')
    d = derived()
    r, zcc = P['crank_r'], P['z_cc']

    parts = [
        ('mount_plate', mount_plate()),
        ('base', base()),
        ('carriage', carriage(d['z_ct'])),
        ('board_adapter', board_adapter(d['z_ct'], d['adapter_h'])),
        ('pin_sleeve', pin_sleeve(0, zcc - r)),
        ('glue_jig', glue_jig(140)),
        ('REF_panel', ref_panel()),
        ('REF_board', ref_board(d['z_board'])),
        ('REF_servo', ref_servo()),
        ('REF_horn', ref_horn()),
        ('REF_rods', ref_rods()),
    ]
    for name, tb in parts:
        occ = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        occ.component.name = name
        body = occ.component.bRepBodies.add(tb)
        body.name = name
        if name == 'REF_board':
            fpc = occ.component.bRepBodies.add(ref_fpc(d['z_board'], d['z_ct']))
            fpc.name = 'REF_fpc'
    restyle()
    print('built; adapter height %.2f mm, travel %.1f mm' % (d['adapter_h'], 2 * r))
    return d


STYLE = {
    'mount_plate': ((230, 120, 40), 1.0),
    'base': ((230, 120, 40), 1.0),
    'carriage': ((40, 150, 170), 1.0),
    'board_adapter': ((240, 200, 40), 1.0),
    'pin_sleeve': ((220, 50, 50), 1.0),
    'glue_jig': ((150, 150, 150), 1.0),
    'REF_panel': ((196, 154, 108), 0.25),
    'REF_board': ((35, 35, 40), 1.0),
    'REF_fpc': ((200, 140, 30), 1.0),
    'REF_servo': ((40, 80, 200), 1.0),
    'REF_horn': ((245, 245, 245), 1.0),
    'REF_rods': ((190, 190, 200), 1.0),
}


def restyle():
    design = _design()
    for occ in design.rootComponent.occurrences:
        for body in occ.component.bRepBodies:
            if body.name in STYLE:
                _style(design, body, *STYLE[body.name])


def set_state(theta_deg):
    """theta = 0: board flush.  theta = 180: board fully raised."""
    r, zcc = P['crank_r'], P['z_cc']
    th = math.radians(theta_deg)
    lift = adsk.core.Matrix3D.create()
    lift.translation = v3(0, 0, r * (1 - math.cos(th)) / 10)
    for name in LIFT_GROUP:
        _set_tf(_occ(name), lift)
    rot = adsk.core.Matrix3D.create()
    rot.setToRotation(th, v3(0, 1, 0), p3(0, 0, zcc))
    for name in CRANK_GROUP:
        _set_tf(_occ(name), rot)


def interference(angles=(0, 45, 90, 135, 180), min_mm3=0.05):
    skip = {frozenset(('REF_servo', 'REF_horn'))}
    hits = []
    for ang in angles:
        set_state(ang)
        items = []
        for occ in _design().rootComponent.occurrences:
            if occ.component.name == 'glue_jig':
                continue
            for b in occ.component.bRepBodies:
                tb = tbm().copy(b)
                tbm().transform(tb, _get_tf(occ))
                items.append((occ.component.name, b.name, tb))
        for i in range(len(items)):
            for j in range(i + 1, len(items)):
                ci, ni, bi = items[i]
                cj, nj, bj = items[j]
                if ci == cj or frozenset((ci, cj)) in skip:
                    continue
                t = tbm().copy(bi)
                ok = tbm().booleanOperation(t, bj, adsk.fusion.BooleanTypes.IntersectionBooleanType)
                vol = t.volume * 1000 if ok and t.faces.count else 0.0
                if vol > min_mm3:
                    hits.append((ang, ni, nj, round(vol, 2)))
    set_state(0)
    return hits


_ORIENT = {
    'mount_plate': 'flip',      # glue face on the bed, pillars up
    'base': 'asis',
    'carriage': 'flip',         # top face on the bed, bushings and yoke up
    'board_adapter': 'asis',
    'pin_sleeve': 'y_to_z',
    'glue_jig': 'asis',
}


def export(out_dir=None):
    out_dir = out_dir or P['out_dir']
    stl_dir = os.path.join(out_dir, 'stl')
    os.makedirs(stl_dir, exist_ok=True)
    set_state(0)
    design = _design()
    em = design.exportManager
    root = design.rootComponent
    written = []
    for name in PRINT_PARTS:
        tb = tbm().copy(_occ(name).component.bRepBodies.item(0))
        m = adsk.core.Matrix3D.create()
        if _ORIENT[name] == 'flip':
            m.setToRotation(math.pi, v3(1, 0, 0), p3(0, 0, 0))
        elif _ORIENT[name] == 'y_to_z':
            m.setToRotation(-math.pi / 2, v3(1, 0, 0), p3(0, 0, 0))
        tbm().transform(tb, m)
        bb = tb.boundingBox
        t = adsk.core.Matrix3D.create()
        t.translation = v3(-(bb.minPoint.x + bb.maxPoint.x) / 2, -(bb.minPoint.y + bb.maxPoint.y) / 2,
                           -bb.minPoint.z)
        tbm().transform(tb, t)
        occ = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        occ.component.name = 'EXPORT_' + name
        body = occ.component.bRepBodies.add(tb)
        path = os.path.join(stl_dir, name + '.stl')
        opts = em.createSTLExportOptions(body, path)
        opts.meshRefinement = adsk.fusion.MeshRefinementSettings.MeshRefinementHigh
        em.execute(opts)
        occ.deleteMe()
        written.append(path)
    step = os.path.join(out_dir, 'board_lift_assembly.step')
    em.execute(em.createSTEPExportOptions(step))
    f3d = os.path.join(out_dir, 'board_lift.f3d')
    em.execute(em.createFusionArchiveExportOptions(f3d))
    return written + [step, f3d]


def view(eye, target=(0, 0, -20), hide=()):
    app = adsk.core.Application.get()
    for occ in _design().rootComponent.occurrences:
        occ.isLightBulbOn = occ.component.name not in hide
    vp = app.activeViewport
    cam = vp.camera
    cam.isSmoothTransition = False
    cam.eye = p3(*eye)
    cam.target = p3(*target)
    cam.upVector = v3(0, 0, 1)
    vp.camera = cam
    vp.fit()
    vp.refresh()


def run(_context):
    build()
    hits = interference()
    print('interference:', hits or 'none')
    for path in export():
        print('wrote', path)
    set_state(180)
    view((300, -380, 90), target=(0, 0, -10), hide=('glue_jig',))
