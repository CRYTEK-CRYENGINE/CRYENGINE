# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindMPEG2
# ---------
#
# Find the native MPEG2 includes and library
#
# This module defines
#
# ::
#
#   MPEG2_INCLUDE_DIR, path to mpeg2dec/mpeg2.h, etc.
#   MPEG2_LIBRARIES, the libraries required to use MPEG2.
#   MPEG2_FOUND, If false, do not try to use MPEG2.
#
# also defined, but not for general use are
#
# ::
#
#   MPEG2_mpeg2_LIBRARY, where to find the MPEG2 library.
#   MPEG2_vo_LIBRARY, where to find the vo library.

find_path(MPEG2_INCLUDE_DIR
  NAMES mpeg2.h mpeg2dec/mpeg2.h
  PATHS /usr/local/livid
)

find_library(MPEG2_mpeg2_LIBRARY mpeg2
  /usr/local/livid/mpeg2dec/libmpeg2/.libs
)

find_library( MPEG2_vo_LIBRARY vo
  /usr/local/livid/mpeg2dec/libvo/.libs
)

include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(MPEG2 DEFAULT_MSG MPEG2_mpeg2_LIBRARY MPEG2_INCLUDE_DIR)

if(MPEG2_FOUND)
  set( MPEG2_LIBRARIES ${MPEG2_mpeg2_LIBRARY}
                        ${MPEG2_vo_LIBRARY})

  #some native mpeg2 installations will depend
  #on libSDL, if found, add it in.
  include(${CMAKE_CURRENT_LIST_DIR}/FindSDL.cmake)
  if(SDL_FOUND)
    set( MPEG2_LIBRARIES ${MPEG2_LIBRARIES} ${SDL_LIBRARY})
  endif()
endif()

mark_as_advanced(MPEG2_INCLUDE_DIR MPEG2_mpeg2_LIBRARY MPEG2_vo_LIBRARY)
