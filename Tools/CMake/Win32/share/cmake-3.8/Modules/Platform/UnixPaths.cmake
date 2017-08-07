# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


# Block multiple inclusion because "CMakeCInformation.cmake" includes
# "Platform/${CMAKE_SYSTEM_NAME}" even though the generic module
# "CMakeSystemSpecificInformation.cmake" already included it.
# The extra inclusion is a work-around documented next to the include()
# call, so this can be removed when the work-around is removed.
if(__UNIX_PATHS_INCLUDED)
  return()
endif()
set(__UNIX_PATHS_INCLUDED 1)

set(UNIX 1)

# also add the install directory of the running cmake to the search directories
# CMAKE_ROOT is CMAKE_INSTALL_PREFIX/share/cmake, so we need to go two levels up
get_filename_component(_CMAKE_INSTALL_DIR "${CMAKE_ROOT}" PATH)
get_filename_component(_CMAKE_INSTALL_DIR "${_CMAKE_INSTALL_DIR}" PATH)

# List common installation prefixes.  These will be used for all
# search types.
list(APPEND CMAKE_SYSTEM_PREFIX_PATH
  # Standard
  /usr/local /usr /

  # CMake install location
  "${_CMAKE_INSTALL_DIR}"
  )
if (NOT CMAKE_FIND_NO_INSTALL_PREFIX)
  list(APPEND CMAKE_SYSTEM_PREFIX_PATH
    # Project install destination.
    "${CMAKE_INSTALL_PREFIX}"
  )
  if(CMAKE_STAGING_PREFIX)
    list(APPEND CMAKE_SYSTEM_PREFIX_PATH
      # User-supplied staging prefix.
      "${CMAKE_STAGING_PREFIX}"
    )
  endif()
endif()

# List common include file locations not under the common prefixes.
list(APPEND CMAKE_SYSTEM_INCLUDE_PATH
  # Windows API on Cygwin
  /usr/include/w32api

  # X11
  /usr/X11R6/include /usr/include/X11

  # Other
  /usr/pkg/include
  /opt/csw/include /opt/include
  /usr/openwin/include
  )

list(APPEND CMAKE_SYSTEM_LIBRARY_PATH
  # Windows API on Cygwin
  /usr/lib/w32api

  # X11
  /usr/X11R6/lib /usr/lib/X11

  # Other
  /usr/pkg/lib
  /opt/csw/lib /opt/lib
  /usr/openwin/lib
  )

list(APPEND CMAKE_SYSTEM_PROGRAM_PATH
  /usr/pkg/bin
  )

list(APPEND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES
  /lib /lib32 /lib64 /usr/lib /usr/lib32 /usr/lib64
  )

list(APPEND CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES
  /usr/include
  )
list(APPEND CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES
  /usr/include
  )

# Enable use of lib32 and lib64 search path variants by default.
set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB32_PATHS TRUE)
set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS TRUE)
