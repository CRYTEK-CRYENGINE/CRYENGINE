# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


# This module is shared by multiple languages; use include blocker.
if(__DARWIN_COMPILER_CLANG)
  return()
endif()
set(__DARWIN_COMPILER_CLANG 1)

macro(__darwin_compiler_clang lang)
  set(CMAKE_${lang}_VERBOSE_FLAG "-v -Wl,-v") # also tell linker to print verbose output
  set(CMAKE_SHARED_LIBRARY_CREATE_${lang}_FLAGS "-dynamiclib -Wl,-headerpad_max_install_names")
  set(CMAKE_SHARED_MODULE_CREATE_${lang}_FLAGS "-bundle -Wl,-headerpad_max_install_names")
  set(CMAKE_${lang}_SYSROOT_FLAG "-isysroot")
  set(CMAKE_${lang}_OSX_DEPLOYMENT_TARGET_FLAG "-mmacosx-version-min=")
  if(NOT CMAKE_${lang}_COMPILER_VERSION VERSION_LESS 3.2)
    set(CMAKE_${lang}_SYSTEM_FRAMEWORK_SEARCH_FLAG "-iframework ")
  endif()
endmacro()
