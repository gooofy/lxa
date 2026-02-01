# CMakeLists.txt for lxa host emulator (x86_64-linux)
# This builds the host process that runs on Linux

# Find required packages
find_package(Threads REQUIRED)

# Find SDL2 for graphics support
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 sdl2)
endif()

if(NOT SDL2_FOUND)
    message(WARNING "SDL2 not found. Graphics support will be disabled.")
    message(STATUS "Install SDL2: sudo apt install libsdl2-dev")
else()
    message(STATUS "Found SDL2: ${SDL2_VERSION}")
endif()

# Find readline library manually
find_library(READLINE_LIBRARY readline
    PATHS /usr/lib /usr/local/lib /usr/lib64
)
find_path(READLINE_INCLUDE_DIR readline/readline.h
    PATHS /usr/include /usr/local/include
)

if(NOT READLINE_LIBRARY)
    message(FATAL_ERROR "readline library not found. Please install readline development package.")
endif()

# Source files for the host emulator
set(LXA_HOST_SOURCES
    lxa.c
    m68kcpu.c
    m68kdasm.c
    m68kops.c
    softfloat.c
    util.c
    vfs.c
    config.c
    display.c
)

# Define the lxa executable
add_executable(lxa ${LXA_HOST_SOURCES})

# Include directories
target_include_directories(lxa PRIVATE
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(READLINE_INCLUDE_DIR)
    target_include_directories(lxa PRIVATE ${READLINE_INCLUDE_DIR})
endif()

if(SDL2_FOUND)
    target_include_directories(lxa PRIVATE ${SDL2_INCLUDE_DIRS})
endif()

# Compiler flags for host build
# Note: Using same flags as legacy Makefile, with additional suppressions
# for warnings that appear with newer GCC versions but not with the older
# compiler configuration used in the legacy Makefile
target_compile_options(lxa PRIVATE
    -Wall
    -Werror
    -g
    -O2
    -Wno-format-truncation
    -Wno-stringop-truncation
    -Wno-format-overflow
    -Wno-return-type
)

# Link libraries
target_link_libraries(lxa PRIVATE
    Threads::Threads
    ${READLINE_LIBRARY}
    m
)

if(SDL2_FOUND)
    target_link_libraries(lxa PRIVATE ${SDL2_LIBRARIES})
endif()

# Define configuration macros
target_compile_definitions(lxa PRIVATE
    LXA_VERSION=\"${PROJECT_VERSION}\"
    LXA_DATA_DIR=\"${LXA_DATA_DIR}\"
    LXA_DEFAULT_PREFIX=\"${LXA_DEFAULT_PREFIX}\"
)

# Add SDL2_FOUND definition if SDL2 is available
if(SDL2_FOUND)
    target_compile_definitions(lxa PRIVATE SDL2_FOUND)
endif()

# Set output directory
set_target_properties(lxa PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${HOST_BINARY_DIR}/bin
)

# Installation
install(TARGETS lxa
    RUNTIME DESTINATION bin
)
