#!/bin/sh
# Apply work/<exe>_symbols_ghidra.txt (tools/ghidra_syms.py) and decompile indexed functions
# to port/decomp/<exe>.c + <exe>_ds.c (postprocess.py). Runs headless in background.
cd "$(dirname "$0")/../.."
export JAVA_HOME="$(cygpath -w "$PWD"/_tools/jdk-21.0.12.1+1)"
H="$(cygpath -w _tools/ghidra_12.1.3_PUBLIC/support/analyzeHeadless.bat)"

# Helper: decompile a single program
decompile() {
  local prog=$1      # VGAME.EXE
  local dgroup=$2    # data segment address
  local name=$3      # vgame
  local ovlseg=${4:-}  # optional overlay segment

  if [ ! -f "work/${name}_symbols_ghidra.txt" ]; then
    echo "Skipping $prog: no symbols file (work/${name}_symbols_ghidra.txt)"
    return
  fi

  if [ ! -f "work/${name}_starts.txt" ]; then
    echo "Skipping $prog: no function starts file (work/${name}_starts.txt)"
    return
  fi

  echo "Decompiling $prog..."
  cmd //c "$H" _ghidra F117 -process "$prog" -noanalysis -scriptPath tools/ghidra \
    -postScript ApplySymbols.java work/${name}_symbols_ghidra.txt "$dgroup" \
    -postScript DecompileAll.java work/${name}_starts.txt port/decomp/${name}.c "$dgroup" 120 only $ovlseg > work/ghidra_${name}_decomp.log 2>&1

  python tools/ghidra/postprocess.py port/decomp/${name}.c "$dgroup" $ovlseg
  echo "  → port/decomp/${name}.c"
}

mkdir -p port/decomp

decompile START.EXE 5348 start
decompile VGAME.EXE 4D55 vgame
decompile END.EXE 51E0 end
decompile MGRAPHIC.EXE 4840 mgraphic

echo "F117 decompile complete"
