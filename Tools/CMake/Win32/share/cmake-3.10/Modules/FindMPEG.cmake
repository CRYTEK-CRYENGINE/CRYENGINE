# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindMPEG
# --------
#
# Find the native MPEG includes and library
#
# This module defines
#
# ::
#
#   MPEG_INCLUDE_DIR, where to find MPEG.h, etc.
#   MPEG_LIBRARIES, the libraries required to use MPEG.
#   MPEG_FOUND, If false, do not try to use MPEG.
#
# also defined, but not for general use are
#
# ::
#
#   MPEG_mpeg2_LIBRARY, where to find the MPEG library.
#   MPEG_vo_LIBRARY, where to find the vo library.

find_path(MPEG_INCLUDE_DIR mpeg2dec/include/video_out.h
  /usr/local/livid
)

find_library(MPEG_mpeg2_LIBRARY mpeg2
  /usr/local/livid/mpeg2dec/libmpeg2/.libs
)

find_library( MPEG_vo_LIBRARY vo
  /usr/local/livid/mpeg2dec/libvo/.libs
)

include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(MPEG DEFAULT_MSG MPEG_INCLUDE_DIR MPEG_mpeg2_LIBRARY MPEG_vo_LIBRARY)

if(MPEG_FOUND)
  set( MPEG_LIBRARIES ${MPEG_mpeg2_LIBRARY} ${MPEG_vo_LIBRARY} )
endif()

mark_as_advanced(MPEG_INCLUDE_DIR MPEG_mpeg2_LIBRARY MPEG_vo_LIBRARY)
