# CMakeLists.txt for lxa test drivers
#
# Test drivers are host-side C programs that use liblxa to control
# the emulator programmatically for automated testing.

# Only build test drivers if liblxa is available
if(NOT TARGET liblxa)
    message(WARNING "liblxa not found, test drivers will not be built")
    return()
endif()

# Common settings for all test drivers
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

# UpdateStrGad - string gadget test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/updatestrgad_test.c)
    add_test_driver(updatestrgad_test)
endif()

# SimpleGTGadget - GadTools gadget test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/simplegtgadget_test.c)
    add_test_driver(simplegtgadget_test)
endif()

message(STATUS "Test drivers configured")
