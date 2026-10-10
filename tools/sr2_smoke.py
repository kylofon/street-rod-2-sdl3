#!/usr/bin/env python3
"""Headless smoke run of the SR2 port through every screen (S5.5).

    python tools/sr2_smoke.py [path ...] [--list] [--jobs N] [--exe sr2port/build/sr2port.exe]

Every path is one sr2port run with the SDL dummy drivers on its own copy of Game/ (work/s55game/<path>; Game/ itself is never
written), SR_TRACE=1 on stderr, snapshots in work/s55snap/<path>/. A path is "ok" when the exit code is 0, the log has no
"fatal" / assert / crash text, every marker of the path is in the log and (paths that finish by themselves) it ended before its
time cap. One line per path: name, ok/FAIL, seconds, snapshot dir (+ the reason on failure). Exit code 0 only if all ran ok.
Keys / clicks are SR_KEYS (host.h; seconds after start-up), the start-up sound menu answer is "1:03" (2 = normal).
Dev aids used: SR_DEBUG_RACE (game/race_debug.c), SR_DEBUG_END (same file), `demo` (autopilot of ui_wait.c), SR_SEED.
"""
import argparse, concurrent.futures as cf, os, re, shutil, subprocess, sys, threading, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SND = "1:03"                       # sound menu: 2 (Normal)
DEMO = ["demo", "v"]
PLAIN = ["v"]

# name: dict(env, args, keys, cap seconds, markers (regexes that must be in the log), must_finish)
PATHS = {
    # title -> protection question -> new game (licence name) -> newspaper -> classifieds
    "title_new": dict(args=PLAIN, env={"SR_SEED": "3"}, cap=48,
                      keys=[SND, "25:1e", "26:1c", "30:30", "31:18", "32:30", "33:1c", "36:c154.51"],
                      markers=[r"ui_push screen 31\b", r"ui_push screen 2\b", r"ui_push screen 3\b", r"ui_push screen 4\b"]),
    # the demo player: the bag of 17 garage codes (all sub-screens), newspaper, classifieds, used cars, gas, Bob's, races
    "demo_tour": dict(args=DEMO, env={"SR_SEED": "11"}, keys=[SND], cap=420,
                      markers=[r"ui_push screen 3\b", r"ui_push screen 4\b", r"race_run mode"]),
    # one race of each kind, then the drive home (demo autopilot drives), race_results inside
    "race_cruise": dict(args=DEMO, env={"SR_DEBUG_RACE": "cruise"}, keys=[SND], cap=240, finish="race_debug: done"),
    "race_bob": dict(args=DEMO, env={"SR_DEBUG_RACE": "bob"}, keys=[SND], cap=420,
                     finish=r"race_run mode (?:1|16|8) racing 1[\s\S]*race_run end", markers=[r"ui_push screen 2\d\b"]),
    "jail": dict(args=DEMO, env={"SR_DEBUG_RACE": "jail"}, keys=[SND], cap=120, finish="race_debug: done",
                 markers=[r"ui_push screen 21\b"]),
    "race_drag": dict(args=DEMO, env={"SR_DEBUG_RACE": "drag"}, keys=[SND], cap=240, finish="race_debug: done",
                      markers=[r"race_run mode 1\b", r"race_results -> "]),
    "race_mulholland": dict(args=DEMO, env={"SR_DEBUG_RACE": "mulholland"}, keys=[SND], cap=300, finish="race_debug: done",
                            markers=[r"race_run mode 16\b", r"race_results -> "]),
    "race_aqueduct": dict(args=DEMO, env={"SR_DEBUG_RACE": "aqueduct"}, keys=[SND], cap=300, finish="race_debug: done",
                          markers=[r"race_run mode 8\b", r"race_results -> "]),
    "race_aqueduct_king": dict(args=DEMO, env={"SR_DEBUG_RACE": "aqueduct", "SR_DEBUG_OPP": "21"}, keys=[SND], cap=300,
                               finish="race_debug: done", markers=[r"race_run mode 8\b"]),
    "race_grudge": dict(args=DEMO, env={"SR_DEBUG_RACE": "grudge"}, keys=[SND], cap=420, finish="race_debug: done"),
    # every garage hot spot by its hot key (Esc leaves each sub-screen): hood, bumpers, roof, under the car (click), car info,
    # paint, tyres, transmission, sell parts, cars, calendar, newspaper -> classifieds
    "garage_tour": dict(args=PLAIN, env={"SR_DEBUG_END": "garage"}, cap=112,
                        keys=[SND, "6:23", "10:01", "13:30", "17:01", "20:21", "24:01", "27:13", "31:01", "34:c107.177", "38:01",
                              "41:17", "45:01", "48:19", "52:01", "55:14", "59:01", "62:1e", "66:01", "69:26", "73:01",
                              "76:2e", "80:01", "83:2d", "87:01", "90:31", "94:c154.51", "98:01", "101:01", "104:10"],
                        markers=[r"garage code %d\b" % c for c in (10, 11, 12, 13, 14, 16, 7, 8, 9, 5, 6, 3, 1)] +
                                [r"ui_push screen 4\b", r"ui_push screen 34\b"]),
    # save a game (Q, Save Game, name "abc") and load it again (Q, Old Game, Enter); ends in the garage
    "save_load": dict(args=PLAIN, env={"SR_DEBUG_END": "garage"}, cap=40,
                      keys=[SND, "6:10", "9:c125.102", "13:1e", "14:30", "15:2e", "16:1c", "20:10", "23:c125.120", "28:1c"],
                      markers=[r"ui_push screen 28\b", r"ui_push screen 26\b"], files=["HOTROD0.SAV"]),
    # the endings: the SR2_end player after beating the King, hall of fame (name edit Enter), game-over menu (Quit)
    "end_king": dict(args=PLAIN, env={"SR_DEBUG_END": "king"}, cap=70, finish=r"ui_menu\(32\) -> 4",
                     keys=[SND, "31:1c", "36:c160.116"], files=["HALL_DAT"], markers=[r"garage code 666"]),
    "end_summer": dict(args=PLAIN, env={"SR_DEBUG_END": "summer"}, cap=40, finish=r"ui_menu\(32\) -> 4",
                       keys=[SND, "8:1c", "14:1c", "20:c160.116"], markers=[r"garage code 666"]),
    "end_broke": dict(args=PLAIN, env={"SR_DEBUG_END": "broke"}, cap=40, finish=r"ui_menu\(32\) -> 4",
                      keys=[SND, "8:1c", "14:1c", "20:c160.116"], markers=[r"garage code 666"]),
}
BAD = re.compile(r"fatal|assert|segmentation|access violation|abort", re.I)


