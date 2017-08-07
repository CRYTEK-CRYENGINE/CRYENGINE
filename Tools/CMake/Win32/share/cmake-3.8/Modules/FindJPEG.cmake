# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindJPEG
# --------
#
# Find JPEG
#
# Find the native JPEG includes and library This module defines
#
# ::
#
#   JPEG_INCLUDE_DIR, where to find jpeglib.h, etc.
#   JPEG_LIBRARIES, the libraries needed to use JPEG.
#   JPEG_FOUND, If false, do not try to use JPEG.
#
# also defined, but not for general use are
#
# ::
#
#   JPEG_LIBRARY, where to find the JPEG library.

find_path(JPEG_INCLUDE_DIR jpeglib.h)

set(JPEG_NAMES ${JPEG_NAMES} jpeg libjpeg)
find_library(JPEG_LIBRARY NAMES ${JPEG_NAMES} )

include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(JPEG DEFAULT_MSG JPEG_LIBRARY JPEG_INCLUDE_DIR)

if(JPEG_FOUND)
  set(JPEG_LIBRARIES ${JPEG_LIBRARY})
endif()

# Deprecated declarations.
set (NATIVE_JPEG_INCLUDE_PATH ${JPEG_INCLUDE_DIR} )
if(JPEG_LIBRARY)
  get_filename_component (NATIVE_JPEG_LIB_PATH ${JPEG_LIBRARY} PATH)
endif()

mark_as_advanced(JPEG_LIBRARY JPEG_INCLUDE_DIR )
