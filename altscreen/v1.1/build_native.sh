#!/usr/bin/env bash
set -euo pipefail
D="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
: "${QNX_RUNTIME:?Set QNX_RUNTIME to the restored K2161 QNX toolchain runtime}"
source "$QNX_RUNTIME/activate.sh"
rm -rf "$D/generated";mkdir -p "$D/generated" "$D/output"
FW="${ALTSCREEN_TARGET:-k2161}";FW_DIR="$D/native/connectors/$FW/output"
source "$FW_DIR/build_sources.sh"
source "$D/native/core/build_sources.sh"
SRC=("${CORE_SOURCES[@]}" "${FW_SOURCES[@]}")
FLAGS=(-fPIC -mfloat-abi=softfp -O2 -std=gnu99 -include stddef.h -I"$D/native")
for inc in "${FW_INCLUDE_DIRS[@]}"; do FLAGS+=("-I$inc"); done
OBJ=()
for src in "${SRC[@]}"; do
 name="${src//\//__}";name="${name%.c}"
 obj="$D/generated/$name.o"
 qcc "${FLAGS[@]}" -c "$D/native/$src" -o "$obj"; OBJ+=("$obj")
done
qcc -shared "${FLAGS[@]}" "${OBJ[@]}" -lsocket -lc -Wl,--no-undefined -o "$D/output/libmib2high-carplay-rgi.so"
python3 "$D/check_native.py" "$D/output/libmib2high-carplay-rgi.so"
