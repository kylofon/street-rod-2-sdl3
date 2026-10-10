cmake_minimum_required(VERSION 3.24)
project(sr2port C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Port of Street Rod 2's SR.EXE, VGA 16-colour mode (0Dh). AdLib sound (S5.4) runs on Nuked-OPL3. CGA / Tandy / Hercules are parked.
set(SR2PORT_VERSION 0.0.1)

find_package(SDL3 REQUIRED CONFIG)

file(GLOB_RECURSE SR_SOURCES CONFIGURE_DEPENDS src/*.c)

# Nuked-OPL3 (LGPL-2.1+, third_party/nuked-opl3/LICENSE); the AdLib chip (host.c).
add_library(nuked_opl3 STATIC third_party/nuked-opl3/opl3.c)
target_include_directories(nuked_opl3 PUBLIC third_party/nuked-opl3)

add_executable(sr2port ${SR_SOURCES})
target_include_directories(sr2port PRIVATE src)
target_link_libraries(sr2port PRIVATE SDL3::SDL3 nuked_opl3)
target_compile_definitions(sr2port PRIVATE SR2PORT_VERSION="${SR2PORT_VERSION}")
if(MSVC)
    target_compile_options(sr2port PRIVATE /W3)
else()
    # The memory model reads multi-byte values through byte pointers (see src/mem.h).
    target_compile_options(sr2port PRIVATE -Wall -Wextra -Wno-unused-parameter -fno-strict-aliasing)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_compile_options(sr2port PRIVATE -Wno-format-truncation)
    endif()
endif()
if(WIN32)
    add_custom_command(TARGET sr2port POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:sr2port> $<TARGET_FILE_DIR:sr2port>
        COMMAND_EXPAND_LISTS)
    # And what those load in turn (MSYS2's SDL3.dll needs libiconv-2.dll), so the game starts outside
    # the MSYS2 shell, e.g. from the launcher.
    get_filename_component(SR2_DLL_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
    add_custom_command(TARGET sr2port POST_BUILD
        COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:sr2port> "-DSEARCH=${SR2_DLL_DIR}"
                -DCMAKE_OBJDUMP=${CMAKE_OBJDUMP} -P ${CMAKE_CURRENT_SOURCE_DIR}/launcher/copy_dlls.cmake)
endif()

# The launcher ("Street Rod 2"): the game folder, graphics and sound, the game's own switches and the
# window options; built beside sr2port.exe. Needs wxWidgets 3.2 (MSYS2: mingw-w64-x86_64-wxwidgets3.2-msw)
# and a C++17 compiler. ON/OFF forces it; by default it is built when wxWidgets 3.2 is found.
set(SR2_LAUNCHER "AUTO" CACHE STRING "Build the launcher, Street Rod 2.exe (wxWidgets): AUTO, ON or OFF")
set_property(CACHE SR2_LAUNCHER PROPERTY STRINGS AUTO ON OFF)
if(SR2_LAUNCHER STREQUAL "AUTO")
    find_package(wxWidgets 3.2 CONFIG QUIET COMPONENTS core base)
    if(NOT wxWidgets_FOUND)
        find_package(wxWidgets 3.2 QUIET COMPONENTS core base)
    endif()
    if(wxWidgets_FOUND)
        enable_language(CXX)
        set(SR2_BUILD_LAUNCHER ON)
    else()
        message(STATUS "wxWidgets 3.2 not found: the launcher is not built (-DSR2_LAUNCHER=ON to require it)")
    endif()
elseif(SR2_LAUNCHER)
    enable_language(CXX)
    set(SR2_BUILD_LAUNCHER ON)
endif()
if(SR2_BUILD_LAUNCHER)
    add_subdirectory(launcher)
endif()
