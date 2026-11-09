# SPDX-License-Identifier: GPL-3.0-or-later
# Builds the bundle on any platform. On Windows, build.bat does the same without CMake.
cmake_minimum_required(VERSION 3.16)
project(bi_lumen C)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(WIN32)
    set(LIB_SUFFIX .dll)
    set(UI_CLASS ui:WindowsUI)
    set(WITH_UI ON)
elseif(APPLE)
    set(LIB_SUFFIX .dylib)
    set(UI_CLASS ui:CocoaUI)
    set(WITH_UI ON)
    enable_language(OBJC) # the Cocoa window layer in ui_lv2.c
else()
    set(LIB_SUFFIX .so)
    set(UI_CLASS ui:X11UI)
    find_package(X11)
    set(WITH_UI ${X11_FOUND})
endif()

set(BUNDLE_DIR ${CMAKE_BINARY_DIR}/bi_lumen.lv2)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

add_library(bi_lumen MODULE src/plugin.c)
set_target_properties(bi_lumen PROPERTIES
    PREFIX ""
    SUFFIX ${LIB_SUFFIX}
    C_STANDARD 11
    C_VISIBILITY_PRESET hidden
    LIBRARY_OUTPUT_DIRECTORY "$<1:${BUNDLE_DIR}>"
    RUNTIME_OUTPUT_DIRECTORY "$<1:${BUNDLE_DIR}>")

if(WITH_UI)
    add_library(bi_lumen_ui MODULE src/ui/ui_lv2.c)
    set_target_properties(bi_lumen_ui PROPERTIES
        PREFIX ""
        SUFFIX ${LIB_SUFFIX}
        C_STANDARD 11
        C_VISIBILITY_PRESET hidden
        LIBRARY_OUTPUT_DIRECTORY "$<1:${BUNDLE_DIR}>"
        RUNTIME_OUTPUT_DIRECTORY "$<1:${BUNDLE_DIR}>")
    if(WIN32)
        target_link_libraries(bi_lumen_ui PRIVATE user32 gdi32)
    elseif(APPLE)
        set_source_files_properties(src/ui/ui_lv2.c PROPERTIES LANGUAGE OBJC)
        target_link_libraries(bi_lumen_ui PRIVATE "-framework Cocoa")
    else()
        target_include_directories(bi_lumen_ui PRIVATE ${X11_INCLUDE_DIR})
        target_link_libraries(bi_lumen_ui PRIVATE ${X11_LIBRARIES} m)
    endif()
endif()

add_executable(test_dsp test/test_dsp.c)
set_target_properties(test_dsp PROPERTIES C_STANDARD 11)
target_link_libraries(test_dsp PRIVATE ${CMAKE_DL_LIBS})

add_executable(ui_render test/ui_render.c)
set_target_properties(ui_render PROPERTIES C_STANDARD 11)

if(NOT MSVC)
    foreach(T bi_lumen test_dsp ui_render)
        target_compile_options(${T} PRIVATE -Wall -Wextra)
    endforeach()
    target_link_libraries(bi_lumen PRIVATE m)
    target_link_libraries(test_dsp PRIVATE m)
    target_link_libraries(ui_render PRIVATE m)
    # ui_render draws the panel but has no mouse, so the input handlers go unused there.
    target_compile_options(ui_render PRIVATE -Wno-unused-function)
endif()

# The manifest names the Windows libraries and UI type, so other platforms get theirs.
file(READ bundle/manifest.ttl MANIFEST)
string(REPLACE "bi_lumen.dll" "bi_lumen${LIB_SUFFIX}" MANIFEST "${MANIFEST}")
string(REPLACE "bi_lumen_ui.dll" "bi_lumen_ui${LIB_SUFFIX}" MANIFEST "${MANIFEST}")
if(WITH_UI)
    string(REPLACE "ui:WindowsUI" "${UI_CLASS}" MANIFEST "${MANIFEST}")
else()
    string(REGEX REPLACE "# UI-BEGIN.*# UI-END\n?" "" MANIFEST "${MANIFEST}")
endif()
file(WRITE ${BUNDLE_DIR}/manifest.ttl "${MANIFEST}")
configure_file(bundle/bi_lumen.ttl ${BUNDLE_DIR}/bi_lumen.ttl COPYONLY)
# Licence, notices and the AI usage statement travel with the bundle.
foreach(DOC LICENSE NOTICE.md AI_USAGE.md)
    configure_file(${DOC} ${BUNDLE_DIR}/${DOC} COPYONLY)
endforeach()
message(STATUS "Davidian Bi-Lumen: GPL-3.0-or-later, made with generative AI (Claude Code); see AI_USAGE.md")

enable_testing()
add_test(NAME test_dsp COMMAND test_dsp ${BUNDLE_DIR})
add_test(NAME ui_render COMMAND ui_render ${CMAKE_BINARY_DIR}/panel.bmp)

# "cmake --install build-cmake --prefix ~/.lv2" style installs: the bundle goes in <prefix>/lib/lv2
# by default, or straight into the prefix with -DLV2_INSTALL_DIR=.
set(LV2_INSTALL_DIR lib/lv2 CACHE STRING "Folder under the install prefix that receives the bundle")
install(DIRECTORY ${BUNDLE_DIR} DESTINATION ${LV2_INSTALL_DIR})
