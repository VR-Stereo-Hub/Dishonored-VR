#!/usr/bin/env python3
"""blade-marker-check.py - VR-173 prerequisite 2: does the MEASURED blade sit on the DRAWN
blade, in both eyes?

The mod measures the held sword's blade (a base near the palm and a tip) and, with
`blade marker on`, hands the simulated headset two dots at those points. This reads one
pose's simulator captures and answers with numbers instead of a look:

  <pose>_marker.json   the capture taken with the marker ON: the dots' positions as the
                       runtime received them, and each eye's position and field of view
  <pose>_drawn_*.png   the same pose, marker OFF, sword drawn
  <pose>_bare_*.png    the same pose, marker OFF, sword SHEATHED

  1. Each dot is projected into each eye's pixels from the JSON alone.
  2. The projection is CHECKED against the image: marker-on minus marker-off must put a
     bright spot where the tip dot was predicted. If it does not, the projection used here
     is wrong and nothing below means anything.
  3. Drawn minus bare is the sword's own silhouette. Along the predicted blade line, in a
     corridor wide enough to disagree with it, the furthest silhouette pixel is where the
     drawn blade ENDS.
  4. The answer is two distances at the tip, in centimetres at the tip's own range: how far
     the drawn blade's end is ALONG the blade from the measured tip, and how far ACROSS.

It can fail both ways. A blade measured too long or too short fails ALONG. A blade measured
off to one side fails ACROSS; `blade marker offset 0.10` is that case made on purpose, and
the run is only evidence if that capture FAILS here (pass --expect-fail for it).

Usage:
  python tools/blade-marker-check.py <dir> <pose> [--along-cm 3.0] [--across-cm 2.0]
                                     [--marker <name>] [--expect-fail]

The captures are the game's own image and are never committed; this tool and its verdict are.
"""
import io
import json
import math
import os
import sys

from PIL import Image, ImageChops, ImageDraw


def tan_deg(d):
    return math.tan(math.radians(d))


def quat_conj_rotate(q, v):
    """Rotate v by the inverse of q (x, y, z, w): world into the head's frame."""
    x, y, z, w = -q[0], -q[1], -q[2], q[3]
    vx, vy, vz = v
    tx = 2.0 * (y * vz - z * vy)
    ty = 2.0 * (z * vx - x * vz)
    tz = 2.0 * (x * vy - y * vx)
    return (vx + w * tx + (y * tz - z * ty),
            vy + w * ty + (z * tx - x * tz),
            vz + w * tz + (x * ty - y * tx))


class Eye:
    def __init__(self, view, head_quat, width, height):
        self.pos = view["pos"]
        f = view["fovDeg"]
        self.tl, self.tr, self.tu, self.td = tan_deg(f["l"]), tan_deg(f["r"]), tan_deg(f["u"]), tan_deg(f["d"])
        self.q = head_quat
        self.w, self.h = width, height

    def project(self, p):
        v = quat_conj_rotate(self.q, (p[0] - self.pos[0], p[1] - self.pos[1], p[2] - self.pos[2]))
        if v[2] > -1e-4:
            return None
        tx, ty = v[0] / -v[2], v[1] / -v[2]
        x = (tx - self.tl) / (self.tr - self.tl) * self.w
        y = (self.tu - ty) / (self.tu - self.td) * self.h
        rng = math.sqrt(v[0] ** 2 + v[1] ** 2 + v[2] ** 2)
        return (x, y, rng, tx, ty)

    def cm_per_px(self, proj):
        """What one pixel is worth at this point's range: the angle a pixel spans there."""
        x, y, rng, tx, ty = proj
        dtan = (self.tr - self.tl) / self.w
        ang = dtan / (1.0 + tx * tx + ty * ty) ** 0.5     # a lower bound on the angle; good to a few percent off axis
        return rng * ang * 100.0


def load(path):
    return Image.open(path).convert("RGB")


def diff_mask(a, b, threshold):
    d = ImageChops.difference(a, b)
    r, g, bl = d.split()
    s = ImageChops.add(ImageChops.add(r, g), bl)
    return s.point(lambda v: 255 if v >= threshold else 0)


