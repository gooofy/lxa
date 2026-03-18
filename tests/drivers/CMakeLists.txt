# CMakeLists.txt for lxa test drivers
#
# Test drivers are host-side C++ programs that use liblxa and Google Test
# to control the emulator programmatically for automated testing.

# Only build test drivers if liblxa is available
if(NOT TARGET liblxa)
    message(WARNING "liblxa not found, test drivers will not be built")
    return()
endif()

# Common settings for Google Test drivers
function(add_gtest_driver_target NAME)
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
endfunction()

function(add_gtest_driver_shard TEST_NAME TARGET_NAME)
    set(options)
    set(oneValueArgs FILTER TIMEOUT)
    cmake_parse_arguments(SHARD "${options}" "${oneValueArgs}" "" ${ARGN})

    set(test_command ${TARGET_NAME})
    if(SHARD_FILTER)
        list(APPEND test_command --gtest_filter=${SHARD_FILTER})
    endif()

    add_test(NAME ${TEST_NAME}
        COMMAND ${test_command}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    set(test_properties LABELS "gtest")
    if(SHARD_TIMEOUT)
        list(APPEND test_properties TIMEOUT ${SHARD_TIMEOUT})
    endif()

    set_tests_properties(${TEST_NAME} PROPERTIES ${test_properties})
endfunction()

function(add_gtest_driver NAME)
    add_gtest_driver_target(${NAME})
    add_gtest_driver_shard(${NAME} ${NAME})
endfunction()

# ========== Google Test Drivers ==========
# All test drivers use Google Test infrastructure

if(BUILD_GTESTS AND GTEST_FOUND)
    message(STATUS "Configuring Google Test drivers...")
    
    # ========== RKM Sample Tests ==========
    
    # SimpleGad - boolean gadgets test (GTest version)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/simplegad_gtest.cpp)
        add_gtest_driver_target(simplegad_gtest)
        add_gtest_driver_shard(simplegad_behavior_gtest simplegad_gtest
            FILTER "SimpleGadTest.*"
            TIMEOUT 60)
        add_gtest_driver_shard(simplegad_pixels_gtest simplegad_gtest
            FILTER "SimpleGadPixelTest.*"
            TIMEOUT 90)
    endif()
    
    # SimpleMenu - menu system test (GTest version)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/simplemenu_gtest.cpp)
        add_gtest_driver_target(simplemenu_gtest)
        add_gtest_driver_shard(simplemenu_behavior_gtest simplemenu_gtest
            FILTER "SimpleMenuTest.*"
            TIMEOUT 60)
        add_gtest_driver_shard(simplemenu_pixels_gtest simplemenu_gtest
            FILTER "SimpleMenuPixelTest.*"
            TIMEOUT 60)
    endif()
    
    # MenuLayout - advanced menu layout test (GTest version)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/menulayout_gtest.cpp)
        add_gtest_driver_target(menulayout_gtest)
        add_gtest_driver_shard(menulayout_behavior_gtest menulayout_gtest
            FILTER "MenuLayoutTest.*"
            TIMEOUT 60)
        add_gtest_driver_shard(menulayout_pixels_gtest menulayout_gtest
            FILTER "MenuLayoutPixelTest.*"
            TIMEOUT 60)
    endif()
    
    # CustomPointer - custom pointer test (GTest version)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/custompointer_gtest.cpp)
        add_gtest_driver(custompointer_gtest)
    endif()
    
    # GadToolsMenu - GadTools menu test (GTest version)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/gadtoolsmenu_gtest.cpp)
        add_gtest_driver(gadtoolsmenu_gtest)
    endif()

    # GadTools API tests
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/gadtools_gtest.cpp)
        add_gtest_driver(gadtools_gtest)
    endif()
    
    # FileReq - ASL file requester test (GTest version)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/filereq_gtest.cpp)
        add_gtest_driver(filereq_gtest)
    endif()
    
    # FontReq - ASL font requester test (GTest version)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/fontreq_gtest.cpp)
        add_gtest_driver_target(fontreq_gtest)
        add_gtest_driver_shard(fontreq_behavior_gtest fontreq_gtest
            FILTER "FontReqTest.RequesterOpens:FontReqTest.WindowPositionAndSize:FontReqTest.FontListDisplayed"
            TIMEOUT 60)
        add_gtest_driver_shard(fontreq_actions_gtest fontreq_gtest
            FILTER "FontReqTest.CancelButton:FontReqTest.OKButtonSelectsFont"
            TIMEOUT 60)
    endif()

    # ScreenModeReq - ASL screenmode requester test (GTest version)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/screenmodereq_gtest.cpp)
        add_gtest_driver(screenmodereq_gtest)
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

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/talk2boopsi_gtest.cpp)
        add_gtest_driver(talk2boopsi_gtest)
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
        add_gtest_driver_target(gadtoolsgadgets_gtest)
        add_gtest_driver_shard(gadtoolsgadgets_core_gtest gadtoolsgadgets_gtest
            FILTER "GadToolsGadgetsTest.AllGadgetsCreated:GadToolsGadgetsTest.WindowOpened:GadToolsGadgetsTest.CloseWindow"
            TIMEOUT 60)
        add_gtest_driver_shard(gadtoolsgadgets_pointer_gtest gadtoolsgadgets_gtest
            FILTER "GadToolsGadgetsTest.ButtonClick:GadToolsGadgetsTest.ButtonClickCompletesWithinOneVBlank:GadToolsGadgetsTest.SliderClick:GadToolsGadgetsTest.ButtonResetsSlider:GadToolsGadgetsTest.DepthGadgetClick:GadToolsGadgetsTest.ResizeDragKeepsWindowResponsive"
            TIMEOUT 60)
        add_gtest_driver_shard(gadtoolsgadgets_navigation_gtest gadtoolsgadgets_gtest
            FILTER "GadToolsGadgetsTest.SliderDrag:GadToolsGadgetsTest.TabCyclesStringGadgets"
            TIMEOUT 60)
        add_gtest_driver_shard(gadtoolsgadgets_keyboard_gtest gadtoolsgadgets_gtest
            FILTER "GadToolsGadgetsTest.ShiftTabCyclesBackward:GadToolsGadgetsTest.VanillaKeySliderIncrease:GadToolsGadgetsTest.VanillaKeySliderDecrease:GadToolsGadgetsTest.VanillaKeyActivateGadget"
            TIMEOUT 60)
        add_gtest_driver_shard(gadtoolsgadgets_pixels_a_gtest gadtoolsgadgets_gtest
            FILTER "GadToolsGadgetsPixelTest.WindowTitleBarRendered:GadToolsGadgetsPixelTest.ButtonBevelBorderRendered:GadToolsGadgetsPixelTest.StringGadgetBevelBorderRendered:GadToolsGadgetsPixelTest.StringGadgetDoubleBevelRendered"
            TIMEOUT 90)
        add_gtest_driver_shard(gadtoolsgadgets_pixels_b_gtest gadtoolsgadgets_gtest
            FILTER "GadToolsGadgetsPixelTest.StringGadgetTextRendered:GadToolsGadgetsPixelTest.ButtonLabelRendered:GadToolsGadgetsPixelTest.SliderKnobVisible:GadToolsGadgetsPixelTest.SliderBevelBorderRendered"
            TIMEOUT 90)
        add_gtest_driver_shard(gadtoolsgadgets_pixels_c_gtest gadtoolsgadgets_gtest
            FILTER "GadToolsGadgetsPixelTest.UnderscoreLabelRendered:GadToolsGadgetsPixelTest.SizeGadgetRendered:GadToolsGadgetsPixelTest.DepthGadgetRendered:GadToolsGadgetsPixelTest.ResizeKeepsSizeGadgetBordersClean"
            TIMEOUT 90)
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
        set_tests_properties(misc_gtest PROPERTIES TIMEOUT 120)
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
        add_gtest_driver_target(cluster2_gtest)
        add_gtest_driver_shard(cluster2_startup_gtest cluster2_gtest
            FILTER "Cluster2Test.WindowOpens:Cluster2Test.ScreenDimensions:Cluster2Test.EditorReady"
            TIMEOUT 60)
        add_gtest_driver_shard(cluster2_input_gtest cluster2_gtest
            FILTER "Cluster2Test.RespondsToInput:Cluster2Test.MouseInput"
            TIMEOUT 60)
        add_gtest_driver_shard(cluster2_navigation_gtest cluster2_gtest
            FILTER "Cluster2Test.CursorKeys:Cluster2Test.RMBMenuAccess"
            TIMEOUT 60)
    endif()

    # BlitzBasic2 - IDE startup and interaction test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/blitzbasic2_gtest.cpp)
        add_gtest_driver_target(blitzbasic2_gtest)
        add_gtest_driver_shard(blitzbasic2_startup_gtest blitzbasic2_gtest
            FILTER "BlitzBasic2Test.StartupOpensVisibleIdeWindow"
            TIMEOUT 60)
        add_gtest_driver_shard(blitzbasic2_interaction_gtest blitzbasic2_gtest
            FILTER "BlitzBasic2Test.ProjectMenuOpensButDropdownIsCurrentlyClipped"
            TIMEOUT 60)
    endif()

    # Vim 5.3 - startup and editing test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/vim_gtest.cpp)
        add_gtest_driver_target(vim_gtest)
        add_gtest_driver_shard(vim_startup_gtest vim_gtest
            FILTER "VimTest.StartupOpensFileAndDrawsEditor:VimTest.CursorAndInsertModeRedrawWindow"
            TIMEOUT 60)
        add_gtest_driver_shard(vim_editing_gtest vim_gtest
            FILTER "VimTest.TypingAndSavingWritesHostFile"
            TIMEOUT 60)
    endif()
    
    # DPaint V - Graphics editor test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/dpaint_gtest.cpp)
        add_gtest_driver(dpaint_gtest)
    endif()
    
    message(STATUS "Google Test drivers configured")
endif()
