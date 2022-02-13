#!/bin/bash
PROC=$( uname -p )
case "$PROC" in
armv*)
    echo $PROC processor;;
aarch64)
    echo $PROC processor;;
*)
    echo ERROR invalid processor $PROC
    exit 1;;
esac

CHIP=$( grep Hardware /proc/cpuinfo | awk '{print $3}')
case "$CHIP" in
BCM283*)
    echo $CHIP chip;;
BCM2711)
    echo $CHIP chip;;
*)
    echo ERROR invalid chip $CHIP
    exit 1;;
esac
/usr/bin/pigpiod -g -m
