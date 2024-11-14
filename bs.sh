export ARCH=arm64 && export CROSS_COMPILE=aarch64-linux-gnu-

make distclean -j16
make defconfig
make -j16
