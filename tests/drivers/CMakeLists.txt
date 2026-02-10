# CMakeLists.txt for lxa test drivers
#
# Test drivers are host-side C programs that use liblxa to control
# the emulator programmatically for automated testing.

# Only build test drivers if liblxa is available
if(NOT TARGET liblxa)
    message(WARNING "liblxa not found, test drivers will not be built")
    return()
endif()

# Common settings for all test drivers (legacy standalone drivers)
function(add_test_driver NAME)
    add_executable(${NAME} ${NAME}.c)
    target_link_libraries(${NAME} PRIVATE liblxa)
    target_include_directories(${NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src/lxa
    )
    set_target_properties(${NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/drivers
    )
    
    # Add as a test
    add_test(NAME ${NAME}
        COMMAND ${NAME}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endfunction()

# Common settings for Google Test drivers
function(add_gtest_driver NAME)
    add_executable(${NAME} ${NAME}.cpp)
    target_link_libraries(${NAME} PRIVATE 
        liblxa
        ${GTEST_LIBRARIES}
        pthread
    )
    target_include_directories(${NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src/lxa
        ${GTEST_INCLUDE_DIRS}
    )
    target_compile_options(${NAME} PRIVATE ${GTEST_CFLAGS})
    set_target_properties(${NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/drivers
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
    
    # Add as a CTest
    add_test(NAME ${NAME}
        COMMAND ${NAME}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    
    # Set test properties for Google Test
    set_tests_properties(${NAME} PROPERTIES
        LABELS "gtest"
    )
endfunction()

# Test drivers
# Each driver tests a specific sample or feature

# SimpleGad - boolean gadgets test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/simplegad_test.c)
    add_test_driver(simplegad_test)
endif()

# SimpleMenu - menu system test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/simplemenu_test.c)
    add_test_driver(simplemenu_test)
endif()

# CustomPointer - custom pointer test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/custompointer_test.c)
    add_test_driver(custompointer_test)
endif()

# GadToolsMenu - GadTools menu test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/gadtoolsmenu_test.c)
    add_test_driver(gadtoolsmenu_test)
endif()

# FileReq - ASL file requester test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/filereq_test.c)
    add_test_driver(filereq_test)
endif()

# UpdateStrGad - string gadget test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/updatestrgad_test.c)
    add_test_driver(updatestrgad_test)
endif()

# SimpleGTGadget - GadTools gadget test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/simplegtgadget_test.c)
    add_test_driver(simplegtgadget_test)
endif()

# MouseTest - mouse input handling test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/mousetest_test.c)
    add_test_driver(mousetest_test)
endif()

# RawKey - raw keyboard input test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/rawkey_test.c)
    add_test_driver(rawkey_test)
endif()

# ========== Deep Dive App Test Drivers ==========

# ASM-One V1.48 - assembler/editor test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/asm_one_test.c)
    add_test_driver(asm_one_test)
endif()

# Devpac - HiSoft assembler test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/devpac_test.c)
    add_test_driver(devpac_test)
endif()

# KickPascal 2 - Pascal IDE test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/kickpascal_test.c)
    add_test_driver(kickpascal_test)
endif()

# MaxonBASIC - BASIC IDE test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/maxonbasic_test.c)
    add_test_driver(maxonbasic_test)
endif()

# DPaint V - Graphics editor test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/dpaint_test.c)
    add_test_driver(dpaint_test)
endif()

# Cluster2 - Oberon-2 IDE test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cluster2_test.c)
    add_test_driver(cluster2_test)
endif()

# ========== API Tests ==========

# API test - tests new liblxa API functions
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/api_test.c)
    add_test_driver(api_test)
endif()

message(STATUS "Test drivers configured")

# ========== Google Test Drivers ==========
# Google Test based test drivers (new testing infrastructure)

if(BUILD_GTESTS AND GTEST_FOUND)
    message(STATUS "Configuring Google Test drivers...")
    
    # ========== RKM Sample Tests ==========
    
    # SimpleGad - boolean gadgets test (GTest version)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/simplegad_gtest.cpp)
        add_gtest_driver(simplegad_gtest)
        set_tests_properties(simplegad_gtest PROPERTIES TIMEOUT 120)
    endif()
    
    # SimpleMenu - menu system test (GTest version)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/simplemenu_gtest.cpp)
        add_gtest_driver(simplemenu_gtest)
    endif()
    
    # MenuLayout - advanced menu layout test (GTest version)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/menulayout_gtest.cpp)
        add_gtest_driver(menulayout_gtest)
    endif()
    
    # CustomPointer - custom pointer test (GTest version)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/custompointer_gtest.cpp)
        add_gtest_driver(custompointer_gtest)
    endif()
    
    # GadToolsMenu - GadTools menu test (GTest version)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/gadtoolsmenu_gtest.cpp)
        add_gtest_driver(gadtoolsmenu_gtest)
    endif()
    
    # FileReq - ASL file requester test (GTest version)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/filereq_gtest.cpp)
        add_gtest_driver(filereq_gtest)
    endif()
    
    # ========== Input Tests ==========
    
    # MouseTest - mouse input handling test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/mousetest_gtest.cpp)
        add_gtest_driver(mousetest_gtest)
    endif()
    
    # RawKey - raw keyboard input test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/rawkey_gtest.cpp)
        add_gtest_driver(rawkey_gtest)
    endif()
    
    # SimpleGTGadget - GadTools gadget test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/simplegtgadget_gtest.cpp)
        add_gtest_driver(simplegtgadget_gtest)
    endif()
    
    # UpdateStrGad - string gadget test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/updatestrgad_gtest.cpp)
        add_gtest_driver(updatestrgad_gtest)
    endif()
    
    # IntuiText - IntuiText rendering test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/intuitext_gtest.cpp)
        add_gtest_driver(intuitext_gtest)
    endif()
    
    # SimpleImage - Image rendering test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/simpleimage_gtest.cpp)
        add_gtest_driver(simpleimage_gtest)
    endif()
    
    # EasyRequest - Easy requester test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/easyrequest_gtest.cpp)
        add_gtest_driver(easyrequest_gtest)
        set_tests_properties(easyrequest_gtest PROPERTIES TIMEOUT 120)
    endif()
    
    # GadToolsGadgets - GadTools gadget creation test (STRING_KIND, SLIDER_KIND, BUTTON_KIND)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/gadtoolsgadgets_gtest.cpp)
        add_gtest_driver(gadtoolsgadgets_gtest)
        set_tests_properties(gadtoolsgadgets_gtest PROPERTIES TIMEOUT 150)
    endif()
    
    # ========== API Tests ==========
    
    # API test - tests liblxa API functions
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/api_gtest.cpp)
        add_gtest_driver(api_gtest)
    endif()
    
    # Samples tests
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/samples_gtest.cpp)
        add_gtest_driver(samples_gtest)
    endif()

    # ========== Deep Dive App Tests ==========
    
    # Exec library tests
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/exec_gtest.cpp)
        add_gtest_driver(exec_gtest)
    endif()

    # DOS library tests
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/dos_gtest.cpp)
        add_gtest_driver(dos_gtest)
    endif()

    # Commands tests
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/commands_gtest.cpp)
        add_gtest_driver(commands_gtest)
    endif()

    # Graphics tests
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/graphics_gtest.cpp)
        add_gtest_driver(graphics_gtest)
    endif()

    # RGBBoxes test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/rgbboxes_gtest.cpp)
        add_gtest_driver(rgbboxes_gtest)
        set_tests_properties(rgbboxes_gtest PROPERTIES TIMEOUT 25)
    endif()

    # Intuition tests
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/intuition_gtest.cpp)
        add_gtest_driver(intuition_gtest)
    endif()

    # Devices tests
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/devices_gtest.cpp)
        add_gtest_driver(devices_gtest)
    endif()

    # Console tests
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/console_gtest.cpp)
        add_gtest_driver(console_gtest)
    endif()

    # Shell tests
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/shell_gtest.cpp)
        add_gtest_driver(shell_gtest)
    endif()

    # Misc tests
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/misc_gtest.cpp)
        add_gtest_driver(misc_gtest)
    endif()

    # Misc Apps tests
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/apps_misc_gtest.cpp)
        add_gtest_driver(apps_misc_gtest)
    endif()

    # Layers tests
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/layers_gtest.cpp)
        add_gtest_driver(layers_gtest)
    endif()

    # ASM-One - assembler/editor test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/asm_one_gtest.cpp)
        add_gtest_driver(asm_one_gtest)
    endif()
    
    # Devpac - HiSoft assembler test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/devpac_gtest.cpp)
        add_gtest_driver(devpac_gtest)
    endif()
    
    # KickPascal - Pascal IDE test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/kickpascal_gtest.cpp)
        add_gtest_driver(kickpascal_gtest)
    endif()
    
    # MaxonBASIC - BASIC IDE test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/maxonbasic_gtest.cpp)
        add_gtest_driver(maxonbasic_gtest)
    endif()
    
    # Cluster2 - Oberon-2 IDE test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cluster2_gtest.cpp)
        add_gtest_driver(cluster2_gtest)
    endif()
    
    # DPaint V - Graphics editor test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/dpaint_gtest.cpp)
        add_gtest_driver(dpaint_gtest)
    endif()
    
    message(STATUS "Google Test drivers configured")
endif()

