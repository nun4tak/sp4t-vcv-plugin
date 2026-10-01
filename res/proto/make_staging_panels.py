#!/usr/bin/env python3
"""Generate the res/staging panels (light + dark) from the res/proto panels.

Usage, from the repository root:

    python3 res/proto/make_staging_panels.py [PROTO_DIR] [STAGING_DIR]

defaulting to res/proto -> res/staging. Re-running is safe: it rewrites every
staging panel from scratch, so res/proto stays the single source of truth.

For each res/proto/*.svg (the subfolders are ignored) it writes
res/staging/<Module>.svg and res/staging/<Module>-dark.svg, and:

  * folds the LEDs layer into the components layer, then drops the LEDs layer
  * replaces every component with a VCV Rack placeholder as described in
    https://vcvrack.com/manual/Panel#Adding-components -- circles coloured
    #ff0000 param / #00ff00 input / #0000ff output / #ff00ff light, each
    carrying an inkscape:label so helper.py generates the C++ names
  * hides the components layer (display:none) while keeping it in the file:
    helper.py selects the layer by inkscape:label / id and performs no
    visibility check, so hiding it does not affect code generation
  * recolours the dark variant role by role (see DARK below)

What it deliberately does NOT do: run helper.py, touch res/proto, or export to
res/ (that is `pixi run export-panels`, which converts text to paths).

Screws get no placeholder -- the manual defines none and screws are added in C++
via createWidget<ScrewSilver>. Placeholder circle radii mirror the real component
size purely so the panel still reads as a layout; helper.py ignores a circle's
size and uses only its centre.
"""
import copy
import glob
import math
import os
import re
import sys
import xml.etree.ElementTree as ET

SVGNS = 'http://www.w3.org/2000/svg'
INKNS = 'http://www.inkscape.org/namespaces/inkscape'
SODNS = 'http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd'
XLINKNS = 'http://www.w3.org/1999/xlink'
for _p, _u in (('', SVGNS), ('inkscape', INKNS), ('sodipodi', SODNS),
               ('xlink', XLINKNS)):
    ET.register_namespace(_p, _u)
SVG, INK, SOD, XLINK = ('{%s}' % SVGNS, '{%s}' % INKNS,
                        '{%s}' % SODNS, '{%s}' % XLINKNS)

PLACEHOLDER = {'param': '#ff0000', 'input': '#00ff00',
               'output': '#0000ff', 'light': '#ff00ff'}
PARAM_SYMBOLS = {'SP4TRoundLargeKnob', 'SP4TRoundSmallKnob', 'SP4TRoundKnob',
                 'SP4TSwitchTwo', 'SP4TSwitchThree', 'SP4TButton', 'SP4TLatch'}
PORT_SYMBOLS = {'PJ301M', 'SP4TAmbiPort'}

# Light -> dark, by role. The screen keeps its colour (it is already near-black
# and its content is drawn by the module widget).
DARK = {
    '#d8dbdf': '#2a2e35',   # panel background
    '#2b3038': '#e6e1cf',   # title
    '#22a4e6': '#59c2ff',   # Ayu blue accent (title "4", ambisonic ring)
    '#3a4049': '#bfbdb6',   # labels and brand
    '#adb2b8': '#8a9199',   # secondary labels (Split-Insert)
    '#777a88': '#9da5ae',   # source numbers (Source4 In)
    '#0d1017': '#0d1017',   # screen, unchanged
    '#737a83': '#61676f',   # thin strokes, knob arcs
    '#bbbfc3': '#4a5059',   # decorative line art, grouping blocks
    '#bbbec3': '#4a5059',
    # a dark ink at opacity 0.15/0.5 disappears on a dark ground, so this one is
    # inverted to a light ink rather than merely darkened
    '#484c51': '#cfd3d9',
    '#5c6370': '#9da5ae',   # Flip chevron icon
    '#0b0d11': '#15181d',   # chevron accent, stays dark over the lifted icon
    # harmonic lobes: the light half already reads on a dark ground; the dark
    # half and its outline are lifted so they stay visible. Note the lobes sit on
    # the grouping rects, not on the bare background.
    '#ffffff': '#ffffff',
    '#b3bbc6': '#b3bbc6',
    '#4d5560': '#6e7681',
    # deliberately NOT #3a4049: that is a key above (label text), and a value
    # colliding with a key makes the result impossible to verify.
    '#16181d': '#363b44',
    '#23272e': '#4a5059',
}
HEX = re.compile(r'#[0-9a-fA-F]{6}')
PAINT_ATTRS = ('fill', 'stroke', 'stop-color')
MM_PER_HP = 5.08

