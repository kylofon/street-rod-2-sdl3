#!/bin/sh
# Import + auto-analyse F117 executables into _ghidra/F117.gpr (headless, phase 1).
# Imports: START.EXE VGAME.EXE END.EXE MGRAPHIC.EXE (data segments auto-detected or pass: -ds ADDR).
# Run from anywhere with Git Bash; logs in work/ghidra_<exe>_import.log. Then run f117_decompile.sh.
cd "$(dirname "$0")/../.."
export JAVA_HOME="$(cygpath -w "$PWD"/_tools/jdk-21.0.12.1+1)"
H=_tools/ghidra_12.1.3_PUBLIC/support/analyzeHeadless.bat

# Helper: import a single EXE
import_exe() {
  local exe=$1
  local ds_arg=$2
  if [ -f "Game/$exe" ]; then
    echo "Importing $exe $ds_arg..."
    cmd //c "$(cygpath -w $H)" _ghidra F117 -import "Game/$exe" -overwrite -scriptPath tools/ghidra -preScript SetDS.java $ds_arg -postScript FixNearFlows.java $ds_arg > work/ghidra_${exe%.EXE}_import.log 2>&1
  fi
}

import_exe START.EXE "5348"
import_exe VGAME.EXE "4D55"
import_exe END.EXE "51E0"
import_exe MGRAPHIC.EXE "4840"

echo "F117 import complete"
