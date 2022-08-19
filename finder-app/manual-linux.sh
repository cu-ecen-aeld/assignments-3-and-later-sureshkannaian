#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

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

    # TODO: Add your kernel build steps here

    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} clean
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs  

fi

echo "********************************************************************"
echo "********************************************************************"
echo "********************************************************************"
echo "Adding the Image in outdir"
echo "********************************************************************"
echo "********************************************************************"
echo "********************************************************************"

cp ${OUTDIR}/linux-stable/arch/arm64/boot/Image ${OUTDIR}/

echo "********************************************************************"
echo "********************************************************************"
echo "********************************************************************"
echo "Creating the staging directory for the root filesystem"
echo "********************************************************************"
echo "********************************************************************"
echo "********************************************************************"

cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir rootfs
cd rootfs
mkdir bin sbin lib home etc lib64 dev proc sys tmp
mkdir -p usr/bin usr/sbin usr/lib var/log


echo "********************************************************************"
echo "********************************************************************"
echo "********************************************************************"
echo "Installing modules from kernel to rootfs"
echo "********************************************************************"
echo "********************************************************************"
echo "********************************************************************"


cd "${OUTDIR}/linux-stable"
make  HOSTCC=gcc-9 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} INSTALL_MOD_PATH="${OUTDIR}/rootfs" INSTALL_MOD_STRIP=1 modules_install

echo "********************************************************************"
echo "********************************************************************"
echo "********************************************************************"
echo "Getting busybox"
echo "********************************************************************"
echo "********************************************************************"
echo "********************************************************************"

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
    chmod +x busybox
    cd busybox
fi



echo "********************************************************************"
echo "********************************************************************"
echo "********************************************************************"
echo "Installing busybox"
echo "********************************************************************"
echo "********************************************************************"
echo "********************************************************************"

# TODO: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX="${OUTDIR}/rootfs" install
cd "${OUTDIR}/rootfs"

#echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"


echo "********************************************************************"
echo "********************************************************************"
echo "********************************************************************"
echo "Adding library dependencies to lib"
echo "********************************************************************"
echo "********************************************************************"
echo "********************************************************************"
# TODO: Add library dependencies to rootfs
SYSROOT=`${CROSS_COMPILE}gcc --print-sysroot`
cp -a "${SYSROOT}"/lib/* lib/
cp -a "${SYSROOT}"/lib64 .


echo "********************************************************************"
echo "********************************************************************"
echo "********************************************************************"
echo "Making device nodes"
echo "********************************************************************"
echo "********************************************************************"
echo "********************************************************************"
# TODO: Make device nodes
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/console c 5 1

echo "********************************************************************"
echo "********************************************************************"
echo "********************************************************************"
echo "Building write utility and copying scripts"
echo "********************************************************************"
echo "********************************************************************"
echo "********************************************************************"
# TODO: Clean and build the writer utility
cd "${FINDER_APP_DIR}"
make clean
make HOSTCC=gcc-9 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp -a writer finder.sh finder-test.sh autorun-qemu.sh "${OUTDIR}/rootfs/home"
mkdir "${OUTDIR}/rootfs/home/conf"
cp -a conf/username.txt "${OUTDIR}/rootfs/home/conf"


echo "********************************************************************"
echo "********************************************************************"
echo "********************************************************************"
echo "Changing rootfs owner"
echo "********************************************************************"
echo "********************************************************************"
echo "********************************************************************"
# TODO: Chown the root directory
cd "${OUTDIR}/rootfs"
sudo chown -R root:root *


echo "********************************************************************"
echo "********************************************************************"
echo "********************************************************************"
echo "creating initramsfs.cpio"
echo "********************************************************************"
echo "********************************************************************"
echo "********************************************************************"
# TODO: Create initramfs.cpio.gz
echo "Starting initramfs.cpio.gz"
cd "$OUTDIR/rootfs"
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio && cd ..
gzip -f initramfs.cpio
echo "Completed initramfs.cpio.gz"
