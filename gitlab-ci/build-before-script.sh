#!/bin/bash

#set -o xtrace

apt update && apt install -y  libmodbus-dev libsystemd-dev libmhash-dev pkg-config autotools-dev autoconf automake libtool systemd

. /ci-scripts/include.sh
