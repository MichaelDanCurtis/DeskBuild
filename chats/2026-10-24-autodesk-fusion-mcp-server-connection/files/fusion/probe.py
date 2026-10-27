import adsk.core, adsk.fusion

def run(_context: str):
    app = adsk.core.Application.get()
    print("version", app.version)
    doc = app.activeDocument
    print("activeDocument", doc.name if doc else None, "docs open:", app.documents.count)
    prod = app.activeProduct
    print("product", prod.objectType if prod else None)
    design = adsk.fusion.Design.cast(prod)
    if design:
        print("designType", design.designType)
        root = design.rootComponent
        print("bodies", root.bRepBodies.count, "occurrences", root.occurrences.count,
              "sketches", root.sketches.count, "timeline", design.timeline.count)
    gp = app.preferences.generalPreferences
    print("orientation", gp.defaultModelingOrientation)
