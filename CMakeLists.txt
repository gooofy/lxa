cmake_minimum_required(VERSION 3.16)
project(lxa VERSION 0.1.0 LANGUAGES C ASM)

# Option to set installation prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/usr" CACHE PATH "Installation prefix")
endif()

# Option to set LXA system data directory
set(LXA_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/lxa" CACHE PATH "LXA system data directory")
set(LXA_DEFAULT_PREFIX "$ENV{HOME}/.lxa" CACHE STRING "Default LXA prefix for user environments")

# Path to romtool for building ROM images
find_program(ROMTOOL romtool
    HINTS $ENV{HOME}/.local/bin $ENV{HOME}/projects/amiga/amitools/.venv/bin
    DOC "ROM image builder tool"
)
if(NOT ROMTOOL)
    message(WARNING "romtool not found. ROM image building will not be available.")
endif()

# Define host and target build directories
set(HOST_BINARY_DIR "${CMAKE_BINARY_DIR}/host")
set(TARGET_BINARY_DIR "${CMAKE_BINARY_DIR}/target")

# Create output directories
file(MAKE_DIRECTORY ${HOST_BINARY_DIR}/bin)
file(MAKE_DIRECTORY ${TARGET_BINARY_DIR}/bin)

# === HOST BUILD (x86_64-linux) ===
# Build the lxa host emulator (runs on Linux)
message(STATUS "Configuring host build (x86_64-linux)...")
add_subdirectory(src/lxa host)

# === TARGET BUILD (m68k-amigaos) ===
# Build the ROM and system commands (runs on Amiga)
# We use ExternalProject to handle cross-compilation cleanly
message(STATUS "Configuring target build (m68k-amigaos)...")

# Check for cross-compiler
find_program(M68K_GCC m68k-amigaos-gcc
    HINTS /opt/amiga/bin
    DOC "m68k-amigaos GCC cross-compiler"
)

if(M68K_GCC)
    message(STATUS "Found m68k-amigaos-gcc: ${M68K_GCC}")
    
    # Add ROM build (cross-compiled)
    add_subdirectory(src/rom target/rom)
    
    # Add system commands (cross-compiled)
    add_subdirectory(sys target/sys)
    
    # Add samples (cross-compiled)
    add_subdirectory(samples target/samples)
else()
    message(WARNING "m68k-amigaos-gcc not found. ROM and system commands will not be built.")
    message(STATUS "Install the Amiga cross-compiler from https://github.com/bebbo/amiga-gcc")
endif()

# === Installation Configuration ===

# Install directories
include(GNUInstallDirs)

# Install lxa binary
install(TARGETS lxa
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install system template from share/lxa directory
if(EXISTS "${CMAKE_SOURCE_DIR}/share/lxa")
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/share/lxa/
        DESTINATION share/lxa
    )
endif()

# Also install the generated commands and ROM to the system template
install(CODE "
    # Copy commands to the installed template after they're built
    if(EXISTS \"${CMAKE_BINARY_DIR}/target/sys/C\")
        file(COPY \"${CMAKE_BINARY_DIR}/target/sys/C/\" 
            DESTINATION \"${CMAKE_INSTALL_PREFIX}/share/lxa/System/C\"
            FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
    endif()
    if(EXISTS \"${CMAKE_BINARY_DIR}/target/sys/System\")
        file(COPY \"${CMAKE_BINARY_DIR}/target/sys/System/\" 
            DESTINATION \"${CMAKE_INSTALL_PREFIX}/share/lxa/System/System\"
            FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
    endif()
    if(EXISTS \"${CMAKE_BINARY_DIR}/target/samples/Samples\")
        file(COPY \"${CMAKE_BINARY_DIR}/target/samples/Samples/\" 
            DESTINATION \"${CMAKE_INSTALL_PREFIX}/share/lxa/System/Samples\"
            FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
    endif()
    if(EXISTS \"${CMAKE_BINARY_DIR}/target/rom/lxa.rom\")
        file(COPY \"${CMAKE_BINARY_DIR}/target/rom/lxa.rom\" 
            DESTINATION \"${CMAKE_INSTALL_PREFIX}/share/lxa/\"
        )
    endif()
    if(EXISTS \"${CMAKE_BINARY_DIR}/target/rom/lxa.rom.map\")
        file(COPY \"${CMAKE_BINARY_DIR}/target/rom/lxa.rom.map\" 
            DESTINATION \"${CMAKE_INSTALL_PREFIX}/share/lxa/\"
        )
    endif()
")

# Custom target to install system commands into the template
add_custom_target(install-sys-commands
    COMMENT "Installing system commands to template directory"
)

# Custom target to install ROM into the data directory
add_custom_target(install-rom
    COMMENT "Installing ROM to data directory"
)

# === Unit Tests ===
# Add unit tests (host-side testing with Unity framework)
option(BUILD_UNIT_TESTS "Build unit tests" ON)
if(BUILD_UNIT_TESTS)
    message(STATUS "Configuring unit tests...")
    add_subdirectory(tests/unit)
endif()

# === Test Drivers ===
# Add test drivers (host-side C programs that use liblxa API)
option(BUILD_TEST_DRIVERS "Build test drivers" ON)
if(BUILD_TEST_DRIVERS)
    message(STATUS "Configuring test drivers...")
    add_subdirectory(tests/drivers)
endif()

# === Summary ===
message(STATUS "")
message(STATUS "LXA Configuration:")
message(STATUS "  Install prefix:       ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Data directory:       ${LXA_DATA_DIR}")
message(STATUS "  Default user prefix:  ${LXA_DEFAULT_PREFIX}")
message(STATUS "  ROM tool:             ${ROMTOOL}")
message(STATUS "  Cross-compiler:       ${M68K_GCC}")
message(STATUS "  Unit tests:           ${BUILD_UNIT_TESTS}")
message(STATUS "  Test drivers:         ${BUILD_TEST_DRIVERS}")
message(STATUS "")
