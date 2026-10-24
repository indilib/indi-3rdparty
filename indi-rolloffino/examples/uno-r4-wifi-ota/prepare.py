#!/usr/bin/env python3
"""Prepare a private token and unique build identity; never prints credentials."""
from pathlib import Path
import datetime
import json
import re
import secrets
import shutil
import os
import argparse

root=Path(__file__).resolve().parent
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--secrets',type=Path,help='Optional existing private settings to copy')
a=p.parse_args()
private=root/'Rolloff/secrets.h'
key=root/'ota-token.txt'
if not private.exists():
    if a.secrets is None or not a.secrets.is_file():
        raise SystemExit('Create Rolloff/secrets.h from secrets.h.example and set Wi-Fi credentials first')
    shutil.copyfile(a.secrets,private)
text=private.read_text()
matches=re.findall(r'^\s*#define\s+OTA_PASSWORD\s+"([0-9a-f]{32})"\s*$',text,re.M)
if len(matches)>1: raise SystemExit('Multiple OTA_PASSWORD definitions; resolve before building')
if matches:
    token=matches[0]
elif re.search(r'^\s*#define\s+OTA_PASSWORD\b',text,re.M):
    raise SystemExit('Existing OTA_PASSWORD must be exactly 32 lowercase hex characters')
else:
    token=secrets.token_hex(16)
    private.write_text(text+'\n#define OTA_PASSWORD "'+token+'"\n')
if key.exists() and key.read_text().strip()!=token:
    raise SystemExit('Token file differs from secrets.h; preserve both and resolve before upload')
key.write_text(token+'\n')
os.chmod(private,0o600); os.chmod(key,0o600)
identity='roof-wifi-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-'+secrets.token_hex(3)
(root/'Rolloff/BuildIdentity.h').write_text('#pragma once\n#define ROOF_BUILD_ID "'+identity+'"\n')
(root/'build-identity.json').write_text(json.dumps({'build_id':identity,'board':'arduino:renesas_uno:unor4wifi','core':'1.6.0','ArduinoOTA':'1.1.1'},indent=2)+'\n')
print('Prepared build:',identity)
print('Private token saved locally; original secrets.h unchanged.')
