# The game's launcher, "Street Rod 2": the game folder, graphics and sound, the game's own
# switches and the window options, then sr2port.
#
#   cmake -S launcher -B launcher/build -G Ninja && cmake --build launcher/build
#
# or, from the main project, -DSR2_LAUNCHER=ON builds it next to srport.exe.
# Needs wxWidgets 3.2 (MSYS2: mingw-w64-x86_64-wxwidgets3.2-msw; Debian and Ubuntu: libwxgtk3.2-dev).
cmake_minimum_required(VERSION 3.24)
project(sr2launcher LANGUAGES CXX)
if(WIN32)
    enable_language(RC)
endif()

# MSYS2 and wxWidgets' own CMake builds install a package config; distributions
# such as Debian only ship wx-config, which CMake's FindwxWidgets uses.
find_package(wxWidgets 3.2 CONFIG QUIET COMPONENTS core base)
if(wxWidgets_FOUND AND TARGET wx::wxcore)
    set(SR2L_WX_LIBRARIES wx::wxcore wx::wxbase)
else()
    find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base)
    include(${wxWidgets_USE_FILE})
    set(SR2L_WX_LIBRARIES ${wxWidgets_LIBRARIES})
endif()

add_executable(sr2launcher WIN32
    app.cpp
    game.cpp
    icon.cpp
    launcher.cpp
    settings.cpp
)
set_target_properties(sr2launcher PROPERTIES OUTPUT_NAME "Street Rod 2" CXX_EXTENSIONS OFF)
target_compile_features(sr2launcher PRIVATE cxx_std_17)
target_link_libraries(sr2launcher PRIVATE ${SR2L_WX_LIBRARIES})
# Built from the main project: beside sr2port.exe, where it looks for it.
if(NOT PROJECT_IS_TOP_LEVEL)
    set_target_properties(sr2launcher PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

if(WIN32)
    target_sources(sr2launcher PRIVATE app.rc)
    target_link_libraries(sr2launcher PRIVATE comctl32)
    if(MINGW)
        # The C++ runtime inside the exe, so the build needs fewer DLLs beside it.
        target_link_options(sr2launcher PRIVATE -static-libgcc -static-libstdc++ $<$<CONFIG:Release>:-s>)
    endif()
    # The wxWidgets DLLs and everything they load, found where the compiler's own DLLs are.
    get_filename_component(SR2L_DLL_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    add_custom_command(TARGET sr2launcher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:sr2launcher> "-DSEARCH=${SR2L_DLL_DIR}"
                -DCMAKE_OBJDUMP=${CMAKE_OBJDUMP} -P ${CMAKE_CURRENT_SOURCE_DIR}/copy_dlls.cmake)
endif()

if(MSVC)
    target_compile_options(sr2launcher PRIVATE /W4 /utf-8)
else()
    target_compile_options(sr2launcher PRIVATE -Wall -Wextra)
endif()
