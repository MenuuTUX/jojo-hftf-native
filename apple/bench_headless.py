#!/usr/bin/env python3
"""Headless CPU-throughput probe for a PSX_DEBUG_TOOLS=ON JoJoPS1 build.

Usage: bench_headless.py <path-to-JoJoPS1> [warmup_s(unused)] [window_s=10] [windows=3] [port=4370]

Starts `JoJoPS1 --headless --debug-port PORT --memcard-dir <tmp>`, waits
`warmup_s`, then reads the debug-server `frame` counter `windows`+1 times,
`window_s` apart, and prints frames/second per window (uncapped headless run,
so this is the CPU-side throughput of the recompiled game + runtime).
"""
import json, os, shutil, socket, subprocess, sys, tempfile, time

exe = os.path.abspath(sys.argv[1])
warm = 0
win = float(sys.argv[3]) if len(sys.argv) > 3 else 10
n = int(sys.argv[4]) if len(sys.argv) > 4 else 3
port = int(sys.argv[5]) if len(sys.argv) > 5 else 4370

tmp = tempfile.mkdtemp(prefix="jojo_bench_")
p = subprocess.Popen([exe, "--headless", "--debug-port", str(port), "--memcard-dir", tmp],
                     cwd=os.path.dirname(os.path.abspath(exe)),
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    def query():
        # Fresh connection per query: the debug server may drop idle clients.
        for _ in range(300):
            try:
                s = socket.create_connection(("127.0.0.1", port), timeout=5)
                s.sendall(b'{"id":1,"cmd":"frame"}\n')
                buf = b""
                while b"\n" not in buf:
                    chunk = s.recv(4096)
                    if not chunk:
                        break
                    buf += chunk
                s.close()
                if buf:
                    return json.loads(buf.split(b"\n")[0])["frame"]
            except OSError:
                pass
            if p.poll() is not None:
                sys.exit("process exited early rc=%s" % p.returncode)
            time.sleep(0.2)
        sys.exit("debug port never answered")
    frame = query
    # 1) Fixed guest-work metric: wall time to run frames A..B. Same game content
    #    for every build, so scene differences cancel out.
    A, B = 3000, 15000
    ta = tb = None
    while tb is None:
        fr = query(); t = time.time()
        if ta is None and fr >= A: ta, fa = t, fr
        if fr >= B: tb, fb = t, fr
        time.sleep(0.02)
    print("fixed range frames %d..%d: %.1f fps" % (fa, fb, (fb - fa) / (tb - ta)))
    # 2) Wall-clock windows: read `frame` twice, `win` seconds apart.
    t0 = time.time(); f0 = frame()
    res = []
    for _ in range(n):
        time.sleep(win)
        t1 = time.time(); f1 = frame()
        res.append((f1 - f0) / (t1 - t0))
        t0, f0 = t1, f1
    print("fps per %gs window:" % win, " ".join("%.1f" % r for r in res),
          "| median %.1f" % sorted(res)[len(res) // 2], "| end frame", f0)
finally:
    p.kill(); p.wait(); shutil.rmtree(tmp, ignore_errors=True)
