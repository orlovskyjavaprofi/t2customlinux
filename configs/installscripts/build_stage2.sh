# --- T2-COPYRIGHT-BEGIN ---
# t2/target/share/install/build_stage2.sh
# Copyright (C) 2004 - 2026 The T2 SDE Project
# SPDX-License-Identifier: GPL-2.0
# --- T2-COPYRIGHT-END ---

. $base/misc/target/functions.in

set -e

disksdir="$build_toolchain"

echo_header "Creating 2nd stage filesystem:"
rm -rf $disksdir/2nd_stage*
mkdir -p $disksdir/2nd_stage; cd $disksdir/2nd_stage

# package map to include
package_map="00-dirtree zlib          parted             cryptsetup
xfsprogs           dosfstools         jfsutils           btrfs-progs
e2fsprogs          reiserfsprogs      reiser4progs       genromfs
openzfs            pcre               pcre2
popt               raidtools          mdadm
lvm                lvm2               device-mapper      libaio
dump               eject              disktype           mac-fdisk
libnvme            nvme-cli           hdparm             sgutils
smartmontools      stressapptest      xxhash
ncurses            readline           libgpg-error       libgcrypt
bash               attr               acl                findutils
mktemp             coreutils          pciutils           libcap
dialog             grep               sed                less
gzip               bzip2              xz                 zstd
lzo                lzop               lz4                keyutils
tar                gawk               cpio
udev               eudev              lrzsz              rsync
sysvinit           shadow             util-linux
net-tools          procps             psmisc             sysklogd
pciutils           portmap            inih               liburcu
netkit-base        netkit-telnet      netkit-tftp        netcat
sysfiles           libpcap            iptables           tcp_wrappers
stone              rocknet            kexec-tools        memtest86
kbd                ntfsprogs          libol              memtester
openssl            openssh            ed
iproute2           libmnl             libtirpc           watchdog"

# TODO: a global multilib package multiplexer that allows distrinct control
#       and avoids such hacks ...
if [ "$SDECFG_POWERPC64_32" = 1 -o "$SDECFG_SPARC64_32BIT" = 1 ]; then
	package_map="$package_map ${SDECFG_LIBC}32"
else
	package_map="$package_map ${SDECFG_LIBC}"
fi

pkginstalled mine && packager=mine || packager=bize
package_map=" $(echo "$packager $package_map" | tr '\n' ' ' | tr '\t' ' ' | tr -s ' ') "

echo_status "Copying files:"
for pkg in `grep '^X ' $base/config/$config/packages | cut -d ' ' -f 5`; do
	# include the package?
	#echo maybe $pkg >&2
	if [ "${package_map/ $pkg /}" != "$package_map" ]; then
		cut -d ' ' -f 2 $build_root/var/adm/flists/$pkg
	fi
done | (
	# quick and dirty filter
	grep  -v -e 'lib[^/]*/[^/]*\.\(a\|la\|o\)$' \
	         -e 'var/\(adm\|games\|mail\|opt\)' \
	         -e 'usr/\(local\|doc\|man\|info\|games\|share\|include\|src\)' \
	         -e 'usr/.*-linux-gnu' -e '/gconv/' -e '/locale/' -e '/pkgconfig/' \
	         -e 'bin/.*-config' \
	         -e 'bin/install' -e 'bin/openssl' -e 'bin/localedef' \
	         -e '/init.d/' -e '/rc.d/' -e 'etc/conf' -e 'etc/mtab' -e 'etc/services'
	# TODO: usr/lib/*/
) > ../2nd_stage.files

# some more stuff
cut -d ' ' -f 2 $build_root/var/adm/flists/{kbd,pciutils,ncurses} |
grep -e 'usr/share/terminfo/.*/\(ansi\|linux\|screen\|tmux\|vt102\|vt220\|xterm\|xterm-color\)' \
     -e 'usr/share/kbd/keymaps/i386/\(include\|azerty\|qwertz\|qwerty\)' \
     -e 'usr/share/kbd/keymaps/include' \
     -e 'usr/share/.*pci.ids' \
 >> ../2nd_stage.files

copy_with_list_from_file $build_root $PWD $PWD/../2nd_stage.files

copy_and_parse_from_source $base/target/share/install/rootfs $PWD

