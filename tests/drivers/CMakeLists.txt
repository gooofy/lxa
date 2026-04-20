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
    # Default timeout 180s — LOG_INFO builds run real m68k work so
    # wall-clock time is longer than the old LOG_DEBUG builds.
    if(NOT SHARD_TIMEOUT)
        set(SHARD_TIMEOUT 180)
    endif()
    list(APPEND test_properties TIMEOUT ${SHARD_TIMEOUT})

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
            TIMEOUT 180)
        add_gtest_driver_shard(simplegad_pixels_gtest simplegad_gtest
            FILTER "SimpleGadPixelTest.*"
            TIMEOUT 180)
    endif()
    
    # SimpleMenu - menu system test (Phase 107: persistent fixture, 2 shards for different configs)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/simplemenu_gtest.cpp)
        add_gtest_driver_target(simplemenu_gtest)
        add_gtest_driver_shard(simplemenu_behavior_gtest simplemenu_gtest
            FILTER "SimpleMenuTest.*"
            TIMEOUT 180)
        add_gtest_driver_shard(simplemenu_pixels_gtest simplemenu_gtest
            FILTER "SimpleMenuPixelTest.*"
            TIMEOUT 180)
    endif()
    
    # MenuLayout - advanced menu layout test (GTest version)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/menulayout_gtest.cpp)
        add_gtest_driver_target(menulayout_gtest)
        add_gtest_driver_shard(menulayout_behavior_gtest menulayout_gtest
            FILTER "MenuLayoutTest.*"
            TIMEOUT 180)
        add_gtest_driver_shard(menulayout_pixels_gtest menulayout_gtest
            FILTER "MenuLayoutPixelTest.*"
            TIMEOUT 180)
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
            TIMEOUT 180)
        add_gtest_driver_shard(fontreq_actions_gtest fontreq_gtest
            FILTER "FontReqTest.CancelButton:FontReqTest.OKButtonSelectsFont"
            TIMEOUT 180)
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
    
    # SimpleGTGadget - GadTools gadget test (Phase 107: persistent fixture, 2 shards for different configs)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/simplegtgadget_gtest.cpp)
        add_gtest_driver_target(simplegtgadget_gtest)
        add_gtest_driver_shard(simplegtgadget_behavior_gtest simplegtgadget_gtest
            FILTER "SimpleGTGadgetTest.*"
            TIMEOUT 180)
        add_gtest_driver_shard(simplegtgadget_pixels_gtest simplegtgadget_gtest
            FILTER "SimpleGTGadgetPixelTest.*"
            TIMEOUT 180)
    endif()
    
    # UpdateStrGad - string gadget test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/updatestrgad_gtest.cpp)
        add_gtest_driver(updatestrgad_gtest)
    endif()

    # Talk2Boopsi - BOOPSI gadget communication test (Phase 107b: persistent fixture, 2 shards for different configs)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/talk2boopsi_gtest.cpp)
        add_gtest_driver_target(talk2boopsi_gtest)
        add_gtest_driver_shard(talk2boopsi_behavior_gtest talk2boopsi_gtest
            FILTER "Talk2BoopsiTest.*"
            TIMEOUT 180)
        add_gtest_driver_shard(talk2boopsi_pixels_gtest talk2boopsi_gtest
            FILTER "Talk2BoopsiPixelTest.*"
            TIMEOUT 180)
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
        set_tests_properties(easyrequest_gtest PROPERTIES TIMEOUT 180)
    endif()

    # GadToolsGadgets - GadTools gadget creation test (STRING_KIND, SLIDER_KIND, BUTTON_KIND)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/gadtoolsgadgets_gtest.cpp)
        add_gtest_driver_target(gadtoolsgadgets_gtest)
        add_gtest_driver_shard(gadtoolsgadgets_core_gtest gadtoolsgadgets_gtest
            FILTER "GadToolsGadgetsTest.AllGadgetsCreated:GadToolsGadgetsTest.WindowOpened:GadToolsGadgetsTest.RootlessWindowShowsContent:GadToolsGadgetsTest.GadgetsRefreshed:GadToolsGadgetsTest.EventLoopEntered:GadToolsGadgetsTest.CloseWindow"
            TIMEOUT 180)
        add_gtest_driver_shard(gadtoolsgadgets_pointer_gtest gadtoolsgadgets_gtest
            FILTER "GadToolsGadgetsTest.ButtonClick:GadToolsGadgetsTest.ButtonClickCompletesWithinOneVBlank:GadToolsGadgetsTest.SliderClick:GadToolsGadgetsTest.ButtonResetsSlider:GadToolsGadgetsTest.DepthGadgetClick:GadToolsGadgetsTest.ResizeDragKeepsWindowResponsive"
            TIMEOUT 180)
        add_gtest_driver_shard(gadtoolsgadgets_navigation_gtest gadtoolsgadgets_gtest
            FILTER "GadToolsGadgetsTest.SliderDrag:GadToolsGadgetsTest.TabCyclesStringGadgets"
            TIMEOUT 180)
        add_gtest_driver_shard(gadtoolsgadgets_keyboard_gtest gadtoolsgadgets_gtest
            FILTER "GadToolsGadgetsTest.ShiftTabCyclesBackward:GadToolsGadgetsTest.VanillaKeySliderIncrease:GadToolsGadgetsTest.VanillaKeySliderDecrease:GadToolsGadgetsTest.VanillaKeyActivateGadget"
            TIMEOUT 180)
        add_gtest_driver_shard(gadtoolsgadgets_pixels_a_gtest gadtoolsgadgets_gtest
            FILTER "GadToolsGadgetsPixelTest.WindowTitleBarRendered:GadToolsGadgetsPixelTest.ButtonBevelBorderRendered:GadToolsGadgetsPixelTest.StringGadgetBevelBorderRendered:GadToolsGadgetsPixelTest.StringGadgetDoubleBevelRendered"
            TIMEOUT 180)
        add_gtest_driver_shard(gadtoolsgadgets_pixels_b_gtest gadtoolsgadgets_gtest
            FILTER "GadToolsGadgetsPixelTest.StringGadgetTextRendered:GadToolsGadgetsPixelTest.ButtonLabelRendered:GadToolsGadgetsPixelTest.SliderKnobVisible:GadToolsGadgetsPixelTest.SliderBevelBorderRendered"
            TIMEOUT 180)
        add_gtest_driver_shard(gadtoolsgadgets_pixels_c_gtest gadtoolsgadgets_gtest
            FILTER "GadToolsGadgetsPixelTest.UnderscoreLabelRendered:GadToolsGadgetsPixelTest.SliderLevelValueRendered:GadToolsGadgetsPixelTest.SizeGadgetRendered:GadToolsGadgetsPixelTest.DepthGadgetRendered:GadToolsGadgetsPixelTest.ResizeKeepsSizeGadgetBordersClean"
            TIMEOUT 180)
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
        set_tests_properties(rgbboxes_gtest PROPERTIES TIMEOUT 180)
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
        set_tests_properties(misc_gtest PROPERTIES TIMEOUT 180)
    endif()

    # Misc Apps tests
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/apps_misc_gtest.cpp)
        add_gtest_driver(apps_misc_gtest)
    endif()

    # SysInfo - startup and interaction coverage
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/sysinfo_gtest.cpp)
        add_gtest_driver(sysinfo_gtest)
    endif()

    # Layers tests
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/layers_gtest.cpp)
        add_gtest_driver(layers_gtest)
    endif()

    # ASM-One - assembler/editor test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/asm_one_gtest.cpp)
        add_gtest_driver(asm_one_gtest)
    endif()
    
    # Devpac - HiSoft assembler test (Phase 107: persistent fixture, no sharding needed)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/devpac_gtest.cpp)
        add_gtest_driver(devpac_gtest)
    endif()
    
    # KickPascal - Pascal IDE test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/kickpascal_gtest.cpp)
        add_gtest_driver(kickpascal_gtest)
    endif()
    
    # MaxonBASIC - BASIC IDE test (Phase 107: persistent fixture, no sharding needed)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/maxonbasic_gtest.cpp)
        add_gtest_driver(maxonbasic_gtest)
    endif()
    
    # Cluster2 - Oberon-2 IDE test (Phase 107: persistent fixture, no sharding needed)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cluster2_gtest.cpp)
        add_gtest_driver(cluster2_gtest)
    endif()

    # BlitzBasic2 - IDE startup and interaction test (Phase 107b: persistent fixture, single shard)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/blitzbasic2_gtest.cpp)
        add_gtest_driver(blitzbasic2_gtest)
    endif()

    # Vim 5.3 - startup and editing test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/vim_gtest.cpp)
        add_gtest_driver_target(vim_gtest)
        add_gtest_driver_shard(vim_startup_gtest vim_gtest
            FILTER "VimTest.StartupOpensFileAndDrawsEditor"
            TIMEOUT 180)
        add_gtest_driver_shard(vim_editing_gtest vim_gtest
            FILTER "VimTest.CursorAndInsertModeRedrawWindow:VimTest.InsertModeTypingAndCommandModeWriteFile:VimTest.TypingAndSavingWritesHostFile"
            TIMEOUT 180)
    endif()

    # FinalWriter_D - word processor startup and interaction test.
    # Sharded into two groups (each ~90s); a single binary cannot fit all
    # 8 enabled tests under the 180s default timeout because each test
    # requires a full FinalWriter_D startup (~19s).  IMPORTANT: when
    # adding a new test, update the appropriate FILTER below or it will
    # be silently orphaned (PPaint Phase 121 / FW Phase 124 lesson).
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/finalwriter_gtest.cpp)
        add_gtest_driver_target(finalwriter_gtest)
        add_gtest_driver_shard(finalwriter_startup_gtest finalwriter_gtest
            FILTER "FinalWriterTest.StartupOpensVisibleWindow:FinalWriterTest.MainWindowCaptureShowsEditorRegions:FinalWriterTest.ZStartupDialogTitleIsFinalWriterVersion:FinalWriterTest.ZStartupDialogExposesExpectedGadgetCount"
            TIMEOUT 180)
        add_gtest_driver_shard(finalwriter_dialog_gtest finalwriter_gtest
            FILTER "FinalWriterTest.ZBottomButtonsResolveToValidGadgets:FinalWriterTest.ZDisplayOptionsListIsLocatable:FinalWriterTest.ZNoMissingLibraryOrPanicAtStartup:FinalWriterTest.ZStartupDialogContentSurvivesIdle"
            TIMEOUT 180)
    endif()

    # PPaint - paint package startup, probe-window geometry, and canvas tests.
    # Unsharded: per-test fixture restart keeps each case under the timeout, and
    # a hardcoded filter previously caused new Z-tests to be silently skipped.
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ppaint_gtest.cpp)
        add_gtest_driver(ppaint_gtest)
    endif()

    # SIGMAth2 - startup, visual smoke, and interaction coverage
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/sigma_gtest.cpp)
        add_gtest_driver_target(sigma_gtest)
        add_gtest_driver_shard(sigma_startup_gtest sigma_gtest
            FILTER "SIGMAth2Test.StartupOpensVisibleAnalysisWindow:SIGMAth2Test.StartupKeepsRunningWithoutResourceFailures:SIGMAth2Test.MainWindowCaptureContainsVisibleUiRegions:SIGMAth2Test.MenuBarRightMouseSmokeKeepsAnalysisWindowAlive"
            TIMEOUT 180)
        add_gtest_driver_shard(sigma_interaction_gtest sigma_gtest
            FILTER "SIGMAth2Test.AnalysisWindowGadgetGridLayout:SIGMAth2Test.AboutDialogOpensAndCloses:SIGMAth2Test.ButtonClickInteraction:SIGMAth2Test.StringGadgetAcceptsInput:SIGMAth2Test.ScrollGadgetInteraction:SIGMAth2Test.ButtonAreaGadgetsAreConsistentlyPlaced:SIGMAth2Test.DisplayFrameContainsRenderedContent:SIGMAth2Test.FileMenuOpenSurvivesRequester"
            TIMEOUT 180)
    endif()

    # DPaint V - Graphics editor test
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/dpaint_gtest.cpp)
        add_gtest_driver(dpaint_gtest)
    endif()

    # Directory Opus 4 - file manager startup and interaction test (Phase 121)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/dopus_gtest.cpp)
        add_gtest_driver(dopus_gtest)
    endif()
    
    message(STATUS "Google Test drivers configured")

    # Shard coverage check — fails immediately if a TEST_F in a sharded driver
    # is not listed in any FILTER string.  Run with: ctest -R shard_coverage_check
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_FOUND)
        add_test(
            NAME shard_coverage_check
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/tools/check_shard_coverage.py
                --drivers-dir ${CMAKE_CURRENT_SOURCE_DIR}
                --cmake ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        )
        set_tests_properties(shard_coverage_check PROPERTIES
            TIMEOUT 30
            LABELS "meta"
        )
    else()
        message(WARNING "Python3 not found; shard_coverage_check test will not be registered")
    endif()
endif()
