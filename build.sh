#!/bin/sh
red=$(tput setaf 1)
green=$(tput setaf 2)
end=$(tput sgr0)

build_dir="../../../out/target/product/d10f/obj/BOOTLOADER_EMMC_OBJ"

print_usage() {
    echo "Usage: $0 [-h|-?|--help] [-f|--flash] [-c|--clean]"
    echo "--help: show this text"
    echo "--flash: flash built binary to device using fastboot flash aboot and reboot device to fastboot"
    echo "--clean: run make clean before building"
}

# Transform long options to short ones
for arg in "$@"; do
  shift
  case "$arg" in
    "--help") set -- "$@" "-h" ;;
    "--clean") set -- "$@" "-c" ;;
    "--flash") set -- "$@" "-f" ;;
    *)        set -- "$@" "$arg"
  esac
done

OPTIND=1
while getopts "hfc" opt
do
  case "$opt" in
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

make DEBUG=2 PROJECT=msm8226 BOOTLOADER_OUT="$build_dir" EMMC_BOOT=1 VERSION="$(git describe --tags)" -j3
if [ $? -gt 0 ]; then
    echo ""
    echo "${red}Build FAILED!${end}"
else
    echo ""
    echo "${green}Successfully built${end}"
    if [ "$flash" = "true" ]; then
        fastboot flash aboot "$build_dir"/../../emmc_appsboot.mbn
        fastboot reboot bootloader
    fi
fi
