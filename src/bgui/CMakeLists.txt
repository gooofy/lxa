# CMakeLists.txt for bgui.library
#
# Builds bgui.library 42.x from the vendored source tree under src/bgui/
# (originally from others/bgui/BGUI/) using m68k-amigaos-gcc (bebbo).
#
# Output: ${CMAKE_BINARY_DIR}/target/bgui/bgui.library
# Installed to: share/lxa/System/Libs/bgui.library
#
# This replaces the prebuilt SAS/C bgui.library that previously shipped
# in share/lxa/System/Libs/.  Building from source is the prerequisite
# for instrumenting BGUI's window-sizing logic when investigating
# Typeface's window-size mismatch (Phase 136 follow-up).

if(NOT M68K_GCC)
    return()
endif()

set(M68K_PREFIX "/opt/amiga")
set(M68K_GCC_BIN "${M68K_PREFIX}/bin/m68k-amigaos-gcc")

# Sources: every .c file in src/bgui/ except the ones that are AROS-only
# scaffolding, build tools, or unused replacements:
#
#   bgui_init.c       AROS-only (uses <libcore/libheader.c>); on classic
#                     Amiga, lib.c is the LibInit replacement.
#   MakeProto.c       Build tool — generates prototype headers.
#   MakeProtoANSI.c   Build tool — same, ANSI variant.
#   Tap.c             Standalone debug-tap utility, not part of library.
#   KillBGUI.c        Standalone tool to kill a running BGUI library.
#
set(BGUI_SOURCES
    areaclass.c
    arexxclass.c
    aslreqclass.c
    baseclass.c
    bgui_locale.c
    blitter.c
    buttonclass.c
    checkboxclass.c
    classes.c
    commodityclass.c
    cycleclass.c
    dgm.c
    endtag.c
    externalclass.c
    filereqclass.c
    fontreqclass.c
    frameclass.c
    gfx.c
    groupclass.c
    indicatorclass.c
    infoclass.c
    labelclass.c
    lib.c
    libfunc.c
    libtag.c
    listclass.c
    misc.c
    miscc.c
    mxclass.c
    pageclass.c
    progressclass.c
    propclass.c
    radiobuttonclass.c
    request.c
    rootclass.c
    screenreqclass.c
    separatorclass.c
    sliderclass.c
    spacingclass.c
    stkext.c
    strformat.c
    stringclass.c
    systemiclass.c
    task.c
    textclass.c
    vectorclass.c
    ver.c
    viewclass.c
    windowclass.c
    # lxa runtime shims (kprintf, exit, stch_l, stcu_d, AsmCreatePool, etc.)
    lxa_shims.c
)

set(BGUI_SRC_DIR  ${CMAKE_CURRENT_SOURCE_DIR})
set(BGUI_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
file(MAKE_DIRECTORY ${BGUI_BUILD_DIR})

# Per-source object compile.  Flags:
#
#   -fbaserel     BGUI relies on A4 holding its global-data pointer at
#                 four sites in classes.c / listclass.c.  See lxa_shims.c
#                 for the matching __restore_a4 stub rationale.
#   -noixemul     Don't pull in ixemul.
#   -O2           Standard production opt level.
#   -m68000       lxa's baseline CPU.
#   -DLIBRARY_BUILD
#                 Selects the library-build path inside BGUI source
#                 (mainly affects lib.c / libtag.c).
#
# Includes ordering matters:
#   - .             so e.g. bgui_compilerspecific.h is found via "..."
#   - include       primary BGUI headers
#   - build_include shim dir mapping <bgui/...> and <libraries/bgui*.h>
#                   to the vendored include/ tree
#
set(BGUI_CFLAGS
    -O2
    -m68000
    -noixemul
    -fbaserel
    -DLIBRARY_BUILD
    -I${BGUI_SRC_DIR}/.
    -I${BGUI_SRC_DIR}/include
    -I${BGUI_SRC_DIR}/build_include
    -Wno-int-conversion
)

set(BGUI_OBJECTS)
foreach(src ${BGUI_SOURCES})
    set(obj "${BGUI_BUILD_DIR}/${src}.o")
    add_custom_command(
        OUTPUT ${obj}
        COMMAND ${M68K_GCC_BIN} ${BGUI_CFLAGS}
                -c ${BGUI_SRC_DIR}/${src} -o ${obj}
        DEPENDS ${BGUI_SRC_DIR}/${src}
        COMMENT "BGUI: compile ${src}"
        VERBATIM
    )
    list(APPEND BGUI_OBJECTS ${obj})
endforeach()

set(BGUI_LIBRARY ${BGUI_BUILD_DIR}/bgui.library)

add_custom_command(
    OUTPUT ${BGUI_LIBRARY}
    COMMAND ${M68K_GCC_BIN}
            -shared -nostartfiles -noixemul -fbaserel -m68000
            ${BGUI_OBJECTS}
            -o ${BGUI_LIBRARY}
    DEPENDS ${BGUI_OBJECTS}
    COMMENT "BGUI: link bgui.library"
    VERBATIM
)

add_custom_target(bgui_library ALL DEPENDS ${BGUI_LIBRARY})

# Stage into the source tree's share/lxa/System/Libs/ so existing
# launch.json / test setup picks it up without any extra plumbing,
# replacing the previously-prebuilt SAS/C binary.
add_custom_command(TARGET bgui_library POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${BGUI_LIBRARY}
            ${CMAKE_SOURCE_DIR}/share/lxa/System/Libs/bgui.library
    COMMENT "BGUI: stage into share/lxa/System/Libs/"
)

install(FILES ${BGUI_LIBRARY}
    DESTINATION share/lxa/System/Libs
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
)
