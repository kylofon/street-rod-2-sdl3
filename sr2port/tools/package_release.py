#!/usr/bin/env python3
"""Packages the Windows release of Street Rod 2 (sr2port.exe + "Street Rod 2.exe" + the DLLs they need).

    python sr2port/tools/package_release.py [--build sr2port/build-release]

Writes  release/sr2port-v<version>-win64.zip, release/SHA256SUMS.txt  and the folder it zips,
release/sr2port-v<version>-win64/ (git-ignored). Adapted from the Street Rod (SR1) port's release
folder (../StreetRod/release). The game's data is never packaged: the zip is checked for it.

Needs MSYS2 mingw64 (objdump, the DLLs) and a Release build of sr2port with the launcher
(-DSR2_LAUNCHER=ON). Run it from any shell; it uses the paths below only.
"""
import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
PORT = os.path.dirname(HERE)                      # sr2port/
REPO = os.path.dirname(PORT)                      # repo root
RELEASE = os.path.join(REPO, "release")
MINGW_BIN = "C:/msys64/mingw64/bin"
SYSTEM_DIR = "C:/Windows/System32"
EXES = ["sr2port.exe", "Street Rod 2.exe"]
# Files in the zip besides the exes and DLLs: source of each, and name in the zip.
DOCS = [
    (os.path.join(PORT, "dist", "README.md"), "README.md"),
    (os.path.join(REPO, "LICENSE"), "LICENSE"),
]
LICENSE_DIR = os.path.join(PORT, "dist", "licenses")
NUKED_LICENSE = (os.path.join(PORT, "third_party", "nuked-opl3", "LICENSE"), "licenses/Nuked-OPL3-LICENSE.txt")
FORBIDDEN = re.compile(r"(^|/)(SR\.EXE|LIB[123]C?|SR2_DATA|SR2_SHP|SR2_END|MSR\.SND|.*\.MUS|HOTROD\d+\.SAV|hall_dat)$", re.I)


def read_version():
    text = open(os.path.join(PORT, "CMakeLists.txt"), encoding="utf-8").read()
    m = re.search(r"set\(SR2PORT_VERSION\s+([0-9.]+)\)", text)
    if not m:
        sys.exit("package: SR2PORT_VERSION not found in CMakeLists.txt")
    return m.group(1)


def imports_of(path):
    """DLL names a PE file imports (objdump -p, 'DLL Name:' lines)."""
    out = subprocess.run([MINGW_BIN + "/objdump.exe", "-p", path], capture_output=True, text=True, check=True).stdout
    return [m.group(1) for m in re.finditer(r"DLL Name:\s*(\S+)", out)]


def find_dll(name, build_dir):
    """Where a DLL comes from: the build dir (copied by CMake) or MSYS2 mingw64; None for Windows' own DLLs."""
    for d in (build_dir, MINGW_BIN):
        p = os.path.join(d, name)
        if os.path.isfile(p):
            return p
    if os.path.isfile(os.path.join(SYSTEM_DIR, name)) or name.lower().startswith(("api-ms-", "ext-ms-")):
        return None
    sys.exit("package: %s is imported but not found in %s, %s or %s" % (name, build_dir, MINGW_BIN, SYSTEM_DIR))


def closure(build_dir):
    """Every non-system DLL the two exes need, transitively, as {dll name: source path}."""
    todo = []
    for exe in EXES:
        todo += imports_of(os.path.join(build_dir, exe))
    need = {}
    while todo:
        name = todo.pop()
        key = name.lower()
        if key in need:
            continue
        src = find_dll(name, build_dir)
        if src is None:
            continue
        need[key] = (name, src)
        todo += imports_of(src)
    return sorted(need.values(), key=lambda t: t[0].lower())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", default=os.path.join(PORT, "build-release"))
    args = ap.parse_args()
    build = os.path.abspath(args.build)
    version = read_version()
    name = "sr2port-v%s-win64" % version
    stage = os.path.join(RELEASE, name)
    zip_path = os.path.join(RELEASE, name + ".zip")

    for exe in EXES:
        if not os.path.isfile(os.path.join(build, exe)):
            sys.exit("package: %s missing in %s (build the Release configuration with the launcher)" % (exe, build))

    # Fresh staging folder
    if os.path.isdir(stage):
        shutil.rmtree(stage)
    os.makedirs(os.path.join(stage, "licenses"))

    files = []  # (source, name in the package)
    for exe in EXES:
        files.append((os.path.join(build, exe), exe))
    dlls = closure(build)
    for _, src in dlls:
        files.append((src, os.path.basename(src)))
    for src, dst in DOCS:
        files.append((src, dst))
    for src in sorted(os.listdir(LICENSE_DIR)):
        files.append((os.path.join(LICENSE_DIR, src), "licenses/" + src))
    files.append(NUKED_LICENSE)

    # Every DLL must have a licence entry, and nothing that is game data may be included
    third = open(os.path.join(LICENSE_DIR, "THIRD-PARTY.txt"), encoding="utf-8").read()
    for dll, _ in dlls:
        if dll not in third:
            sys.exit("package: %s has no entry in licenses/THIRD-PARTY.txt" % dll)
    for _, dst in files:
        if FORBIDDEN.search(dst):
            sys.exit("package: game data would be packaged: " + dst)

    for src, dst in files:
        target = os.path.join(stage, dst.replace("/", os.sep))
        os.makedirs(os.path.dirname(target), exist_ok=True)
        shutil.copyfile(src, target)

    if os.path.isfile(zip_path):
        os.remove(zip_path)
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for _, dst in sorted(files, key=lambda t: t[1].lower()):
            z.write(os.path.join(stage, dst.replace("/", os.sep)), dst)

    digest = hashlib.sha256(open(zip_path, "rb").read()).hexdigest()
    with open(os.path.join(RELEASE, "SHA256SUMS.txt"), "w", encoding="ascii", newline="\n") as f:
        f.write("%s *%s\n" % (digest, os.path.basename(zip_path)))

    with zipfile.ZipFile(zip_path) as z:
        names = z.namelist()
    print("zip:      %s" % zip_path)
    print("size:     %.2f MB (%d bytes)" % (os.path.getsize(zip_path) / 1e6, os.path.getsize(zip_path)))
    print("sha256:   %s" % digest)
    print("entries:  %d (%d DLLs: %s)" % (len(names), len(dlls), ", ".join(d for d, _ in dlls)))
    for n in names:
        print("  " + n)


if __name__ == "__main__":
    main()
