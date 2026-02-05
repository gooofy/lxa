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
    "utility|tag1|Tag1"
    "utility|hooks1|Hooks1"
    "utility|istr|IStr"
    "utility|a2d|A2D"
    "math|ffpexample|FFPExample"
    "iffparse|sift|Sift"
    "expansion|findboards|FindBoards"
    "asl|filereq|FileReq"
    "asl|fontreq|FontReq"
)

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

# Custom target to build all samples
add_custom_target(lxa-samples ALL DEPENDS ${ALL_SAMPLES})

# Add dependency to ensure samples are built
add_dependencies(lxa-samples lxa.rom)
