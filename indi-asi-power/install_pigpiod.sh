#!/bin/bash -x
# Run this script as sudo
cp pigpiod.service /etc/systemd/system/
chmod 644 /etc/systemd/system/pigpiod.service
systemctl daemon-reload
if [ $? -eq 0 ]
then
  systemctl enable pigpiod.service
  systemctl start pigpiod.service
fi
