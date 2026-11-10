#!/usr/bin/env python3
"""Pick textures by eye, for Glow (docs/mods.md, Lighting) or anything else keyed by RT64's texture hash.

  texsheet.py sheet <dl.txt> <ram.bin> <out.png>
      A numbered contact sheet of every texture one frame draws, from --dump-dl and --dump-ram taken at the same
      second: the TMEM loads replayed from the display list, each drawn tile decoded, see-through draws first (orange
      frame). Writes <out.png>.txt (a line a number) and <out.png>.entries/<n>.png.
  texsheet.py match <out.png> <dump dir> <n> [<n> ...]
      RT64's hash for each picked number: the textures --dump-textures wrote (<hash>.v5.tmem and .tile.json) decoded
      the same way and compared pixel by pixel, the three nearest printed (mean RGB difference, 0 = the same; one far
      nearer than the rest is the match). A sky texture can differ by a few levels. A 32-bit texture doesn't decode
      cleanly from RT64's TMEM (it keeps the halves apart): compare those by eye. A texture drawn after the dump's run
      stopped isn't there: run it longer.

Needs Pillow. Reads dumps of the game's memory: keep them and the sheets out of git.
"""
import glob
import json
import os
import re
import sys

from PIL import Image, ImageDraw

ram = b''
tmem = bytearray(4096)
seg = [0] * 16
timg = dict(addr=0, fmt=0, siz=0, width=1)
tiles = [dict(fmt=0, siz=0, line=0, tmem=0, pal=0, w=0, h=0) for _ in range(8)]
state = dict(render_mode=0, tlut_mode=0, tile=0)
seen = {}


def phys(a):
    if (a >> 24) >= 0x80:
        return a & 0x7FFFFF
    return (seg[(a >> 24) & 0xF] + (a & 0xFFFFFF)) & 0x7FFFFF


def c5(v):
    return (v << 3) | (v >> 2)


