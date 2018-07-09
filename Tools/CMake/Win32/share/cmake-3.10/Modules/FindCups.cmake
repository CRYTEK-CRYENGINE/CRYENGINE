# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindCups
# --------
#
# Try to find the Cups printing system
#
# Once done this will define
#
# ::
#
#   CUPS_FOUND - system has Cups
#   CUPS_INCLUDE_DIR - the Cups include directory
#   CUPS_LIBRARIES - Libraries needed to use Cups
#   CUPS_VERSION_STRING - version of Cups found (since CMake 2.8.8)
#   Set CUPS_REQUIRE_IPP_DELETE_ATTRIBUTE to TRUE if you need a version which
#   features this function (i.e. at least 1.1.19)

find_path(CUPS_INCLUDE_DIR cups/cups.h )

find_library(CUPS_LIBRARIES NAMES cups )

if (CUPS_INCLUDE_DIR AND CUPS_LIBRARIES AND CUPS_REQUIRE_IPP_DELETE_ATTRIBUTE)
    include(${CMAKE_CURRENT_LIST_DIR}/CheckLibraryExists.cmake)
    include(${CMAKE_CURRENT_LIST_DIR}/CMakePushCheckState.cmake)
    cmake_push_check_state()
    set(CMAKE_REQUIRED_QUIET ${Cups_FIND_QUIETLY})

    # ippDeleteAttribute is new in cups-1.1.19 (and used by kdeprint)
    CHECK_LIBRARY_EXISTS(cups ippDeleteAttribute "" CUPS_HAS_IPP_DELETE_ATTRIBUTE)
    cmake_pop_check_state()
endif ()

if (CUPS_INCLUDE_DIR AND EXISTS "${CUPS_INCLUDE_DIR}/cups/cups.h")
    file(STRINGS "${CUPS_INCLUDE_DIR}/cups/cups.h" cups_version_str
         REGEX "^#[\t ]*define[\t ]+CUPS_VERSION_(MAJOR|MINOR|PATCH)[\t ]+[0-9]+$")

    unset(CUPS_VERSION_STRING)
    foreach(VPART MAJOR MINOR PATCH)
        foreach(VLINE ${cups_version_str})
            if(VLINE MATCHES "^#[\t ]*define[\t ]+CUPS_VERSION_${VPART}[\t ]+([0-9]+)$")
                set(CUPS_VERSION_PART "${CMAKE_MATCH_1}")
                if(CUPS_VERSION_STRING)
                    string(APPEND CUPS_VERSION_STRING ".${CUPS_VERSION_PART}")
                else()
                    set(CUPS_VERSION_STRING "${CUPS_VERSION_PART}")
                endif()
            endif()
        endforeach()
    endforeach()
endif ()

include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)

if (CUPS_REQUIRE_IPP_DELETE_ATTRIBUTE)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(Cups
                                      REQUIRED_VARS CUPS_LIBRARIES CUPS_INCLUDE_DIR CUPS_HAS_IPP_DELETE_ATTRIBUTE
                                      VERSION_VAR CUPS_VERSION_STRING)
else ()
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(Cups
                                      REQUIRED_VARS CUPS_LIBRARIES CUPS_INCLUDE_DIR
                                      VERSION_VAR CUPS_VERSION_STRING)
endif ()

mark_as_advanced(CUPS_INCLUDE_DIR CUPS_LIBRARIES)
