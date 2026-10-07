# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2026 kio-unpack contributors

# libkio-unpack-core: Qt Core + KI18n, no KIO (docs/architecture.md, section 3).
# Static and position-independent, so the worker plugin, the command-line tool
# and the tests link the same code; it is never installed.
add_library(kio-unpack-core STATIC
    core/error.cpp
    core/filebytesource.cpp
    core/sniff.cpp
    core/entrytable.cpp
    core/pathresolver.cpp
    backends/libarchive/libarchivevolume.cpp
)
set_target_properties(kio-unpack-core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(kio-unpack-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kio-unpack-core
    PUBLIC Qt6::Core KF6::I18n
    PRIVATE LibArchive::LibArchive
)

add_subdirectory(cli)
add_subdirectory(worker)
