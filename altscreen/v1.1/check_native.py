#!/usr/bin/env python3
import json,re,subprocess,sys
from pathlib import Path
R=Path(__file__).resolve().parent
p=Path(sys.argv[1]); assert p.is_file() and p.read_bytes()[:4]==b'\x7fELF'
cmd='/opt/qnx49-wrap/arm-unknown-nto-qnx6.5.0eabi-readelf'
a=subprocess.check_output([cmd,'-Ws',str(p)],text=True)
found=sorted(set(re.findall(r'^\s*\d+:\s+[0-9a-f]+\s+\d+\s+FUNC\s+GLOBAL\s+DEFAULT\s+\d+\s+(\S+)',a,re.M)))
expected=sorted(set(json.loads((R/'reference/expected_native_exports.json').read_text())['exports']))
assert found==expected, (len(found),len(expected),set(found)^set(expected))
h=subprocess.check_output([cmd,'-h',str(p)],text=True)
assert 'ARM' in h and 'Version5 EABI' in h
print('ALTSCREEN_NATIVE_ABI=PASS exports='+str(len(found)))
