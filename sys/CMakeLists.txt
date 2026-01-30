# CMakeLists.txt for lxa system commands (m68k-amigaos)
# This builds all the commands in sys/C and sys/System

# Only build if we have the cross-compiler
if(NOT M68K_GCC)
    return()
endif()

# Toolchain paths
set(M68K_PREFIX "/opt/amiga")
set(M68K_GCC_BIN "${M68K_PREFIX}/bin/m68k-amigaos-gcc")

# Common compiler flags for all commands
set(COMMAND_CFLAGS
    -march=68000
    -mcpu=68000
    -O2
    -Wall
    -I${M68K_PREFIX}/m68k-amigaos/include
    -I${M68K_PREFIX}/m68k-amigaos/sys-include
)

# Common linker flags
set(COMMAND_LDFLAGS -mcrt=nix20)

# Create output directories
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/C)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/System)

# === SYS:C Commands ===
set(C_COMMANDS
    assign
    avail
    break
    changetaskpri
    delete
    dir
    makedir
    run
    stack
    status
    type
)

foreach(cmd ${C_COMMANDS})
    set(src "${CMAKE_CURRENT_SOURCE_DIR}/C/${cmd}.c")
    set(output "${CMAKE_CURRENT_BINARY_DIR}/C/${cmd}")
    
    add_custom_command(
        OUTPUT ${output}
        COMMAND ${M68K_GCC_BIN} ${COMMAND_CFLAGS} ${COMMAND_LDFLAGS} -o ${output} ${src}
        DEPENDS ${src}
        COMMENT "Building C:${cmd}"
    )
    
    list(APPEND ALL_COMMANDS ${output})
    
    # Install to system template
    install(FILES ${output}
        DESTINATION share/lxa/System/C
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
endforeach()

# === SYS:System Commands ===
set(SYSTEM_COMMANDS
    Shell
)

foreach(cmd ${SYSTEM_COMMANDS})
    set(src "${CMAKE_CURRENT_SOURCE_DIR}/System/${cmd}.c")
    set(output "${CMAKE_CURRENT_BINARY_DIR}/System/${cmd}")
    
    add_custom_command(
        OUTPUT ${output}
        COMMAND ${M68K_GCC_BIN} ${COMMAND_CFLAGS} ${COMMAND_LDFLAGS} -o ${output} ${src}
        DEPENDS ${src}
        COMMENT "Building System:${cmd}"
    )
    
    list(APPEND ALL_COMMANDS ${output})
    
    # Install to system template
    install(FILES ${output}
        DESTINATION share/lxa/System/System
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
endforeach()

# Custom target to build all system commands
add_custom_target(lxa-sys ALL DEPENDS ${ALL_COMMANDS})

# Add dependency to ensure commands are built
add_dependencies(lxa-sys lxa.rom)
