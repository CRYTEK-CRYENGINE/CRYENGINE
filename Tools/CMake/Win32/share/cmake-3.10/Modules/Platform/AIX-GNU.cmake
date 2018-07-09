# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


# This module is shared by multiple languages; use include blocker.
if(__AIX_COMPILER_GNU)
  return()
endif()
set(__AIX_COMPILER_GNU 1)

#
# By default, runtime linking is enabled. All shared objects specified on the command line
# will be listed, even if there are no symbols referenced, in the output file.
string(APPEND CMAKE_SHARED_LINKER_FLAGS_INIT " -Wl,-brtl")
string(APPEND CMAKE_MODULE_LINKER_FLAGS_INIT " -Wl,-brtl")
string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT " -Wl,-brtl")


macro(__aix_compiler_gnu lang)
  set(CMAKE_SHARED_LIBRARY_RUNTIME_${lang}_FLAG "-Wl,-blibpath:")
  set(CMAKE_SHARED_LIBRARY_RUNTIME_${lang}_FLAG_SEP ":")
  string(APPEND CMAKE_SHARED_LIBRARY_CREATE_${lang}_FLAGS " -Wl,-G,-bnoipath")
  set(CMAKE_SHARED_LIBRARY_LINK_${lang}_FLAGS "-Wl,-bexpall")
  set(CMAKE_${lang}_USE_IMPLICIT_LINK_DIRECTORIES_IN_RUNTIME_PATH 1)

  set(CMAKE_${lang}_LINK_FLAGS "-Wl,-bnoipath")
  unset(CMAKE_${lang}_COMPILE_OPTIONS_VISIBILITY)
endmacro()
