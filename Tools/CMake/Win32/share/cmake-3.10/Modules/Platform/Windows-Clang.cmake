# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


# This module is shared by multiple languages; use include blocker.
if(__WINDOWS_CLANG)
  return()
endif()
set(__WINDOWS_CLANG 1)

if("x${CMAKE_C_SIMULATE_ID}" STREQUAL "xMSVC"
    OR "x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC")
  include(Platform/Windows-MSVC)
  macro(__windows_compiler_clang lang)
    set(_COMPILE_${lang} "${_COMPILE_${lang}_MSVC}")
    __windows_compiler_msvc(${lang})
  endmacro()
else()
  include(Platform/Windows-GNU)
  macro(__windows_compiler_clang lang)
    __windows_compiler_gnu(${lang})
  endmacro()
endif()
