# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


# This module is shared by multiple languages; use include blocker.
if(__COMPILER_INTEL)
  return()
endif()
set(__COMPILER_INTEL 1)

include(Compiler/CMakeCommonCompilerMacros)

if(CMAKE_HOST_WIN32)
  # MSVC-like
  macro(__compiler_intel lang)
  endmacro()
else()
  # GNU-like
  macro(__compiler_intel lang)
    set(CMAKE_${lang}_VERBOSE_FLAG "-v")

    string(APPEND CMAKE_${lang}_FLAGS_INIT " ")
    string(APPEND CMAKE_${lang}_FLAGS_DEBUG_INIT " -g")
    string(APPEND CMAKE_${lang}_FLAGS_MINSIZEREL_INIT " -Os")
    string(APPEND CMAKE_${lang}_FLAGS_RELEASE_INIT " -O3")
    string(APPEND CMAKE_${lang}_FLAGS_RELWITHDEBINFO_INIT " -O2 -g")

    set(CMAKE_${lang}_COMPILER_PREDEFINES_COMMAND "${CMAKE_${lang}_COMPILER}" "-QdM" "-P" "-Za" "${CMAKE_ROOT}/Modules/CMakeCXXCompilerABI.cpp")
  endmacro()
endif()
