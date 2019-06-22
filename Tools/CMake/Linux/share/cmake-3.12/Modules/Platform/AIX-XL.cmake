# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


# This module is shared by multiple languages; use include blocker.
if(__AIX_COMPILER_XL)
  return()
endif()
set(__AIX_COMPILER_XL 1)

#
# By default, runtime linking is enabled. All shared objects specified on the command line
# will be listed, even if there are no symbols referenced, in the output file.
string(APPEND CMAKE_SHARED_LINKER_FLAGS_INIT " -Wl,-brtl")
string(APPEND CMAKE_MODULE_LINKER_FLAGS_INIT " -Wl,-brtl")
string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT " -Wl,-brtl")


macro(__aix_compiler_xl lang)
  set(CMAKE_SHARED_LIBRARY_RUNTIME_${lang}_FLAG "-Wl,-blibpath:")
  set(CMAKE_SHARED_LIBRARY_RUNTIME_${lang}_FLAG_SEP ":")
  set(CMAKE_SHARED_LIBRARY_CREATE_${lang}_FLAGS "-G -Wl,-bnoipath")  # -shared
  set(CMAKE_SHARED_LIBRARY_LINK_${lang}_FLAGS "-Wl,-bexpall")
  set(CMAKE_SHARED_LIBRARY_${lang}_FLAGS " ")
  set(CMAKE_SHARED_MODULE_${lang}_FLAGS  " ")

  set(CMAKE_${lang}_LINK_FLAGS "-Wl,-bnoipath")
endmacro()
