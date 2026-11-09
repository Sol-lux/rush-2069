"""A hand-played Battle session: the user plays blue on port 1, red (port 2) copies the menus then parks. Port 1's
input is recorded, RDRAM dumped every second for 10 minutes, a screenshot every second. Ends when the window closes."""
import os, subprocess, sys, time
ROOT = os.path.dirname(os.path.abspath(__file__)); d = os.path.join(ROOT, "human", time.strftime("%H%M%S")); os.makedirs(d)
EXE = r"E:\Rush2069\build\Rush2049Recompiled.exe"
p = subprocess.Popen([EXE, "--boot", "--players", "2", "--park", "--record", os.path.join(d, "input.txt"),
                      "--dump-ram", os.path.join(d, "d"), ",".join(str(t / 2) for t in range(10, 1200))] + sys.argv[1:], stdout=open(os.path.join(d, "log.txt"), "w"), stderr=subprocess.STDOUT, cwd="E:/Rush2069")
start = time.time()
while p.poll() is None:
    time.sleep(1)
    subprocess.run([sys.executable, "-I", os.path.join(ROOT, "shot.py"), str(p.pid), os.path.join(d, f"s{int(time.time() - start):03d}.png")], capture_output=True)
print("session in", d, "exit", p.returncode)
