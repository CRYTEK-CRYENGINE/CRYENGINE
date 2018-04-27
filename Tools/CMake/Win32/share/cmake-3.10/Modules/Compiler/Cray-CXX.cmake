# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

include(Compiler/Cray)

set(CMAKE_CXX_VERBOSE_FLAG "-v")

string(APPEND CMAKE_CXX_FLAGS_MINSIZEREL_INIT " -DNDEBUG")
string(APPEND CMAKE_CXX_FLAGS_RELEASE_INIT " -DNDEBUG")

if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 8.1)
  set(CMAKE_CXX98_STANDARD_COMPILE_OPTION  -h conform)
  set(CMAKE_CXX98_EXTENSION_COMPILE_OPTION -h gnu)
  if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.4)
    set(CMAKE_CXX11_STANDARD_COMPILE_OPTION  -h std=c++11)
    set(CMAKE_CXX11_EXTENSION_COMPILE_OPTION -h std=c++11,gnu)
  endif ()
endif ()

__compiler_check_default_language_standard(CXX 8.1 98)
