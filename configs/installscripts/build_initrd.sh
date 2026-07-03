#!/bin/bash
# --- T2-COPYRIGHT-BEGIN ---
# t2/target/share/install/build_initrd.sh
# Copyright (C) 2004 - 2026 The T2 SDE Project
# SPDX-License-Identifier: GPL-2.0
# --- T2-COPYRIGHT-END ---

set -e

[ "$boot_title" ] || boot_title="Custom T2 Linux Installation - build by Orlovsky Consulting GbR"

. $base/misc/target/initrd.in
. $base/misc/target/boot.in

cd $build_toolchain

# Additional initrd overlay
#
rm -rf initramfs
mkdir -p initramfs/{,usr/}{,s}bin
# TODO: add gzip ip
cp $build_root/usr/embutils/{tar,readlink,rmdir} initramfs/bin/
cp -a $build_root/usr/bin/{,un}zstd initramfs/usr/bin/
cp $build_root/usr/bin/fget initramfs/bin/

# lib64 symlink → lib
mkdir -p initramfs/usr/share/locale
mkdir -p initramfs/usr/lib/locale
mkdir -p initramfs/usr/lib64
mkdir -p initramfs/etc
mkdir -p initramfs/lib

chroot $build_root localedef -i C -f UTF-8 C.utf8 2>/dev/null || true
cp -a $build_root/usr/lib64/locale/locale-archive initramfs/usr/lib/locale/
cp -a $build_root/usr/share/locale initramfs/usr/share/

# i18n source data (charmaps, locales)
mkdir -p initramfs/usr/share/i18n
cp -a $build_root/usr/share/i18n initramfs/usr/share/

if [ -f initramfs/usr/lib/locale/locale-archive ]; then
    ln -sf /usr/lib/locale initramfs/usr/lib64/locale
	printf 'LANG=C.utf8\nLC_ALL=C.utf8\n' > initramfs/etc/locale.conf
else
    echo "WARNING: locale-archive missing - falling back to C"
    printf 'LANG=C\nLC_ALL=C\n' > initramfs/etc/locale.conf
fi

# Gnome locale
mkdir -p initramfs/opt/gnome/share/locale
cp -a $build_root/opt/gnome/share/locale initramfs/opt/gnome/share/

echo "Copy libcap to initrd image!"
cp -a $build_root/usr/lib64/libcap.so.2* initramfs/usr/lib64/
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
