#!/usr/bin/env python3
"""Compile only; never uploads. Requires prepare.py and the pinned library/core."""
from pathlib import Path
import subprocess
import json
import hashlib
import re
import sys
import os
import shutil
root=Path(__file__).resolve().parent
cli=os.environ.get('ARDUINO_CLI') or shutil.which('arduino-cli') or str(Path.home()/'.local/bin/arduino-cli')
core = subprocess.run([str(cli), 'core', 'list', '--format', 'json'],
                      check=True, capture_output=True, text=True)
platforms = json.loads(core.stdout).get('platforms', [])
if not any(x.get('id') == 'arduino:renesas_uno' and x.get('installed_version') == '1.6.0' for x in platforms):
    raise SystemExit('Install required core: arduino-cli core install arduino:renesas_uno@1.6.0')
identity=json.loads((root/'build-identity.json').read_text())
source=root/'Rolloff'
out=root/'build'
subprocess.run([str(cli),'compile','--jobs','2','--fqbn',identity['board'],
                '--output-dir',str(out),str(source)],check=True)
binary=out/'Rolloff.ino.bin'
data=binary.read_bytes()
marker=identity['build_id'].encode()
if marker not in data: raise SystemExit('Built image lacks expected identity; no manifest written')
# UNO R4 max flash / bootloader differs by board. Runtime backend enforces its
# precise half-flash minus one-page limit too; this is a conservative early gate.
if not 256<=len(data)<=112*1024: raise SystemExit('Image exceeds conservative OTA size budget')
secret=(source/'secrets.h').read_text()
match=re.findall(r'^\s*#define\s+OTA_PASSWORD\s+"([0-9a-f]{32})"\s*$',secret,re.M)
if len(match)!=1 or (root/'ota-token.txt').read_text().strip()!=match[0]:
    raise SystemExit('Private token mismatch')
# The upload client checks the expected new token is actually embedded, so it
# cannot silently rotate the key away from the local management credential.
if match[0].encode() not in data: raise SystemExit('Image lacks expected token')
manifest={**identity,'file':binary.name,'size':len(data),'sha256':hashlib.sha256(data).hexdigest()}
(out/'firmware.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Compiled candidate:',identity['build_id'])
print('Manifest:',out/'firmware.json')
print('No upload performed.')
