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
endif()
