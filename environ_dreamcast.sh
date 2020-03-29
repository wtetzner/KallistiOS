# KallistiOS environment variable settings. These are the shared pieces
# for the Dreamcast(tm) platform.

export KOS_CFLAGS="${KOS_CFLAGS} -ml -m4-single-only -ffunction-sections -fdata-sections"
export KOS_AFLAGS="${KOS_AFLAGS} -little"

if [ x${KOS_SUBARCH} = xnaomi ]; then
	export KOS_LDFLAGS="${KOS_LDFLAGS} -ml -m4-single-only -Wl,-Ttext=0x8c020000 -Wl,--gc-sections"
	export KOS_LD_SCRIPT="-T${KOS_BASE}/utils/ldscripts/shlelf-naomi.xc"
else
	export KOS_LDFLAGS="${KOS_LDFLAGS} -ml -m4-single-only -Wl,-Ttext=0x8c010000 -Wl,--gc-sections"
	export KOS_LD_SCRIPT="-T${KOS_BASE}/utils/ldscripts/shlelf.xc"
fi

# If we're building for DC, we need the ARM compiler paths as well.
if [ x${KOS_ARCH} = xdreamcast ]; then
	export DC_ARM_CC="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-gcc"
	export DC_ARM_AS="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-as"
	export DC_ARM_AR="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-ar"
	export DC_ARM_OBJCOPY="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-objcopy"
	export DC_ARM_LD="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-ld"
	export DC_ARM_CFLAGS="-mcpu=arm7di -Wall -O2 -fno-strict-aliasing -Wl,--fix-v4bx -Wa,--fix-v4bx"
	export DC_ARM_AFLAGS="-mcpu=arm7di --fix-v4bx"
fi
