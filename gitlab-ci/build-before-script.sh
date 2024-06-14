#!/bin/bash

#set -o xtrace

apt update && apt install -y  libmodbus-dev libsystemd-dev libi2c-dev libmhash-dev pkg-config autotools-dev autoconf automake systemd

. /ci-scripts/include.sh