# --------------------------------------------------------------- geometry ----
NUM_RE = re.compile(r'[-+]?(?:\d*\.\d+|\d+\.?)(?:[eE][-+]?\d+)?')
CMD_RE = re.compile(r'([MmLlHhVvCcSsQqTtAaZz])([^MmLlHhVvCcSsQqTtAaZz]*)')


def mul(a, b):
    return (a[0]*b[0]+a[2]*b[1], a[1]*b[0]+a[3]*b[1], a[0]*b[2]+a[2]*b[3],
            a[1]*b[2]+a[3]*b[3], a[0]*b[4]+a[2]*b[5]+a[4], a[1]*b[4]+a[3]*b[5]+a[5])


def parse_tf(s):
    m = (1, 0, 0, 1, 0, 0)
    for name, args in re.findall(r'(\w+)\s*\(([^)]*)\)', s or ''):
        v = [float(x) for x in re.split(r'[,\s]+', args.strip()) if x]
        if name == 'translate':
            t = (1, 0, 0, 1, v[0], v[1] if len(v) > 1 else 0)
        elif name == 'matrix':
            t = tuple(v)
        elif name == 'scale':
            t = (v[0], 0, 0, v[1] if len(v) > 1 else v[0], 0, 0)
        elif name == 'rotate':
            a = math.radians(v[0]); c, s2 = math.cos(a), math.sin(a)
            t = (c, s2, -s2, c, 0, 0)
            if len(v) == 3:
                t = mul((1, 0, 0, 1, v[1], v[2]), mul(t, (1, 0, 0, 1, -v[1], -v[2])))
        else:
            continue
        m = mul(m, t)
    return m


def ap(m, x, y):
    return (m[0]*x + m[2]*y + m[4], m[1]*x + m[3]*y + m[5])


def num(v, dec):
    r = round(v, dec)
    if r == 0:
        return '0'
    return ('%.*f' % (dec, r)).rstrip('0').rstrip('.') or '0'


def path_points(d):
    """Absolute points along a path's `d`, control points included."""
    pts, cur, start = [], [0.0, 0.0], [0.0, 0.0]
    for cmd, args in CMD_RE.findall(d or ''):
        vals = [float(x) for x in NUM_RE.findall(args)]
        rel, c = cmd.islower(), cmd.upper()
        if c == 'Z':
            cur = start[:]; pts.append(tuple(cur)); continue
        i = 0
        while i < len(vals):
            if c in ('M', 'L', 'T'):
                x, y = vals[i], vals[i+1]; i += 2
                if rel:
                    x += cur[0]; y += cur[1]
                cur = [x, y]
                if c == 'M' and i == 2:
                    start = cur[:]
            elif c == 'H':
                x = vals[i]; i += 1
                cur = [x + cur[0] if rel else x, cur[1]]
            elif c == 'V':
                y = vals[i]; i += 1
                cur = [cur[0], y + cur[1] if rel else y]
            elif c == 'C':
                if i + 5 >= len(vals): break
                for k in range(3):
                    x, y = vals[i+2*k], vals[i+2*k+1]
                    pts.append((x + cur[0] if rel else x, y + cur[1] if rel else y))
                x, y = vals[i+4], vals[i+5]
                cur = [x + cur[0] if rel else x, y + cur[1] if rel else y]; i += 6
            elif c in ('S', 'Q'):
                if i + 3 >= len(vals): break
                for k in range(2):
                    x, y = vals[i+2*k], vals[i+2*k+1]
                    pts.append((x + cur[0] if rel else x, y + cur[1] if rel else y))
                x, y = vals[i+2], vals[i+3]
                cur = [x + cur[0] if rel else x, y + cur[1] if rel else y]; i += 4
            elif c == 'A':
                if i + 6 >= len(vals): break
                x, y = vals[i+5], vals[i+6]
                cur = [x + cur[0] if rel else x, y + cur[1] if rel else y]; i += 7
            else:
                break
            pts.append(tuple(cur))
    return pts


