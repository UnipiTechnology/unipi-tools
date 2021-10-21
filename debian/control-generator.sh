#!/bin/bash


. /ci-scripts/include.sh

arch=`dpkg-architecture -q DEB_BUILD_ARCH`

#  -- undefined ! ${PRODUCT}           set by gitlab-ci
#  ${DEBIAN_VERSION}    from bob-the-builder image
#  ${arch}              from building arch

PROJECT_VERSION=$(dpkg-parsechangelog -S Version)


if [ "${DEBIAN_VERSION}" == "stretch" -o "${DEBIAN_VERSION}" == "buster" ]; then
    if [ "${arch}" == "armhf" ]; then
        PKG_KERNEL_HEADERS=raspberrypi-kernel-headers
        PKG_KERNEL_IMAGE=raspberrypi-kernel
        LINUX_DIR_ARR=($(dpkg -L ${PKG_KERNEL_HEADERS} | sed -n '/^\/lib\/modules\/.*-v7.*\/build$/p'))
        EXTRA_BUILD_PAR="LINUX_DIR_PATH=${LINUX_DIR_ARR[0]}"
    fi

    #echo "unipi:PreDepends=unipi-common(>=1.2.22~)" > debian/substvars
    #echo "unipi:Depends=unipi-kernel-modules (>=1.42) | unipi-kernel-modules-dkms (>=1.42) | neuron-kernel (>=1.42) | axon-kernel (>= 1.13.20180719)" >> debian/substvars
    #####################################################################
    ### Modify control - add unipi-common
    cat >>debian/control <<EOF

Package: unipi-common
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}, busybox, initramfs-tools libi2c0
Breaks: unipi-firmware (<<5.50), unipi-kernel-modules (<<1.42), unipi-kernel-modules-dkms (<<1.42), neuron-kernel (<<1.13.20180719), axon-kernel (<<1.13.20180719), unipi-lte (<<0.10~)
Description: Common utilities for Unipi/Neuron/Axon
 Check model and version of Unipi platform

EOF

    #####################################################################
    ### Create rules.in
    cat  >debian/rules.in <<EOF

override_dh_systemd_start:
	dh_systemd_start -Xset_image_read_only.service unipihostname.service unipicheck.service

override_dh_systemd_enable:
	dh_systemd_enable -Xset_image_read_only.service

override_dh_auto_build:
	dh_auto_build -- ${EXTRA_BUILD_PAR} PROJECT_VERSION=${PROJECT_VERSION}

override_dh_auto_install:
	cat debian/substvars
	dh_auto_install -- ${EXTRA_BUILD_PAR} PROJECT_VERSION=${PROJECT_VERSION}

override_dh_gencontrol:
	dh_gencontrol -- -Vunipi:PreDepends=unipi-common(>=1.2.22~) -V${EXTRA_BUILD_PAR} PROJECT_VERSION=${PROJECT_VERSION} \
	       -Vunipi:Depends=unipi-kernel-modules (>=1.42) | unipi-kernel-modules-dkms (>=1.42) | neuron-kernel (>=1.42) | axon-kernel (>= 1.13.20180719)

EOF


else
    ################################################################################
    #for bullseye ++ - unipi-common replaced by unipi-os-configurator in own project

    #echo "unipi:PreDepends=unipi-os-configurator" > debian/substvars
    #echo "unipi:Depends=unipi-os-configurator" >> debian/substvars

    #####################################################################
    ### Create rules.in
    cat  >debian/rules.in <<EOF

override_dh_auto_build:
	dh_auto_build -- PROJECT_VERSION=${PROJECT_VERSION}
	cat debian/substvars

override_dh_gencontrol:
	dh_gencontrol -- -Vunipi:PreDepends=unipi-os-configurator \
	       -Vunipi:Depends=unipi-os-configurator

EOF

fi

