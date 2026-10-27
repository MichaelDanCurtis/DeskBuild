import adsk.core, adsk.fusion

def run(_context: str):
    app = adsk.core.Application.get()
    print("YUp enum", adsk.core.DefaultModelingOrientations.YUpModelingOrientation,
          "ZUp enum", adsk.core.DefaultModelingOrientations.ZUpModelingOrientation)
    cam = app.activeViewport.camera
    print("cam up", cam.upVector.asArray(), "eye", cam.eye.asArray(), "target", cam.target.asArray())
    tbm = adsk.fusion.TemporaryBRepManager.get()
    P = adsk.core.Point3D.create
    cone = tbm.createCylinderOrCone(P(0, 0, 0), 1.0, P(0, 0, 2), 0.0)
    print("cone ok", cone is not None, cone.volume if cone else None)
    lib = None
    for l in app.materialLibraries:
        if "Appearance" in l.name:
            lib = l
            print("lib:", l.name, l.appearances.count)
    keys = ["red", "chrome", "polished", "black", "white", "orange", "yellow", "carbon", "glass", "emissive", "gold", "neon"]
    names = [lib.appearances.item(i).name for i in range(lib.appearances.count)]
    for k in keys:
        hits = [n for n in names if k in n.lower()][:8]
        print(k, "->", hits)
