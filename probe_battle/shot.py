# shot.py <pid> <out.png>: PrintWindow the visible window of a pid, half size, to PNG.
import ctypes, ctypes.wintypes as W, struct, sys, zlib
u = ctypes.windll.user32; g = ctypes.windll.gdi32; u.SetProcessDPIAware()
pid = int(sys.argv[1]); found = []
@ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
def cb(h, l):
    p = W.DWORD(); u.GetWindowThreadProcessId(h, ctypes.byref(p))
    if p.value == pid and u.IsWindowVisible(h): found.append(h)
    return True
u.EnumWindows(cb, 0)
if not found: sys.exit("no window")
h = found[0]; r = W.RECT(); u.GetClientRect(h, ctypes.byref(r)); w, hh = r.right, r.bottom
dc = u.GetDC(h); m = g.CreateCompatibleDC(dc); b = g.CreateCompatibleBitmap(dc, w, hh); g.SelectObject(m, b)
u.PrintWindow(h, m, 3)
class BI(ctypes.Structure):
    _fields_ = [("s", W.DWORD), ("w", W.LONG), ("h", W.LONG), ("p", W.WORD), ("bc", W.WORD), ("c", W.DWORD),
                ("si", W.DWORD), ("x", W.LONG), ("y", W.LONG), ("u", W.DWORD), ("i", W.DWORD)]
bi = BI(40, w, hh, 1, 24, 0, 0, 0, 0, 0, 0); stride = (w * 3 + 3) & ~3; buf = ctypes.create_string_buffer(stride * hh)
g.GetDIBits(m, b, 0, hh, buf, ctypes.byref(bi), 0)
raw = buf.raw; rows = []
for y in range(hh - 1, -1, -2):
    row = raw[y * stride:y * stride + w * 3]; px = bytearray()
    for x in range(0, w - 1, 2): px += bytes((row[3 * x + 2], row[3 * x + 1], row[3 * x]))
    rows.append(b"\0" + bytes(px))
def ch(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
open(sys.argv[2], "wb").write(bytes([137]) + b"PNG\r\n\x1a\n" +
    ch(b"IHDR", struct.pack(">IIBBBBB", len(rows[0]) // 3, len(rows), 8, 2, 0, 0, 0)) +
    ch(b"IDAT", zlib.compress(b"".join(rows))) + ch(b"IEND", b""))
