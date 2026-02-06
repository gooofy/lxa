# CMakeLists.txt for lxa samples (m68k-amigaos)
# This builds all the samples in samples/

# Only build if we have the cross-compiler
if(NOT M68K_GCC)
    return()
endif()

# Toolchain paths
set(M68K_PREFIX "/opt/amiga")
set(M68K_GCC_BIN "${M68K_PREFIX}/bin/m68k-amigaos-gcc")

# Common compiler flags for all samples
set(SAMPLES_CFLAGS
    -march=68000
    -mcpu=68000
    -O2
    -Wall
    -I${M68K_PREFIX}/m68k-amigaos/include
    -I${M68K_PREFIX}/m68k-amigaos/sys-include
    -I${CMAKE_CURRENT_SOURCE_DIR}/../src/include
)

# Common linker flags
set(SAMPLES_LDFLAGS -mcrt=nix20)

# Create output directory
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Samples)

# === Samples ===
# format: directory|filename|output_name
set(SAMPLES_LIST
    "exec|simpletask|SimpleTask"
    "exec|signals|Signals"
    "exec|semaphore|Semaphore"
    "exec|port1|Port1"
    "exec|port2|Port2"
    "exec|buildlist|BuildList"
    "exec|allocate|Allocate"
    "exec|allocentry|AllocEntry"
    "exec|deviceuse|DeviceUse"
    "exec|timersoftint|TimerSoftInt"
    "exec|tasklist|TaskList"
    "graphics|RGBBoxes|RGBBoxes"
    "graphics|clipping|Clipping"
    "graphics|availfonts|AvailFonts"
    "graphics|measuretext|MeasureText"
    "graphics|wbclone|WBClone"
    "intuition|openwindowtags|OpenWindowTags"
    "intuition|simplegad|SimpleGad"
    "intuition|updatestrgad|UpdateStrGad"
    "intuition|intuitext|IntuiText"
    "intuition|simpleimage|SimpleImage"
    "intuition|shadowborder|ShadowBorder"
    "intuition|easyintuition|EasyIntuition"
    "intuition|simplemenu|SimpleMenu"
    "intuition|easyrequest|EasyRequest"
    "intuition|simplegtgadget|SimpleGTGadget"
    "intuition|gadtoolsmenu|GadToolsMenu"
    "intuition|gadtoolsgadgets|GadToolsGadgets"
    "intuition|mousetest|MouseTest"
    "intuition|rawkey|RawKey"
    "intuition|custompointer|CustomPointer"
    "intuition|clonescreen|CloneScreen"
    "intuition|publicscreen|PublicScreen"
    "intuition|visiblewindow|VisibleWindow"
    "intuition|winpubscreen|WinPubScreen"
    "intuition|talk2boopsi|Talk2Boopsi"
    "intuition|doublebuffer|DoubleBuffer"
    "devices|simpletimer|SimpleTimer"
    "devices|askkeymap|AskKeymap"
    "utility|tag1|Tag1"
    "utility|hooks1|Hooks1"
    "utility|istr|IStr"
    "utility|a2d|A2D"
    "utility|uptime|Uptime"
    "math|ffpexample|FFPExample"
    "math|ffptrans|FFPTrans"
    "math|ieeedpexample|IEEEDPExample"
    "iffparse|sift|Sift"
    "expansion|findboards|FindBoards"
    "asl|filereq|FileReq"
    "asl|fontreq|FontReq"
)

