#!/usr/bin/env python3
"""Checks every factory preset in Sources/GuitarSynthX/Presets.swift.

1. Static checks against the engine's parameter table (Sources/GSXCore/Params.cpp):
   known keys, values in range, whole numbers for choice/toggle/integer params,
   no "rig" parameters (input, tracking calibration, master), unique names,
   known categories.
2. Renders each preset through the real engine (Tests/preset_render.cpp) and
   checks that it is finite, plays every test note, doesn't clip, and is
   level-matched (within --tolerance dB of the target loudness).

    python3 Tests/check_presets.py              # check
    python3 Tests/check_presets.py --suggest    # also print amp.level fixes
"""
import argparse, collections, os, re, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TARGET_LUFS = -26.0
RIG = {"trk.lowest", "trk.highest", "trk.threshold", "trk.stability", "trk.hop", "trk.retrigger"}

def load_params():
    src = open(os.path.join(ROOT, "Sources/GSXCore/Params.cpp")).read()
    rows = re.findall(r'^\s+\{"([a-zA-Z.]+)",\s*"[^"]*",\s*"[^"]*",\s*"[^"]*",\s*(?:"[^"]*"|kWaves),'
                      r'\s*([-0-9.f]+),\s*([-0-9.f]+),\s*([-0-9.f]+),\s*(C|I|CH|T),', src, re.M)
    f = lambda x: float(x.rstrip("f"))
    return {k: (f(a), f(b), f(c), t) for k, a, b, c, t in rows}

def load_presets():
    src = open(os.path.join(ROOT, "Sources/GuitarSynthX/Presets.swift")).read()
    order = re.search(r"static let categoryOrder: \[String\] = \[(.*?)\]", src).group(1)
    cats = re.findall(r'"([^"]+)"', order)
    presets = []
    for m in re.finditer(r'p\("((?:[^"\\]|\\.)+)", "([^"]+)", \[(.*?)\]\)', src, re.S):
        vals = {k: float(v) for k, v in re.findall(r'"([a-zA-Z.]+)":\s*(-?[0-9.]+)', m.group(3))}
        presets.append((m.group(1).replace('\\"', '"'), m.group(2), vals))
    return cats, presets

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tolerance", type=float, default=1.0)
    ap.add_argument("--suggest", action="store_true")
    args = ap.parse_args()
    params = load_params()
    cats, presets = load_presets()
    errors = []
    for n, c in collections.Counter(p[0] for p in presets).items():
        if c > 1: errors.append(f"duplicate preset name: {n}")
    for name, cat, vals in presets:
        if cat not in cats: errors.append(f"{name}: category '{cat}' missing from categoryOrder")
        for k, v in vals.items():
            if k not in params: errors.append(f"{name}: unknown parameter {k}"); continue
            if k.startswith(("in.", "out.", "mst.")) or k in RIG: errors.append(f"{name}: rig parameter {k} in a preset")
            lo, hi, _, t = params[k]
            if not lo <= v <= hi: errors.append(f"{name}: {k}={v:g} outside {lo:g}..{hi:g}")
            if t != "C" and v != int(v): errors.append(f"{name}: {k}={v:g} must be a whole number")
    print(f"{len(presets)} presets in {len(set(p[1] for p in presets))} categories; static errors: {len(errors)}")
    for e in errors: print("  " + e)

    # ---- render through the engine
    tmp = tempfile.mkdtemp()
    exe = os.path.join(tmp, "preset_render")
    srcs = [os.path.join(ROOT, "Tests/preset_render.cpp")] + [os.path.join(ROOT, "Sources/GSXCore", f) for f in
            ("Engine.cpp", "PitchTracker.cpp", "Synth.cpp", "Effects.cpp", "Params.cpp")]
    extra = ["-framework", "Accelerate"] if sys.platform == "darwin" else []
    cxx = os.environ.get("CXX", "clang++")
    subprocess.run([cxx, "-std=c++17", "-O2", "-o", exe] + srcs + extra, check=True)
    listing = os.path.join(tmp, "presets.txt")
    with open(listing, "w") as f:
        for name, _, vals in presets:
            f.write(name + "|" + ",".join(f"{k}={v}" for k, v in vals.items()) + "\n")
    out = subprocess.run([exe, listing], capture_output=True, text=True, check=True).stdout
    render_errors = 0
    for line in out.strip().split("\n"):
        name, lufs, peak, finite, notes = line.split("|")
        lufs, peak = float(lufs), float(peak)
        problems = []
        if finite != "1": problems.append("non-finite output")
        if notes != "4": problems.append(f"only {notes}/4 notes triggered")
        if peak >= 0.97: problems.append(f"peak {peak:.2f} hits the limiter")
        if abs(lufs - TARGET_LUFS) > args.tolerance: problems.append(f"loudness {lufs:.1f} LUFS (target {TARGET_LUFS:g})")
        if problems:
            render_errors += 1
            level = dict(next(p for p in presets if p[0] == name)[2]).get("amp.level", 0.0)
            fix = f'  -> set "amp.level": {max(-24, min(12, level + TARGET_LUFS - lufs)):.1f}' if args.suggest else ""
            print(f"  {name}: " + "; ".join(problems) + fix)
    print(f"render check: {len(presets) - render_errors}/{len(presets)} presets OK")
    sys.exit(1 if errors or render_errors else 0)

if __name__ == "__main__":
    main()
