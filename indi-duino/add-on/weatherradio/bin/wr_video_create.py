#!/usr/bin/python3

#-----------------------------------------------------------------------
# Script for creating videos sequences from time lapse frames.
#
# Copyright (C) 2020 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>
#
# This application is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
#-----------------------------------------------------------------------

from wr_config import *

import os
import tempfile
import shutil
from pathlib import Path
from datetime import datetime
import argparse
import ffmpeg

def create_video(input, starttime, targetdir, mode):
    # create a temporary directory
    tmpdir = tempfile.mkdtemp()
    name = datetime.fromtimestamp(starttime).strftime(targetdir.as_posix()+"/timelapse_%Y-%m-%d_%Hh.mp4")

    # do not overwrite
    if Path(name).exists():
        return
    
    print("Creating video %s" % (name))
    pos = 0
    # link the files
    for file in input:
        filename = ("tl-%04d" + file.suffix)  % (pos)
        pos += 1
        linkname = os.path.join(tmpdir, filename)
        os.symlink(file.as_posix(), linkname)

    stream = ffmpeg.input(tmpdir + "/tl-%04d.jpg", f="image2", loglevel="level+error")
    stream = ffmpeg.output(stream, name, s=mode)
    ffmpeg.run(stream)
    # clean up
    shutil.rmtree(tmpdir, ignore_errors=True)
    

parser = argparse.ArgumentParser(description="Create video sequences from a single directory")
parser.add_argument("-v", "--verbose", action='store_true',
                    help="Display progress information")
parser.add_argument("-d", "--mediadir", default=MEDIADIR,
                    help="Directory holding the media files")
parser.add_argument("-l", "--length", default=1, type=int,
                    help="Duration of the video sequences [hours]")
parser.add_argument("-m", "--mode", default="640x480",
                    help="video resolution")

args = parser.parse_args()

files = sorted(Path(args.mediadir).iterdir(), key=lambda d: d.stat().st_mtime)

if len(files) == 0:
    print("No media files found")
    sys.exit()

starttime = files[0].stat().st_mtime
input     = [] 

for file in files:
    if Path(file).suffix in [".jpg", ".png"]:
        if file.stat().st_mtime - starttime > args.length * 3600:
            create_video(input, starttime, file.parent.parent, args.mode)
            input = []
            starttime = file.stat().st_mtime
        else:
            input.append(file)

if len(input) > 0:
    create_video(input, starttime, file.parent.parent, args.mode)
