# Unit Tests CMakeLists.txt
# Uses Unity framework for host-side unit testing

cmake_minimum_required(VERSION 3.16)

# === Coverage Option ===
option(COVERAGE "Enable code coverage" OFF)
if(COVERAGE)
    message(STATUS "Code coverage enabled")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

# Find source files
set(UNITY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/unity)
set(UNITY_SRCS
    ${UNITY_DIR}/unity.c
)

# Unity library
add_library(unity STATIC ${UNITY_SRCS})
target_include_directories(unity PUBLIC ${UNITY_DIR})

# Include project headers
set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(LXA_SRC_DIR ${PROJECT_ROOT}/src/lxa)
set(ROM_SRC_DIR ${PROJECT_ROOT}/src/rom)
set(INCLUDE_DIR ${PROJECT_ROOT}/src/include)

# === Stub library for unit tests ===
# Provides stub implementations to avoid pulling in full emulator
add_library(test_stubs STATIC
    stubs.c
)
target_include_directories(test_stubs PUBLIC
    ${LXA_SRC_DIR}
    ${INCLUDE_DIR}
)

# === VFS Unit Tests ===
# Tests the host-side VFS layer (path resolution, case-insensitivity, etc.)
add_executable(test_vfs
    test_vfs.c
    ${LXA_SRC_DIR}/vfs.c
    ${LXA_SRC_DIR}/config.c
)
target_include_directories(test_vfs PRIVATE
    ${UNITY_DIR}
    ${LXA_SRC_DIR}
    ${INCLUDE_DIR}
)
target_link_libraries(test_vfs unity test_stubs)
target_compile_definitions(test_vfs PRIVATE 
    UNIT_TESTING=1
    _GNU_SOURCE
)

# Register the test
add_test(NAME unit_vfs COMMAND test_vfs)

# === Config Unit Tests ===
add_executable(test_config
    test_config.c
    ${LXA_SRC_DIR}/config.c
    ${LXA_SRC_DIR}/vfs.c
)
target_include_directories(test_config PRIVATE
    ${UNITY_DIR}
    ${LXA_SRC_DIR}
    ${INCLUDE_DIR}
)
target_link_libraries(test_config unity test_stubs)
target_compile_definitions(test_config PRIVATE 
    UNIT_TESTING=1
    _GNU_SOURCE
)

add_test(NAME unit_config COMMAND test_config)

# === Custom target to run all unit tests ===
add_custom_target(test-unit
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_vfs test_config
    COMMENT "Running unit tests..."
)

# === Coverage targets ===
if(COVERAGE)
    find_program(LCOV lcov)
    find_program(GENHTML genhtml)
    
    if(LCOV AND GENHTML)
        add_custom_target(coverage
            COMMENT "Generating coverage report..."
            COMMAND ${LCOV} --capture --directory . --output-file coverage.info
            COMMAND ${LCOV} --remove coverage.info '/usr/*' '*/unity/*' --output-file coverage.info
            COMMAND ${GENHTML} coverage.info --output-directory coverage_report
            COMMAND echo "Coverage report generated in coverage_report/index.html"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
        
        add_custom_target(coverage-clean
            COMMENT "Cleaning coverage data..."
            COMMAND find . -name "*.gcda" -delete
            COMMAND find . -name "*.gcno" -delete
            COMMAND rm -rf coverage.info coverage_report
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    else()
        message(WARNING "lcov or genhtml not found - coverage reporting disabled")
    endif()
endif()

# Enable testing
enable_testing()
