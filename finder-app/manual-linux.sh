#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
GCC_ARM_VERSION=13.3.rel1


if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

if ! mkdir -p ${OUTDIR} 2>/dev/null; then
    echo "Error: Could not create directory path: $OUTDIR" >&2
    exit 1
fi

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    sudo apt-get update && \
    sudo apt-get install -y --no-install-recommends \
        bc u-boot-tools kmod cpio flex bison libssl-dev psmisc && \
    sudo apt-get install -y qemu-system-arm

    wget -O gcc-arm.tar.xz \
    https://developer.arm.com/-/media/Files/downloads/gnu/$GCC_ARM_VERSION/binrel/arm-gnu-toolchain-$GCC_ARM_VERSION-x86_64-aarch64-none-linux-gnu.tar.xz && \
    mkdir -p install && \
    tar x -C install -f gcc-arm.tar.xz && \
    rm -r gcc-arm.tar.xz

    # TODO: Add your kernel build steps here
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs

fi

# echo "Adding the Image in outdir"

cp "${OUTDIR}/linux-stable/arch/arm64/boot/Image" "$OUTDIR/"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p rootfs
cd rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/sbin usr/lib
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
    
else
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install 

# echo "Library dependencies"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
cp -a ${SYSROOT}/lib/ld-linux-aarch64.so.* ${OUTDIR}/rootfs/lib64/
cp -a ${SYSROOT}/lib/ld-linux-aarch64.so.* ${OUTDIR}/rootfs/lib/
cp -a ${SYSROOT}/lib64/libc.so.*  ${OUTDIR}/rootfs/lib64/
cp -a ${SYSROOT}/lib64/libm.so.*  ${OUTDIR}/rootfs/lib64/
cp -a ${SYSROOT}/lib64/libresolv.so.*  ${OUTDIR}/rootfs/lib64/


# TODO: Make device nodes
cd "$OUTDIR/rootfs"
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1


# TODO: Clean and build the writer utility
cd "$FINDER_APP_DIR"
rm -f writer *.o
${CROSS_COMPILE}gcc -g -Wall -Wextra -std=c11 -o writer writer.c


# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp -a finder.sh ${OUTDIR}/rootfs/home/
cp -a ../conf ${OUTDIR}/rootfs/home
cp -a finder-test.sh ${OUTDIR}/rootfs/home/
cp -a ${FINDER_APP_DIR}/writer ${OUTDIR}/rootfs/home/
cp -a autorun-qemu.sh ${OUTDIR}/rootfs/home/



# TODO: Chown the root directory
cd "$OUTDIR/rootfs"
sudo chown -R root:root *


# TODO: Create initramfs.cpio.gz
find . | cpio  -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd "$OUTDIR"
gzip -f initramfs.cpio