def pal(p, rt64_tlut):
    if rt64_tlut:  # RT64's TMEM holds each TLUT entry four times over, 8 bytes an entry
        p = 0x800 + ((p - 0x800) // 2) * 8
    p &= 0xFFF
    v = (tmem[p] << 8) | tmem[(p + 1) & 0xFFF]
    if state['tlut_mode'] == 3:  # IA16
        return (v >> 8, v >> 8, v >> 8, v & 0xFF)
    return (c5(v >> 11), c5((v >> 6) & 31), c5((v >> 1) & 31), 255 if v & 1 else 0)


def decode(t, rt64_tlut=False):
    w, h = t['w'], t['h']
    if w <= 0 or h <= 0 or w > 256 or h > 256:
        return None
    img = Image.new('RGBA', (w, h))
    px = img.load()
    base = t['tmem'] * 8
    stride = max(t['line'], 1) * 8
    fmt, siz = t['fmt'], t['siz']
    for y in range(h):
        row = base + y * stride
        for x in range(w):
            if siz == 0:
                b = tmem[(row + x // 2) & 0xFFF]
                v = (b >> 4) if x % 2 == 0 else (b & 0xF)
                if fmt == 2:  # CI4
                    px[x, y] = pal(0x800 + t['pal'] * 32 + v * 2, rt64_tlut)
                elif fmt == 3:  # IA4
                    i = (v >> 1) * 255 // 7
                    px[x, y] = (i, i, i, 255 if v & 1 else 0)
                else:  # I4
                    px[x, y] = (v * 17, v * 17, v * 17, v * 17)
            elif siz == 1:
                v = tmem[(row + x) & 0xFFF]
                if fmt == 2:  # CI8
                    px[x, y] = pal(0x800 + v * 2, rt64_tlut)
                elif fmt == 3:  # IA8
                    i = (v >> 4) * 17
                    px[x, y] = (i, i, i, (v & 0xF) * 17)
                else:  # I8
                    px[x, y] = (v, v, v, v)
            elif siz == 2:
                o = (row + x * 2) & 0xFFF
                v = (tmem[o] << 8) | tmem[(o + 1) & 0xFFF]
                if fmt == 3:  # IA16
                    px[x, y] = (v >> 8, v >> 8, v >> 8, v & 0xFF)
                else:  # RGBA16
                    px[x, y] = (c5(v >> 11), c5((v >> 6) & 31), c5((v >> 1) & 31), 255 if v & 1 else 0)
            else:  # RGBA32, read as it lies
                o = (row + x * 4) & 0xFFF
                px[x, y] = tuple(tmem[(o + k) & 0xFFF] for k in range(4))
    return img


def load(dst, src, n):
    for i in range(n):
        tmem[(dst + i) & 0xFFF] = ram[(src + i) & 0x7FFFFF]


def replay(dl_path):
    global timg
    order = 0
    for line in open(dl_path):
        m = re.match(r'\s*[0-9A-F]{8}: ([0-9A-F]{8}) ([0-9A-F]{8})', line)
        if not m:
            continue
        w0, w1 = int(m.group(1), 16), int(m.group(2), 16)
        op = w0 >> 24
        if op == 0xDB and ((w0 >> 16) & 0xFF) == 6:  # MOVEWORD segment
            seg[(w0 & 0xFFFF) // 4] = w1 & 0xFFFFFF
        elif op == 0xFD:  # SETTIMG
            timg = dict(addr=phys(w1), fmt=(w0 >> 21) & 7, siz=(w0 >> 19) & 3, width=(w0 & 0xFFF) + 1)
        elif op == 0xF5:  # SETTILE
            tiles[(w1 >> 24) & 7].update(fmt=(w0 >> 21) & 7, siz=(w0 >> 19) & 3, line=(w0 >> 9) & 0x1FF,
                                         tmem=w0 & 0x1FF, pal=(w1 >> 20) & 0xF)
        elif op == 0xF2:  # SETTILESIZE
            t = tiles[(w1 >> 24) & 7]
            t['w'] = (((w1 >> 12) & 0xFFF) - ((w0 >> 12) & 0xFFF)) // 4 + 1
            t['h'] = ((w1 & 0xFFF) - (w0 & 0xFFF)) // 4 + 1
        elif op == 0xF3:  # LOADBLOCK
            t = tiles[(w1 >> 24) & 7]
            texels = ((w1 >> 12) & 0xFFF) + 1
            load(t['tmem'] * 8, timg['addr'], min((texels << timg['siz']) >> 1, 4096))
        elif op == 0xF4:  # LOADTILE
            t = tiles[(w1 >> 24) & 7]
            uls, ult = ((w0 >> 12) & 0xFFF) >> 2, (w0 & 0xFFF) >> 2
            lrs, lrt = ((w1 >> 12) & 0xFFF) >> 2, (w1 & 0xFFF) >> 2
            bpp = (1 << timg['siz']) / 2
            for r in range(lrt - ult + 1):
                src = timg['addr'] + int(((ult + r) * timg['width'] + uls) * bpp)
                load(t['tmem'] * 8 + r * t['line'] * 8, src, int((lrs - uls + 1) * bpp))
        elif op == 0xF0:  # LOADTLUT
            t = tiles[(w1 >> 24) & 7]
            load(t['tmem'] * 8, timg['addr'], (((w1 >> 14) & 0x3FF) + 1) * 2)
        elif op == 0xE2 and (w0 & 0xFF) == 0x1C:  # render mode
            state['render_mode'] = w1
        elif op == 0xE3 and ((w0 >> 8) & 0xFF) == 0x10:  # TLUT type
            state['tlut_mode'] = (w1 >> 14) & 3
        elif op == 0xD7:  # TEXTURE
            state['tile'] = (w0 >> 8) & 7
        elif op in (0x05, 0x06, 0x07):  # TRI1, TRI2, QUAD
            t = tiles[state['tile']]
            start = t['tmem'] * 8
            key = (t['fmt'], t['siz'], t['w'], t['h'], bytes(tmem[start:start + max(t['line'], 1) * 8 * t['h']]))
            if key in seen:
                seen[key]['calls'] += 1
                continue
            img = decode(t)
            if img is None:
                continue
            rm = state['render_mode']
            # the second cycle's blender B input is 1-A: see-through
            seen[key] = dict(img=img, xlu=((rm >> 16) & 3) == 0, rm=rm, addr=timg['addr'], calls=1, order=order)
            order += 1


def write_sheet(out_path):
    entries = sorted(seen.values(), key=lambda e: (not e['xlu'], e['order']))
    cell, cols = 96, 10
    rows = (len(entries) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * cell, rows * (cell + 26)), (40, 40, 48))
    d = ImageDraw.Draw(sheet)
    checker = Image.new('RGB', (cell - 8, cell - 8))
    cd = ImageDraw.Draw(checker)
    for y in range(0, cell, 8):
        for x in range(0, cell, 8):
            cd.rectangle([x, y, x + 7, y + 7], fill=(90, 90, 90) if (x + y) // 8 % 2 else (60, 60, 60))
    os.makedirs(out_path + '.entries', exist_ok=True)
    for i, e in enumerate(entries):
        e['img'].save(f"{out_path}.entries/{i}.png")
        cx, cy = (i % cols) * cell, (i // cols) * (cell + 26)
        img = e['img']
        s = (cell - 8) / max(img.width, img.height)
        img = img.resize((max(1, int(img.width * s)), max(1, int(img.height * s))), Image.NEAREST)
        bg = checker.copy()
        bg.paste(img, (0, 0), img)
        sheet.paste(bg, (cx + 4, cy + 4))
        d.rectangle([cx + 2, cy + 2, cx + cell - 3, cy + cell - 3], outline=(255, 160, 40) if e['xlu'] else (80, 80, 90))
        d.text((cx + 4, cy + cell - 2), f"#{i} {e['addr']:06X}", fill=(230, 230, 230))
        d.text((cx + 4, cy + cell + 10), f"{e['img'].width}x{e['img'].height} x{e['calls']}", fill=(160, 160, 170))
    sheet.save(out_path)
    with open(out_path + '.txt', 'w') as f:
        for i, e in enumerate(entries):
            f.write(f"#{i} addr {e['addr']:06X} {e['img'].width}x{e['img'].height} rm {e['rm']:08X} "
                    f"{'see-through' if e['xlu'] else 'opaque'} calls {e['calls']}\n")
    print(len(entries), 'textures,', sum(e['xlu'] for e in entries), 'see-through')


def match(out_path, dump_dir, picks):
    want = {n: Image.open(f"{out_path}.entries/{n}.png").convert('RGBA') for n in picks}
    found = {n: [] for n in picks}
    for tj in glob.glob(os.path.join(dump_dir, '*.tile.json')):
        h = os.path.basename(tj).split('.')[0]
        j = json.load(open(tj))
        t = j['tile']
        tmem[:] = open(tj.replace('.tile.json', '.tmem'), 'rb').read()
        state['tlut_mode'] = 3 if j.get('tlut') == 'IA16' else 2
        img = decode(dict(fmt=t['fmt'], siz=t['siz'], line=t['line'], tmem=t['tmem'], pal=t['palette'],
                          w=j['width'], h=j['height']), rt64_tlut=True)
        if img is None:
            continue
        for n, w in want.items():
            other = img if img.size == w.size else (img.resize(w.size) if img.width * img.height >= 64 else None)
            if other is None:
                continue
            a, b = w.tobytes(), other.tobytes()
            diff = sum(abs(a[k] - b[k]) for k in range(len(a)) if k % 4 != 3) / (len(a) * 3 / 4)
            found[n].append((round(diff, 2), h))
    for n in picks:
        print(n, sorted(found[n])[:3])


if __name__ == '__main__':
    if len(sys.argv) >= 5 and sys.argv[1] == 'sheet':
        ram = open(sys.argv[3], 'rb').read()
        replay(sys.argv[2])
        write_sheet(sys.argv[4])
    elif len(sys.argv) >= 5 and sys.argv[1] == 'match':
        match(sys.argv[2], sys.argv[3], [int(n) for n in sys.argv[4:]])
    else:
        sys.exit(__doc__)
