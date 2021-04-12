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
import os, sys, stat, re

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
regex = re.compile("(.*)_([0-9]{2})([0-9]{2})([0-9]{2})\.jpg")


# ensure that FIFO exists
if not os.path.exists(args.fifo):
    os.mkfifo(args.fifo)

# read file name from the pipe
while True:
    pipein = open(args.fifo, 'r')
    filename = pipein.readline().strip()
    # convert file

    if Path(args.inputdir + "/" + filename).is_file():
        # extract date and time from the filename
        date = regex.sub(r"\1", filename)
        time = regex.sub(r"\2:\3:\4", filename)
        input     = args.inputdir + "/" + filename
        outputdir = args.mediadir + '/' +  date
        output    = outputdir + "/" + filename
        if args.verbose:
            print("Converting \"%s\" to %s" % (filename, outputdir))

        # ensure that the image directory exists
        if not Path(outputdir).exists():
            Path(outputdir).mkdir(parents=True)

        # sleep for testing purposes
        # dur = 60
        # print("Sleeping %ds" % dur)
        # sleep(dur)
        # print("Sleeping %ds (finished)" % dur)

        # convert to target width
        os.system("convert %s -font helvetica -fill white -pointsize 60 -annotate +40+100 \'%s %s\' -resize %s %s" % (input, date, time, args.width, output))

        # remove original file
        os.remove(input)
        if args.verbose:
            print("Converting \"%s\" to %s (finished)" % (filename, outputdir))
    else:
        print("%s does not exist in %s" % (filename, args.inputdir))
    
