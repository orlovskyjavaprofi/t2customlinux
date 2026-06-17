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

[ "$boot_title" ] || boot_title="Custom T2 Linux Installation - build by Orlovsky Consulting GbR"

. $base/misc/target/initrd.in
. $base/misc/target/boot.in

cd $build_toolchain

# Additional initrd overlay
#
rm -rf initramfs
mkdir -p initramfs/{,usr/}{,s}bin

copy_with_libs "$build_root/bin/tar" "initramfs/bin/"
copy_with_libs "$build_root/bin/df" "initramfs/bin/"
copy_with_libs "$build_root/bin/readlink" "initramfs/bin/"
copy_with_libs "$build_root/bin/rmdir" "initramfs/bin/"
copy_with_libs "$build_root/bin/lsblk" "initramfs/bin/"
copy_with_libs "$build_root/bin/dmesg" "initramfs/bin/"
copy_with_libs "$build_root/usr/bin/du" "initramfs/usr/bin/"
copy_with_libs "$build_root/usr/bin/curl" "initramfs/usr/bin/"
copy_with_libs "$build_root/usr/bin/zstd" "initramfs/usr/bin/"
copy_with_libs "$build_root/usr/bin/unzstd" "initramfs/usr/bin/"
copy_with_libs "$build_root/usr/bin/fget" "initramfs/usr/bin/"
copy_with_libs "$build_root/usr/bin/wget" "initramfs/usr/bin/"
copy_with_libs "$build_root/usr/bin/tmux" "initramfs/usr/bin/"
copy_with_libs "$build_root/usr/bin/man" "initramfs/usr/bin/"
copy_with_libs "$build_root/usr/bin/ifconfig" "initramfs/usr/bin/"
copy_with_libs "$build_root/usr/man" "initramfs/usr/"
copy_with_libs "$build_root/usr/share/man" "initramfs/usr/share"
copy_with_libs "$build_root/usr/doc/man" "initramfs/usr/doc/"
copy_with_libs "$build_root/usr/bin/install" "initramfs/usr/bin/"
copy_with_libs "$build_root/usr/sbin/parted" "initramfs/usr/sbin/"
copy_with_libs "$build_root/usr/sbin/shutdown" "initramfs/usr/sbin/"
copy_with_libs "$build_root/usr/bin/svn" "initramfs/usr/bin/"
copy_with_libs "$build_root/usr/bin/git" "initramfs/usr/bin/"
copy_with_libs "$build_root/usr/bin/killall" "initramfs/usr/bin/"
copy_with_libs "$build_root/sbin/fdisk" "initramfs/sbin/"
copy_with_libs "$build_root/sbin/cfdisk" "initramfs/sbin/"
copy_with_libs "$build_root/sbin/ip" "initramfs/sbin/"
copy_with_libs "$build_root/sbin/ping" "initramfs/sbin/"
copy_with_libs "$build_root/usr/bin/nano" "initramfs/usr/bin/"
copy_with_libs "$build_root/bin/grep" "initramfs/bin/"

mkdir -p initramfs/lib64 initramfs/lib
mkdir -p initramfs/usr/lib64/
cp -a $build_root/usr/lib64/libcap.so.2* initramfs/usr/lib64/
ln -sf /usr/lib64/libcap.so.2 initramfs/lib64/libcap.so.2
ln -sf /usr/lib64/libcap.so.2 initramfs/lib/libcap.so.2

#Add here missing libs if one of packages requires a lib
copy_with_libs "$build_root/usr/lib64/libmagic.so.1" "initramfs/usr/lib64/"
copy_with_libs "$build_root/usr/lib64/libmagic.so.1.0.0" "initramfs/usr/lib64/"
copy_with_libs "$build_root/usr/lib64/libmagic.so" "initramfs/usr/lib64/"
copy_with_libs "$build_root/usr/lib64/libevent_core-2.1.so.7.0.1" "initramfs/usr/lib64/"
copy_with_libs "$build_root/usr/lib64/libevent_core-2.1.so.7" "initramfs/usr/lib64/"
copy_with_libs "$build_root/usr/lib64/libpipeline.so.1" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libpipeline.so" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libpipeline.so.1.5.8" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libwget.a" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libwget.so" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libwget.so.4" "initramfs/usr/lib64"                                      
copy_with_libs "$build_root/usr/lib64/libwget.so.4.0.0" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libelf.a" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libelf-0.195.so" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libelf.so.1" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libelf.so" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libgdbm.a" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libgdbm.so" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libgdbm.so.6" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libgdbm.so.6.0.0" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libnghttp2.a" "initramfs/usr/lib64"
copy_with_libs "$build_root/usr/lib64/libnghttp2.so" "initramfs/usr/lib64"		                              
copy_with_libs "$build_root/usr/lib64/libnghttp2.so.14" "initramfs/usr/lib64"		                              								  
copy_with_libs "$build_root/usr/lib64/libnghttp2.so.14.29.4" "initramfs/usr/lib64"

# These allow systemctl to know it is acting as 'reboot' or 'poweroff'
echo "echo b > /proc/sysrq-trigger" > "initramfs/bin/reboot"
echo "echo o > /proc/sysrq-trigger" > "initramfs/bin/poweroff"
echo "echo h > /proc/sysrq-trigger" > "initramfs/bin/halt"
chmod +x initramfs/bin/reboot
chmod +x initramfs/bin/poweroff
chmod +x initramfs/bin/halt

# Copy essential stuff 
cp -a $build_root/bin/tar initramfs/bin/

sed '/PANICMARK/Q' $build_root/sbin/initrdinit > initramfs/init
cat $base/target/share/install/init >> initramfs/init
cp -a initramfs/init initramfs/sbin/init
chmod +x initramfs/init
chmod +x initramfs/sbin/init
chmod +x initramfs/bin/tar

mkdir -p "initramfs/etc/"
echo "export LANG=C.UTF-8" > "initramfs/etc/profile"
echo "export LC_ALL=C.UTF-8" >> "initramfs/etc/profile"

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
