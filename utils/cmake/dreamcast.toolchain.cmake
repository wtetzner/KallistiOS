# CMake Toolchain file for targeting the Dreamcast or NAOMI with CMake.
#  (c)2023 Falco Girgis, Luke Benstead
#
# This file is to be passed to CMake when compiling a regular CMake project
# to cross-compile for the Dreamcast, using the KOS environment and settings.
#
#     cmake /path/to/src -DCMAKE_TOOLCHAIN_FILE=${KOS_BASE}/kos/utils/cmake/dreamcast.toolchain.cmake
#
#   or alternatively:
#
#     cmake /path/to/src -DCMAKE_TOOLCHAIN_FILE=${KOS_CMAKE_TOOLCHAIN}
#
# The toolchain enables the options required for the compiler to use the 
# special SH4 instructions for sin/cos and invSqrt when in release build
# and disables them in debug builds. Note that even when these options
# are present, they are not activated unless -ffast-math is also provied. 
# Typically you will want to provide a project-level option for enabling
# this flag, which will make the toolchain's SH4 instruction flags also
# take effect.
#
# Frame pointers are enabled in debug builds as these are required for 
# stack traces and GDB. They are disabled in release.
# 
# The original toolchain file was created by Kazade for the Simulant 
# engine who has graciously allowed the rest of the scene to warez it. 
#
# Special thanks to Matt Godbolt of Compiler Explorer, for supporting
# the SH toolchains, which allowed us to figure out the precise magical
# incantations of compiler flags for fast math.  

##### Configure CMake System #####
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)
set(PLATFORM_DREAMCAST TRUE)

##### Configure Cross-Compiler #####
set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_C_COMPILER $ENV{KOS_CC_BASE}/bin/sh-elf-gcc)
set(CMAKE_CXX_COMPILER $ENV{KOS_CC_BASE}/bin/sh-elf-g++)

set(CMAKE_EXECUTABLE_SUFFIX ".elf")

set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

##### Add Platform-Specific #defines #####
ADD_DEFINITIONS(
    -D__DREAMCAST__
    -D_arch_dreamcast
    -D__arch_dreamcast
)

if($ENV{KOS_SUBARCH} MATCHES naomi)
    ADD_DEFINITONS(
        -D__NAOMI__
        -D_arch_sub_naomi
    )
else()
    ADD_DEFINITIONS(-D_arch_sub_pristine)
endif()

##### Configure Build Flags #####
add_compile_options(-ml -m4-single-only -ffunction-sections -fdata-sections)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mfsrra -mfsca -fomit-frame-pointer -g0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mfsrra -mfsca -fomit-frame-pointer -g0")

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DFRAME_POINTERS -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DFRAME_POINTERS -fno-omit-frame-pointer")

set(CMAKE_ASM_FLAGS "")
set(CMAKE_ASM_FLAGS_RELEASE "")

##### Configure Include Directories #####
set(CMAKE_SYSTEM_INCLUDE_PATH "${CMAKE_SYSTEM_INCLUDE_PATH} $ENV{KOS_BASE}/include $ENV{KOS_BASE}/kernel/arch/dreamcast/include $ENV{KOS_BASE}/addons/include $ENV{KOS_BASE}/../kos-ports/include")

INCLUDE_DIRECTORIES(
    $ENV{KOS_BASE}/include
    $ENV{KOS_BASE}/kernel/arch/dreamcast/include
    $ENV{KOS_BASE}/addons/include
    $ENV{KOS_BASE}/../kos-ports/include
)

##### Configure Libraries #####
set(CMAKE_SYSTEM_LIBRARY_PATH "${CMAKE_SYSTEM_LIBRARY_PATH} $ENV{KOS_BASE}/addons/lib/dreamcast $ENV{KOS_PORTS}/lib")

if($ENV{KOS_SUBARCH} MATCHES naomi)
    set(CMAKE_EXE_LINKER_FLAGS " -ml -m4-single-only -Wl,-Ttext=0x8c020000 -Wl,--gc-sections -T$ENV{KOS_BASE}/utils/ldscripts/shlelf-naomi.xc -nodefaultlibs" CACHE INTERNAL "" FORCE)
else()
    set(CMAKE_EXE_LINKER_FLAGS " -ml -m4-single-only -Wl,-Ttext=0x8c010000 -Wl,--gc-sections -T$ENV{KOS_BASE}/utils/ldscripts/shlelf.xc -nodefaultlibs" CACHE INTERNAL "" FORCE)
endif()

LINK_DIRECTORIES(
    $ENV{KOS_BASE}/addons/lib/dreamcast
    $ENV{KOS_PORTS}/lib
)

add_link_options(-L$ENV{KOS_BASE}/lib/dreamcast -L$ENV{KOS_BASE}/addons/lib/dreamcast)
LINK_LIBRARIES(-Wl,--start-group -lstdc++ -lkallisti -lc -lgcc -Wl,--end-group m)
LINK_LIBRARIES(c gcc kallisti stdc++)


