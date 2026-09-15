# /// script
# requires-python = ">=3.11"
# dependencies = []
#
# [tool.pixi.workspace]
# channels = ["conda-forge"]
#
# [tool.pixi.dependencies]
# python = ">=3.11"
# numpy = "*"
# scipy = "*"
# scikit-image = "*"
# shapely = "*"
# ///
"""Generate minimal SVG icons of real spherical harmonics (ACN / SN3D) up to order 3.

Each lobe of |Y(dir)| is rendered as a flat-filled region (visible part only,
computed with a z-buffer), colored by the sign of the harmonic.

Usage:

    pixi run --script res/proto/harmonics/generate_harmonics.py [-- [--out DIR] [--az DEG] [--el DEG]]

(the `--` separator is needed to pass options to the script rather than to pixi)

"""

import argparse
import math
from pathlib import Path

import numpy as np
from scipy import ndimage
from scipy.special import lpmv
from shapely.geometry import Polygon
from skimage import measure

RES = 480  # z-buffer resolution (pixels)
VB = 100.0  # svg viewBox size
MARGIN = 4.0

POS_FILL, POS_STROKE = "#22A4E6", "#1673A3"  # ayu blue
NEG_FILL, NEG_STROKE = "#FA8D3E", "#B85F1E"  # ayu orange
STROKE_W = 2.2

FUMA = "WYZXVTRSUQOMKLNP"


def sn3d(l, m, az, el):
    am = abs(m)
    norm = math.sqrt((2 - (m == 0)) * math.factorial(l - am) / math.factorial(l + am))
    # scipy's lpmv includes the Condon-Shortley phase; ambiX does not
    p = lpmv(am, l, np.sin(el)) * (-1) ** am
    trig = np.cos(am * az) if m >= 0 else np.sin(am * az)
    return norm * p * trig


def render(acn, cam_az, cam_el):
    l = math.isqrt(acn)
    m = acn - l * l - l

    n_el, n_az = 1400, 2800
    el = np.linspace(-math.pi / 2, math.pi / 2, n_el)
    az = np.linspace(0, 2 * math.pi, n_az, endpoint=False)
    AZ, EL = np.meshgrid(az, el)
    Y = sn3d(l, m, AZ, EL)
    Y /= np.abs(Y).max()

    # lobes = connected sign regions on the sphere (azimuth wraps around)
    sign = np.sign(Y)
    labels = np.zeros(Y.shape, dtype=int)
    nlab = 0
    for s in (1, -1):
        lab, n = measure.label(sign == s, connectivity=1, return_num=True)
        labels[lab > 0] = lab[lab > 0] + nlab
        nlab += n
    parent = list(range(nlab + 1))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    for i in range(n_el):
        a, b = labels[i, 0], labels[i, -1]
        if a and b and sign[i, 0] == sign[i, -1]:
            parent[find(a)] = find(b)
    labels = np.vectorize(find)(labels)

    # geometry
    r = np.abs(Y)
    P = np.stack(
        [r * np.cos(EL) * np.cos(AZ), r * np.cos(EL) * np.sin(AZ), r * np.sin(EL)], -1
    )
    c = np.array(
        [
            math.cos(cam_el) * math.cos(cam_az),
            math.cos(cam_el) * math.sin(cam_az),
            math.sin(cam_el),
        ]
    )
    fwd = -c
    right = np.cross(fwd, [0, 0, 1.0])
    right /= np.linalg.norm(right)
    up = np.cross(right, fwd)
    sx, sy, depth = P @ right, P @ up, P @ c

    keep = (r > 1e-3) & (labels > 0)
    sx, sy, depth, lab = sx[keep], sy[keep], depth[keep], labels[keep]
    lab_sign = {int(k): int(np.sign(Y[labels == k][0])) for k in np.unique(lab)}

    # project onto the z-buffer (unit radius fits the buffer)
    scale = RES / 2 - 2
    px = np.clip((sx * scale + RES / 2).astype(int), 0, RES - 1)
    py = np.clip((RES / 2 - sy * scale).astype(int), 0, RES - 1)

    order = np.lexsort((-depth, py * RES + px))
    key = (py * RES + px)[order]
    first = np.ones(len(key), bool)
    first[1:] = key[1:] != key[:-1]
    img = np.zeros(RES * RES, int)
    img[key[first]] = lab[order][first]
    img = img.reshape(RES, RES)

    # fill sampling holes with neighbour labels
    for _ in range(3):
        holes = img == 0
        filled = ndimage.grey_dilation(img, size=3)
        inside = ndimage.binary_closing(img > 0, iterations=3)
        img[holes & inside] = filled[holes & inside]

    # fit the tight bounding box into the view box
    ys, xs = np.nonzero(img)
    cx, cy = (xs.min() + xs.max() + 1) / 2, (ys.min() + ys.max() + 1) / 2
    k = (VB - 2 * MARGIN) / max(xs.max() - xs.min() + 1, ys.max() - ys.min() + 1)

    def to_svg(pt):
        return VB / 2 + (pt[1] - cx) * k, VB / 2 + (pt[0] - cy) * k

    shapes = []
    for L in sorted(np.unique(img)):
        if L == 0:
            continue
        mask = np.pad(img == L, 1)
        if mask.sum() < 30:
            continue
        rings = []
        for cont in measure.find_contours(mask.astype(float), 0.5):
            cont = cont - 1
            if len(cont) < 8:
                continue
            poly = Polygon([to_svg(p) for p in cont]).simplify(0.35)
            if poly.is_empty or poly.area < 1.0:
                continue
            rings.append(list(poly.exterior.coords))
        if not rings:
            continue
        d = " ".join(
            "M" + " L".join(f"{x:.2f},{y:.2f}" for x, y in ring[:-1]) + " Z"
            for ring in rings
        )
        mean_depth = depth[lab == L].mean()
        shapes.append((mean_depth, lab_sign[int(L)], d))

    # visible regions don't overlap, but draw back to front so that the
    # strokes of front lobes are on top
    body = []
    for _, s, d in sorted(shapes):
        fill, stroke = (POS_FILL, POS_STROKE) if s > 0 else (NEG_FILL, NEG_STROKE)
        body.append(f'  <path fill="{fill}" stroke="{stroke}" d="{d}"/>')

    return l, m, "\n".join(body)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--out",
        type=Path,
        default=Path(__file__).parent,
        help="output directory (default: this script's directory)",
    )
    parser.add_argument(
        "--az",
        type=float,
        default=240.0,
        help="camera azimuth in degrees, counter-clockwise from front (+X) (default: 240)",
    )
    parser.add_argument(
        "--el",
        type=float,
        default=20.0,
        help="camera elevation in degrees (default: 20)",
    )
    args = parser.parse_args()

    args.out.mkdir(parents=True, exist_ok=True)
    for acn in range(16):
        l, m, body = render(acn, math.radians(args.az), math.radians(args.el))
        svg = f"""<svg xmlns="http://www.w3.org/2000/svg" width="10mm" height="10mm" viewBox="0 0 {VB:g} {VB:g}">
  <title>ACN {acn} ({FUMA[acn]}): order {l}, degree {m:+d}</title>
  <g fill-rule="evenodd" stroke-width="{STROKE_W}" stroke-linejoin="round">
{body}
  </g>
</svg>
"""
        path = args.out / f"SP4THarmonic-ACN{acn:02d}.svg"
        path.write_text(svg)
        print(f"wrote {path}")


if __name__ == "__main__":
    main()
