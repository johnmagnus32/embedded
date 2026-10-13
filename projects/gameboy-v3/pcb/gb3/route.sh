#!/bin/bash
# route.sh — autoroute gb3 with FreeRouting: DSN out, N independent jobs in parallel, each scored by
# KiCad itself (import + plane fanout + DRC), best one kept.
#
#   ./route.sh [jobs] [timeout]          defaults: 3 jobs, 00:45:00 each
#
# Works under route/ and never touches gb3.kicad_pcb. Apply the winner with
#   for s in "import_route.py route/best.ses" fanout_planes.py finish_planes.py post_route_fixes.py; do
#     <kicad python> $s; done
#
# Each job shuffles its net order differently, so a few in parallel beat one long run: past ~45 min a
# single job only trades one unrouted connection for another. Single-threaded on purpose: -mt makes
# 2.1.0's output even less trustworthy. The pieces and the gotchas behind them:
#   export_dsn.py     DSN out, plus a DSN-only keepout band for the 0.5 mm edge rule
#   FrRun.java        runs FreeRouting's own CLI but writes the board it actually finished with
#   import_route.py   SES in, via drills fixed, dead ends cleaned (also: the full list of 2.1.0 bugs)
#   fanout_planes.py  stitching vias for GND / +3V3 pads the router left off-plane, EP via arrays
#   finish_planes.py  0.3 mm pour clearance + unused inner via pads removed (closes antipad slots)
#   post_route_fixes.py  U12 control-ground stub, U18 mask margin, U4 SMD attribute
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
KPY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
CLI=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
# 2.1.0 is the newest release that runs on Java 23 (2.2+ are built for Java 25)
JAR=${FR_JAR:-$HOME/.cache/freerouting/freerouting-2.1.0.jar}
JOBS=${1:-3}; TIMEOUT=${2:-00:45:00}; W=$HERE/route
quiet() { grep -vE 'assert|Debug: Adding|memory leak' || true; }

if [ ! -f "$JAR" ]; then
  mkdir -p "$(dirname "$JAR")"
  curl -fsSL -o "$JAR" https://github.com/freerouting/freerouting/releases/download/v2.1.0/freerouting-2.1.0.jar
fi
rm -rf "$W"; mkdir -p "$W/classes"
javac -d "$W/classes" -cp "$JAR" "$HERE/FrRun.java"
$KPY "$HERE/export_dsn.py" "$HERE/gb3.kicad_pcb" "$W/gb3.dsn" 2>&1 | quiet
(cd "$HERE" && $CLI pcb drc --format json -o "$W/baseline.json" --severity-error --severity-warning gb3.kicad_pcb >/dev/null)

echo "route.sh: $JOBS jobs x $TIMEOUT, started $(date +%H:%M)"
for j in $(seq 1 "$JOBS"); do
  mkdir -p "$W/j$j/board"
  sed "1s#.*#(pcb gb3_job$j#" "$W/gb3.dsn" > "$W/j$j/gb3.dsn"    # distinct CRC per job, same design
  (cd "$W/j$j" && FREEROUTING__USAGE_AND_DIAGNOSTIC_DATA__DISABLE_ANALYTICS=true \
     FREEROUTING__ROUTER__JOB_TIMEOUT="$TIMEOUT" FREEROUTING__GUI__ENABLED=false \
     caffeinate -i java -Xmx3500m -Djava.awt.headless=true -cp "$JAR:$W/classes" \
       FrRun "$W/j$j/final.ses" -de "$W/j$j/gb3.dsn" -do "$W/j$j/fr.ses" > "$W/j$j/run.log" 2>&1) &
done
wait

best=; bu=999999
for j in $(seq 1 "$JOBS"); do
  d=$W/j$j/board; cp "$HERE/gb3.kicad_pro" "$d/"
  [ -f "$W/j$j/final.ses" ] || { echo "  j$j: no final.ses"; continue; }
  $KPY "$HERE/import_route.py" "$W/j$j/final.ses" "$d/gb3.kicad_pcb" 2>&1 | quiet > "$W/j$j/import.log"
  $KPY "$HERE/fanout_planes.py" "$d/gb3.kicad_pcb" 2>&1 | quiet > "$W/j$j/fanout.log"
  $KPY "$HERE/finish_planes.py" "$d/gb3.kicad_pcb" 2>&1 | quiet >> "$W/j$j/fanout.log"
  (cd "$d" && $CLI pcb drc --format json -o drc.json --severity-error --severity-warning gb3.kicad_pcb >/dev/null)
  read u bad <<< "$(/usr/bin/python3 - "$W/baseline.json" "$d/drc.json" <<'EOF'
import json, sys, collections
BAD = {"clearance", "shorting_items", "tracks_crossing", "hole_clearance", "hole_to_hole",
       "copper_edge_clearance", "items_not_allowed", "annular_width"}
base, new = (json.load(open(f)) for f in sys.argv[1:3])
count = lambda d: collections.Counter(v["type"] for v in d["violations"] if v["type"] in BAD)
print(len(new["unconnected_items"]), sum((count(new) - count(base)).values()))
EOF
)"
  echo "  j$j: $u unconnected after fanout, $bad new clearance-type errors"
  if [ "$bad" -eq 0 ] && [ "$u" -lt "$bu" ]; then bu=$u; best=$j; fi
done
[ -n "$best" ] || { echo "route.sh: no clean job"; exit 1; }
cp "$W/j$best/final.ses" "$W/best.ses"
echo "route.sh: best is j$best ($bu unconnected after fanout) -> route/best.ses"
