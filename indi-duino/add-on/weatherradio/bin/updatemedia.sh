#!/bin/bash
#-----------------------------------------------------------------------
# Script updating the time lapse videos and carousel images. Add this script
# as cron job.
#
# Copyright (C) 2020-21 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>
#
# This application is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
#-----------------------------------------------------------------------

mediadir="/usr/local/share/weatherradio/html/media"
savedir="$mediadir/save"
img_limit=6

# current day
YESTERDAY=$(date --date="yesterday" +"%F")
DATE=`date +"%F"`
# DATE=$YESTERDAY

BIN="/usr/local/share/weatherradio/bin"
WEATHERDIR="/ext/multimedia/weather"

# ensure that save directory exists
mkdir -p $savedir

# clear old images
i=0; h="no"; for f in `ls -Rt $mediadir/*.jpg`; do
    if [ $i -gt $img_limit ]; then
	mv $f $savedir
    else
	# determine the capture hour
	hh=`echo $f | sed -e 's/.*_//' -e 's/\([0-9][0-9]\).*/\1/'`
	if [ $h != $hh ]; then
	    # new hour
	    i=$(($i + 1))
	else
	    mv $f $savedir
	fi
	h=$hh
    fi
done

# update JSON file for carousel
$BIN/wr_list_media.py

# restore images from save dir
mv $savedir/* $mediadir

# remove latest time lapse
if [ `find $WEATHERDIR -maxdepth 1 -name timelapse_${DATE}_[0-2]\*.mp4 | wc -l` -gt 0 ]; then

    latest=`ls $WEATHERDIR/timelapse_${DATE}_[0-2]*.mp4 | tail -1`

    if [ -f $latest ]; then
	rm -f $latest
    fi
fi

if [ -d $WEATHERDIR/$DATE ]; then
    $BIN/wr_video_create.py -d $WEATHERDIR/$DATE
else
    exit 0
fi

(cd $WEATHERDIR
 #empty time lapse list
 rm -f timelapse_$DATE.txt
 # list all matching time lapse videos
# for f in timelapse_${DATE}_*.mp4;  do
 for f in `ls timelapse_*h.mp4 | tail -24`;  do
     if [ -f $f ]; then
	 echo "file $f" >> timelapse_$DATE.txt
     fi
 done
 # remove old video
if [ -f timelapse_$DATE.mp4 ]; then
    rm -f timelapse_$DATE.mp4
fi

 # concatenate all time lapse videos of totay into one single video
ffmpeg -f concat -i timelapse_$DATE.txt -loglevel "level+error" -c copy timelapse_$DATE.mp4 
)

# update web page

(cd /usr/local/share/weatherradio/html/media
 cp -p $WEATHERDIR/timelapse_$DATE.mp4 .

 if [ -f timelapse_current.mp4 ]; then
    rm -f timelapse_current.mp4
 fi

 ln -s ./timelapse_$DATE.mp4 timelapse_current.mp4
 )
