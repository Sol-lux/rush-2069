"""Overnight Battle-mode RAM probe. Each episode: boot, menus to 2P Battle (track as default), a scenario of scripted
input (port 2 copies port 1 through the menus, then stays still: red is a parked target), RDRAM dumped every second, a screenshot every second (health-bar widths measured from
it). Per episode the words that changed are kept (npz), the raw dumps deleted."""
import json, os, random, subprocess, sys, time, struct
import numpy as np
from PIL import Image

EXE = r"E:\Rush2069\build\Rush2049Recompiled.exe"
ROOT = os.path.dirname(os.path.abspath(__file__))
PREFIX = [(0, 0), (10, 0x1000), (10.2, 0), (12, 0x0100), (12.2, 0), (13, 0x8000), (13.2, 0), (15, 0x8000), (15.2, 0),
          (16, 0x0400), (16.15, 0), (16.5, 0x0400), (16.65, 0), (17, 0x0400), (17.15, 0), (18, 0x8000), (18.2, 0),
          (20, 0x8000), (20.2, 0), (22, 0x8000), (22.2, 0), (24, 0x8000), (24.2, 0), (27, 0x8000), (27.2, 0)]
A, B, Z, L, R, CU, CD, CL, CR = 0x8000, 0x4000, 0x2000, 0x20, 0x10, 0x8, 0x4, 0x2, 0x1
T0, T1 = 30, 120  # race is live by ~28 s

def scenario(kind, rng):
    ev = []; t = T0
    while t < T1:
        if kind == "idle":
            ev.append((t, 0, 0, 0)); t += 5; continue
        gas = A
        x = rng.choice([-40, -15, 0, 0, 15, 40]) if kind != "straight" else 0
        fire = 0
        if kind.startswith("fire_"):
            fire = {"fire_z": Z, "fire_b": B, "fire_r": R, "fire_l": L, "fire_cd": CD, "fire_cu": CU}[kind]
        if kind == "hold_z":
            ev.append((t, gas | Z, x, 0)); t += 2; continue
        if kind == "mixed":
            fire = rng.choice([0, Z, B, R, L, CD, CU, CL, CR])
        if fire:
            ev.append((t, gas | fire, x, 0)); ev.append((t + 0.15, gas, x, 0)); t += rng.choice([0.5, 1.0, 2.0])
        else:
            ev.append((t, gas, x, 0)); t += rng.choice([1.0, 2.0])
    return ev

def bar(img, y, x0, x1):
    row = np.asarray(img)[y, x0:x1].astype(int)
    return int(((row[:, 1] > 60) & (row[:, 0] < 40) & (row[:, 2] < 40)).sum())

def episode(n, kind):
    rng = random.Random(n)
    d = os.path.join(ROOT, f"ep{n:03d}_{kind}"); os.makedirs(d, exist_ok=True)
    lines = [f"{t} {b:x}" for t, b in PREFIX] + [f"{t:.2f} {b | 0x20000:x} {x} {y}" for t, b, x, y in scenario(kind, rng)]
    open(os.path.join(d, "input.txt"), "w").write("\n".join(lines) + "\n")
    times = ",".join(str(t) for t in range(T0 - 2, T1))
    log = open(os.path.join(d, "log.txt"), "w")
    p = subprocess.Popen([EXE, "--boot", "--players", "2", "--input", os.path.join(d, "input.txt"),
                          "--dump-ram", os.path.join(d, "d"), times], stdout=log, stderr=subprocess.STDOUT, cwd="E:/Rush2069")
    start = time.time(); bars = []
    while time.time() - start < T1 + 4 and p.poll() is None:
        time.sleep(1); s = time.time() - start
        if s > T0 - 3:
            png = os.path.join(d, f"s{int(s)}.png")
            subprocess.run([sys.executable, "-I", os.path.join(ROOT, "shot.py"), str(p.pid), png], capture_output=True)
            try:
                im = Image.open(png).convert("RGB"); w, h = im.size
                bars.append((round(s, 1), bar(im, int(h * 0.444), int(w * 0.31), int(w * 0.48)), bar(im, int(h * 0.944), int(w * 0.31), int(w * 0.48))))
                if int(s) % 10 and s < T1 - 10: os.remove(png)  # keep one shot in ten
            except Exception as e:
                bars.append((round(s, 1), -1, -1))
    crashed = p.poll(); died_at = round(time.time() - start, 1)
    p.kill(); p.wait()
    files = sorted((f for f in os.listdir(d) if f.startswith("d") and f.endswith(".bin")), key=lambda f: int(f[1:-4]))
    if len(files) < 10:
        json.dump({"kind": kind, "error": "few dumps", "dumps": len(files), "crashed": crashed}, open(os.path.join(d, "meta.json"), "w")); return
    arr = np.stack([np.frombuffer(open(os.path.join(d, f), "rb").read(), dtype=">u4") for f in files])
    state = arr[:, (0x801174B4 - 0x80000000) // 4]
    changed = np.nonzero((arr != arr[0]).any(axis=0))[0]
    np.savez_compressed(os.path.join(d, "changed.npz"), idx=changed.astype(np.uint32), val=arr[:, changed], state=state,
                        first=arr[0])
    for f in files: os.remove(os.path.join(d, f))
    json.dump({"kind": kind, "dump_times": list(range(T0 - 2, T0 - 2 + len(files))), "bars": bars, "exit": crashed, "died_at": died_at, "died_at": died_at,
               "racing": int(((state & 0x400000) != 0).sum())}, open(os.path.join(d, "meta.json"), "w"))
    print(time.strftime("%H:%M:%S"), d, "racing dumps", int(((state & 0x400000) != 0).sum()), "changed", len(changed), flush=True)

if __name__ == "__main__":
    kinds = ["idle", "straight", "fire_z", "fire_b", "fire_r", "fire_l", "fire_cd", "fire_cu", "hold_z", "mixed"]
    n0 = int(sys.argv[1]) if len(sys.argv) > 1 else 0; count = int(sys.argv[2]) if len(sys.argv) > 2 else 200
    for n in range(n0, n0 + count):
        if os.path.exists(os.path.join(ROOT, "STOP")): break
        episode(n, kinds[n % len(kinds)] if n < 2 * len(kinds) else random.Random(n).choice(kinds[1:]))
