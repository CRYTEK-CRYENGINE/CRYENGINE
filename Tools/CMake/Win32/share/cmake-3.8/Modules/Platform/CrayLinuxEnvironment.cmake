# Compute Node Linux doesn't quite work the same as native Linux so all of this
# needs to be custom.  We use the variables defined through Cray's environment
# modules to set up the right paths for things.

set(UNIX 1)

if(DEFINED ENV{CRAYOS_VERSION})
  set(CMAKE_SYSTEM_VERSION "$ENV{CRAYOS_VERSION}")
elseif(DEFINED ENV{XTOS_VERSION})
  set(CMAKE_SYSTEM_VERSION "$ENV{XTOS_VERSION}")
elseif(EXISTS /etc/opt/cray/release/cle-release)
  file(STRINGS /etc/opt/cray/release/cle-release release REGEX "^RELEASE=.*")
  string(REGEX REPLACE "^RELEASE=(.*)$" "\\1" CMAKE_SYSTEM_VERSION "${release}")
  unset(release)
elseif(EXISTS /etc/opt/cray/release/clerelease)
  file(READ /etc/opt/cray/release/clerelease CMAKE_SYSTEM_VERSION)
endif()

# Guard against multiple messages
if(NOT __CrayLinuxEnvironment_message)
  set(__CrayLinuxEnvironment_message 1 CACHE INTERNAL "")
  if(NOT CMAKE_SYSTEM_VERSION)
    message(STATUS "CrayLinuxEnvironment: Unable to determine CLE version.  This platform file should only be used from inside the Cray Linux Environment for targeting compute nodes (NIDs).")
  else()
    message(STATUS "Cray Linux Environment ${CMAKE_SYSTEM_VERSION}")
  endif()
endif()

# All cray systems are x86 CPUs and have been for quite some time
# Note: this may need to change in the future with 64-bit ARM
set(CMAKE_SYSTEM_PROCESSOR "x86_64")

set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")
set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")

set(CMAKE_FIND_LIBRARY_PREFIXES "lib")

# Don't override shared lib support if it's already been set and possibly
# overridden elsewhere by the CrayPrgEnv module
if(NOT CMAKE_FIND_LIBRARY_SUFFIXES)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".so" ".a")
  set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)
endif()

set(CMAKE_DL_LIBS dl)

# Note: Much of this is pulled from UnixPaths.cmake but adjusted to the Cray
# environment accordingly

# Get the install directory of the running cmake to the search directories
# CMAKE_ROOT is CMAKE_INSTALL_PREFIX/share/cmake, so we need to go two levels up
get_filename_component(__cmake_install_dir "${CMAKE_ROOT}" PATH)
get_filename_component(__cmake_install_dir "${__cmake_install_dir}" PATH)

# Note: Some Cray's have the SYSROOT_DIR variable defined, pointing to a copy
# of the NIDs userland.  If so, then we'll use it.  Otherwise, just assume
# the userland from the login node is ok

# List common installation prefixes.  These will be used for all
# search types.
list(APPEND CMAKE_SYSTEM_PREFIX_PATH
  # Standard
  $ENV{SYSROOT_DIR}/usr/local $ENV{SYSROOT_DIR}/usr $ENV{SYSROOT_DIR}/

  # CMake install location
  "${__cmake_install_dir}"
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

list(APPEND CMAKE_SYSTEM_INCLUDE_PATH
  $ENV{SYSROOT_DIR}/usr/include
  $ENV{SYSROOT_DIR}/usr/include/X11
)
list(APPEND CMAKE_SYSTEM_LIBRARY_PATH
  $ENV{SYSROOT_DIR}/usr/local/lib64
  $ENV{SYSROOT_DIR}/usr/lib64
  $ENV{SYSROOT_DIR}/lib64
)
list(APPEND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES
  $ENV{SYSROOT_DIR}/usr/local/lib64
  $ENV{SYSROOT_DIR}/usr/lib64
  $ENV{SYSROOT_DIR}/lib64
)

# Compute the intersection of several lists
function(__cray_list_intersect OUTPUT INPUT0)
  if(ARGC EQUAL 2)
    list(APPEND ${OUTPUT} ${${INPUT0}})
  else()
    foreach(I IN LISTS ${INPUT0})
      set(__is_common 1)
      foreach(L IN LISTS ARGN)
        list(FIND ${L} "${I}" __idx)
        if(__idx EQUAL -1)
          set(__is_common 0)
          break()
        endif()
      endforeach()
      if(__is_common)
        list(APPEND ${OUTPUT}  "${I}")
      endif()
    endforeach()
  endif()
  set(${OUTPUT} ${${OUTPUT}} PARENT_SCOPE)
endfunction()

macro(__list_clean_dupes var)
  if(${var})
    list(REMOVE_DUPLICATES ${var})
  endif()
endmacro()

get_property(__langs GLOBAL PROPERTY ENABLED_LANGUAGES)
set(__cray_inc_path_vars)
set(__cray_lib_path_vars)
foreach(__lang IN LISTS __langs)
  list(APPEND __cray_inc_path_vars CMAKE_${__lang}_IMPLICIT_INCLUDE_DIRECTORIES)
  list(APPEND __cray_lib_path_vars CMAKE_${__lang}_IMPLICIT_LINK_DIRECTORIES)
endforeach()
if(__cray_inc_path_vars)
  __cray_list_intersect(__cray_implicit_include_dirs ${__cray_inc_path_vars})
  if(__cray_implicit_include_dirs)
    list(INSERT CMAKE_SYSTEM_INCLUDE_PATH 0 ${__cray_implicit_include_dirs})
  endif()
endif()
if(__cray_lib_path_vars)
  __cray_list_intersect(__cray_implicit_library_dirs ${__cray_lib_path_vars})
  if(__cray_implicit_library_dirs)
    list(INSERT CMAKE_SYSTEM_LIBRARY_PATH 0 ${__cray_implicit_library_dirs})
  endif()
endif()
__list_clean_dupes(CMAKE_SYSTEM_PREFIX_PATH)
__list_clean_dupes(CMAKE_SYSTEM_INCLUDE_PATH)
__list_clean_dupes(CMAKE_SYSTEM_LIBRARY_PATH)
__list_clean_dupes(CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES)

# Enable use of lib64 search path variants by default.
set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS TRUE)
