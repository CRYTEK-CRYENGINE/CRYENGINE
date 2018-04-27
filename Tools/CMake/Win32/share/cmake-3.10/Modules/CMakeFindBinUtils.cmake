# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


# search for additional tools required for C/C++ (and other languages ?)
#
# If the internal cmake variable _CMAKE_TOOLCHAIN_PREFIX is set, this is used
# as prefix for the tools (e.g. arm-elf-gcc etc.)
# If the cmake variable _CMAKE_TOOLCHAIN_LOCATION is set, the compiler is
# searched only there. The other tools are at first searched there, then
# also in the default locations.
#
# Sets the following variables:
#   CMAKE_AR
#   CMAKE_RANLIB
#   CMAKE_LINKER
#   CMAKE_STRIP
#   CMAKE_INSTALL_NAME_TOOL

# on UNIX, cygwin and mingw

# if it's the MS C/CXX compiler, search for link
if("x${CMAKE_C_SIMULATE_ID}" STREQUAL "xMSVC"
   OR "x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC"
   OR "x${CMAKE_Fortran_SIMULATE_ID}" STREQUAL "xMSVC"
   OR "x${CMAKE_C_COMPILER_ID}" STREQUAL "xMSVC"
   OR "x${CMAKE_CXX_COMPILER_ID}" STREQUAL "xMSVC"
   OR "x${CMAKE_CUDA_SIMULATE_ID}" STREQUAL "xMSVC"
   OR (CMAKE_HOST_WIN32 AND (
        "x${CMAKE_C_COMPILER_ID}" STREQUAL "xPGI"
         OR "x${CMAKE_Fortran_COMPILER_ID}" STREQUAL "xPGI"
   ))
   OR (CMAKE_GENERATOR MATCHES "Visual Studio"
       AND NOT CMAKE_VS_PLATFORM_NAME STREQUAL "Tegra-Android"))

  find_program(CMAKE_LINKER NAMES link HINTS ${_CMAKE_TOOLCHAIN_LOCATION})

  mark_as_advanced(CMAKE_LINKER)

# in all other cases search for ar, ranlib, etc.
else()
  if(CMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN)
    set(_CMAKE_TOOLCHAIN_LOCATION ${_CMAKE_TOOLCHAIN_LOCATION} ${CMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN}/bin)
  endif()
  if(CMAKE_CXX_COMPILER_EXTERNAL_TOOLCHAIN)
    set(_CMAKE_TOOLCHAIN_LOCATION ${_CMAKE_TOOLCHAIN_LOCATION} ${CMAKE_CXX_COMPILER_EXTERNAL_TOOLCHAIN}/bin)
  endif()
  find_program(CMAKE_AR NAMES ${_CMAKE_TOOLCHAIN_PREFIX}ar${_CMAKE_TOOLCHAIN_SUFFIX} HINTS ${_CMAKE_TOOLCHAIN_LOCATION})

  find_program(CMAKE_RANLIB NAMES ${_CMAKE_TOOLCHAIN_PREFIX}ranlib HINTS ${_CMAKE_TOOLCHAIN_LOCATION})
  if(NOT CMAKE_RANLIB)
    set(CMAKE_RANLIB : CACHE INTERNAL "noop for ranlib")
  endif()

  find_program(CMAKE_STRIP NAMES ${_CMAKE_TOOLCHAIN_PREFIX}strip${_CMAKE_TOOLCHAIN_SUFFIX} HINTS ${_CMAKE_TOOLCHAIN_LOCATION})
  find_program(CMAKE_LINKER NAMES ${_CMAKE_TOOLCHAIN_PREFIX}ld HINTS ${_CMAKE_TOOLCHAIN_LOCATION})
  find_program(CMAKE_NM NAMES ${_CMAKE_TOOLCHAIN_PREFIX}nm HINTS ${_CMAKE_TOOLCHAIN_LOCATION})
  find_program(CMAKE_OBJDUMP NAMES ${_CMAKE_TOOLCHAIN_PREFIX}objdump HINTS ${_CMAKE_TOOLCHAIN_LOCATION})
  find_program(CMAKE_OBJCOPY NAMES ${_CMAKE_TOOLCHAIN_PREFIX}objcopy HINTS ${_CMAKE_TOOLCHAIN_LOCATION})

  mark_as_advanced(CMAKE_AR CMAKE_RANLIB CMAKE_STRIP CMAKE_LINKER CMAKE_NM CMAKE_OBJDUMP CMAKE_OBJCOPY)

endif()

if(CMAKE_PLATFORM_HAS_INSTALLNAME)
  find_program(CMAKE_INSTALL_NAME_TOOL NAMES install_name_tool HINTS ${_CMAKE_TOOLCHAIN_LOCATION})

  if(NOT CMAKE_INSTALL_NAME_TOOL)
    message(FATAL_ERROR "Could not find install_name_tool, please check your installation.")
  endif()

  mark_as_advanced(CMAKE_INSTALL_NAME_TOOL)
endif()
