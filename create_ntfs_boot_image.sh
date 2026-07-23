#!/usr/bin/env bash

##
## PROJECT:     ReactOS NTFS-3G Boot Image
## LICENSE:     GPL-2.0-or-later (https://spdx.org/licenses/GPL-2.0-or-later)
## PURPOSE:     Convert a ReactOS BootCD ISO to a UEFI GPT image with an NTFS root
## COPYRIGHT:   Copyright 2026 Ahmed ARIF <arif.ing@outlook.com>
##

set -euo pipefail

readonly SECTOR_SIZE=512
readonly SECTORS_PER_MIB=2048
readonly ESP_START_SECTOR=2048
readonly DEFAULT_ESP_SIZE_MIB=64
readonly DEFAULT_ROOT_PADDING_MIB=256

usage()
{
    cat <<'EOF'
Usage:
  create_ntfs_boot_image.sh
  create_ntfs_boot_image.sh [--force] [--no-run] BOOTCD_ISO [OUTPUT_IMAGE]

Create a GPT disk image containing:
  1. a FAT32 EFI System Partition with the UEFI FreeLoader bootstrap;
  2. an NTFS partition containing the ReactOS BootCD file tree.

With no ISO argument, run the script from a ReactOS Ninja output directory.
It builds the livecd and bootcd targets, replaces reactos-ntfs-uefi.img after
successful validation, and starts QEMU with UEFI firmware.

The generated FreeLoader configuration boots \reactos from partition 2 and
enables COM1 debugging without a menu delay.

Options:
  --force              Replace an existing explicit output image
  --no-run             Create the image without starting QEMU
  -h, --help           Show this help

Environment:
  ESP_SIZE_MIB       ESP size in MiB (default: 64)
  ROOT_PADDING_MIB   Free space added after the extracted ISO tree (default: 256)
  KEEP_WORK_DIR=1    Preserve the temporary construction directory
  OVMF_FIRMWARE      Combined OVMF firmware image
  QEMU_BIN           QEMU executable (default: qemu-system-x86_64)
  QEMU_CPUS          Virtual CPU count (default: 1)
  QEMU_MEMORY_MIB    Guest memory in MiB (default: 2048)
  QEMU_DISPLAY       Optional QEMU display backend, such as gtk or none
  QEMU_SERIAL_LOG    Serial log path (default: ntfs3g-qemu.log beside the image)
EOF
}

die()
{
    printf 'error: %s\n' "$*" >&2
    exit 1
}

require_tool()
{
    command -v "$1" >/dev/null 2>&1 || die "required tool not found: $1"
}

find_ovmf_firmware()
{
    local candidate

    if [[ -n ${OVMF_FIRMWARE:-} ]]; then
        [[ -f $OVMF_FIRMWARE ]] ||
            die "OVMF firmware not found: $OVMF_FIRMWARE"
        realpath "$OVMF_FIRMWARE"
        return
    fi

    for candidate in \
        /usr/share/ovmf/OVMF.fd \
        /usr/share/qemu/OVMF.fd \
        /usr/share/edk2/x64/OVMF.fd; do
        if [[ -f $candidate ]]; then
            printf '%s\n' "$candidate"
            return
        fi
    done

    die "combined OVMF firmware not found; set OVMF_FIRMWARE"
}

launch_qemu()
{
    local qemu_bin=${QEMU_BIN:-qemu-system-x86_64}
    local qemu_cpus=${QEMU_CPUS:-1}
    local qemu_memory_mib=${QEMU_MEMORY_MIB:-2048}
    local serial_log=${QEMU_SERIAL_LOG:-$output_dir/ntfs3g-qemu.log}
    local firmware
    local -a acceleration
    local -a display

    require_tool "$qemu_bin"
    [[ $qemu_cpus =~ ^[0-9]+$ && $qemu_cpus -ge 1 ]] ||
        die "QEMU_CPUS must be a positive integer"
    [[ $qemu_memory_mib =~ ^[0-9]+$ && $qemu_memory_mib -ge 256 ]] ||
        die "QEMU_MEMORY_MIB must be an integer of at least 256"

    firmware=$(find_ovmf_firmware)
    serial_log=$(realpath -m "$serial_log")
    [[ -d $(dirname "$serial_log") ]] ||
        die "QEMU serial log directory not found: $(dirname "$serial_log")"

    if [[ -r /dev/kvm && -w /dev/kvm ]]; then
        acceleration=(-machine q35,accel=kvm -cpu host)
    else
        acceleration=(-machine q35,accel=tcg -cpu max)
    fi
    if [[ -n ${QEMU_DISPLAY:-} ]]; then
        display=(-display "$QEMU_DISPLAY")
    else
        display=()
    fi

    printf 'Starting QEMU with %s\n' "$output_image"
    printf '  Serial log: %s\n' "$serial_log"
    "$qemu_bin" \
        "${acceleration[@]}" \
        "${display[@]}" \
        -name "ReactOS NTFS-3G" \
        -smp "$qemu_cpus" \
        -m "$qemu_memory_mib" \
        -bios "$firmware" \
        -drive "file=$output_image,format=raw,if=ide,snapshot=on" \
        -boot order=c \
        -serial "file:$serial_log" \
        -monitor none \
        -net none \
        -no-reboot
}

