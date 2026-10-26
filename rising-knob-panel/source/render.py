"""Offscreen VTK previews of the rising knob module: flush/raised, outside/cutaway."""
import math
import sys

import vtk
from PIL import Image, ImageDraw

import rising_knob as rk


def actors(phi, clip=None, hide=()):
    out = []
    for name, shape, key in rk.static_parts() + rk.moving_parts(phi):
        if name in hide:
            continue
        mapper = vtk.vtkPolyDataMapper()
        mapper.SetInputData(shape.toVtkPolyData(0.05, 0.2))
        if clip is not None and name not in ("Servo MG90S", "Crank", "Link", "Guide rod L", "Guide rod R"):
            plane = vtk.vtkPlane()
            plane.SetOrigin(*clip[0])
            plane.SetNormal(*clip[1])
            mapper.AddClippingPlane(plane)
        a = vtk.vtkActor()
        a.SetMapper(mapper)
        p = a.GetProperty()
        p.SetColor(*rk.COLORS[key])
        p.SetSpecular(0.3 if key in ("steel", "knob", "glass") else 0.1)
        p.SetSpecularPower(30)
        if key == "wood" and clip is None and phi == 0 and False:
            p.SetOpacity(0.5)
        out.append(a)
    return out


def shot(fn, phi, eye, target, up, clip=None, hide=(), size=(900, 700), title=""):
    ren = vtk.vtkRenderer()
    ren.SetBackground(0.93, 0.94, 0.96)
    for a in actors(phi, clip, hide):
        ren.AddActor(a)
    cam = ren.GetActiveCamera()
    cam.SetPosition(*eye)
    cam.SetFocalPoint(*target)
    cam.SetViewUp(*up)
    cam.SetViewAngle(28)
    ren.ResetCameraClippingRange()
    light = vtk.vtkLight()
    light.SetLightTypeToCameraLight()
    light.SetPosition(0.4, 0.6, 1)
    ren.AddLight(light)
    win = vtk.vtkRenderWindow()
    win.SetOffScreenRendering(1)
    win.SetMultiSamples(8)
    win.AddRenderer(ren)
    win.SetSize(*size)
    win.Render()
    w2i = vtk.vtkWindowToImageFilter()
    w2i.SetInput(win)
    w2i.Update()
    wr = vtk.vtkPNGWriter()
    wr.SetFileName(fn)
    wr.SetInputConnection(w2i.GetOutputPort())
    wr.Write()
    if title:
        im = Image.open(fn)
        ImageDraw.Draw(im).text((16, 12), title, fill=(30, 30, 30))
        im.save(fn)


if __name__ == "__main__":
    out = sys.argv[1]
    up = (0, 1, 0)
    front_eye, front_tgt = (170, 120, 260), (0, -5, -10)
    back_eye, back_tgt = (-190, -150, -230), (-5, -12, -42)
    side_eye, side_tgt = (260, 30, -10), (0, -18, -30)
    clip = ((0, 0, 0), (-1, 0, 0))
    shots = [
        ("1_front_flush.png", 0.0, front_eye, front_tgt, None, (), "Flush: knob face level with the wood"),
        ("2_front_raised.png", math.pi, front_eye, front_tgt, None, (), "Raised 12 mm: ring exposed to turn"),
        ("3_back_flush.png", 0.0, back_eye, back_tgt, None, (), "Behind the panel (flush)"),
        ("4_cutaway_flush.png", 0.0, side_eye, side_tgt, clip, (), "Cutaway, flush (servo crank at bottom dead centre)"),
        ("5_cutaway_mid.png", math.pi / 2, side_eye, side_tgt, clip, (), "Cutaway, halfway (crank at 90 deg)"),
        ("6_cutaway_raised.png", math.pi, side_eye, side_tgt, clip, (), "Cutaway, raised (crank at top dead centre)"),
    ]
    for fn, phi, eye, tgt, cl, hide, title in shots:
        shot(f"{out}/{fn}", phi, eye, tgt, up, cl, hide, title=title)
        print("rendered", fn)
    ims = [Image.open(f"{out}/{s[0]}") for s in shots]
    w, h = ims[0].size
    grid = Image.new("RGB", (w * 3, h * 2), "white")
    for i, im in enumerate(ims):
        grid.paste(im, ((i % 3) * w, (i // 3) * h))
    grid.save(f"{out}/overview.png")
    print("rendered overview.png")
