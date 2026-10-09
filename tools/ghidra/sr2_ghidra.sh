#!/bin/bash
# Import + auto-analyse work/SR2_unp.exe into _ghidra/SR2.gpr, apply port/symbols.csv and decompile
# every indexed function to port/decomp/sr2.c (+ sr2_ds.c via postprocess.py). Headless; run it in the
# background from Git Bash. Logs: work/ghidra_sr2_import.log, work/ghidra_sr2_decomp.log.
# Ghidra loads the image at segment 1000 (symbols are written with +1000; DG stays the file value).
cd "$(dirname "$0")/../.."
export JAVA_HOME="$(cygpath -w "$PWD"/_tools/jdk-21.0.12.1+1)"
H="$(cygpath -w _tools/ghidra_12.1.3_PUBLIC/support/analyzeHeadless.bat)"
DG=3E08   # file DGROUP; the scripts add the load segment themselves
mkdir -p _ghidra port/decomp work
python - <<'PY'
import csv
with open('work/sr2_symbols_ghidra.txt', 'w') as o:
    for r in csv.DictReader(open('port/symbols.csv')):
        if r['kind'] == 'func':
            s, f = r['address'].split(':')
            o.write('func %04X:%s %s\n' % (int(s, 16) + 0x1000, f.upper(), r['name']))
PY
cp port/sr2_starts.txt work/sr2_starts.txt
if [ "$1" != "decompile" ]; then
  cmd //c "$H" _ghidra SR2 -import work/SR2_unp.exe -overwrite -scriptPath tools/ghidra \
    -preScript SetDS.java $DG -postScript FixNearFlows.java $DG > work/ghidra_sr2_import.log 2>&1
fi
cmd //c "$H" _ghidra SR2 -process SR2_unp.exe -noanalysis -scriptPath tools/ghidra \
  -postScript ApplySymbols.java work/sr2_symbols_ghidra.txt $DG \
  -postScript DecompileAll.java work/sr2_starts.txt port/decomp/sr2.c $DG 120 only > work/ghidra_sr2_decomp.log 2>&1
python tools/ghidra/postprocess.py port/decomp/sr2.c $DG
grep -h 'decompiled .* functions' work/ghidra_sr2_decomp.log | sed 's/.*> //'