partition_sector()
{
    local partition_number=$1
    local field=$2

    sgdisk --info="$partition_number" "$disk_image" |
        awk -v field="$field" '$1 == field && $2 == "sector:" { print $3; exit }'
}

cleanup()
{
    if [[ ${root_mounted:-0} -eq 1 ]]; then
        if ! fusermount3 -u "$root_mount"; then
            printf 'warning: preserving mounted work directory: %s\n' "$work_dir" >&2
            return
        fi
    fi

    if [[ -n ${work_dir:-} && -d $work_dir ]]; then
        if [[ ${KEEP_WORK_DIR:-0} == 1 ]]; then
            printf 'Preserved work directory: %s\n' "$work_dir"
        else
            case "$work_dir" in
                "$output_dir"/.reactos-ntfs-image.*)
                    chmod -R u+w "$work_dir" || true
                    rm -rf -- "$work_dir"
                    ;;
                *)
                    printf 'warning: refusing to remove unexpected work directory: %s\n' "$work_dir" >&2
                    ;;
            esac
        fi
    fi
}

force=0
run_qemu=1
arguments=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --force)
            force=1
            ;;
        --no-run)
            run_qemu=0
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            arguments+=("$@")
            break
            ;;
        -*)
            die "unknown option: $1"
            ;;
        *)
            arguments+=("$1")
            ;;
    esac
    shift
done
set -- "${arguments[@]}"