def run_path(name, spec, exe):
    snap = os.path.join(ROOT, "work", "s55snap", name)
    game = os.path.join(ROOT, "work", "s55game", name)
    for d in (snap, game):
        shutil.rmtree(d, ignore_errors=True)
    os.makedirs(snap)
    shutil.copytree(os.path.join(ROOT, "Game"), game, ignore=shutil.ignore_patterns("*.exo"))   # never write into Game/
    env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy", SR_TRACE="1", SR_SEED="1",
               SR_SNAPSHOT_DIR=snap, SR_SNAPSHOT_MS="4000", SR_KEYS=",".join(spec["keys"]))
    env.update(spec.get("env", {}))
    cap = spec["cap"]
    cmd = [exe, "--game-dir", game, "--run-ms", str(cap * 1000)] + spec["args"]
    t0 = time.time()
    fin = re.compile(spec["finish"]) if "finish" in spec else None
    p = subprocess.Popen(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
    lines, hit = [], None

    def pump():
        for ln in p.stdout:
            lines.append(ln)
    th = threading.Thread(target=pump, daemon=True)
    th.start()
    stopped = False
    while p.poll() is None and time.time() - t0 < cap + 30:
        time.sleep(0.5)
        if fin and hit is None and fin.search("".join(lines)):
            hit = time.time()
        if hit and time.time() - hit > 2:          # finished: platform_exit in demo mode waits on a "Quit demo" box
            p.kill()
            stopped = True
            break
    if p.poll() is None:
        p.kill()
        stopped = True
    p.wait()
    th.join(5)
    rc, log = (0 if stopped and hit else p.returncode), "".join(lines)
    secs = time.time() - t0
    with open(os.path.join(snap, "_log.txt"), "w", encoding="utf-8") as f:
        f.write(log)
    why = []
    if rc != 0:
        why.append("exit code %d" % rc)
    m = BAD.search(log)
    if m:
        why.append("log: " + log[max(0, m.start() - 20):m.end() + 60].replace("\n", " | "))
    for mk in spec.get("markers", []) + ([spec["finish"]] if "finish" in spec else []):
        if not re.search(mk, log):
            why.append("missing /%s/" % mk)

    for fn in spec.get("files", []):
        if not any(x.upper() == fn for x in os.listdir(game)):
            why.append("file %s not written" % fn)
    shots = len([x for x in os.listdir(snap) if x.endswith(".bmp")])
    if shots == 0:
        why.append("no snapshots")
    screens = sorted({int(x) for x in re.findall(r"ui_push screen (\d+)", log)})
    return name, not why, secs, os.path.relpath(snap, ROOT), why, shots, screens


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("paths", nargs="*")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--exe", default=os.path.join(ROOT, "sr2port", "build", "sr2port.exe"))
    a = ap.parse_args()
    if a.list:
        print("\n".join(PATHS))
        return 0
    names = a.paths or list(PATHS)
    bad = [n for n in names if n not in PATHS]
    if bad:
        print("unknown path(s):", ", ".join(bad), "(--list)")
        return 2
    os.environ["PATH"] = r"C:\msys64\mingw64\bin" + os.pathsep + os.environ.get("PATH", "")
    ok = True
    with cf.ThreadPoolExecutor(a.jobs) as ex:
        futs = [ex.submit(run_path, n, PATHS[n], a.exe) for n in names]
        for f in cf.as_completed(futs):
            n, good, secs, snap, why, shots, screens = f.result()
            ok &= good
            print("%-20s %-4s %6.1fs  %s  (%d frames)%s" % (n, "ok" if good else "FAIL", secs, snap, shots,
                                                          "" if good else "  <- " + "; ".join(why)), flush=True)
            if n == "demo_tour":
                print("   demo_tour screens seen: %s" % " ".join(map(str, screens)))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
