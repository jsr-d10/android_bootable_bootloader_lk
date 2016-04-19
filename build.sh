#!/bin/sh
red=$(tput setaf 1)
green=$(tput setaf 2)
end=$(tput sgr0)

mkdir -p ../../../out/target/product/d10f/obj/BOOTLOADER_EMMC_OBJ
make DEBUG=2 PROJECT=msm8226 BOOTLOADER_OUT=../../../out/target/product/d10f/obj/BOOTLOADER_EMMC_OBJ EMMC_BOOT=1 VERSION="$(git describe --tags)" -j3
if [ $? -gt 0 ]; then
    echo ""
    echo "${red}Build FAILED!${end}"
else
    echo ""
    echo "${green}Successfully built${end}"
fi
