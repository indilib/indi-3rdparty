#!/bin/bash -x
# Run this script as sudo
systemctl daemon-reload
if [ $? -eq 0 ]
then
  systemctl enable pigpiod.service
  systemctl start pigpiod.service
fi
