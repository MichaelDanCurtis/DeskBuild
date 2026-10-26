import adsk.core, adsk.fusion, math

P = adsk.core.Point3D.create
V = adsk.core.Vector3D.create
VI = adsk.core.ValueInput.createByReal
OPS = adsk.fusion.FeatureOperations
UNION = adsk.fusion.BooleanTypes.UnionBooleanType
DIFF = adsk.fusion.BooleanTypes.DifferenceBooleanType
# author in Z-up (x stern->bow, y beam, z up); the design is Y-up
W = lambda x, y, z: P(x, z, -y)
Wv = lambda x, y, z: V(x, z, -y)

PLAN = [(0.0, 1.38), (0.9, 1.55), (2.4, 1.56), (3.8, 1.30), (4.9, 0.80), (5.6, 0.33), (6.05, 0.0)]
SHEER = [(0.0, 1.95), (2.2, 2.0), (4.5, 2.25), (6.4, 2.8)]

tbm = adsk.fusion.TemporaryBRepManager.get()


def interp(pts, x):
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        if x0 <= x <= x1:
            return y0 + (y1 - y0) * (x - x0) / (x1 - x0)
    return pts[-1][1]


def norm(v):
    m = math.sqrt(sum(c * c for c in v))
    return tuple(c / m for c in v)


def box(c, L, Wd, H, ldir=(1, 0, 0), wdir=(0, 1, 0)):
    lv, wv = Wv(*norm(ldir)), Wv(*norm(wdir))
    return tbm.createBox(adsk.core.OrientedBoundingBox3D.create(W(*c), lv, wv, L, Wd, H))


def cyl(p1, r1, p2, r2=None):
    return tbm.createCylinderOrCone(W(*p1), r1, W(*p2), r1 if r2 is None else r2)


def along(p, d, t):
    return tuple(a + b * t for a, b in zip(p, d))


def merge(target, *tools, op=UNION):
    for t in tools:
        if not tbm.booleanOperation(target, t, op):
            raise RuntimeError("boolean failed")
    return target


