#!/bin/bash

#set -o xtrace

apt update && apt install -y  libmodbus-dev libsystemd-dev libi2c-dev

. /ci-scripts/include.sh

if [ "${DEBIAN_VERSION}" == "stretch" -o "${DEBIAN_VERSION}" == "buster" ]; then
    ln -s debian.pre debian
else 
    ln -s debian.n debian
fi
