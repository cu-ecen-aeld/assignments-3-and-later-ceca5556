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
    #--------------------------------------
    # deep clean
    echo "deep cleaning kernel build tree"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    
    # configure virtual dev board
    echo "configuring arm dev board simulation"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    
    # build kernal image
    echo "build kernel image"
    make -j8 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    
    # build any kernal modules
    echo "building modules"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    
    # build devicetree
    echo "building devicetree"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
    
    echo "NOTICE:all build kernel build steps complete!"
    #--------------------------------------
fi

# copy kernel image to output directory
echo "NOTICE:Adding the Image in outdir..."
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/
echo "NOTICE:kernel image copied!"


echo "NOTICE:Creating the staging directory for the root filesystem..."
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
#--------------------------------------
# create and cd into rootfs directory 
mkdir ${OUTDIR}/rootfs && cd ${OUTDIR}/rootfs

echo "NOTICE:creating rootfs subdirectories..."
# create all subdirectories
mkdir -p bin dev etc home lib lib64 proc sys sbin tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log
echo "NOTICE:rootfs and subriectories created!"
#--------------------------------------

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    #--------------------------------------
    echo "NOTICE:configuring busybox"
    make distclean
    make defconfig
    #--------------------------------------
else
    cd busybox
fi

# TODO: Make and install busybox
#--------------------------------------
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install
#--------------------------------------

# cd into rootfs
cd ${OUTDIR}/rootfs

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
#--------------------------------------

# copy necessary files from toolchain
TC_SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)

echo "NOTICE:copying dependecies to rootfs..."
cp ${TC_SYSROOT}/lib/ld-linux-aarch64.so.1 lib
cp ${TC_SYSROOT}/lib64/libm.so.6 lib64
cp ${TC_SYSROOT}/lib64/libresolv.so.2 lib64
cp ${TC_SYSROOT}/lib64/libc.so.6 lib64
echo "NOTICE:dependencies copied!"
#--------------------------------------

# TODO: Make device nodes
#--------------------------------------
echo "NOTICE:creating device nodes..."
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1
echo "NOTICE:device nodes created!"
#--------------------------------------

# TODO: Clean and build the writer utility
#--------------------------------------
echo "NOTICE:building writer app..."
cd ${FINDER_APP_DIR}
make clean
make
echo "NOTICE:writer app succesfully built"
#--------------------------------------

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
#--------------------------------------
echo "NOTICE:copying finder related scripts to rootfs/home..."
cp writer ${OUTDIR}/rootfs/home

cp finder.sh ${OUTDIR}/rootfs/home
cp finder-test.sh ${OUTDIR}/rootfs/home
cp -rL conf ${OUTDIR}/rootfs/home

cp autorun-qemu.sh ${OUTDIR}/rootfs/home
echo "NOTICE:files copied!"
#--------------------------------------

# TODO: Chown the root directory
#--------------------------------------
echo "NOTICE:changing owner of rootfs to root"
cd ${OUTDIR}/rootfs
sudo chown -R root:root *
#--------------------------------------

# TODO: Create initramfs.cpio.gz
#--------------------------------------
echo "NOTICE:creating initramfs.cpio"
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
gzip -f ${OUTDIR}/initramfs.cpio
#--------------------------------------