def run(_context: str):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    comp = design.rootComponent
    parts = []  # (name, temp body, appearance key)

    # cabin with door, windows and headlights
    cabin = box((2.4, 0, 2.65), 2.3, 2.1, 2.4)
    door = merge(box((1.3, 0, 2.2), 0.6, 1.1, 1.6), cyl((1.0, 0, 3.0), 0.55, (1.6, 0, 3.0)))
    merge(cabin,
          box((2.4, 0, 2.475), 1.9, 1.7, 2.35),
          door,
          box((3.45, 0, 3.0), 0.5, 1.4, 0.9),
          box((2.8, 0, 2.95), 0.85, 3.0, 0.9),
          box((1.8, 0, 3.05), 0.5, 3.0, 0.7), op=DIFF)
    parts.append(("Cabin", cabin, "white"))
    parts.append(("Roof", box((2.425, 0, 3.975), 2.85, 2.64, 0.25), "carbon"))
    for side, y in (("L", 0.7), ("R", -0.7)):
        parts.append(("Headlight " + side, cyl((3.5, y, 2.15), 0.17, (3.72, y, 2.15)), "led"))

    # twin raked exhaust stacks with flared, hollow tips
    d = (-math.sin(math.radians(25)), 0, math.cos(math.radians(25)))
    for side, y in (("L", 0.68), ("R", -0.68)):
        b = (2.05, y, 3.95)
        stack = merge(cyl(b, 0.2, along(b, d, 1.75)), cyl(along(b, d, 1.4), 0.2, along(b, d, 2.0), 0.32))
        merge(stack, cyl(along(b, d, 0.5), 0.13, along(b, d, 2.1)),
              cyl(along(b, d, 1.6), 0.13, along(b, d, 2.05), 0.27), op=DIFF)
        parts.append(("Exhaust stack " + side, stack, "chrome"))

    # rocket boosters on pylons, with nose cones, bells and flames
    for side, s in (("L", 1), ("R", -1)):
        yb, zb = 2.08 * s, 1.3
        booster = merge(cyl((-0.3, yb, zb), 0.42, (3.1, yb, zb)),
                        cyl((0.35, yb, zb), 0.45, (0.45, yb, zb)),
                        cyl((2.35, yb, zb), 0.45, (2.45, yb, zb)),
                        box((1.4, 1.75 * s, zb), 1.6, 0.7, 0.28),
                        box((0.15, yb, zb + 0.59), 0.7, 0.06, 0.4),
                        box((0.15, yb, zb - 0.59), 0.7, 0.06, 0.4),
                        box((0.15, yb + 0.59 * s, zb), 0.7, 0.4, 0.06))
        parts.append(("Booster " + side, booster, "aluminum"))
        parts.append(("Nose cone " + side, cyl((3.1, yb, zb), 0.42, (4.1, yb, zb), 0.0), "red"))
        bell = merge(cyl((-0.3, yb, zb), 0.30, (-0.85, yb, zb), 0.40),
                     cyl((-0.25, yb, zb), 0.22, (-0.9, yb, zb), 0.35), op=DIFF)
        parts.append(("Nozzle " + side, bell, "blackchrome"))
        parts.append(("Flame " + side, cyl((-0.75, yb, zb), 0.33, (-2.5, yb, zb), 0.0), "flame"))
        parts.append(("Flame core " + side, cyl((-0.75, yb, zb), 0.2, (-1.8, yb, zb), 0.0), "core"))

    # rear wing on raked struts
    a = math.radians(12)
    wing = box((-0.05, 0, 4.68), 1.3, 4.7, 0.12, ldir=(math.cos(a), 0, -math.sin(a)))
    for y in (0.95, -0.95):
        merge(wing, box((0.096, y, 3.063), 3.4, 0.12, 0.35, ldir=(-math.sin(a), 0, math.cos(a))))
    for y in (2.33, -2.33):
        merge(wing, box((-0.05, y, 4.6), 1.5, 0.07, 0.9))
    parts.append(("Rear wing", wing, "carbon"))

    # bow ram
    parts.append(("Bow ram", cyl((5.7, 0, 1.75), 0.3, (7.4, 0, 1.6), 0.0), "chrome"))

    # hydrofoils: splayed rear struts + wide foil, raked front strut + small foil
    foils = box((2.0, 0, -1.75), 0.9, 4.7, 0.14)
    for s in (1, -1):
        merge(foils, box((2.0, 1.5 * s, -0.725), 2.4, 0.6, 0.12, ldir=(0, 1.0 * s, -1.95), wdir=(1, 0, 0)))
    merge(foils, box((4.9, 0, -1.75), 0.7, 2.2, 0.14),
          box((4.8, 0, -0.685), 2.2, 0.5, 0.12, ldir=(0.2, 0, -2.07), wdir=(2.07, 0, 0.2)))
    parts.append(("Hydrofoils", foils, "blackpaint"))

    # spiked gunwale crown
    for side, s in (("L", 1), ("R", -1)):
        spikes = None
        for x in (0.5, 1.3, 2.1, 2.9, 3.7, 4.5, 5.3):
            hb, zs = interp(PLAN, x), interp(SHEER, x)
            sp = cyl((x, (hb - 0.11) * s, zs - 0.12), 0.1, (x - 0.25, (hb + 0.12) * s, zs + 0.55), 0.0)
            if spikes is None:
                spikes = sp
            else:
                merge(spikes, sp)
        parts.append(("Spikes " + side, spikes, "gold"))

    bf = comp.features.baseFeatures.add()
    bf.name = "Wild parts"
    bf.startEdit()
    for name, body, _ in parts:
        comp.bRepBodies.add(body, bf).name = name
    bf.finishEdit()

    # shark fin between the stacks
    sk = comp.sketches.add(comp.xYConstructionPlane)
    sk.name = "Shark fin"
    s = lambda x, z: sk.modelToSketchSpace(W(x, 0, z))
    pts = adsk.core.ObjectCollection.create()
    for x, z in ((3.0, 4.05), (2.3, 4.45), (1.6, 5.0), (1.0, 5.35)):
        pts.add(s(x, z))
    lead = sk.sketchCurves.sketchFittedSplines.add(pts)
    pts = adsk.core.ObjectCollection.create()
    for x, z in ((1.0, 5.35), (1.35, 4.7), (1.4, 4.05)):
        pts.add(s(x, z))
    trail = sk.sketchCurves.sketchFittedSplines.add(pts)
    trail.startSketchPoint.merge(lead.endSketchPoint)
    sk.sketchCurves.sketchLines.addByTwoPoints(trail.endSketchPoint, lead.startSketchPoint)
    ext = comp.features.extrudeFeatures
    fi = ext.createInput(sk.profiles.item(0), OPS.NewBodyFeatureOperation)
    fi.setSymmetricExtent(VI(0.16), True)
    ext.add(fi).bodies.item(0).name = "Shark fin"

    # paint
    lib = app.materialLibraries.itemByName("Fusion Appearance Library")
    want = {
        "hull": ["Paint - Enamel Glossy (Red)", "Aluminum - Anodized Glossy (Red)"],
        "red": ["Paint - Enamel Glossy (Red)", "Aluminum - Anodized Glossy (Red)"],
        "white": ["Paint - Enamel Glossy (White)", "Plastic - Glossy (White)", "ABS (White)"],
        "carbon": ["Carbon Fiber - Plain"],
        "chrome": ["Chrome"],
        "blackchrome": ["Chrome - Black"],
        "aluminum": ["Aluminum - Polished"],
        "gold": ["Gold - Polished"],
        "blackpaint": ["Paint - Enamel Glossy (Black)", "Plastic - Glossy (Black)"],
        "flame": ["Glass (Red)", "LED (Red)"],
        "core": ["Plastic - Translucent Glossy (Yellow)", "Paint - Enamel Glossy (Yellow)"],
        "led": ["LED - SMD 5050 - 20lm (White)", "LED (Red)"],
        "fin": ["Paint - Enamel Glossy (Black)", "Plastic - Glossy (Black)"],
    }
    cache = {}

    def appearance(key):
        if key not in cache:
            cache[key] = None
            for n in want[key]:
                a = design.appearances.itemByName(n)
                if not a:
                    la = lib.appearances.itemByName(n)
                    a = design.appearances.addByCopy(la, n) if la else None
                if a:
                    cache[key] = a
                    break
            print("appearance", key, "->", cache[key].name if cache[key] else None)
        return cache[key]

    keys = {name: key for name, _, key in parts}
    keys["Hull"] = "hull"
    keys["Shark fin"] = "fin"
    for b in comp.bRepBodies:
        a = appearance(keys.get(b.name, "white"))
        if a:
            b.appearance = a

    for sk in comp.sketches:
        sk.isVisible = False
    for pl in comp.constructionPlanes:
        pl.isLightBulbOn = False

    print("bodies:", ", ".join(b.name for b in comp.bRepBodies))
    bb = comp.boundingBox
    print("overall bbox", [round(v, 2) for v in bb.minPoint.asArray()], [round(v, 2) for v in bb.maxPoint.asArray()])
    app.activeViewport.fit()
