# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


# This module is shared by multiple languages; use include blocker.
if(__COMPILER_CLANG)
  return()
endif()
set(__COMPILER_CLANG 1)

if("x${CMAKE_C_SIMULATE_ID}" STREQUAL "xMSVC"
    OR "x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC")
  macro(__compiler_clang lang)
  endmacro()
else()
  include(Compiler/GNU)

  macro(__compiler_clang lang)
    __compiler_gnu(${lang})
    set(CMAKE_${lang}_COMPILE_OPTIONS_PIE "-fPIE")
    set(CMAKE_INCLUDE_SYSTEM_FLAG_${lang} "-isystem ")
    set(CMAKE_${lang}_COMPILE_OPTIONS_VISIBILITY "-fvisibility=")
    if(CMAKE_${lang}_COMPILER_VERSION VERSION_LESS 3.4.0)
      set(CMAKE_${lang}_COMPILE_OPTIONS_TARGET "-target ")
      set(CMAKE_${lang}_COMPILE_OPTIONS_EXTERNAL_TOOLCHAIN "-gcc-toolchain ")
    else()
      set(CMAKE_${lang}_COMPILE_OPTIONS_TARGET "--target=")
      set(CMAKE_${lang}_COMPILE_OPTIONS_EXTERNAL_TOOLCHAIN "--gcc-toolchain=")
    endif()
  endmacro()
endif()
