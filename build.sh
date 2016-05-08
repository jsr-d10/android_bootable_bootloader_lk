#!/bin/sh
red=$(tput setaf 1)
green=$(tput setaf 2)
end=$(tput sgr0)

build_dir="../../../out/target/product/d10f/obj/BOOTLOADER_EMMC_OBJ"

print_usage() {
    echo "Usage: $0 [-h|-?|--help] [-b|--boot] [-f|--flash] [-c|--clean]"
    echo "--help: show this text"
    echo "--boot:  create bootable image and boot it to device using fastboot boot without flashing"
    echo "--flash: flash built binary to device using fastboot flash aboot and reboot device to fastboot"
    echo "--clean: run make clean before building"
}

# Transform long options to short ones
for arg in "$@"; do
  shift
  case "$arg" in
    "--help") set -- "$@" "-h" ;;
    "--boot") set -- "$@" "-b" ;;
    "--clean") set -- "$@" "-c" ;;
    "--flash") set -- "$@" "-f" ;;
    *)        set -- "$@" "$arg"
  esac
done

OPTIND=1
while getopts "hfcb" opt
do
  case "$opt" in
    "b") boot=true ;;
    "c") clean=true ;;
    "f") flash=true ;;
    "h") print_usage; exit 0 ;;
    *)   print_usage >&2; exit 1 ;;
  esac
done
shift $(expr $OPTIND - 1) # remove options from positional parameters

if [ "$clean" = "true" ]; then
    rm -rf "$build_dir"
fi

mkdir -p "$build_dir"

touch dev/fbcon/fbcon.c # Force rebuilding it to make sure that version string is updated
make DEBUG=2 PROJECT=msm8226 BOOTLOADER_OUT="$build_dir" EMMC_BOOT=1 VERSION="$(git describe --tags)" -j3
if [ $? -gt 0 ]; then
    echo ""
    echo "${red}Build FAILED!${end}"
else
    echo ""
    echo "${green}Successfully built${end}"
    if [ "$boot" = "true" ]; then
        rm -f "$build_dir/../../IBL.img" # Throw error on boot attempt if mkbootimg fails
        mkbootimg --kernel "$build_dir/../../emmc_appsboot.raw" --dt "$build_dir/../../dt.img" --ramdisk /dev/null -o "$build_dir/../../IBL.img"
        fastboot boot "$build_dir/../../IBL.img"
        exit 0
    fi
    if [ "$flash" = "true" ]; then
        fastboot flash aboot "$build_dir"/../../emmc_appsboot.mbn
        fastboot reboot-bootloader
        exit 0
    fi
fi
