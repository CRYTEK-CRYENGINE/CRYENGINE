# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindGIF
# -------
#
# This finds the GIF library (giflib)
#
# The module defines the following variables:
#
# ``GIF_FOUND``
#   True if giflib was found
# ``GIF_LIBRARIES``
#   Libraries to link to in order to use giflib
# ``GIF_INCLUDE_DIR``
#   where to find the headers
# ``GIF_VERSION``
#   3, 4 or a full version string (eg 5.1.4) for versions >= 4.1.6
#
# The minimum required version of giflib can be specified using the
# standard syntax, e.g.  find_package(GIF 4)
#
# $GIF_DIR is an environment variable that would correspond to the
# ./configure --prefix=$GIF_DIR

# Created by Eric Wing.
# Modifications by Alexander Neundorf, Ben Campbell

find_path(GIF_INCLUDE_DIR gif_lib.h
  HINTS
    ENV GIF_DIR
  PATH_SUFFIXES include
  PATHS
  ~/Library/Frameworks
  /usr/freeware
)

# the gif library can have many names :-/
set(POTENTIAL_GIF_LIBS gif libgif ungif libungif giflib giflib4)

find_library(GIF_LIBRARY
  NAMES ${POTENTIAL_GIF_LIBS}
  HINTS
    ENV GIF_DIR
  PATH_SUFFIXES lib
  PATHS
  ~/Library/Frameworks
  /usr/freeware
)

# see readme.txt
set(GIF_LIBRARIES ${GIF_LIBRARY})

# Very basic version detection.
# The GIF_LIB_VERSION string in gif_lib.h seems to be unreliable, since it seems
# to be always " Version 2.0, " in versions 3.x of giflib.
# In version 4 the member UserData was added to GifFileType, so we check for this
# one.
# Versions after 4.1.6 define GIFLIB_MAJOR, GIFLIB_MINOR, and GIFLIB_RELEASE
# see http://giflib.sourceforge.net/gif_lib.html#compatibility
if(GIF_INCLUDE_DIR)
  include(${CMAKE_CURRENT_LIST_DIR}/CMakePushCheckState.cmake)
  include(${CMAKE_CURRENT_LIST_DIR}/CheckStructHasMember.cmake)
  CMAKE_PUSH_CHECK_STATE()
  set(CMAKE_REQUIRED_QUIET ${GIF_FIND_QUIETLY})
  set(CMAKE_REQUIRED_INCLUDES "${GIF_INCLUDE_DIR}")

  # Check for the specific version defines (>=4.1.6 only)
  file(STRINGS ${GIF_INCLUDE_DIR}/gif_lib.h _GIF_DEFS REGEX "^[ \t]*#define[ \t]+GIFLIB_(MAJOR|MINOR|RELEASE)")
  if(_GIF_DEFS)
    # yay - got exact version info
    string(REGEX REPLACE ".*GIFLIB_MAJOR ([0-9]+).*" "\\1" _GIF_MAJ "${_GIF_DEFS}")
    string(REGEX REPLACE ".*GIFLIB_MINOR ([0-9]+).*" "\\1" _GIF_MIN "${_GIF_DEFS}")
    string(REGEX REPLACE ".*GIFLIB_RELEASE ([0-9]+).*" "\\1" _GIF_REL "${_GIF_DEFS}")
    set(GIF_VERSION "${_GIF_MAJ}.${_GIF_MIN}.${_GIF_REL}")
  else()
    # use UserData field to sniff version instead
    CHECK_STRUCT_HAS_MEMBER(GifFileType UserData gif_lib.h GIF_GifFileType_UserData )
    if(GIF_GifFileType_UserData)
      set(GIF_VERSION 4)
    else()
      set(GIF_VERSION 3)
    endif()
  endif()

  unset(_GIF_MAJ)
  unset(_GIF_MIN)
  unset(_GIF_REL)
  unset(_GIF_DEFS)
  CMAKE_POP_CHECK_STATE()
endif()

include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GIF  REQUIRED_VARS  GIF_LIBRARY  GIF_INCLUDE_DIR
                                       VERSION_VAR GIF_VERSION )

mark_as_advanced(GIF_INCLUDE_DIR GIF_LIBRARY)
