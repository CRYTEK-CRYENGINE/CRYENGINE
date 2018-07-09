# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

# This module is shared by multiple languages; use include blocker.
if(__DARWIN_COMPILER_PGI)
  return()
endif()
set(__DARWIN_COMPILER_PGI 1)

macro(__darwin_compiler_pgi lang)
  set(CMAKE_${lang}_OSX_COMPATIBILITY_VERSION_FLAG "-Wl,-compatibility_version,")
  set(CMAKE_${lang}_OSX_CURRENT_VERSION_FLAG "-Wl,-current_version,")
  set(CMAKE_SHARED_LIBRARY_SONAME_${lang}_FLAG "-Wl,-install_name")
endmacro()