# === Tests ===
# format: directory|filename|output_name
set(TESTS_LIST
    "../tests/exec/lists|main|Tests/Exec/Lists"
    "../tests/exec/memory|main|Tests/Exec/Memory"
    "../tests/exec/msgport|main|Tests/Exec/MsgPort"
    "../tests/exec/multitask|main|Tests/Exec/Multitask"
    "../tests/exec/rawdofmt|main|Tests/Exec/RawDoFmt"
    "../tests/exec/resources|main|Tests/Exec/Resources"
    "../tests/exec/signal_pingpong|main|Tests/Exec/SignalPingPong"
    "../tests/exec/signals|main|Tests/Exec/Signals"
    "../tests/exec/sync|main|Tests/Exec/Sync"
    "../tests/exec/tasks|main|Tests/Exec/Tasks"
    "../tests/dos/chario|main|Tests/Dos/CharIO"
    "../tests/dos/createdir|main|Tests/Dos/CreateDir"
    "../tests/dos/fhutils|main|Tests/Dos/FHUtils"
    "../tests/dos/fileio|main|Tests/Dos/FileIO"
    "../tests/dos/helloworld|main|Tests/Dos/HelloWorld"
    "../tests/dos/lock_shared|main|Tests/Dos/LockShared"
    "../tests/dos/matchfirst|main|Tests/Dos/MatchFirst"
    "../tests/dos/path_navigation|main|Tests/Dos/PathNavigation"
    "../tests/dos/pathutils|main|Tests/Dos/PathUtils"
    "../tests/dos/patterns|main|Tests/Dos/Patterns"
    "../tests/dos/process_advanced|main|Tests/Dos/ProcessAdvanced"
    "../tests/dos/readargs_empty|main|Tests/Dos/ReadArgsEmpty"
    "../tests/dos/readargs_full|main|Tests/Dos/ReadArgsFull"
    "../tests/dos/vprintf|main|Tests/Dos/VPrintf"
    "../tests/commands/assign|main|Tests/Commands/Assign"
    "../tests/commands/avail|main|Tests/Commands/Avail"
    "../tests/commands/break_cmd|main|Tests/Commands/Break"
    "../tests/commands/copy|main|Tests/Commands/Copy"
    "../tests/commands/delete|main|Tests/Commands/Delete"
    "../tests/commands/dir|main|Tests/Commands/Dir"
    "../tests/commands/eval|main|Tests/Commands/Eval"
    "../tests/commands/filenote|main|Tests/Commands/FileNote"
    "../tests/commands/join|main|Tests/Commands/Join"
    "../tests/commands/list|main|Tests/Commands/List"
    "../tests/commands/makedir|main|Tests/Commands/MakeDir"
    "../tests/commands/protect|main|Tests/Commands/Protect"
    "../tests/commands/rename|main|Tests/Commands/Rename"
    "../tests/commands/run|main|Tests/Commands/run_test"
    "../tests/commands/search|main|Tests/Commands/Search"
    "../tests/commands/setenv|main|Tests/Commands/SetEnv"
    "../tests/commands/sort|main|Tests/Commands/Sort"
    "../tests/commands/status|main|Tests/Commands/Status"
    "../tests/commands/syscmd|main|Tests/Commands/SysCmd"
    "../tests/commands/type|main|Tests/Commands/Type"
    "../tests/graphics/area_ellipse|main|Tests/Graphics/AreaEllipse"
    "../tests/graphics/blt_bitmap|main|Tests/Graphics/BltBitMap"
    "../tests/graphics/bltpattern|main|Tests/Graphics/BltPattern"
    "../tests/graphics/draw_ellipse|main|Tests/Graphics/DrawEllipse"
    "../tests/graphics/init_bitmap|main|Tests/Graphics/InitBitMap"
    "../tests/graphics/init_rastport|main|Tests/Graphics/InitRastPort"
    "../tests/graphics/line_draw|main|Tests/Graphics/LineDraw"
    "../tests/graphics/pen_state|main|Tests/Graphics/PenState"
    "../tests/graphics/pixel_ops|main|Tests/Graphics/PixelOps"
    "../tests/graphics/rect_fill|main|Tests/Graphics/RectFill"
    "../tests/graphics/regions|main|Tests/Graphics/Regions"
    "../tests/graphics/rpattrs|main|Tests/Graphics/RPAttrs"
    "../tests/graphics/text_extent|main|Tests/Graphics/TextExtent"
    "../tests/graphics/text_render|main|Tests/Graphics/TextRender"
    "../tests/intuition/boopsi|main|Tests/Intuition/BOOPSI"
    "../tests/intuition/gadget_click|main|Tests/Intuition/GadgetClick"
    "../tests/intuition/gadget_refresh|main|Tests/Intuition/GadgetRefresh"
    "../tests/intuition/gadgetclass|main|Tests/Intuition/GadgetClass"
    "../tests/intuition/idcmp|main|Tests/Intuition/IDCMP"
    "../tests/intuition/idcmp_input|main|Tests/Intuition/IDCMPInput"
    "../tests/intuition/menu_select|main|Tests/Intuition/MenuSelect"
    "../tests/intuition/menu_strip|main|Tests/Intuition/MenuStrip"
    "../tests/intuition/requester_basic|main|Tests/Intuition/RequesterBasic"
    "../tests/intuition/screen_basic|main|Tests/Intuition/ScreenBasic"
    "../tests/intuition/screen_manipulation|main|Tests/Intuition/ScreenManipulation"
    "../tests/intuition/validation_test|main|Tests/Intuition/Validation"
    "../tests/intuition/window_basic|main|Tests/Intuition/WindowBasic"
    "../tests/intuition/window_manipulation|main|Tests/Intuition/WindowManipulation"
    "../tests/console/csi_cursor|main|Tests/Console/csi_cursor"
    "../tests/console/csi_unit|main|Tests/Console/csi_unit"
    "../tests/console/sgr_unit|main|Tests/Console/sgr_unit"
    "../tests/console/con_handler|main|Tests/Console/con_handler"
    "../tests/console/kp2_test|main|Tests/Console/kp2_test"
    "../tests/console/input_inject|main|Tests/Console/input_inject"
    "../tests/console/input_console|main|Tests/Console/input_console"
    "../tests/console_advanced|main|Tests/ConsoleAdvanced/console_advanced"
    "../tests/devices/clipboard|main|Tests/Devices/Clipboard"
    "../tests/devices/console_async|main|Tests/Devices/ConsoleAsync"
    "../tests/devices/timer|main|Tests/Devices/Timer"
    "../tests/devices/timer_async|main|Tests/Devices/TimerAsync"
    "../tests/layers/cliprects|main|Tests/Layers/ClipRects"
    "../tests/layers/layer_info|main|Tests/Layers/LayerInfo"
    "../tests/layers/locking|main|Tests/Layers/Locking"
    "../tests/shell/alias|main|Tests/Shell/Alias/Test"
    "../tests/shell/controlflow|main|Tests/Shell/ControlFlow/Test"
    "../tests/shell/script|main|Tests/Shell/Script/Test"
    "../tests/shell/variables|main|Tests/Shell/Variables/Test"
    "../tests/icon/tooltype|main|Tests/Icon/ToolType"
    "../tests/iffparse/basic|main|Tests/IffParse/Basic"
    "../tests/datatypes/test_basic|main|Tests/DataTypes/Basic"
    "../tests/stress/filesystem|main|Tests/Stress/Filesystem"
    "../tests/stress/memory|main|Tests/Stress/Memory"
    "../tests/stress/tasks|main|Tests/Stress/Tasks"
    "../tests/apps/dopus|main|Tests/Apps/Dopus"
    "../tests/apps/sysinfo|main|Tests/Apps/SysInfo"
    "../tests/apps/kickpascal2|main|Tests/Apps/KickPascal2"
)

