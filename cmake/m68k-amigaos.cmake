# CMake Toolchain File for m68k-amigaos-gcc
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/m68k-amigaos.cmake ..

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR m68k)

# Set the AmigaOS toolchain prefix
if(NOT DEFINED AMIGA_TOOLCHAIN_PREFIX)
    set(AMIGA_TOOLCHAIN_PREFIX "/opt/amiga")
endif()

# Specify the cross compiler
set(CMAKE_C_COMPILER "${AMIGA_TOOLCHAIN_PREFIX}/bin/m68k-amigaos-gcc")
set(CMAKE_CXX_COMPILER "${AMIGA_TOOLCHAIN_PREFIX}/bin/m68k-amigaos-g++")
set(CMAKE_ASM_COMPILER "${AMIGA_TOOLCHAIN_PREFIX}/bin/m68k-amigaos-as")
set(CMAKE_AR "${AMIGA_TOOLCHAIN_PREFIX}/bin/m68k-amigaos-ar")
set(CMAKE_RANLIB "${AMIGA_TOOLCHAIN_PREFIX}/bin/m68k-amigaos-ranlib")
set(CMAKE_OBJCOPY "${AMIGA_TOOLCHAIN_PREFIX}/bin/m68k-amigaos-objcopy")
set(CMAKE_OBJDUMP "${AMIGA_TOOLCHAIN_PREFIX}/bin/m68k-amigaos-objdump")
set(CMAKE_STRIP "${AMIGA_TOOLCHAIN_PREFIX}/bin/m68k-amigaos-strip")

# Compiler flags for m68k-amigaos
set(CMAKE_C_FLAGS_INIT "-march=68000 -mcpu=68000 -O2 -Wall")
set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -I${AMIGA_TOOLCHAIN_PREFIX}/m68k-amigaos/include")
set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -I${AMIGA_TOOLCHAIN_PREFIX}/m68k-amigaos/sys-include")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS_INIT "-mcrt=nix20")

# Don't use RPATH for cross-compiled binaries
set(CMAKE_SKIP_RPATH TRUE)

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH "${AMIGA_TOOLCHAIN_PREFIX}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Set the library architecture
set(CMAKE_LIBRARY_ARCHITECTURE "m68k-amigaos")

# Custom target properties
set(CMAKE_C_CREATE_STATIC_LIBRARY "<CMAKE_AR> rcs <TARGET> <OBJECTS>")

# Disable compiler detection
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)
set(CMAKE_ASM_COMPILER_FORCED TRUE)
