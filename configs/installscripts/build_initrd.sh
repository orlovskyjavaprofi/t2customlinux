#!/bin/bash
# --- T2-COPYRIGHT-BEGIN ---
# t2/target/share/install/build_initrd.sh
# Copyright (C) 2004 - 2026 The T2 SDE Project
# SPDX-License-Identifier: GPL-2.0
# --- T2-COPYRIGHT-END ---

set -e

copy_with_libs() {
    local src=$1
    local dest=$2
    # Copy the main binary
    cp -a "$src" "$dest/"
    # Crawl and copy dependencies
    for lib in $(ldd "$src" | grep "=> /" | awk '{print $3}'); do
        mkdir -p "initramfs/$(dirname "$lib")"
        cp -an "$lib" "initramfs/$(dirname "$lib")"
    done
}

[ "$boot_title" ] || boot_title="T2 Installation"

. $base/misc/target/initrd.in
. $base/misc/target/boot.in

cd $build_toolchain

# Additional initrd overlay
#
rm -rf initramfs
mkdir -p initramfs/{,usr/}{,s}bin

copy_with_libs "$build_root/bin/tar" "initramfs/bin/"
copy_with_libs "$build_root/bin/readlink" "initramfs/bin/"
copy_with_libs "$build_root/bin/rmdir" "initramfs/bin/"
copy_with_libs "$build_root/usr/bin/zstd" "initramfs/usr/bin/"
copy_with_libs "$build_root/usr/bin/fget" "initramfs/bin/"
copy_with_libs "$build_root/usr/bin/install" "initramfs/usr/bin/"
copy_with_libs "$build_root/bin/grep" "initramfs/bin/"

mkdir -p initramfs/lib64 initramfs/lib
mkdir -p initramfs/usr/lib64/
cp -a $build_root/usr/lib64/libcap.so.2* initramfs/usr/lib64/
ln -sf /usr/lib64/libcap.so.2 initramfs/lib64/libcap.so.2
ln -sf /usr/lib64/libcap.so.2 initramfs/lib/libcap.so.2

sed '/PANICMARK/Q' $build_root/sbin/initrdinit > initramfs/init
cat $base/target/share/install/init >> initramfs/init
chmod +x initramfs/init

# For each available kernel, version extracted from kconfig-...
#
arch_boot_cd_pre $isofsdir
for kernel in `egrep 'X .* KERNEL .*' $base/config/$config/packages |
          cut -d ' ' -f 5`; do
  # for all kernel variants
  for var in $(ls $build_root/boot/initrd-*); do
    initrd=${var##*/}
    var=${var#*initrd-}

    # use 2nd expanded kernel image, which might be a compressed vmlinuz
    kernel=`ls $build_root/boot/vmlinu?-$var`
    kernel=${kernel##*/}

    kernelver=$var

    # copy symlinks and it's targets
    for f in $build_root/boot/{vmlinu?-$var,$initrd}; do
	cp -a $f $isofsdir/boot/
	#f=$(readlink $f) [ "$f" ] && cp $build_root/boot/$f $isofsdir/boot/
    done

    extend_initrd $isofsdir/boot/$initrd $build_toolchain/initramfs
    arch_boot_cd_add $isofsdir $kernelver "$boot_title" \
                     /boot/$kernel /boot/$initrd

    # copy a initrd variants, too?
    for initrd in ${initrd/initrd/netrd} ${initrd/initrd/minird}; do
     
      if [ -e $build_root/boot/$initrd ]; then
	cp -a $build_root/boot/$initrd $isofsdir/boot/

	extend_initrd $isofsdir/boot/$initrd $build_toolchain/initramfs
	arch_boot_cd_add $isofsdir $kernelver "$boot_title (${initrd##*/})" \
		/boot/$kernel /boot/$initrd
      fi
    done
  done
done

arch_boot_cd_post $isofsdir
