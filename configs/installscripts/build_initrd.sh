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

copy_with_libs() {
    local src=$1
    local dest=$2
    cp -a "$src" "$dest/"
    
    # 1. Copy libraries
    for lib in $(ldd "$src" | grep "=> /" | awk '{print $3}'); do
        mkdir -p "initramfs/$(dirname "$lib")"
        cp -an "$lib" "initramfs/$(dirname "$lib")"
    done
    
    # 2. CRITICAL: Copy the dynamic loader (ld-linux)
    # The loader is often not listed by ldd, but it is required.
    local loader=$(ldd "$src" | grep "ld-linux" | awk '{print $1}')
    if [ -n "$loader" ]; then
        # If the loader is just a filename, find its full path
        local loader_path=$(find /lib64 /lib -name "$loader" | head -n 1)
        if [ -n "$loader_path" ]; then
            mkdir -p "initramfs/$(dirname "$loader_path")"
            cp -an "$loader_path" "initramfs/$(dirname "$loader_path")"
        fi
    fi
}

copy_with_libs "$build_root/usr/lib64/libpcre2-8.so.0" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libpcre2-8.so" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libpcre2-16.so" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libpcre2-16.so.0" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libpcre2-32.so.0" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libpcre2-32.so" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libpcre2-posix.so" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libpcre2-posix.so.3" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libpcre2-32.so.0.15.0" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libpcre2-posix.so.3.0.7" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libpcre2-8.so.0.15.0" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libpcre2-16.so.0.15.0" "initramfs/usr/lib64"

copy_with_libs "$build_root/usr/lib64/libfdisk.so" "initramfs/usr/lib64"
copy_with_libs "$build_root/lib64/libfdisk.so.1" "initramfs/lib64"
copy_with_libs "$build_root/lib64/libfdisk.so.1.1.0" "initramfs/lib64"

copy_with_libs "$build_root/usr/lib64/libparted.so.2.0.5" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libparted.so.2" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libparted.so" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libparted.a" "initramfs/usr/lib64"

copy_with_libs "$build_root/usr/lib64/libproc2.so.1" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libproc2.so.1.0.1" "initramfs/usr/lib64"

copy_with_libs "$build_root/usr/lib64/libmagic.so.1" "initramfs/usr/lib64/"
copy_with_libs "$build_root/usr/lib64/libmagic.so.1.0.0" "initramfs/usr/lib64/"
copy_with_libs "$build_root/usr/lib64/libmagic.so" "initramfs/usr/lib64/"

copy_with_libs "$build_root/usr/lib64/libext2fs.a" "initramfs/usr/lib64/"
ln -sf /usr/lib64/libext2fs.a initramfs/lib/libext2fs.a
ln -sf /usr/lib64/libext2fs.a initramfs/lib64/libext2fs.a
copy_with_libs "$build_root/lib64/libext2fs.so" "initramfs/lib64"
copy_with_libs "$build_root/lib64/libext2fs.so.2" "initramfs/lib64"
copy_with_libs "$build_root/lib64/libext2fs.so.2.4" "initramfs/lib64"

copy_with_libs "$build_root/usr/lib64/liburcu.a" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/liburcu.so" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/liburcu.so.8" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/liburcu.so.8.1.0" "initramfs/usr/lib64"

copy_with_libs "$build_root/usr/lib64/liblzo2.a" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/liblzo2.so" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/liblzo2.so.2" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/liblzo2.so.2.0.0" "initramfs/usr/lib64"

copy_with_libs "$build_root/usr/lib64/libelf.a" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libelf-0.195.so" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libelf.so.1" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libelf.so" "initramfs/usr/lib64"

copy_with_libs "$build_root/usr/lib64/libmpfr.so.6" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libmpfr.so.6.2.2" "initramfs/usr/lib64"

copy_with_libs "$build_root/usr/lib64/libmnl.so" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libmnl.so.0.2.0" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libmnl.so.0" "initramfs/usr/lib64"

copy_with_libs "$build_root/lib64/libe2p.so.2" "initramfs/lib64"
copy_with_libs "$build_root/lib64/libe2p.so" "initramfs/lib64"
copy_with_libs "$build_root/lib64/libe2p.so.2.3" "initramfs/lib64"

copy_with_libs "$build_root/usr/lib64/libreadline.so.8" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libreadline.so" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libreadline.so.8.3" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libreadline.a" "initramfs/usr/lib64"

copy_with_libs "$build_root/lib64/libncursesw.so.6.6" "initramfs/lib64"
copy_with_libs "$build_root/lib64/libncursesw.so.6" "initramfs/lib64"
copy_with_libs "$build_root/lib64/libncursesw.so" "initramfs/lib64"

copy_with_libs "$build_root/bin/awk" "initramfs/bin"
copy_with_libs "$build_root/usr/share/awk" "initramfs/usr/share"
copy_with_libs "$build_root/usr/bin/awk" "initramfs/usr/bin"

# Custom tools for power down and or reboot
echo "echo b > /proc/sysrq-trigger" > "initramfs/bin/reboot"
echo "echo o > /proc/sysrq-trigger" > "initramfs/bin/poweroff"
echo "echo o > /proc/sysrq-trigger" > "initramfs/bin/shutdown"
echo "echo h > /proc/sysrq-trigger" > "initramfs/bin/halt"
chmod +x initramfs/bin/reboot
chmod +x initramfs/bin/poweroff
chmod +x initramfs/bin/shutdown
chmod +x initramfs/bin/halt

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
