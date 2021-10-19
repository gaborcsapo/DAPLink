#!/bin/bash

source "${WORKSPACE}/buildtools/images/images-wrapper.sh"

bamboo_BuildVersion="${bamboo_VersionBase}${bamboo_buildNumber}"
TEMPSHARE="$(mktemp -d /tmp/udp-XXXXXXXX)"

mkdir -p "${TEMPSHARE}/${bamboo_BuildVersion}"

find ./projectfiles/make_gcc_arm/stm32h743ii_if/build \( -name "*.c" -o -name "*.bin" -o -name "*.hex" -o -name "*.elf" -o -name "*.txt" -o -name "*.ld" -name "*.map" \) -exec cp {} "${TEMPSHARE}/${bamboo_BuildVersion}" \;


copy_to_images "$TEMPSHARE/${bamboo_BuildVersion}" "$bamboo_BuildImagesPath"
