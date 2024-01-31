#!/bin/bash

#set -o xtrace

apt update && apt install -y  libmodbus-dev libsystemd-dev libi2c-dev libmhash-dev pkg-config

. /ci-scripts/include.sh

if [ "${DEBIAN_VERSION}" == "stretch" -o "${DEBIAN_VERSION}" == "buster" ]; then
    ln -s debian.pre debian
    ARCH="$(dpkg-architecture -q DEB_BUILD_ARCH)"
    if [ "$ARCH" = "armhf" ]; then
        /ci-scripts/fix-product-repository.sh "${DEBIAN_VERSION}" "${PRODUCT}" "${ARCH}"
        apt update  && apt-get install -y raspberrypi-kernel-headers
    fi
else 
    ln -s debian.n debian
fi
