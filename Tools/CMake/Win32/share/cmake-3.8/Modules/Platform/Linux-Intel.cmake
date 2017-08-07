# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


# This module is shared by multiple languages; use include blocker.
if(__LINUX_COMPILER_INTEL)
  return()
endif()
set(__LINUX_COMPILER_INTEL 1)

if(NOT XIAR)
  set(_intel_xiar_hints)
  foreach(lang C CXX Fortran)
    if(IS_ABSOLUTE "${CMAKE_${lang}_COMPILER}")
      get_filename_component(_hint "${CMAKE_${lang}_COMPILER}" PATH)
      list(APPEND _intel_xiar_hints ${_hint})
    endif()
  endforeach()
  find_program(XIAR NAMES xiar HINTS ${_intel_xiar_hints})
  mark_as_advanced(XIAR)
endif()

macro(__linux_compiler_intel lang)
  set(CMAKE_${lang}_COMPILE_OPTIONS_PIC "-fPIC")
  set(CMAKE_${lang}_COMPILE_OPTIONS_PIE "-fPIE")
  set(CMAKE_SHARED_LIBRARY_${lang}_FLAGS "-fPIC")
  set(CMAKE_SHARED_LIBRARY_CREATE_${lang}_FLAGS "-shared")

  # We pass this for historical reasons.  Projects may have
  # executables that use dlopen but do not set ENABLE_EXPORTS.
  set(CMAKE_SHARED_LIBRARY_LINK_${lang}_FLAGS "-rdynamic")

  if(XIAR)
    # INTERPROCEDURAL_OPTIMIZATION
    set(CMAKE_${lang}_COMPILE_OPTIONS_IPO -ipo)
    set(CMAKE_${lang}_CREATE_STATIC_LIBRARY_IPO
      "${XIAR} cr <TARGET> <LINK_FLAGS> <OBJECTS> "
      "${XIAR} -s <TARGET> ")
  endif()

  if(NOT CMAKE_${lang}_COMPILER_VERSION VERSION_LESS 12.0)
    set(CMAKE_${lang}_COMPILE_OPTIONS_VISIBILITY "-fvisibility=")
  endif()
endmacro()
