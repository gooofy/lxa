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

# Core source files (shared between library and executable)
set(LXA_CORE_SOURCES
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

# Common compile options
set(LXA_COMPILE_OPTIONS
    -Wall
    -Werror
    -g
    -O2
    -Wno-format-truncation
    -Wno-stringop-truncation
    -Wno-format-overflow
    -Wno-return-type
)

# Common compile definitions
set(LXA_COMPILE_DEFINITIONS
    LXA_VERSION=\"${PROJECT_VERSION}\"
    LXA_DATA_DIR=\"${LXA_DATA_DIR}\"
    LXA_DEFAULT_PREFIX=\"${LXA_DEFAULT_PREFIX}\"
)

if(SDL2_FOUND)
    list(APPEND LXA_COMPILE_DEFINITIONS SDL2_FOUND)
endif()

# === liblxa static library ===
# This is used by test drivers to control the emulator programmatically
add_library(liblxa STATIC
    ${LXA_CORE_SOURCES}
    lxa_api.c
)

target_include_directories(liblxa PUBLIC
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(READLINE_INCLUDE_DIR)
    target_include_directories(liblxa PRIVATE ${READLINE_INCLUDE_DIR})
endif()

if(SDL2_FOUND)
    target_include_directories(liblxa PRIVATE ${SDL2_INCLUDE_DIRS})
endif()

target_compile_options(liblxa PRIVATE ${LXA_COMPILE_OPTIONS})
target_compile_definitions(liblxa PRIVATE ${LXA_COMPILE_DEFINITIONS} LXA_LIBRARY_BUILD)

target_link_libraries(liblxa PUBLIC
    Threads::Threads
    ${READLINE_LIBRARY}
    m
)

if(SDL2_FOUND)
    target_link_libraries(liblxa PUBLIC ${SDL2_LIBRARIES})
endif()

set_target_properties(liblxa PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${HOST_BINARY_DIR}/lib
    OUTPUT_NAME lxa
)

# === lxa executable ===
add_executable(lxa ${LXA_CORE_SOURCES})

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

target_compile_options(lxa PRIVATE ${LXA_COMPILE_OPTIONS})
target_compile_definitions(lxa PRIVATE ${LXA_COMPILE_DEFINITIONS})

target_link_libraries(lxa PRIVATE
    Threads::Threads
    ${READLINE_LIBRARY}
    m
)

if(SDL2_FOUND)
    target_link_libraries(lxa PRIVATE ${SDL2_LIBRARIES})
endif()

set_target_properties(lxa PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${HOST_BINARY_DIR}/bin
)

# Installation
install(TARGETS lxa
    RUNTIME DESTINATION bin
)

install(TARGETS liblxa
    ARCHIVE DESTINATION lib
)

install(FILES lxa_api.h
    DESTINATION include
)
