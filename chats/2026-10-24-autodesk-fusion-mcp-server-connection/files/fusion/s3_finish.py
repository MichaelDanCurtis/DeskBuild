import adsk.core, adsk.fusion, math

P = adsk.core.Point3D.create
V = adsk.core.Vector3D.create
VI = adsk.core.ValueInput.createByReal
OPS = adsk.fusion.FeatureOperations
W = lambda x, y, z: P(x, z, -y)
Wv = lambda x, y, z: V(x, z, -y)


def run(_context: str):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    comp = design.rootComponent
    wing = comp.bRepBodies.itemByName("Rear wing")

    # wing top face: planar, normal tilted 12 deg back from up
    a = math.radians(12)
    up = Wv(math.sin(a), 0, math.cos(a))
    tops = [f for f in wing.faces
            if f.geometry.objectType == adsk.core.Plane.classType() and
            f.evaluator.getNormalAtPoint(f.pointOnFace)[1].dotProduct(up) > 0.99]
    top = max(tops, key=lambda f: f.area)
    print("wing top face area", round(top.area, 3))

    sk = comp.sketches.add(top)
    sk.name = "Wing decal"
    u = (math.cos(a), 0, -math.sin(a))
    n = (math.sin(a), 0, math.cos(a))
    c = (-0.05 + n[0] * 0.06 + u[0] * 0.2, 0, 4.68 + n[2] * 0.06 + u[2] * 0.2)
    p1 = sk.modelToSketchSpace(W(c[0], -1.95, c[2]))
    p2 = sk.modelToSketchSpace(W(c[0], 1.95, c[2]))
    path = sk.sketchCurves.sketchLines.addByTwoPoints(p1, p2)
    path.isConstruction = True
    ti = sk.sketchTexts.createInput2("WILD BENCHY", 0.42)
    ti.setAsAlongPath(path, True, adsk.core.HorizontalAlignments.CenterHorizontalAlignment, 0)
    ti.fontName = "Arial Black"
    ti.isBold = True
    txt = sk.sketchTexts.add(ti)

    sk_n = sk.xDirection.crossProduct(sk.yDirection)
    face_n = top.evaluator.getNormalAtPoint(top.pointOnFace)[1]
    outward = sk_n.dotProduct(face_n) > 0
    ext = comp.features.extrudeFeatures
    ei = ext.createInput(txt, OPS.NewBodyFeatureOperation)
    ei.setOneSideExtent(adsk.fusion.DistanceExtentDefinition.create(VI(0.035)),
                        adsk.fusion.ExtentDirections.PositiveExtentDirection if outward
                        else adsk.fusion.ExtentDirections.NegativeExtentDirection)
    decal = ext.add(ei).bodies
    for i in range(decal.count):
        b = decal.item(i)
        b.name = "Wing decal" if decal.count == 1 else "Wing decal %d" % (i + 1)
        b.appearance = design.appearances.itemByName("Gold - Polished")
    sk.isVisible = False
    print("decal bodies", decal.count, "outward", outward)

    # stand it on its foils
    lift = -comp.boundingBox.minPoint.y
    bodies = adsk.core.ObjectCollection.create()
    for b in comp.bRepBodies:
        bodies.add(b)
    m = adsk.core.Matrix3D.create()
    m.translation = V(0, lift, 0)
    mv = comp.features.moveFeatures
    mi = mv.createInput2(bodies)
    mi.defineAsFreeMove(m)
    mv.add(mi)
    print("lifted by", round(lift, 3), "new min y", round(comp.boundingBox.minPoint.y, 3))

    vp = app.activeViewport
    cam = vp.camera
    cam.isSmoothTransition = False
    cam.target = P(2.4, 2.6, 0)
    cam.eye = P(2.4 + 14, 2.6 + 8.5, 15)
    cam.upVector = V(0, 1, 0)
    vp.camera = cam
    vp.fit()
