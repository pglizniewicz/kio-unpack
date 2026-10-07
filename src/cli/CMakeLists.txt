# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2026 kio-unpack contributors

# Development tool (REQ-103); built into build/bin, never installed.
add_executable(kio-unpack-cli main.cpp)
target_link_libraries(kio-unpack-cli PRIVATE kio-unpack-core)
