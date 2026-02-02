# CMakeLists.txt for lxa.rom (m68k-amigaos)
# This builds the Amiga Kickstart ROM replacement

# Only build if we have the cross-compiler
if(NOT M68K_GCC)
    return()
endif()

# Toolchain paths
set(M68K_PREFIX "/opt/amiga")
set(M68K_AS "${M68K_PREFIX}/bin/m68k-amigaos-as")
set(M68K_LD "${M68K_PREFIX}/bin/m68k-amigaos-ld")
set(M68K_OBJCOPY "${M68K_PREFIX}/bin/m68k-amigaos-objcopy")
set(M68K_OBJDUMP "${M68K_PREFIX}/bin/m68k-amigaos-objdump")

# Compiler flags for ROM
set(ROM_CFLAGS
    -march=68000
    -mcpu=68000
    -O2
    -Wall
    -Werror
    -fno-builtin
    -mcrt=nix20
    -I${CMAKE_SOURCE_DIR}/src/include
)

# Assembler flags
set(ROM_ASFLAGS
    -march=68000
    -mcpu=68000
    -I${M68K_PREFIX}/m68k-amigaos/ndk-include
)

# Source files
set(ROM_C_SOURCES
    exec.c
    lxa_dos.c
    lxa_utility.c
    lxa_mathffp.c
    lxa_mathtrans.c
    lxa_dev_input.c
    lxa_graphics.c
    lxa_intuition.c
    lxa_layers.c
    lxa_expansion.c
    lxa_icon.c
    lxa_diskfont.c
    lxa_translator.c
    lxa_locale.c
    lxa_gadtools.c
    lxa_workbench.c
    lxa_asl.c
    lxa_dev_console.c
    lxa_dev_timer.c
    lxa_dev_clipboard.c
    lxa_dev_audio.c
    lxa_dev_gameport.c
    util.c
    amath.c
)

set(ROM_ASM_SOURCES
    romhdr.s
    exceptions.s
)

# Object files - ASM first (romhdr.o and exceptions.o must come first!)
set(ROM_OBJECTS)

# Assemble ASM sources FIRST (order matters for ROM header)
foreach(src ${ROM_ASM_SOURCES})
    get_filename_component(name ${src} NAME_WE)
    set(obj "${CMAKE_CURRENT_BINARY_DIR}/${name}.o")
    add_custom_command(
        OUTPUT ${obj}
        COMMAND ${M68K_AS} ${ROM_ASFLAGS} -o ${obj} ${CMAKE_CURRENT_SOURCE_DIR}/${src}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${src}
        COMMENT "Assembling ${src}"
    )
    list(APPEND ROM_OBJECTS ${obj})
endforeach()

# Compile C sources
foreach(src ${ROM_C_SOURCES})
    get_filename_component(name ${src} NAME_WE)
    set(obj "${CMAKE_CURRENT_BINARY_DIR}/${name}.o")
    add_custom_command(
        OUTPUT ${obj}
        COMMAND ${M68K_GCC} -c ${ROM_CFLAGS} -o ${obj} ${CMAKE_CURRENT_SOURCE_DIR}/${src}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${src}
        COMMENT "Compiling ${src}"
    )
    list(APPEND ROM_OBJECTS ${obj})
endforeach()

# Link the ROM hunk
set(ROM_HUNK "${CMAKE_CURRENT_BINARY_DIR}/lxa.rom.hunk")
set(ROM_BIN "${CMAKE_CURRENT_BINARY_DIR}/lxa.rom.bin")
set(ROM_FILE "${CMAKE_CURRENT_BINARY_DIR}/lxa.rom")

# Linker script
set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/rom.ld")

add_custom_command(
    OUTPUT ${ROM_HUNK}
    COMMAND ${M68K_LD} -T ${LINKER_SCRIPT} -o ${ROM_HUNK} -Map ${CMAKE_CURRENT_BINARY_DIR}/lxa.rom.map ${ROM_OBJECTS}
    DEPENDS ${ROM_OBJECTS} ${LINKER_SCRIPT}
    COMMENT "Linking ROM hunk"
)

# Convert to binary
add_custom_command(
    OUTPUT ${ROM_BIN}
    COMMAND ${M68K_OBJCOPY} -O binary --only-section=.text ${ROM_HUNK} ${ROM_BIN}
    DEPENDS ${ROM_HUNK}
    COMMENT "Converting ROM to binary"
)

# Build final ROM with romtool
if(ROMTOOL)
    add_custom_command(
        OUTPUT ${ROM_FILE}
        COMMAND ${ROMTOOL} build -o ${ROM_FILE} -t kick -s 256 ${ROM_BIN}
        DEPENDS ${ROM_BIN}
        COMMENT "Building ROM image"
    )
    
    # Custom target to build the ROM
    add_custom_target(lxa.rom ALL DEPENDS ${ROM_FILE})
    
    # Install the ROM
    install(FILES ${ROM_FILE}
        DESTINATION share/lxa
        RENAME lxa.rom
    )
else()
    # Without romtool, just create a custom target for the binary
    add_custom_target(lxa.rom ALL DEPENDS ${ROM_BIN})
    install(FILES ${ROM_BIN}
        DESTINATION share/lxa
        RENAME lxa.rom
    )
endif()

# Disassembly target
add_custom_target(rom-disasm
    COMMAND ${M68K_OBJDUMP} -s -d -x ${ROM_HUNK}
    DEPENDS ${ROM_HUNK}
    COMMENT "Disassembling ROM"
)