# === Copy Test Data Files ===
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../tests/iffparse/basic/T:test.iff" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Samples/Tests/IffParse")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../tests/shell/alias/script.txt" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Samples/Tests/Shell/Alias")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../tests/shell/controlflow/script.txt" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Samples/Tests/Shell/ControlFlow")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../tests/shell/script/script.txt" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Samples/Tests/Shell/Script")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../tests/shell/variables/script.txt" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Samples/Tests/Shell/Variables")

foreach(sample_info ${SAMPLES_LIST})
    string(REPLACE "|" ";" args ${sample_info})
    list(GET args 0 dir)
    list(GET args 1 filename)
    list(GET args 2 output_name)

    set(src "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/${filename}.c")
    set(output "${CMAKE_CURRENT_BINARY_DIR}/Samples/${output_name}")
    
    add_custom_command(
        OUTPUT ${output}
        COMMAND ${M68K_GCC_BIN} ${SAMPLES_CFLAGS} ${SAMPLES_LDFLAGS} -o ${output} ${src}
        DEPENDS ${src}
        COMMENT "Building Sample: ${output_name}"
    )
    
    list(APPEND ALL_SAMPLES ${output})
    
    # Install to system template
    install(FILES ${output}
        DESTINATION share/lxa/System/Samples
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
endforeach()

foreach(test_info ${TESTS_LIST})
    string(REPLACE "|" ";" args ${test_info})
    list(GET args 0 dir)
    list(GET args 1 filename)
    list(GET args 2 output_name)

    set(src "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/${filename}.c")
    set(output "${CMAKE_CURRENT_BINARY_DIR}/Samples/${output_name}")
    
    # Ensure directory exists for output
    get_filename_component(output_dir ${output} DIRECTORY)
    file(MAKE_DIRECTORY ${output_dir})

    add_custom_command(
        OUTPUT ${output}
        COMMAND ${M68K_GCC_BIN} ${SAMPLES_CFLAGS} ${SAMPLES_LDFLAGS} -o ${output} ${src}
        DEPENDS ${src}
        COMMENT "Building Test: ${output_name}"
    )
    
    list(APPEND ALL_SAMPLES ${output})
    
    # Install to system template
    get_filename_component(rel_dir ${output_name} DIRECTORY)
    install(FILES ${output}
        DESTINATION share/lxa/System/Samples/${rel_dir}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
endforeach()

# Custom target to build all samples
add_custom_target(lxa-samples ALL DEPENDS ${ALL_SAMPLES})

# Add dependency to ensure samples are built
add_dependencies(lxa-samples lxa.rom)
