#!/usr/bin/python3

#-----------------------------------------------------------------------
# Daemon for reducing full scale images as direct input for
# timelapse creation.
#
# Copyright (C) 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>
#
# This application is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
#-----------------------------------------------------------------------

from datetime import datetime
import argparse
from pathlib import Path

parser = argparse.ArgumentParser(description="Transform raw images to smaller size for timelapse creation")
parser.add_argument("-v", "--verbose", action='store_true',
                    help="Display progress information")
parser.add_argument("-d", "--mediadir", default=".",
                    help="Directory holding the media files")
parser.add_argument("-i", "--inputdir", default="/tmp/cam",
                    help="Directory holding the raw (full scale) images")
parser.add_argument("-w", "--width", default=640, type=int,
                    help="Target picture width in px")
parser.add_argument("-f", "--fifo", default="/tmp/imageconverter.fifo",
                    help="Named pipe where new image creation will be signalled")

args = parser.parse_args()

import os, sys, stat

# ensure that FIFO exists
if not os.path.exists(args.fifo):
    os.mkfifo(args.fifo)

# read file name from the pipe
while True:
    pipein = open(args.fifo, 'r')
    filename = pipein.readline().strip()
    # convert file

    if os.path.exists(args.inputdir + "/" + filename):
        now       = datetime.now()
        input     = args.inputdir + "/" + filename
        outputdir = args.mediadir + '/' +  now.strftime("%Y-%m-%d")
        output    = outputdir + "/" + filename
        if args.verbose:
            print("Converting \"%s\" to %s" % (filename, outputdir))
        
        # ensure that the image directory exists
        if not Path(outputdir).exists():
            Path(outputdir).mkdir(parents=True)
        # convert to target width
        os.system("convert %s -resize %s %s" % (input, args.width, output))

        # remove original file
        os.remove(input)
       
    
