#!/usr/bin/env python3
"""Draw each module -- panel plus components -- as one SVG, for the manual.

Rack draws the knobs, jacks and switches over the panel at run time, so the
exported panels in res/ have none of them. This rebuilds that view: the
positions come from the `components` layer of the staging panels (the
placeholders the panels are laid out against) and the artwork from
res/components/ and Rack's own component library.

    python3 doc/make_screenshots.py

Writes doc/images/modules/<Module>[-dark].svg, light and dark theme. The
artwork is inlined rather than linked, so each file stands alone and stays
sharp at any size. Needs the network the first time, to fetch the Rack
components this repository does not ship itself (CC BY-NC 4.0, as the panels
already use them).
"""

import pathlib
import re
import sys
import urllib.request
from xml.etree import ElementTree

ROOT = pathlib.Path(__file__).resolve().parent.parent
ART = ROOT / "doc" / "_art"
OUT = ROOT / "doc" / "images" / "modules"

# Rack's own component library, fetched rather than vendored.
RACK_ART = "https://raw.githubusercontent.com/VCVRack/Rack/v2/res/ComponentLibrary/"
RACK_COMPONENTS = ["PJ301M.svg", "PJ301M-dark.svg", "ScrewSilver.svg", "ScrewBlack.svg"]

MM_PER_PX = 25.4 / 75  # Rack's unit, for component art sized in pixels
RACK_GRID_WIDTH = 15 * MM_PER_PX
RACK_GRID_HEIGHT = 380 * MM_PER_PX

# What the radius of a placeholder in the components layer stands for, to the
# nearest tenth of a millimetre: they are hand-placed, so not all identical. A
# knob is two pieces, the body and the part that turns.
BY_RADIUS = {
    0.567: "led",
    2.278: "switch",
    2.646: ("SP4TButton_0",),
    3.084: ("SP4TRoundSmallKnob_bg", "SP4TRoundSmallKnob"),
    4.013: "jack",
    5.054: ("SP4TRoundLargeKnob_bg", "SP4TRoundLargeKnob"),
}

PLACEHOLDER = re.compile(
    r'<circle id="([^"]+)"[^>]*cx="([\d.]+)" cy="([\d.]+)" r="([\d.]+)"')
OPEN_SVG = re.compile(r"<svg\b.*?>", re.S)
VIEWBOX = re.compile(r'viewBox="([\d.-]+ [\d.-]+ ([\d.]+) ([\d.]+))"')
ID = re.compile(r'\bid="([^"]+)"')

# Editor leftovers in the artwork. They have to go rather than be carried over:
# they are written in namespaces this document does not declare, which makes it
# invalid XML, and a browser then refuses to draw any of it.
EDITOR_ELEMENT = re.compile(r"<(sodipodi|inkscape|metadata)\b.*?(/>|</\1:?\w*>)", re.S)
EDITOR_ATTRIBUTE = re.compile(r'\s+(sodipodi|inkscape):[\w-]+="[^"]*"')


def fetch_rack_components():
    ART.mkdir(parents=True, exist_ok=True)
    for name in RACK_COMPONENTS:
        target = ART / name
        if not target.exists():
            print(f"  fetching {name}")
            urllib.request.urlretrieve(RACK_ART + name, target)


def art(name, dark):
    """Path of a component's artwork, falling back to the light one."""
    for stem in ((name + "-dark", name) if dark else (name,)):
        for folder in (ROOT / "res" / "components", ART):
            path = folder / f"{stem}.svg"
            if path.exists():
                return path
    raise FileNotFoundError(name)


def read_svg(path):
    """An SVG's size in mm, its viewBox and its contents.

    The artwork is a mix: ours is sized in mm, Rack's in px or by its viewBox
    alone, which is in Rack's own unit.
    """
    text = path.read_text()
    open_tag = OPEN_SVG.search(text)
    tag = open_tag.group(0)

    box = VIEWBOX.search(tag)
    lengths = []
    for attribute in ("width", "height"):
        m = re.search(rf'\b{attribute}="([\d.]+)(mm|px)?"', tag)
        lengths.append((float(m.group(1)), m.group(2)) if m else None)
    if None in lengths:
        lengths = [(float(box.group(2)), None), (float(box.group(3)), None)]
    size = tuple(value if unit == "mm" else value * MM_PER_PX for value, unit in lengths)

    viewbox = box.group(1) if box else f"0 0 {size[0]} {size[1]}"
    body = text[open_tag.end():text.rindex("</svg>")]
    body = EDITOR_ELEMENT.sub("", body)
    body = EDITOR_ATTRIBUTE.sub("", body)
    return size, viewbox, body


