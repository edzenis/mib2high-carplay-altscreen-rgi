#!/usr/bin/env python3
"""Create a stable JAR from freshly compiled Java classes (never modify class bytes)."""
from pathlib import Path
from zipfile import ZipFile,ZipInfo,ZIP_DEFLATED
import sys
classes=Path(sys.argv[1]);out=Path(sys.argv[2]);out.parent.mkdir(parents=True,exist_ok=True)
# Preserve the same default Java 8 manifest content as the former `jar cf`.
manifest=b'Manifest-Version: 1.0\r\nCreated-By: 1.8.0_504 (Temurin)\r\n\r\n'
paths={str(p.relative_to(classes)).replace('\\','/'):p for p in classes.rglob('*') if p.is_file()}
assert paths and all(k.endswith('.class') for k in paths), 'JAR must contain compiled classes only'
with ZipFile(out,'w') as jar:
    def add(name,data,mode=0o644):
        i=ZipInfo(name,(2026,10,22,0,0,0));i.compress_type=ZIP_DEFLATED;i.external_attr=mode<<16
        jar.writestr(i,data,compress_type=ZIP_DEFLATED,compresslevel=9)
    add('META-INF/',b'',0o755)
    add('META-INF/MANIFEST.MF',manifest)
    dirs=set()
    for name in paths:
        parent=Path(name).parent
        while str(parent)!='.':
            dirs.add(str(parent).replace('\\','/')+'/');parent=parent.parent
    for d in sorted(dirs):add(d,b'',0o755)
    for name,path in sorted(paths.items()):add(name,path.read_bytes())
print('ALTAPI_JAR_PACKAGING=DETERMINISTIC classes=%d'%len(paths))
