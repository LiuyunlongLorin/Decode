#!/bin/sh

NDK=$1
SYSROOT=$NDK/platforms/android-21/arch-arm
TOOLCHAIN=$NDK/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64

ISYSROOT=$NDK/sysroot
ASM=$ISYSROOT/usr/include/arm-linux-androideabi

function build_one
{
echo "NDK PATH: $NDK"
echo "begin ffmpeg configure"

./configure \
    --prefix=$PREFIX \
    --enable-static \
    --enable-network \
    --enable-asm \
    --enable-inline-asm \
    --enable-neon \
    --disable-shared \
    --disable-debug \
    --disable-doc \
	--disable-linux-perf\
    --enable-avresample \
    --disable-ffmpeg \
    --disable-ffplay \
    --disable-ffprobe \
    --disable-muxers \
    --disable-ffserver \
    --disable-avdevice \
    --disable-symver \
    --cross-prefix=$TOOLCHAIN/bin/arm-linux-androideabi- \
    --target-os=linux \
    --arch=arm \
    --sysroot=$SYSROOT \
    --enable-cross-compile \
    --extra-cflags="-fpic $ADDI_CFLAGS -isysroot $ISYSROOT -I$ASM -D__ANDROID_API__=21" \
    --extra-ldflags="-Wl,-rpath-link=$SYSROOT/usr/lib -L$SYSROOT/usr/lib" 

echo "already configured"
make clean
make -j32
make install
}
CPU=arm
PREFIX=$(pwd)/android/$CPU 
CURDIR=$(pwd)
ADDI_CFLAGS="-march=armv7-a -mfloat-abi=softfp -mfpu=neon"
build_one