def shape_points(el, m, acc):
    """Absolute corner points of a shape subtree (circle/ellipse/rect/use/path)."""
    tag = el.tag.split('}')[-1] if isinstance(el.tag, str) else ''
    m = mul(m, parse_tf(el.get('transform')))

    def g(a, d=0.0):
        try:
            return float(el.get(a))
        except (TypeError, ValueError):
            return d
    if tag == 'circle':
        cx, cy, r = g('cx'), g('cy'), g('r')
        acc += [ap(m, cx-r, cy-r), ap(m, cx+r, cy+r)]
    elif tag == 'ellipse':
        cx, cy, rx, ry = g('cx'), g('cy'), g('rx'), g('ry')
        acc += [ap(m, cx-rx, cy-ry), ap(m, cx+rx, cy+ry)]
    elif tag == 'rect':
        x, y, w, h = g('x'), g('y'), g('width'), g('height')
        acc += [ap(m, x, y), ap(m, x+w, y+h)]
    elif tag == 'use' and el.get('width') and el.get('height'):
        x, y = g('x'), g('y')
        acc += [ap(m, x, y), ap(m, x+float(el.get('width')), y+float(el.get('height')))]
    elif tag == 'path' and el.get('d'):
        pts = path_points(el.get('d'))
        if pts:
            xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
            acc += [ap(m, min(xs), min(ys)), ap(m, max(xs), max(ys))]
    for c in el:
        if isinstance(c.tag, str):
            shape_points(c, m, acc)
    return acc


def centre_and_size(el, m):
    pts = shape_points(el, m, [])
    if not pts:
        return None
    xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
    return ((min(xs)+max(xs))/2.0, (min(ys)+max(ys))/2.0,
            max(xs)-min(xs), max(ys)-min(ys))


# ------------------------------------------------------------- svg tidying ----
GEOM_ATTRS = ('x', 'y', 'width', 'height', 'cx', 'cy', 'r', 'rx', 'ry',
              'x1', 'y1', 'x2', 'y2')


def parse_style(s):
    out = []
    for decl in (s or '').split(';'):
        if ':' in decl:
            k, v = decl.split(':', 1)
            out.append((k.strip(), v.strip()))
    return out


def dec_for(scale):
    if scale <= 0:
        return 3
    return max(3, int(math.ceil(3 - math.log10(scale))))


def round_numbers(el, scale):
    """Round mm-space geometry to ~0.001 mm, with more decimals inside scaled
    groups so the error stays below that in millimetres."""
    dec = dec_for(scale)
    for a in GEOM_ATTRS:
        v = el.get(a)
        if v is not None:
            try:
                el.set(a, num(float(v), dec))
            except ValueError:
                pass
    d = el.get('d')
    if d:
        el.set('d', NUM_RE.sub(lambda m: num(float(m.group()), dec + 1), d))
    tf = el.get('transform')
    if tf:
        def one(m):
            name, args = m.group(1), m.group(2)
            v = [float(x) for x in re.split(r'[,\s]+', args.strip()) if x]
            if name == 'matrix':
                v = [num(v[0], 8), num(v[1], 8), num(v[2], 8), num(v[3], 8),
                     num(v[4], dec), num(v[5], dec)]
            elif name == 'translate':
                v = [num(x, dec) for x in v]
            else:
                v = [num(x, 8) for x in v]
            return '%s(%s)' % (name, ','.join(v))
        el.set('transform', re.sub(r'(\w+)\s*\(([^)]*)\)', one, tf))
    pairs = parse_style(el.get('style'))
    if pairs:
        new = []
        for k, v in pairs:
            if k in ('stroke-width', 'stroke-dashoffset'):
                try:
                    v = num(float(v), max(4, dec))
                except ValueError:
                    pass
            new.append((k, v))
        el.set('style', ';'.join('%s:%s' % kv for kv in new))


def round_tree(root):
    def walk(el, m):
        for ch in el:
            if not isinstance(ch.tag, str) or ch.tag == SVG + 'defs':
                continue            # vendored component artwork: leave alone
            own = mul(m, parse_tf(ch.get('transform')))
            round_numbers(ch, abs(own[0]) or 1.0)
            walk(ch, own)
    walk(root, (1, 0, 0, 1, 0, 0))


