#!/bin/bash

set -x -e

# Install libindi
echo "==> Install libindi nightly release."
apt-add-repository -y ppa:mutlaqja/indinightly
apt-get update
apt-get -y install libindi

exit 0
