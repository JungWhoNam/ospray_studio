## Copyright 2020 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

if(ospray_FOUND)
    return()
endif()

## Look for any available version
message(STATUS "Looking for OSPRay...")
find_package(ospray QUIET)

if(NOT DEFINED OSPRAY_VERSION)
  set(OSPRAY_VERSION 2.12.0)
endif()

if(ospray_FOUND)
    message(STATUS "Found OSPRay")
else()
    ## Download and build if not found
    if(WIN32)
        set(_ARCHIVE_EXT "windows.zip")
    elseif(APPLE)
        set(_ARCHIVE_EXT "macosx.zip")
    else()
        set(_ARCHIVE_EXT "linux.tar.gz")
    endif()

    if(NOT DEFINED OSPRAY_URL)
    set(OSPRAY_URL "https://github.com/ospray/ospray/releases/download/v${OSPRAY_VERSION}/ospray-${OSPRAY_VERSION}.x86_64.${_ARCHIVE_EXT}")
    endif()

    include(FetchContent)

    message(STATUS "Downloading ospray ${OSPRAY_URL}...")
    FetchContent_Declare(
        ospray
        URL "${OSPRAY_URL}"
    )
    FetchContent_MakeAvailable(ospray)
    ## This library is pre-built, so simply "find" the package
    set(ospray_ROOT "${ospray_SOURCE_DIR}")

    ## Ensure same version of TBB is found that pre-build library was compiled with.
    set(FORCE_TBB_VERSION ON)

    find_package(ospray ${OSPRAY_VERSION} REQUIRED)

    unset(${_ARCHIVE_EXT})

endif()

## The build of rkcommon depends on settings related to downloading OSPRay
include(rkcommon)
