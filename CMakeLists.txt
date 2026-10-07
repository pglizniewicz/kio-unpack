# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2026 kio-unpack contributors

cmake_minimum_required(VERSION 3.20)

project(kio-unpack VERSION 0.1.0 LANGUAGES CXX)

set(QT_MIN_VERSION "6.8.0")
set(KF_MIN_VERSION "6.13.0")
set(LIBARCHIVE_MIN_VERSION "3.3.3")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

# ADR 0014: KDE compiler settings at level 6.13 (C++20, KDE warning set and
# Qt definitions). The level cannot be newer than the ECM version required above.
set(KDE_COMPILERSETTINGS_LEVEL 6.13.0)

include(KDEInstallDirs6)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMDeprecationSettings)
include(KDEClangFormat)
include(FeatureSummary)

# KDECompilerSettings disables exceptions; the core reports every failure by
# throwing, and the worker maps it to a KIO error (docs/architecture.md, section 4).
kde_enable_exceptions()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(KIO_UNPACK_WERROR "Treat compiler warnings as errors (ADR 0014)" ON)
if(KIO_UNPACK_WERROR)
    add_compile_options(-Werror)
endif()

option(KIO_UNPACK_PROTOTYPES "Build the throwaway spike programs in prototypes/" OFF)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS KIO CoreAddons I18n)
find_package(LibArchive ${LIBARCHIVE_MIN_VERSION} REQUIRED)
set_package_properties(LibArchive PROPERTIES
    URL "https://libarchive.org"
    DESCRIPTION "Reads ISO 9660 images and archive formats"
    TYPE REQUIRED)

if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
endif()

ecm_set_disabled_deprecation_versions(
    QT ${QT_MIN_VERSION}
    KF ${KF_MIN_VERSION}
)

add_definitions(-DTRANSLATION_DOMAIN=\"kio6_unpack\")

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(KIO_UNPACK_PROTOTYPES)
    add_subdirectory(prototypes)
endif()

# ADR 0014: KDE formatting for our code; prototypes/ is left out.
file(GLOB_RECURSE KIO_UNPACK_FORMAT_FILES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.cpp ${CMAKE_SOURCE_DIR}/src/*.h
    ${CMAKE_SOURCE_DIR}/tests/*.cpp ${CMAKE_SOURCE_DIR}/tests/*.h)
kde_clang_format(${KIO_UNPACK_FORMAT_FILES})

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
