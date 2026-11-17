#!/usr/bin/env bash
set -euo pipefail
R="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
: "${JAVA_HOME:?Set JAVA_HOME to Java 8}"
: "${K2161_BASE_JAR:?Set K2161_BASE_JAR to the stock K2161 compile-only reference JAR}"
C="$R/java/build/classes"; rm -rf "$C"; mkdir -p "$C" "$R/output"
mapfile -t SOURCES < <(cat "$R/java/core/source.manifest" "$R/java/shared/source.manifest" "$R/java/connectors/k2161/source.manifest")
for ((i=0;i<${#SOURCES[@]};i++)); do
 [[ "${SOURCES[$i]}" != *..* && -f "$R/java/${SOURCES[$i]}" ]] || exit 2
 SOURCES[$i]="$R/java/${SOURCES[$i]}"
done
"$JAVA_HOME/bin/javac" -source 1.4 -target 1.4 -encoding UTF-8 -cp "$K2161_BASE_JAR" -d "$C" "${SOURCES[@]}"
python3 "$R/java/build_jar_deterministic.py" "$C" "$R/output/mib2high-carplay-rgi.jar"
python3 - "$R/output/mib2high-carplay-rgi.jar" <<'PYIN'
from zipfile import ZipFile
import sys, struct
with ZipFile(sys.argv[1]) as z:
 classes=[x for x in z.namelist() if x.endswith('.class')]
 assert len(classes)==70,len(classes)
 assert all(z.read(x)[:4]==b'\xca\xfe\xba\xbe' and struct.unpack('>H',z.read(x)[6:8])[0]==48 for x in classes)
 for n in ('BAPBridge','RouteGuidance','AltCoreNavigationEngine'):
  assert 'com/luka/carplay/routeguidance/'+n+'.class' in classes
print('ALTSCREEN_JAVA_BUILD=PASS')
PYIN