if [[ $# -eq 0 ]]; then
    build_dir=$(pwd -P)
    [[ -f $build_dir/build.ninja ]] ||
        die "no-argument mode must run from a ReactOS Ninja output directory"
    require_tool ninja
    printf 'Building ReactOS livecd and bootcd in %s...\n' "$build_dir"
    ninja -C "$build_dir" livecd bootcd
    iso_image=$build_dir/bootcd.iso
    output_image=$build_dir/reactos-ntfs-uefi.img
    force=1
elif [[ $# -le 2 ]]; then
    [[ -f $1 ]] || die "ISO image not found: $1"
    iso_image=$(realpath "$1")
    if [[ $# -eq 2 ]]; then
        output_image=$(realpath -m "$2")
    else
        output_image=${iso_image%.*}-ntfs.img
    fi
else
    usage >&2
    exit 2
fi

[[ -f $iso_image ]] || die "ISO image not found: $iso_image"
[[ $output_image != "$iso_image" ]] || die "output image must differ from the input ISO"
output_dir=$(dirname "$output_image")
[[ -d $output_dir ]] || die "output directory not found: $output_dir"
if [[ -e $output_image && $force -ne 1 ]]; then
    die "output already exists; pass --force to replace it: $output_image"
fi

for tool in awk cat chmod cp dd du fusermount3 grep mkdir mkfs.ntfs mkfs.vfat \
            mktemp mcopy mmd mtype mv ntfscat ntfs-3g ntfsls realpath rm \
            sgdisk sync truncate xorriso; do
    require_tool "$tool"
done

esp_size_mib=${ESP_SIZE_MIB:-$DEFAULT_ESP_SIZE_MIB}
root_padding_mib=${ROOT_PADDING_MIB:-$DEFAULT_ROOT_PADDING_MIB}
[[ $esp_size_mib =~ ^[0-9]+$ && $esp_size_mib -ge 32 ]] ||
    die "ESP_SIZE_MIB must be an integer of at least 32"
[[ $root_padding_mib =~ ^[0-9]+$ && $root_padding_mib -ge 64 ]] ||
    die "ROOT_PADDING_MIB must be an integer of at least 64"

work_dir=$(mktemp -d "$output_dir/.reactos-ntfs-image.XXXXXX")
iso_tree=$work_dir/iso
esp_image=$work_dir/esp.img
root_image=$work_dir/root.ntfs
root_mount=$work_dir/root
boot_ini=$work_dir/freeldr.ini
disk_image=$work_dir/disk.img
root_mounted=0
trap cleanup EXIT HUP INT TERM

mkdir "$iso_tree" "$root_mount"
printf 'Extracting %s...\n' "$iso_image"
xorriso -report_about WARNING -osirrox on -indev "$iso_image" \
        -extract / "$iso_tree" >/dev/null
chmod -R u+w "$iso_tree"

for required_file in \
    efi/boot/bootx64.efi \
    freeldr.ini \
    loader/rosload.exe \
    reactos/system32/ntoskrnl.exe \
    reactos/system32/drivers/ntfs3g.sys; do
    [[ -f $iso_tree/$required_file ]] ||
        die "input is not a complete NTFS-3G BootCD; missing /$required_file"
done

cat > "$boot_ini" <<'EOF'
[FREELOADER]
DefaultOS=NTFS_Debug
TimeOut=0

[Display]
TitleText=ReactOS NTFS-3G Boot
MinimalUI=Yes

[Operating Systems]
NTFS_Debug="ReactOS NTFS-3G (Debug)"

[NTFS_Debug]
BootType=Windows2003
SystemPath=multi(0)disk(0)rdisk(0)partition(2)\reactos
Options=/DEBUG /DEBUGPORT=COM1 /BAUDRATE=115200 /SOS /FASTDETECT /MININT
EOF

tree_bytes=$(du -sb "$iso_tree" | awk '{ print $1 }')
tree_mib=$(( (tree_bytes + 1024 * 1024 - 1) / (1024 * 1024) ))
root_size_mib=$(( tree_mib + root_padding_mib ))
disk_size_mib=$(( 1 + esp_size_mib + root_size_mib + 2 ))

truncate -s "${disk_size_mib}M" "$disk_image"
sgdisk --clear \
       --new=1:${ESP_START_SECTOR}:+${esp_size_mib}M \
       --typecode=1:ef00 \
       --change-name=1:ReactOS-ESP \
       --new=2:0:0 \
       --typecode=2:0700 \
       --change-name=2:ReactOS-NTFS \
       "$disk_image" >/dev/null

esp_start=$(partition_sector 1 First)
esp_end=$(partition_sector 1 Last)
root_start=$(partition_sector 2 First)
root_end=$(partition_sector 2 Last)
[[ -n $esp_start && -n $esp_end && -n $root_start && -n $root_end ]] ||
    die "failed to read the generated GPT layout"

esp_sectors=$(( esp_end - esp_start + 1 ))
root_sectors=$(( root_end - root_start + 1 ))
truncate -s "$((esp_sectors * SECTOR_SIZE))" "$esp_image"
truncate -s "$((root_sectors * SECTOR_SIZE))" "$root_image"

printf 'Formatting ESP (%s MiB) and NTFS root (%s MiB)...\n' \
       "$esp_size_mib" "$((root_sectors / SECTORS_PER_MIB))"
mkfs.vfat -F 32 -n REACTOS_ESP "$esp_image" >/dev/null
mkfs.ntfs -F -Q -q -L ReactOS -p "$root_start" -H 255 -S 63 "$root_image"

mmd -i "$esp_image" ::/EFI ::/EFI/BOOT ::/loader
mcopy -i "$esp_image" "$iso_tree/efi/boot/bootx64.efi" ::/EFI/BOOT/BOOTX64.EFI
mcopy -i "$esp_image" "$iso_tree/loader/rosload.exe" ::/loader/rosload.exe
mcopy -i "$esp_image" "$boot_ini" ::/freeldr.ini

printf 'Copying the ReactOS tree to NTFS...\n'
ntfs-3g -o permissions "$root_image" "$root_mount"
root_mounted=1
cp -R --no-preserve=mode,ownership,timestamps "$iso_tree/." "$root_mount/"
cp "$boot_ini" "$root_mount/freeldr.ini"
sync "$root_mount"
fusermount3 -u "$root_mount"
root_mounted=0

ntfsls -p /reactos/system32/drivers "$root_image" | grep -Fq ntfs3g.sys ||
    die "NTFS root validation failed: ntfs3g.sys not found"
[[ $(ntfscat "$root_image" /freeldr.ini) == *'partition(2)\reactos'* ]] ||
    die "NTFS root validation failed: FreeLoader configuration is incorrect"

dd if="$esp_image" of="$disk_image" bs=1M \
   seek="$((esp_start / SECTORS_PER_MIB))" conv=notrunc,sparse status=none
dd if="$root_image" of="$disk_image" bs=1M \
   seek="$((root_start / SECTORS_PER_MIB))" conv=notrunc,sparse status=none
sync "$disk_image"

sgdisk --verify "$disk_image" |
    grep -Fq 'No problems found' ||
    die "GPT validation failed"
mtype -i "$disk_image@@$((esp_start * SECTOR_SIZE))" ::/freeldr.ini |
    grep -Fq 'partition(2)\reactos' ||
    die "ESP validation failed"

if [[ $force -eq 1 ]]; then
    mv -f "$disk_image" "$output_image"
else
    mv "$disk_image" "$output_image"
fi

printf 'Created %s\n' "$output_image"
printf '  ESP:  partition 1, FAT32, %s MiB\n' "$esp_size_mib"
printf '  Root: partition 2, NTFS, %s MiB\n' "$((root_sectors / SECTORS_PER_MIB))"

cleanup
trap - EXIT HUP INT TERM
if [[ $run_qemu -eq 1 ]]; then
    launch_qemu
fi
