#!/usr/bin/python

#-----------------------------------------------------------------------
# Script for listing all media files and store the information as JSON document.
#
# Copyright (C) 2020 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>
#
# This application is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
#-----------------------------------------------------------------------

import sys
import os
from pathlib import Path
import time
from wr_config import *
import argparse
import simplejson as json

parser = argparse.ArgumentParser(description="List all media files and store the information as JSON document")
parser.add_argument("-v", "--verbose", action='store_true',
                    help="Display progress information")
parser.add_argument("-d", "--mediadir", default=MEDIADIR,
                    help="Directory holding the media files")
parser.add_argument("-o", "--output", default=DATAPATH+"/images.json",
                    help="JSON file to be written")

args = parser.parse_args()

files = []

for file in sorted(Path(args.mediadir).iterdir(), key=lambda d: d.stat().st_mtime):
    if Path(file).suffix in [".jpg", ".png"]:
        entry = {}
        entry["name"]  = Path(file).name
        entry["type"]  = "img"
        entry["ctime"] = file.stat().st_mtime
        files.append(entry)

output = open(args.output, 'w')
output.write(json.dumps(files, indent=2, separators=(',', ':'), sort_keys=True, ignore_nan=True))
output.close()
