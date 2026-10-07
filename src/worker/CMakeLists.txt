# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2026 kio-unpack contributors

# Lands in build/bin/kf6/kio/kio_unpack.so, where the tests, kioclient and the
# dev-loop Dolphin find it through QT_PLUGIN_PATH (ADR 0009).
kcoreaddons_add_plugin(kio_unpack INSTALL_NAMESPACE "kf6/kio")
target_sources(kio_unpack PRIVATE
    main.cpp
    unpackworker.cpp
    unpack.json
)
target_link_libraries(kio_unpack PRIVATE kio-unpack-core KF6::KIOCore KF6::I18n)
