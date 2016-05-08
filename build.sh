#!/bin/sh
red=$(tput setaf 1)
green=$(tput setaf 2)
yellow=$(tput setaf 3)
end=$(tput sgr0)
VERSION="$(git describe --tags)"
build_dir="../../../out/target/product/d10f/obj/BOOTLOADER_EMMC_OBJ"

print_usage() {
    echo "Usage: $0 [-h|-?|--help] [-b|--boot] [-f|--flash] [-c|--clean] [-z|--zip]"
    echo "--help: show this text"
    echo "--boot:  create bootable image and boot it to device using fastboot boot without flashing"
    echo "--flash: flash built binary to device using fastboot flash aboot and reboot device to fastboot"
    echo "--clean: run make clean before building"
    echo "--zip: build flashable Recovery zip after building"
}

# Transform long options to short ones
for arg in "$@"; do
  shift
  case "$arg" in
    "--help") set -- "$@" "-h" ;;
    "--boot") set -- "$@" "-b" ;;
    "--clean") set -- "$@" "-c" ;;
    "--flash") set -- "$@" "-f" ;;
    "--zip") set -- "$@" "-z" ;;
    *)        set -- "$@" "$arg"
  esac
done

OPTIND=1
while getopts "hfcbz" opt
do
  case "$opt" in
    "b") boot=true ;;
    "c") clean=true ;;
    "f") flash=true ;;
    "z") zip=true ;;
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
make DEBUG=2 PROJECT=msm8226 BOOTLOADER_OUT="$build_dir" EMMC_BOOT=1 VERSION="$VERSION" -j3
if [ $? -gt 0 ]; then
    echo ""
    echo "${red}Build FAILED!${end}"
else
    echo ""
    echo "${green}Successfully built${end}"

    if [ "$zip" = "true" ]; then
        path=$(readlink -f $build_dir/../../)
        zipname="$path/IBL_$VERSION.zip"
        rm -rf /tmp/zip_template/ "$zipname"
        cp -r zip_template /tmp/
        cp "$build_dir/../../emmc_appsboot.mbn" "/tmp/zip_template/firmware-update/IBL_$VERSION.mbn"
        cat > /tmp/zip_template/META-INF/com/google/android/updater-script <<EOF
assert(getprop("ro.product.device") == "D10A_HighScreen" ||
getprop("ro.build.product") == "D10A_HighScreen");
# ---- radio update tasks ----

ui_print("Installing IBL $VERSION...");
ifelse(msm.boot_update("main"), (
package_extract_file("firmware-update/tz.mbn", "/dev/block/platform/msm_sdcc.1/by-name/tz");
package_extract_file("firmware-update/sbl1.mbn", "/dev/block/platform/msm_sdcc.1/by-name/sbl1");
package_extract_file("firmware-update/rpm.mbn", "/dev/block/platform/msm_sdcc.1/by-name/rpm");
package_extract_file("firmware-update/IBL_$VERSION.mbn", "/dev/block/platform/msm_sdcc.1/by-name/aboot");
), "");
msm.boot_update("backup");
msm.boot_update("finalize");
ui_print("Done...");
EOF
        cd "/tmp/zip_template/"
        zip "$zipname-unsigned" * -r
        cd - > /dev/null
        java -Xmx2048m -jar zip_sign/signapk.jar -w zip_sign/testkey.x509.pem zip_sign/testkey.pk8  "$zipname-unsigned"  "$zipname"
        echo "${yellow}$zipname ${green}built${end}"
        rm -rf "/tmp/zip_template/" "$zipname-unsigned"
    fi

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