def namespaced(body, prefix):
    """The same contents, with every id of its own renamed.

    Several pieces of artwork end up in one document here, and each was written
    as if alone: without this, two of them defining the same gradient id would
    have one drawn with the other's.
    """
    for name in sorted(set(ID.findall(body)), key=len, reverse=True):
        for pattern, replacement in ((f'id="{name}"', f'id="{prefix}{name}"'),
                                     (f"url(#{name})", f"url(#{prefix}{name})"),
                                     (f'href="#{name}"', f'href="#{prefix}{name}"')):
            body = body.replace(pattern, replacement)
    return body


class Drawing:
    """The picture being assembled: artwork defined once, placed as often as needed."""

    def __init__(self):
        self.symbols = {}
        self.parts = []

    def symbol(self, path):
        """Define a piece of artwork, once per document, and return its size."""
        if path not in self.symbols:
            size, viewbox, body = read_svg(path)
            name = f"c{len(self.symbols)}"
            self.symbols[path] = (
                name, size,
                f'<symbol id="{name}" viewBox="{viewbox}">'
                f"{namespaced(body, name + '_')}</symbol>")
        name, size, _ = self.symbols[path]
        return name, size

    def place(self, path, cx, cy):
        """Draw a piece of artwork centred on (cx, cy)."""
        name, (w, h) = self.symbol(path)
        self.parts.append(f'<use href="#{name}" x="{cx - w / 2:.4f}" y="{cy - h / 2:.4f}" '
                          f'width="{w:.4f}" height="{h:.4f}" />')

    def circle(self, cx, cy, r, fill, opacity=1.0):
        self.parts.append(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{fill}" '
                          f'fill-opacity="{opacity}" />')

    def svg(self, width, height):
        defs = "\n".join(s for _, _, s in self.symbols.values())
        return (f'<svg xmlns="http://www.w3.org/2000/svg" '
                f'xmlns:xlink="http://www.w3.org/1999/xlink" '
                f'width="{width}mm" height="{height}mm" '
                f'viewBox="0 0 {width} {height}">\n'
                f"<defs>\n{defs}\n</defs>\n" + "\n".join(self.parts) + "\n</svg>\n")


def components_of(module):
    """The placeholders of a module's staging panel, as (id, x, y, radius)."""
    staging = (ROOT / "res" / "staging" / f"{module}.svg").read_text()
    layer = re.search(r'<g id="components".*?</g>\s*</svg>', staging, re.S)
    return [(i, float(x), float(y), float(r))
            for i, x, y, r in PLACEHOLDER.findall(layer.group(0))]


def screws_of(module, panel_width):
    """Where the module widget puts its screws, in mm."""
    source = (ROOT / "src" / f"{module}.cpp").read_text()
    names = {"RACK_GRID_WIDTH": RACK_GRID_WIDTH,
             "RACK_GRID_HEIGHT": RACK_GRID_HEIGHT,
             "box_size_x": panel_width}
    corners = []
    for expr in re.findall(r"createWidget<ThemedScrew>\(Vec\(([^()]*)\)\)", source):
        x, y = expr.split(",", 1)
        corners.append(tuple(eval(part.replace("box.size.x", "box_size_x"), names)  # noqa: S307
                             for part in (x, y)))
    return corners


def draw(module, dark):
    suffix = "-dark" if dark else ""
    panel = ROOT / "res" / f"{module}{suffix}.svg"
    (width, height), _, body = read_svg(panel)

    drawing = Drawing()
    drawing.parts.append(f'<g>{namespaced(body, "panel_")}</g>')

    for name, cx, cy, radius in components_of(module):
        kind = min(BY_RADIUS.items(), key=lambda kv: abs(kv[0] - radius))
        assert abs(kind[0] - radius) < 0.1, f"{module}: unknown component {name} (r={radius})"
        kind = kind[1]
        if kind == "led":
            # Unlit, as a module sits in the browser: the rim only.
            drawing.circle(cx, cy, radius, "#000000", 0.85)
            continue
        if kind == "jack":
            kind = ("SP4TAmbiPort",) if "ambi" in name else ("PJ301M",)
        elif kind == "switch":
            # The order switch is the only three-position one.
            kind = ("SP4TSwitchThree_0",) if name == "order-param" else ("SP4TSwitchTwo_0",)
        for piece in kind:
            drawing.place(art(piece, dark), cx, cy)

    screw = art("ScrewBlack" if dark else "ScrewSilver", False)
    _, (screw_w, screw_h) = drawing.symbol(screw)
    for x, y in screws_of(module, width):
        drawing.place(screw, x + screw_w / 2, y + screw_h / 2)

    picture = drawing.svg(width, height)
    # A browser drops an SVG that does not parse, so check here rather than
    # find out by looking at the rendered manual.
    ElementTree.fromstring(picture)

    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / f"{module}{suffix}.svg").write_text(picture)


def main():
    modules = sorted(p.stem for p in (ROOT / "src").glob("SP4T*.cpp"))
    fetch_rack_components()
    for module in modules:
        print(f"  {module}")
        for dark in (False, True):
            draw(module, dark)
    print(f"{len(modules)} modules -> {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    sys.exit(main())
