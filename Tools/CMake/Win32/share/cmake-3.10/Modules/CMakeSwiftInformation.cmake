# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


set(CMAKE_Swift_OUTPUT_EXTENSION .o)
set(CMAKE_INCLUDE_FLAG_Swift "-I")

# Load compiler-specific information.
if(CMAKE_Swift_COMPILER_ID)
  include(Compiler/${CMAKE_Swift_COMPILER_ID}-Swift OPTIONAL)
endif()

# load the system- and compiler specific files
if(CMAKE_Swift_COMPILER_ID)
  # load a hardware specific file, mostly useful for embedded compilers
  if(CMAKE_SYSTEM_PROCESSOR)
    include(Platform/${CMAKE_SYSTEM_NAME}-${CMAKE_Swift_COMPILER_ID}-Swift-${CMAKE_SYSTEM_PROCESSOR} OPTIONAL)
  endif()
  include(Platform/${CMAKE_SYSTEM_NAME}-${CMAKE_Swift_COMPILER_ID}-Swift OPTIONAL)
endif()

# for most systems a module is the same as a shared library
# so unless the variable CMAKE_MODULE_EXISTS is set just
# copy the values from the LIBRARY variables
if(NOT CMAKE_MODULE_EXISTS)
  set(CMAKE_SHARED_MODULE_Swift_FLAGS ${CMAKE_SHARED_LIBRARY_Swift_FLAGS})
  set(CMAKE_SHARED_MODULE_CREATE_Swift_FLAGS ${CMAKE_SHARED_LIBRARY_CREATE_Swift_FLAGS})
endif()

include(CMakeCommonLanguageInclude)

set(CMAKE_Swift_INFORMATION_LOADED 1)