echo_status "Creating usability sym-links:"
[ ! -e usr/bin/vi -a -e usr/bin/nvi ] && ln -s nvi usr/bin/vi
[ ! -e usr/bin/emacs -a -e usr/bin/zile ] && ln -s zile usr/bin/emacs
[ -e usr/sbin/stone ] && ln -s stone usr/sbin/install

[ "$SDECFG_CROSSBUILD" != 1 ] && (chroot . /sbin/ldconfig || true)

echo '$STONE install' > etc/stone.d/default.sh

cd $disksdir/

echo_header "Creating 2nd_stage_small filesystem:"
mkdir -p 2nd_stage_small; cd 2nd_stage_small

mkdir -p share {,usr/}{,s}bin

# we re-use some of the initrd files, too!
progs="agetty sh bash cat cp date dd df dmesg ifconfig ln ls $packager mkdir \
       mkswap mount mv rm reboot route sleep swapoff swapon sync umount cut \
       setsid eject chmod chroot grep halt rmdir init shutdown uname killall5 \
       install stone tar mktemp sort fold sed wipefs blockdev blkid disktype \
       login-shell stat gzip mkfs.ext3 mkfs.fat dialog stty \
       head tail wc fmt"

progs="$progs parted fdisk sfdisk"
[[ $arch = powerpc* ]] && progs="$progs mac-fdisk pdisk"

[ $packager = mine ] && progs="$progs gasgui"
[ $packager = bize ] && progs="$progs md5sum"