def prune_defs(root):
    """Drop <defs> entries nothing references any more (the component symbols
    become unused once the components layer holds only placeholders)."""
    defs = [d for d in root if isinstance(d.tag, str) and d.tag == SVG + 'defs']
    if not defs:
        return
    tops = {}
    for d in defs:
        for e in d:
            if isinstance(e.tag, str) and e.get('id'):
                tops[e.get('id')] = (d, e)

    def refs_of(el):
        out = set()
        for e in el.iter():
            if not isinstance(e.tag, str):
                continue
            for k, v in e.attrib.items():
                out |= set(re.findall(r'url\(#([^)]+)\)', v))
                if k.endswith('href') and v.startswith('#'):
                    out.add(v[1:])
        return out

    live = set()
    for g in root:
        if isinstance(g.tag, str) and g.tag != SVG + 'defs':
            live |= refs_of(g)
    changed = True
    while changed:
        changed = False
        for rid in list(live):
            if rid in tops:
                for r in refs_of(tops[rid][1]):
                    if r not in live:
                        live.add(r); changed = True
    for rid, (d, e) in list(tops.items()):
        if rid not in live:
            d.remove(e)
    for d in defs:
        if len(d) == 0:
            root.remove(d)


def indent_safe(el, space='  ', level=0):
    """Pretty-print, but never descend into <text>: injecting whitespace there
    would change the rendered string and shift the glyphs."""
    if not isinstance(el.tag, str) or el.tag == SVG + 'text' or len(el) == 0:
        return
    inner = '\n' + space * (level + 1)
    if not el.text or not el.text.strip():
        el.text = inner
    last = None
    for child in el:
        indent_safe(child, space, level + 1)
        if not child.tail or not child.tail.strip():
            child.tail = inner
        last = child
    if last is not None and (not last.tail or not last.tail.strip()):
        last.tail = '\n' + space * level


# ------------------------------------------------------------- generation ----
def layers(root):
    return [g for g in root if isinstance(g.tag, str)
            and g.get(INK + 'groupmode') == 'layer']


def layer_by_label(root, label):
    for g in layers(root):
        if (g.get(INK + 'label') or '').lower() == label.lower():
            return g
    return None


def collect(root):
    """[(kind, name, cx, cy, radius)] for every component and LED on the panel."""
    out = []
    comp = layer_by_label(root, 'Components')
    leds = layer_by_label(root, 'LEDs')

    if comp is not None:
        def walk(el, m):
            for ch in el:
                if not isinstance(ch.tag, str):
                    continue
                if ch.tag == SVG + 'use':
                    href = (ch.get(XLINK + 'href') or '').lstrip('#')
                    if href == 'ScrewSilver':
                        continue
                    geo = centre_and_size(ch, m)
                    if geo is None:
                        continue
                    name = ch.get('id') or href
                    if href in PARAM_SYMBOLS:
                        kind = 'param'
                    elif href in PORT_SYMBOLS:
                        kind = 'output' if '-output' in name else 'input'
                    else:
                        print('  ! unknown component %s (%s)' % (name, href))
                        continue
                    out.append((kind, name, geo[0], geo[1], min(geo[2], geo[3]) / 2.0))
                    continue
                walk(ch, mul(m, parse_tf(ch.get('transform'))))
        walk(comp, parse_tf(comp.get('transform')))

    if leds is not None:
        found = []

        def walk_led(el, m):
            for ch in el:
                if not isinstance(ch.tag, str):
                    continue
                if ch.tag in (SVG + 'path', SVG + 'circle', SVG + 'rect',
                              SVG + 'ellipse'):
                    geo = centre_and_size(ch, m)
                    if geo:
                        found.append(geo)
                    continue
                walk_led(ch, mul(m, parse_tf(ch.get('transform'))))
        walk_led(leds, parse_tf(leds.get('transform')))
        found.sort(key=lambda g: (round(g[1], 1), g[0]))
        names = (['left-led', 'right-led'] if len(found) == 2
                 else ['ch%d-led' % (i + 1) for i in range(len(found))])
        for name, g in zip(names, found):
            out.append(('light', name, g[0], g[1], min(g[2], g[3]) / 2.0))
    return out


