# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

# This file sets the basic flags for the C# language in CMake.
# It also loads the available platform file for the system-compiler
# if it exists.

set(CMAKE_BASE_NAME)
get_filename_component(CMAKE_BASE_NAME "${CMAKE_CSharp_COMPILER}" NAME_WE)

set(CMAKE_BUILD_TYPE_INIT Debug)

set(CMAKE_CSharp_FLAGS_INIT "/define:TRACE /langversion:3")
set(CMAKE_CSharp_FLAGS_DEBUG_INIT "/debug:full /optimize- /warn:3 /errorreport:prompt /define:DEBUG")
set(CMAKE_CSharp_FLAGS_RELEASE_INIT "/debug:none /optimize  /warn:1  /errorreport:queue")
set(CMAKE_CSharp_FLAGS_RELWITHDEBINFO_INIT "/debug:full /optimize-")
set(CMAKE_CSharp_FLAGS_MINSIZEREL_INIT "/debug:none /optimize")
set(CMAKE_CSharp_LINKER_SUPPORTS_PDB ON)

set(CMAKE_CSharp_STANDARD_LIBRARIES_INIT "System")

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(CMAKE_CSharp_FLAGS_INIT "/platform:x86 ${CMAKE_CSharp_FLAGS_INIT}")
else()
  set(CMAKE_CSharp_FLAGS_INIT "/platform:x64 ${CMAKE_CSharp_FLAGS_INIT}")
endif()

# This should be included before the _INIT variables are
# used to initialize the cache.  Since the rule variables
# have if blocks on them, users can still define them here.
# But, it should still be after the platform file so changes can
# be made to those values.

# for most systems a module is the same as a shared library
# so unless the variable CMAKE_MODULE_EXISTS is set just
# copy the values from the LIBRARY variables
if(NOT CMAKE_MODULE_EXISTS)
  set(CMAKE_SHARED_MODULE_CSharp_FLAGS ${CMAKE_SHARED_LIBRARY_CSharp_FLAGS})
  set(CMAKE_SHARED_MODULE_CREATE_CSharp_FLAGS ${CMAKE_SHARED_LIBRARY_CREATE_CSharp_FLAGS})
endif()

# add the flags to the cache based
# on the initial values computed in the platform/*.cmake files
# use _INIT variables so that this only happens the first time
# and you can set these flags in the cmake cache
set(CMAKE_CSharp_FLAGS_INIT "$ENV{CSFLAGS} ${CMAKE_CSharp_FLAGS_INIT}")
# avoid just having a space as the initial value for the cache
if(CMAKE_CSharp_FLAGS_INIT STREQUAL " ")
  set(CMAKE_CSharp_FLAGS_INIT)
endif()
set (CMAKE_CSharp_FLAGS "${CMAKE_CSharp_FLAGS_INIT}" CACHE STRING
     "Flags used by the C# compiler during all build types.")

if(NOT CMAKE_NOT_USING_CONFIG_FLAGS)
  set (CMAKE_CSharp_FLAGS_DEBUG "${CMAKE_CSharp_FLAGS_DEBUG_INIT}" CACHE STRING
     "Flags used by the C# compiler during debug builds.")
  set (CMAKE_CSharp_FLAGS_MINSIZEREL "${CMAKE_CSharp_FLAGS_MINSIZEREL_INIT}" CACHE STRING
     "Flags used by the C# compiler during release builds for minimum size.")
  set (CMAKE_CSharp_FLAGS_RELEASE "${CMAKE_CSharp_FLAGS_RELEASE_INIT}" CACHE STRING
     "Flags used by the C# compiler during release builds.")
  set (CMAKE_CSharp_FLAGS_RELWITHDEBINFO "${CMAKE_CSharp_FLAGS_RELWITHDEBINFO_INIT}" CACHE STRING
     "Flags used by the C# compiler during release builds with debug info.")
endif()

if(CMAKE_CSharp_STANDARD_LIBRARIES_INIT)
  set(CMAKE_CSharp_STANDARD_LIBRARIES "${CMAKE_CSharp_STANDARD_LIBRARIES_INIT}"
    CACHE STRING "Libraries linked by default with all C# applications.")
  mark_as_advanced(CMAKE_CSharp_STANDARD_LIBRARIES)
endif()

# set missing flags (if they are not defined). This is needed in the
# unlikely case that you have only C# and no C/C++ targets in your
# project.
if(NOT DEFINED CMAKE_SHARED_LINKER_FLAGS)
    set(CMAKE_SHARED_LINKER_FLAGS "" CACHE STRING "" FORCE)
endif()
if(NOT DEFINED CMAKE_SHARED_LINKER_FLAGS_DEBUG)
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "" CACHE STRING "" FORCE)
endif()
if(NOT DEFINED CMAKE_SHARED_LINKER_FLAGS_RELEASE)
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "" CACHE STRING "" FORCE)
endif()
if(NOT DEFINED CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL)
    set(CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL "" CACHE STRING "" FORCE)
endif()
if(NOT DEFINED CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO)
    set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "" CACHE STRING "" FORCE)
endif()

if(NOT DEFINED CMAKE_EXE_LINKER_FLAGS)
    set(CMAKE_EXE_LINKER_FLAGS "" CACHE STRING "" FORCE)
endif()
if(NOT DEFINED CMAKE_EXE_LINKER_FLAGS_DEBUG)
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "" CACHE STRING "" FORCE)
endif()
if(NOT DEFINED CMAKE_EXE_LINKER_FLAGS_RELEASE)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "" CACHE STRING "" FORCE)
endif()
if(NOT DEFINED CMAKE_EXE_LINKER_FLAGS_MINSIZEREL)
    set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "" CACHE STRING "" FORCE)
endif()
if(NOT DEFINED CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO)
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "" CACHE STRING "" FORCE)
endif()

set(CMAKE_CSharp_CREATE_SHARED_LIBRARY "CSharp_NO_CREATE_SHARED_LIBRARY")
set(CMAKE_CSharp_CREATE_SHARED_MODULE "CSharp_NO_CREATE_SHARED_MODULE")
set(CMAKE_CSharp_LINK_EXECUTABLE "CSharp_NO_LINK_EXECUTABLE")

mark_as_advanced(
    CMAKE_CSharp_FLAGS
    CMAKE_CSharp_FLAGS_RELEASE
    CMAKE_CSharp_FLAGS_RELWITHDEBINFO
    CMAKE_CSharp_FLAGS_MINSIZEREL
    CMAKE_CSharp_FLAGS_DEBUG
    )

set(CMAKE_CSharp_USE_RESPONSE_FILE_FOR_OBJECTS 1)
set(CMAKE_CSharp_INFORMATION_LOADED 1)
