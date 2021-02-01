#!/bin/sh
#-----------------------------------------------------------------------
# Script to stop the timelapse service.
#
# Copyright (C) 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>
#
# This application is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
#-----------------------------------------------------------------------

PID=$1

echo "Stopping timelapse service ..."

touch /tmp/timelapse.stop

while [ `ps -p $PID | wc -l` -gt 1 ]; do
    sleep 2
done

echo "Stopping timelapse service ... (succeeded)"