def rebuild_components(root, items, log):
    comp = layer_by_label(root, 'Components')
    leds = layer_by_label(root, 'LEDs')
    if leds is not None:
        root.remove(leds)
        log.append('  LEDs layer folded into components and removed')
    if comp is None:
        return
    for ch in list(comp):
        comp.remove(ch)
    comp.set(INK + 'label', 'components')      # the layer name helper.py expects
    comp.set('id', 'components')
    comp.attrib.pop('transform', None)
    for kind, name, cx, cy, r in items:
        ET.SubElement(comp, SVG + 'circle', {
            'id': name, INK + 'label': name,
            'cx': num(cx, 3), 'cy': num(cy, 3), 'r': num(max(r, 0.4), 3),
            'fill': PLACEHOLDER[kind]})
    # hidden, but still in the file: helper.py does no visibility check
    comp.set('style', 'display:none')
    counts = {}
    for kind, *_ in items:
        counts[kind] = counts.get(kind, 0) + 1
    log.append('  placeholders: %s' % ', '.join(
        '%d %s' % (counts[k], k) for k in ('param', 'input', 'output', 'light')
        if k in counts))


def darken(root):
    """Remap panel artwork to the dark palette, leaving placeholders alone.

    The whole components subtree is excluded, not just the layer element: the
    placeholder colours are fixed by the Rack convention and must survive in
    both themes.
    """
    comp = layer_by_label(root, 'components')
    skip = {id(e) for e in comp.iter()} if comp is not None else set()

    def sub(value):
        return HEX.sub(lambda m: DARK.get(m.group(0).lower(), m.group(0)), value)

    for el in root.iter():
        if not isinstance(el.tag, str) or id(el) in skip:
            continue
        for a in PAINT_ATTRS:
            v = el.get(a)
            if v and HEX.search(v):
                el.set(a, sub(v))
        st = el.get('style')
        if st and HEX.search(st):
            el.set('style', sub(st))


def header(module, width_mm, dark):
    return ('<!-- SP4T %s panel, %s theme. %g HP. Units: mm.\n'
            '     Components layer holds VCV Rack placeholders (see\n'
            '     https://vcvrack.com/manual/Panel#Adding-components); it is\n'
            '     removed when the production panel is exported. -->'
            % (module, 'dark' if dark else 'light', round(width_mm / MM_PER_HP, 3)))


def write(root, path, module, width_mm, dark):
    out = copy.deepcopy(root)
    if dark:
        darken(out)
    prune_defs(out)
    round_tree(out)
    indent_safe(out)
    text = ('<?xml version="1.0" encoding="UTF-8" standalone="no"?>\n'
            + header(module, width_mm, dark) + '\n'
            + ET.tostring(out, encoding='unicode').rstrip() + '\n')
    with open(path, 'w', encoding='utf-8') as fh:
        fh.write(text)
    return len(text)


def process(src, outdir, log):
    raw = open(src, encoding='utf-8').read()
    root = ET.fromstring(raw, parser=ET.XMLParser(
        target=ET.TreeBuilder(insert_comments=True)))
    stem = os.path.splitext(os.path.basename(src))[0]
    module = stem[:-6] if stem.endswith('Test01') else stem
    width = float(root.get('viewBox').split()[2])
    pretty = re.sub(r'(?<!^)(?=[A-Z0-9])', ' ', module[4:]).strip()

    items = collect(root)
    rebuild_components(root, items, log)
    a = write(root, os.path.join(outdir, module + '.svg'), pretty, width, False)
    b = write(root, os.path.join(outdir, module + '-dark.svg'), pretty, width, True)
    return module, len(items), a, b


def main(argv):
    proto = argv[1] if len(argv) > 1 else 'res/proto'
    outdir = argv[2] if len(argv) > 2 else 'res/staging'
    if not os.path.isdir(proto):
        sys.exit('no such directory: %s' % proto)
    os.makedirs(outdir, exist_ok=True)
    total = 0
    for src in sorted(glob.glob(os.path.join(proto, '*.svg'))):
        log = []
        module, n, a, b = process(src, outdir, log)
        total += n
        print('%-18s -> %s.svg (%d B) + %s-dark.svg (%d B)'
              % (module, module, a, module, b))
        for line in log:
            print(line)
    print('\ntotal placeholders: %d' % total)


if __name__ == '__main__':
    main(sys.argv)