def check_eye(name, eye, tip, base, marker_img, drawn_img, bare_img, along_cm, across_cm, out_png):
    res = {"eye": name, "ok": False, "why": ""}
    pt, pb = eye.project(tip), eye.project(base)
    if pt is None or pb is None:
        res["why"] = "the marker is behind the eye"
        return res
    inside = lambda p: 0 <= p[0] < eye.w and 0 <= p[1] < eye.h
    if not inside(pt) or not inside(pb):
        res["why"] = "the measured %s is outside this eye's image (tip %.0f,%.0f base %.0f,%.0f): choose a pose with the whole blade in view" % (
            "tip" if not inside(pt) else "base", pt[0], pt[1], pb[0], pb[1])
        return res
    res["tipPx"] = (pt[0], pt[1]); res["basePx"] = (pb[0], pb[1])
    ux, uy = pt[0] - pb[0], pt[1] - pb[1]
    length = math.hypot(ux, uy)
    if length < 30:
        res["why"] = "the blade is %.0f px long in this eye: end on, nothing to judge" % length
        return res
    ux, uy = ux / length, uy / length
    scale = eye.cm_per_px(pt)
    res["cmPerPx"] = scale

    # 2. is the projection right? the tip dot must be where it was predicted
    md = diff_mask(marker_img, drawn_img, 60).load()
    sx = sy = n = 0
    rad = 14
    for y in range(max(0, int(pt[1]) - rad), min(eye.h, int(pt[1]) + rad + 1)):
        for x in range(max(0, int(pt[0]) - rad), min(eye.w, int(pt[0]) + rad + 1)):
            if md[x, y]:
                sx += x; sy += y; n += 1
    if n < 4:
        res["why"] = "no marker dot within %d px of where the tip was predicted: the projection used by this check is not the runtime's, or the marker was not drawn" % rad
        return res
    dot = (sx / n + 0.5, sy / n + 0.5)
    res["dotPx"] = dot
    res["projectionErrPx"] = math.hypot(dot[0] - pt[0], dot[1] - pt[1])
    if res["projectionErrPx"] > 3.0:
        res["why"] = "the tip dot is %.1f px from its predicted pixel: the projection is not trustworthy" % res["projectionErrPx"]
        return res

    # 3. the drawn blade's own silhouette, in a corridor about the predicted line
    sm = diff_mask(drawn_img, bare_img, 36).load()
    t0, t1, half = 0.30 * length, 1.60 * length, 0.35 * length
    pts = []
    x0 = int(max(0, min(pb[0], pb[0] + ux * t1) - half)); x1 = int(min(eye.w - 1, max(pb[0], pb[0] + ux * t1) + half))
    y0 = int(max(0, min(pb[1], pb[1] + uy * t1) - half)); y1 = int(min(eye.h - 1, max(pb[1], pb[1] + uy * t1) + half))
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if not sm[x, y]:
                continue
            dx, dy = x + 0.5 - pb[0], y + 0.5 - pb[1]
            t = dx * ux + dy * uy
            s = -dx * uy + dy * ux
            if t0 <= t <= t1 and abs(s) <= half:
                pts.append((t, s))
    res["silhouettePx"] = len(pts)
    if len(pts) < 40:
        res["why"] = "only %d silhouette pixels in the corridor: the drawn and sheathed captures do not differ along the predicted blade" % len(pts)
        return res
    # The blade is ONE narrow connected run. Two captures taken seconds apart differ in more
    # than the sword (a guard walks, a light flickers, a rat moves), and the first version of
    # this step took the furthest difference in the corridor for the blade's end: it put the
    # "end" on something that had moved 33 cm past the tip. So: find where the run STARTS
    # (the densest across-position near the hand, anywhere in the corridor, so a blade that
    # is off the predicted line is still found), then follow it outward a bin at a time,
    # accepting only pixels that continue it, until it stops.
    bins = {}
    step = 4.0
    for t, s in pts:
        bins.setdefault(int(t // step), []).append(s)
    first = int(t0 // step)
    start = [s for k in range(first, first + 8) for s in bins.get(k, [])]
    if len(start) < 12:
        res["why"] = "no silhouette near the hand end of the predicted blade (%d pixels in the first %d px)" % (len(start), int(8 * step))
        return res
    bucket = 6.0
    hist = {}
    for s in start:
        hist[int(math.floor(s / bucket))] = hist.get(int(math.floor(s / bucket)), 0) + 1
    s_run = (max(hist, key=hist.get) + 0.5) * bucket
    # A bin continues the run only if it holds a fair share of what the run's recent bins
    # held: image noise past the tip is a few scattered pixels, the blade is a filled strip
    # that narrows slowly. The second version of this step had no such floor and walked 16 cm
    # past the tip on speckle in one eye.
    # The share is of the WHOLE run's median, not of its last few bins: a guard walking behind
    # the blade fattens a stretch of the silhouette, and measured against that stretch the
    # blade beyond it looked too thin to be the blade (the third version ended 19 cm short).
    band, max_gap = 7.0, 4
    recent = []
    gap = 0; last = None; k = first
    while k * step <= t1 and gap < max_gap:
        near = sorted(s for s in bins.get(k, []) if abs(s - s_run) <= band)
        floor = 3
        if len(recent) >= 3:
            r = sorted(recent)
            floor = max(3, int(0.30 * r[len(r) // 2]))
        if len(near) >= floor:
            s_run = 0.6 * s_run + 0.4 * near[len(near) // 2]
            recent.append(len(near))
            last = k; gap = 0
        else:
            gap += 1
        k += 1
    if last is None:
        res["why"] = "no run of silhouette along the predicted blade"
        return res
    s_tip = s_run
    t_tip = (last + 1) * step
    res["along_cm"] = (t_tip - length) * scale
    res["across_cm"] = s_tip * scale
    res["ok"] = abs(res["along_cm"]) <= along_cm and abs(res["across_cm"]) <= across_cm
    res["why"] = "on the blade" if res["ok"] else "OFF the drawn blade"

    if out_png:
        im = drawn_img.copy(); d = ImageDraw.Draw(im)
        d.line([(pb[0], pb[1]), (pt[0], pt[1])], fill=(0, 255, 0), width=1)
        d.ellipse([pt[0] - 5, pt[1] - 5, pt[0] + 5, pt[1] + 5], outline=(0, 255, 0))
        ex, ey = pb[0] + ux * t_tip - uy * s_tip, pb[1] + uy * t_tip + ux * s_tip
        d.ellipse([ex - 5, ey - 5, ex + 5, ey + 5], outline=(255, 0, 0))
        im.save(out_png)
    return res


def main(argv):
    if len(argv) < 2:
        print(__doc__); return 2
    folder, pose = argv[0], argv[1]
    along_cm, across_cm, marker, expect_fail = 3.0, 2.0, pose + "_marker", False
    i = 2
    while i < len(argv):
        if argv[i] == "--along-cm": along_cm = float(argv[i + 1]); i += 2
        elif argv[i] == "--across-cm": across_cm = float(argv[i + 1]); i += 2
        elif argv[i] == "--marker": marker = argv[i + 1]; i += 2
        elif argv[i] == "--expect-fail": expect_fail = True; i += 1
        else: print("unknown argument", argv[i]); return 2
    j = json.load(io.open(os.path.join(folder, marker + ".json"), encoding="utf-8-sig"))
    quads = [L for L in j["layers"] if L["type"] == "quad" and L.get("space") == "local" and max(L["sizeM"]) < 0.05]
    if len(quads) < 2:
        print("%s: %d small local quad(s) in the capture, the marker needs two (is `blade marker on`, and the aim dot off?)" % (marker, len(quads)))
        return 1
    # The mod appends the tip first (the largest dot), then the base.
    tip, base = quads[0]["pose"][:3], quads[1]["pose"][:3]
    blade_m = math.dist(tip, base)
    print("%s: tip (%.3f %.3f %.3f) base (%.3f %.3f %.3f) XR LOCAL m, %.3f m apart | tolerance %.1f cm along, %.1f cm across" % (
        marker, tip[0], tip[1], tip[2], base[0], base[1], base[2], blade_m, along_cm, across_cm))
    ok_all = True
    for k, view in enumerate(j["views"]):
        name = view["eye"]
        eye = Eye(view, j["head"]["quat"], j["width"], j["height"])
        paths = [os.path.join(folder, "%s_%s.png" % (n, name)) for n in (marker, pose + "_drawn", pose + "_bare")]
        miss = [p for p in paths if not os.path.exists(p)]
        if miss:
            print("  %s: missing %s" % (name, ", ".join(miss))); ok_all = False; continue
        r = check_eye(name, eye, tip, base, load(paths[0]), load(paths[1]), load(paths[2]), along_cm, across_cm,
                      os.path.join(folder, "%s_check_%s.png" % (marker, name)))
        if "along_cm" in r:
            print("  %s: %s - the drawn blade ends %+.1f cm along and %+.1f cm across from the measured tip (tip at pixel %.0f,%.0f, "
                  "%.2f cm a pixel there; the dot landed %.1f px from its predicted pixel; %d silhouette pixels)" % (
                      name, "PASS" if r["ok"] else "FAIL", r["along_cm"], r["across_cm"], r["tipPx"][0], r["tipPx"][1],
                      r["cmPerPx"], r["projectionErrPx"], r["silhouettePx"]))
        else:
            print("  %s: NOT JUDGED - %s" % (name, r["why"]))
        ok_all = ok_all and r["ok"]
    if expect_fail:
        print("VERDICT: %s" % ("the deliberate error was CAUGHT, so the check can fail" if not ok_all
                               else "the deliberate error PASSED: this check cannot see a marker that is off the blade, and is not evidence"))
        return 0 if not ok_all else 1
    print("VERDICT: %s" % ("the measured blade sits on the drawn blade in both eyes" if ok_all else "NOT accepted"))
    return 0 if ok_all else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
