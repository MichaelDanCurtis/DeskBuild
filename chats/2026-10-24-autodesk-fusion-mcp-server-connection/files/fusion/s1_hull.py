import adsk.core, adsk.fusion, math

P = adsk.core.Point3D.create
VI = adsk.core.ValueInput.createByReal
OPS = adsk.fusion.FeatureOperations

# half-beam outline, stern -> bow (local cm, Z up)
PLAN = [(0.0, 1.38), (0.9, 1.55), (2.4, 1.56), (3.8, 1.30), (4.9, 0.80), (5.6, 0.33), (6.05, 0.0)]


def coll(items):
    c = adsk.core.ObjectCollection.create()
    for i in items:
        c.add(i)
    return c


def draw_plan(sk, z):
    m2s = lambda x, y: sk.modelToSketchSpace(P(x, y, z))
    splines = sk.sketchCurves.sketchFittedSplines
    port = splines.add(coll([m2s(x, y) for x, y in PLAN]))
    stbd = splines.add(coll([m2s(x, -y) for x, y in PLAN]))
    stbd.endSketchPoint.merge(port.endSketchPoint)
    stern = sk.sketchCurves.sketchLines.addByTwoPoints(port.startSketchPoint, stbd.startSketchPoint)
    return [port, stbd, stern]


def run(_context: str):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    root = design.rootComponent

    # build Z-up inside the component, rotate it to the design's Y-up world
    m = adsk.core.Matrix3D.create()
    m.setToRotation(-math.pi / 2, adsk.core.Vector3D.create(1, 0, 0), P(0, 0, 0))
    occ = root.occurrences.addNewComponent(m)
    comp = occ.component
    comp.name = "Wild Benchy"
    ext = comp.features.extrudeFeatures

    # plan view
    sk = comp.sketches.add(comp.xYConstructionPlane)
    sk.name = "Hull plan"
    draw_plan(sk, 0)
    hull = ext.addSimple(sk.profiles.item(0), VI(3.5), OPS.NewBodyFeatureOperation).bodies.item(0)
    hull.name = "Hull"

    # side view, intersected with the plan body
    sk2 = comp.sketches.add(comp.xZConstructionPlane)
    sk2.name = "Hull side"
    s = lambda x, z: sk2.modelToSketchSpace(P(x, 0, z))
    L = sk2.sketchCurves.sketchLines
    S = sk2.sketchCurves.sketchFittedSplines
    A, B, C, D, E, F = s(0, 1.95), s(0.3, 0.32), s(0.9, 0), s(3.6, 0), s(6.3, 1.7), s(6.4, 2.8)
    l1 = L.addByTwoPoints(A, B)
    l2 = L.addByTwoPoints(l1.endSketchPoint, C)
    l3 = L.addByTwoPoints(l2.endSketchPoint, D)
    bow = S.add(coll([D, s(4.7, 0.25), s(5.5, 0.85), E]))
    bow.startSketchPoint.merge(l3.endSketchPoint)
    l4 = L.addByTwoPoints(bow.endSketchPoint, F)
    sheer = S.add(coll([F, s(4.5, 2.25), s(2.2, 2.0), A]))
    sheer.startSketchPoint.merge(l4.endSketchPoint)
    sheer.endSketchPoint.merge(l1.startSketchPoint)
    xi = ext.createInput(sk2.profiles.item(0), OPS.IntersectFeatureOperation)
    xi.setSymmetricExtent(VI(4.0), True)
    xi.participantBodies = [hull]
    ext.add(xi)
    hull = comp.bRepBodies.item(0)

    fil = comp.features.filletFeatures

    def try_fillet(edges, radii, label):
        for r in radii:
            fi = fil.createInput()
            fi.edgeSetInputs.addConstantRadiusEdgeSet(coll(edges), VI(r), True)
            try:
                fil.add(fi)
                print(label, "fillet r=", r, "edges", len(edges))
                return
            except RuntimeError as e:
                print(label, "fillet r=", r, "failed:", str(e)[:120])
        print(label, "fillet skipped")

    # rounded stern corners (near-vertical transom/side edges)
    hull = comp.bRepBodies.item(0)
    stern_edges = [e for e in hull.edges
                   if e.boundingBox.maxPoint.x < 0.5 and
                   e.boundingBox.maxPoint.z - e.boundingBox.minPoint.z > 1.0 and
                   abs(e.boundingBox.minPoint.y) > 1.0]
    try_fillet(stern_edges, [0.4, 0.3, 0.2], "stern")

    # rounded bilge
    hull = comp.bRepBodies.item(0)
    bilge = [e for e in hull.edges if e.boundingBox.maxPoint.z < 1.4]
    try_fillet(bilge, [0.5, 0.4, 0.3, 0.2], "bilge")

    # deck pocket, leaving a raised foredeck past x=5.1
    pl = comp.constructionPlanes
    pin = pl.createInput()
    pin.setByOffset(comp.xYConstructionPlane, VI(1.45))
    deck_plane = pl.add(pin)
    deck_plane.name = "Deck"
    sk3 = comp.sketches.add(deck_plane)
    sk3.name = "Deck pocket"
    curves = draw_plan(sk3, 1.45)
    sk3.offset(coll(curves), sk3.modelToSketchSpace(P(3, 0, 1.45)), 0.22)
    sk3.sketchCurves.sketchLines.addByTwoPoints(sk3.modelToSketchSpace(P(5.1, -2, 1.45)),
                                                sk3.modelToSketchSpace(P(5.1, 2, 1.45)))
    best = max(sk3.profiles, key=lambda p: p.areaProperties().area)
    print("pocket profile area", round(best.areaProperties().area, 2))
    ci = ext.createInput(best, OPS.CutFeatureOperation)
    ci.setDistanceExtent(False, VI(3.0))
    ci.participantBodies = [comp.bRepBodies.item(0)]
    ext.add(ci)

    hull = comp.bRepBodies.item(0)
    bb = hull.boundingBox
    print("hull bbox", [round(v, 2) for v in bb.minPoint.asArray()], [round(v, 2) for v in bb.maxPoint.asArray()],
          "volume", round(hull.volume, 2))
    app.activeViewport.fit()
