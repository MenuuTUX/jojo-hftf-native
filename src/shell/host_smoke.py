#!/usr/bin/env python3
"""Hosted-shell smoke test (README.md, "Hosted runtime"). Run after shell/build-host.sh.

The app runs the PS1 runtime in-process. This driver:
  1. makes a new run directory runs/phase1-shell-NNN (saves, log, PNG, result.json);
  2. starts shell/build/host/JojoShell.app/Contents/MacOS/JojoShell with the debug server on port 4430 (the only port this
     driver uses), JOJO_SHELL_AUTOQUIT_FRAMES and JOJO_SHELL_DUMP_FRAME set;
  3. installs the boot-to-fight input route (tools-phase1/scripts/boot_fight_a.json) through routes.install while the game
     is still early, the same way the reference comparisons do;
  4. at frame >= DUMP reads the fight block at 0x800CD268 (64 bytes) through the debug server;
  5. waits for the app to exit by itself at AUTOQUIT frames and checks the exit status is 0;
  6. checks the PNG was written and no JojoShell process is left behind.
Only the process this script starts is ever killed (its own process group, after a failed run).

Usage: python3 -I host_smoke.py [--pace 0|1] [--dump 9400] [--autoquit 9410]
"""
import argparse, json, os, signal, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
PORT_ROOT = os.path.abspath(os.path.join(HERE, '..'))
TOOLS = os.path.join(PORT_ROOT, 'tools-phase1')
sys.path.insert(0, TOOLS)
import runlib, routes  # noqa: E402

APP_BIN = os.path.join(HERE, 'build', 'host', 'JojoShell.app', 'Contents', 'MacOS', 'JojoShell')
PATTERN = 'JojoShell.app/Contents/MacOS/JojoShell'
EXTRACTED = os.path.join(PORT_ROOT, 'extracted')
ROUTE = os.path.join(TOOLS, 'scripts', 'boot_fight_a.json')
DEBUG_PORT = 4430
FIGHT_BLOCK = 0x800CD268
PIECES = 'pad,cd,audio,gpu,kernel,gte,math,saves'


class Game:
    """The adapter routes.install expects: call(), frame()."""
    def __init__(self, dbg):
        self.dbg = dbg
    def call(self, cmd, **kw):
        return self.dbg.call(cmd, **kw)
    def frame(self):
        return self.dbg.frame()


def app_running():
    import subprocess as sp
    r = sp.run(['pgrep', '-fl', PATTERN], capture_output=True, text=True)
    return [l for l in r.stdout.splitlines() if 'pgrep' not in l]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--pace', type=int, default=1, help='1 = 60 vblanks per second (default), 0 = unpaced')
    ap.add_argument('--dump', type=int, default=9400)
    ap.add_argument('--autoquit', type=int, default=9410)
    args = ap.parse_args()

    if not os.path.exists(APP_BIN):
        print('FAIL: %s not found. Run shell/build-host.sh first.' % APP_BIN)
        return 1
    if app_running():
        print('FAIL: a JojoShell process is already running:', app_running())
        return 1

    rd = runlib.new_run_dir('phase1-shell')
    saves = os.path.join(rd, 'saves')
    os.makedirs(saves, exist_ok=True)
    png = os.path.join(rd, 'frame-%d.png' % args.dump)
    log_path = os.path.join(rd, 'app.log')
    result = {'run_dir': rd, 'pace': args.pace, 'dump_frame': args.dump, 'autoquit_frames': args.autoquit}

    env = {k: v for k, v in os.environ.items() if not k.startswith('JOJO_')}
    env.update({
        'JOJO_PHASE1': PIECES,
        'JOJO_FILE_LAYER': '1',
        'JOJO_SHELL_DEBUG_PORT': str(DEBUG_PORT),
        'JOJO_SHELL_MEMCARD_DIR': saves,
        'JOJO_SHELL_AUTOQUIT_FRAMES': str(args.autoquit),
        'JOJO_SHELL_DUMP_FRAME': str(args.dump),
        'JOJO_SHELL_DUMP_PATH': png,
        'JOJO_SHELL_PACE': str(args.pace),
    })
    logf = open(log_path, 'wb')
    proc = subprocess.Popen([APP_BIN, '-gameFolderPath', EXTRACTED], cwd=rd, env=env,
                            stdout=logf, stderr=subprocess.STDOUT, start_new_session=True)
    print('launched pid %d, run dir %s' % (proc.pid, rd))
    ok = False
    try:
        dbg = runlib.Dbg(DEBUG_PORT)
        t0 = time.time()
        while True:
            if proc.poll() is not None:
                raise RuntimeError('app exited early with status %s (see %s)' % (proc.returncode, log_path))
            try:
                dbg.call('ping')
                break
            except Exception:
                if time.time() - t0 > 120:
                    raise RuntimeError('debug server did not come up on port %d' % DEBUG_PORT)
                time.sleep(0.3)
        game = Game(dbg)
        print('debug server up at frame %s' % game.frame())
        install = routes.install(game, json.load(open(ROUTE)), first_frame=400)
        result['route_install'] = install
        print('route installed')

        fight = None
        last_print = 0
        while True:
            if proc.poll() is not None:
                break
            try:
                frame = game.frame()
                if frame >= args.dump and fight is None:
                    raw = dbg.ram(FIGHT_BLOCK, 64)
                    fight = {'frame': frame, 'nonzero_bytes': sum(1 for b in raw if b), 'hex': raw.hex()}
                    print('fight block at frame %d: %d nonzero bytes of 64' % (frame, fight['nonzero_bytes']))
            except (OSError, EOFError, ValueError, KeyError) as exc:
                # The app may be exiting (autoquit) between two calls; only a live app is an error.
                if proc.poll() is not None:
                    break
                print('debug call failed while the app runs: %s' % exc)
                time.sleep(0.5)
                continue
            if frame - last_print >= 600:
                print('frame %d' % frame)
                last_print = frame
            if fight is not None and frame >= args.autoquit:
                break
            time.sleep(0.2)
        result['fight_block'] = fight

        try:
            status = proc.wait(timeout=600)
        except subprocess.TimeoutExpired:
            status = None
        result['exit_status'] = status
        print('app exit status: %s' % status)
        result['png_exists'] = os.path.exists(png)
        result['png_bytes'] = os.path.getsize(png) if result['png_exists'] else 0
        stats = [l for l in open(log_path, 'r', errors='replace') if 'jojo-host:' in l]
        result['host_lines'] = [l.strip() for l in stats[-3:]]
        ok = (status == 0 and fight is not None and fight['nonzero_bytes'] > 0 and result['png_exists'])
    except Exception as exc:
        print('FAIL:', exc)
        result['error'] = str(exc)
    finally:
        if proc.poll() is None:
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except OSError:
                pass
            proc.wait()
            print('killed our own app process group (test failed or timed out)')
        logf.close()
        left = app_running()
        result['processes_left'] = left
        with open(os.path.join(rd, 'result.json'), 'w') as f:
            json.dump(result, f, indent=2)
        if left:
            ok = False
            print('FAIL: process left behind:', left)

    print('PASS' if ok else 'FAIL', rd)
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
