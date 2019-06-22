# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindLibXslt
# -----------
#
# Try to find the LibXslt library
#
# Once done this will define
#
# ::
#
#   LIBXSLT_FOUND - system has LibXslt
#   LIBXSLT_INCLUDE_DIR - the LibXslt include directory
#   LIBXSLT_LIBRARIES - Link these to LibXslt
#   LIBXSLT_DEFINITIONS - Compiler switches required for using LibXslt
#   LIBXSLT_VERSION_STRING - version of LibXslt found (since CMake 2.8.8)
#
# Additionally, the following two variables are set (but not required
# for using xslt):
#
# ``LIBXSLT_EXSLT_LIBRARIES``
#   Link to these if you need to link against the exslt library.
# ``LIBXSLT_XSLTPROC_EXECUTABLE``
#   Contains the full path to the xsltproc executable if found.

# use pkg-config to get the directories and then use these values
# in the find_path() and find_library() calls
find_package(PkgConfig QUIET)
PKG_CHECK_MODULES(PC_LIBXSLT QUIET libxslt)
set(LIBXSLT_DEFINITIONS ${PC_LIBXSLT_CFLAGS_OTHER})

find_path(LIBXSLT_INCLUDE_DIR NAMES libxslt/xslt.h
    HINTS
   ${PC_LIBXSLT_INCLUDEDIR}
   ${PC_LIBXSLT_INCLUDE_DIRS}
  )

find_library(LIBXSLT_LIBRARIES NAMES xslt libxslt
    HINTS
   ${PC_LIBXSLT_LIBDIR}
   ${PC_LIBXSLT_LIBRARY_DIRS}
  )

find_library(LIBXSLT_EXSLT_LIBRARY NAMES exslt libexslt
    HINTS
    ${PC_LIBXSLT_LIBDIR}
    ${PC_LIBXSLT_LIBRARY_DIRS}
  )

set(LIBXSLT_EXSLT_LIBRARIES ${LIBXSLT_EXSLT_LIBRARY} )

find_program(LIBXSLT_XSLTPROC_EXECUTABLE xsltproc)

if(PC_LIBXSLT_VERSION)
    set(LIBXSLT_VERSION_STRING ${PC_LIBXSLT_VERSION})
elseif(LIBXSLT_INCLUDE_DIR AND EXISTS "${LIBXSLT_INCLUDE_DIR}/libxslt/xsltconfig.h")
    file(STRINGS "${LIBXSLT_INCLUDE_DIR}/libxslt/xsltconfig.h" libxslt_version_str
         REGEX "^#define[\t ]+LIBXSLT_DOTTED_VERSION[\t ]+\".*\"")

    string(REGEX REPLACE "^#define[\t ]+LIBXSLT_DOTTED_VERSION[\t ]+\"([^\"]*)\".*" "\\1"
           LIBXSLT_VERSION_STRING "${libxslt_version_str}")
    unset(libxslt_version_str)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibXslt
                                  REQUIRED_VARS LIBXSLT_LIBRARIES LIBXSLT_INCLUDE_DIR
                                  VERSION_VAR LIBXSLT_VERSION_STRING)

mark_as_advanced(LIBXSLT_INCLUDE_DIR
                 LIBXSLT_LIBRARIES
                 LIBXSLT_EXSLT_LIBRARY
                 LIBXSLT_XSLTPROC_EXECUTABLE)