for x in $progs; do
	fn=
	for f in ../2nd_stage/{,usr/}{s,}bin/$x; do
		[ -e $f ] && fn=${f#../2nd_stage/} && break
	done

	if [ "$fn" ]; then
		mv ../2nd_stage/$fn $fn
	else
		echo_error "\`- Program not found: $x"
	fi
done

echo_status "Moving the required libraries:"
found=1
while [ $found = 1 ]; do
	found=0
	for x in ../2nd_stage/{,usr/}lib{64,}; do
		for y in $(cd $x 2>/dev/null && ls *.so.* 2>/dev/null); do
			dir=${x#../2nd_stage/}
			# TODO: maybe use readelf, too?
			if [ ! -e $dir/$y ] &&
			   grep -q $y {s,}bin/* usr/{s,}bin/* lib{64,}/* 2>/dev/null
			then
				echo_status "\`- Found $dir/$y"
				mkdir -p $dir
				xx=$x # save for update in symlink loop
				while z=`readlink $x/$y`; [ "$z" ]; do
					echo "	$dir/$y SYMLINKS to $z"
					mv $x/$y $dir/
					[[ $z = /* ]] && x=../2nd_stage/
					y=$z
				done
				# if multiple symlinks, might have been moved already
				[ -e $x/$y ] && mv $x/$y $dir/
				x=$xx
				found=1
			fi
		done
	done
done

#
echo_status "Moving additional files:"
mkdir -p etc/SDE-CONFIG
mv ../2nd_stage/etc/SDE-CONFIG/config etc/SDE-CONFIG/
mkdir -p etc/stone.d
for i in gui_text gui_dialog mod_install mod_packages mod_gas default; do
	mv ../2nd_stage/etc/stone.d/$i.sh etc/stone.d
done
mkdir -p usr/share/terminfo/{l,x}
mv ../2nd_stage/usr/share/terminfo/l/linux usr/share/terminfo/l/
mv ../2nd_stage/usr/share/terminfo/x/xterm-color usr/share/terminfo/x/
mv ../2nd_stage/root root

echo_status "Removing shared libraries already in initrd:"

# remove libs already in the regular initrd, for each available kernel:
for x in `egrep 'X .* KERNEL .*' $base/config/$config/packages |
          cut -d ' ' -f 5`; do
  kernel=${x/_*/}
  for kernelver in `sed -n "s,.*boot/kconfig.,,p" $build_root/var/adm/flists/$kernel`; do
    initrd="minird-$kernelver"
    kernelimg=`ls $build_root/boot/vmlinu?-$kernelver`
    kernelimg=${kernelimg##*/}

    rm -rf $disksdir/2nd_stage_initrd; mkdir -p $disksdir/2nd_stage_initrd
    zstd -d < $isofsdir/boot/$initrd |
	cpio -iv -D $disksdir/2nd_stage_initrd 2> $disksdir/2nd_stage_initrd.files
    grep -e "lib.*\.so" -e bin/ $disksdir/2nd_stage_initrd.files | grep -v "libcap" |
    while read exe; do
      for fn in ../2nd_stage{,_small}/$exe; do
        if [ -e $fn -o -L $fn ]; then
          if [[ $fn = *bin* ]]; then
	    # only delete bin/* if different, e.g. embutils replacement
	    if [ -L $fn ] || cmp -s $disksdir/2nd_stage_initrd/$exe $fn
	    then rm -vf $fn; fi
	  else
	    rm -vf $fn
	  fi
	fi
      done
    done
  done
done

fix_missing_libs() {
    echo_status "Attempting to fix missing library dependencies..."
    # The binaries are looking for libraries that were likely in 2nd_stage 
    # but not moved to 2nd_stage_small.
    find ../2nd_stage/ -name "libfdisk.so.*" -o -name "libsmartcols.so.*" | while read lib; do
        target_dir="./lib64"
        [ ! -d "$target_dir" ] && target_dir="./lib"
        mkdir -p "$target_dir"
        cp -ad "$lib" "$target_dir/"
        echo_status "Copied $lib to $target_dir"
    done
}

check_library_dependencies() {
    echo_status "Validating library dependencies for all binaries..."
    
    local bin_dirs=("./bin" "./usr/bin" "./sbin" "./usr/sbin")
    local total_errors=0
    
    # You must loop through the directories defined in bin_dirs
    for dir in "${bin_dirs[@]}"; do
        if [ -d "$dir" ]; then
            for bin in "$dir"/*; do
                if [ -f "$bin" ] && [ -x "$bin" ] && file "$bin" | grep -q "ELF"; then
                    
                    missing=$(ldd "$bin" 2>/dev/null)
                    if [ $? -ne 0 ]; then
                        echo_error "WARNING: Could not run ldd on $bin. Is the dynamic linker set?"
                        continue 
                    fi

                    if echo "$missing" | grep -q "not found"; then
                        echo_error "CRITICAL: Binary $bin has missing dependencies:"
                        echo "$missing" | grep "not found"
                        total_errors=$((total_errors + 1))
                    fi
                fi
            done
        fi
    done

    if [ "$total_errors" -gt 0 ]; then
        echo_error "Audit Complete: Found $total_errors binaries with missing dependencies."
        return 1 
    else
        echo_status "All binary dependencies resolved."
        return 0
    fi
}

check_libcap_availability() {
    echo_status "Validating libcap presence..."
    
    # Define locations to check
    local search_paths=("./lib" "./lib64" "./usr/lib" "./usr/lib64")
    local found=0

    for path in "${search_paths[@]}"; do
        if [ -f "$path/libcap.so.2" ] || [ -L "$path/libcap.so.2" ]; then
            echo_status "Found libcap.so.2 in $path"
            found=1
            break
        fi
    done

    if [ "$found" -eq 0 ]; then
        echo_error "CRITICAL: libcap.so.2 not found in any standard library path!"
        echo_error "Build stopped. Check build logs for libcap compilation errors."
        exit 1
    fi
}

# validation checks
fix_missing_libs
check_libcap_availability
check_library_dependencies

echo_status "Creating stage2 archives"

# Define the absolute paths to your staging directories
STAGING_SMALL="$disksdir/2nd_stage_small"
STAGING_EXT="$disksdir/2nd_stage"

# Create archives using absolute paths, ensuring we are in the right place
if [ -d "$STAGING_SMALL" ]; then
    (cd "$STAGING_SMALL" && find . -type f | tar -cf- --no-recursion --files-from=-) | zstd -14 -T0 > "$isofsdir/stage2.tar.zst"
else
    echo_error "Directory $STAGING_SMALL not found! Archive creation failed."
    exit 1
fi

if [ -d "$STAGING_EXT" ]; then
    (cd "$STAGING_EXT" && find . -type f | tar -cf- --no-recursion --files-from=-) | zstd -18 -T0 > "$isofsdir/stage2ext.tar.zst"
else
    echo_error "Directory $STAGING_EXT not found! Archive creation failed."
    exit 1
fi