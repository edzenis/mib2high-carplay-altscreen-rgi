#!/usr/bin/env python3
"""Create a deterministic, full editable-source ZIP from the tracked public AltScreen tree.

This packages all files under altscreen/, not private historical build intermediates.
The compiled car-tested installer is deliberately NOT modified.
"""
from pathlib import Path
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED
import hashlib
import json
import sys

repo = Path(__file__).resolve().parents[1]
src = repo / "altscreen"
target = Path(sys.argv[1]) if len(sys.argv) > 1 else repo / "AltScreen-v1.0-source.zip"
assert src.is_dir()
paths = sorted(
    p for p in src.rglob("*")
    if p.is_file() and "__pycache__" not in p.parts and not p.name.endswith((".pyc", ".pyo"))
    and not any(part in ("generated", "output", ".git") for part in p.relative_to(src).parts)
    and not (p.relative_to(src).parts[0] == "renderer" and "build" in p.relative_to(src).parts[1:-1])
)
expected = {
    "deployment/bundles.awk", "deployment/smartphone.awk", "deployment/trace.awk",
    "renderer/upstream/vc_render.c", "renderer/PROVENANCE.md",
    "java/build_java_release.sh", "java/verify_release_classes.py", "BUILDING.md",
}
present = {p.relative_to(src).as_posix() for p in paths}
assert expected <= present, sorted(expected - present)
digest = lambda b: hashlib.sha256(b).hexdigest()
manifest = {p.relative_to(src).as_posix(): digest(p.read_bytes()) for p in paths}
target.parent.mkdir(parents=True, exist_ok=True)
with ZipFile(target, "w", compression=ZIP_DEFLATED, compresslevel=9) as z:
    for p in paths:
        rel = "altscreen-v1.0-source/" + p.relative_to(src).as_posix()
        info = ZipInfo(rel, date_time=(2026,10,12,0,0,0))
        info.compress_type = ZIP_DEFLATED
        info.external_attr = (0o100644 << 16)
        z.writestr(info, p.read_bytes(), compress_type=ZIP_DEFLATED, compresslevel=9)
    info = ZipInfo("altscreen-v1.0-source/PUBLIC_SOURCE_MANIFEST.json", date_time=(2026,10,12,0,0,0))
    info.compress_type = ZIP_DEFLATED
    info.external_attr = (0o100644 << 16)
    z.writestr(info, (json.dumps({"edition":"AltScreen v1.0 — source publication update","files_sha256":manifest},sort_keys=True,indent=2)+"\n").encode(), compress_type=ZIP_DEFLATED, compresslevel=9)
with ZipFile(target) as z:
    assert z.testzip() is None
    check = json.loads(z.read("altscreen-v1.0-source/PUBLIC_SOURCE_MANIFEST.json"))["files_sha256"]
    for rel, sha in check.items():
        assert digest(z.read("altscreen-v1.0-source/"+rel))==sha, rel
print("COMPLETE_SOURCE_ZIP=PASS",len(paths),"files",target)
print("SHA256",digest(target.read_bytes()))
