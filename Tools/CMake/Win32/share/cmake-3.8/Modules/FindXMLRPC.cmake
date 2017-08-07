# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindXMLRPC
# ----------
#
# Find xmlrpc
#
# Find the native XMLRPC headers and libraries.
#
# ::
#
#   XMLRPC_INCLUDE_DIRS      - where to find xmlrpc.h, etc.
#   XMLRPC_LIBRARIES         - List of libraries when using xmlrpc.
#   XMLRPC_FOUND             - True if xmlrpc found.
#
# XMLRPC modules may be specified as components for this find module.
# Modules may be listed by running "xmlrpc-c-config".  Modules include:
#
# ::
#
#   c++            C++ wrapper code
#   libwww-client  libwww-based client
#   cgi-server     CGI-based server
#   abyss-server   ABYSS-based server
#
# Typical usage:
#
# ::
#
#   find_package(XMLRPC REQUIRED libwww-client)

# First find the config script from which to obtain other values.
find_program(XMLRPC_C_CONFIG NAMES xmlrpc-c-config)

# Check whether we found anything.
if(XMLRPC_C_CONFIG)
  set(XMLRPC_FOUND 1)
else()
  set(XMLRPC_FOUND 0)
endif()

# Lookup the include directories needed for the components requested.
if(XMLRPC_FOUND)
  # Use the newer EXECUTE_PROCESS command if it is available.
  if(COMMAND EXECUTE_PROCESS)
    execute_process(
      COMMAND ${XMLRPC_C_CONFIG} ${XMLRPC_FIND_COMPONENTS} --cflags
      OUTPUT_VARIABLE XMLRPC_C_CONFIG_CFLAGS
      OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE XMLRPC_C_CONFIG_RESULT
      )
  else()
    exec_program(${XMLRPC_C_CONFIG} ARGS "${XMLRPC_FIND_COMPONENTS} --cflags"
      OUTPUT_VARIABLE XMLRPC_C_CONFIG_CFLAGS
      RETURN_VALUE XMLRPC_C_CONFIG_RESULT
      )
  endif()

  # Parse the include flags.
  if("${XMLRPC_C_CONFIG_RESULT}" STREQUAL "0")
    # Convert the compile flags to a CMake list.
    string(REGEX REPLACE " +" ";"
      XMLRPC_C_CONFIG_CFLAGS "${XMLRPC_C_CONFIG_CFLAGS}")

    # Look for -I options.
    set(XMLRPC_INCLUDE_DIRS)
    foreach(flag ${XMLRPC_C_CONFIG_CFLAGS})
      if("${flag}" MATCHES "^-I(.+)")
        file(TO_CMAKE_PATH "${CMAKE_MATCH_1}" DIR)
        list(APPEND XMLRPC_INCLUDE_DIRS "${DIR}")
      endif()
    endforeach()
  else()
    message("Error running ${XMLRPC_C_CONFIG}: [${XMLRPC_C_CONFIG_RESULT}]")
    set(XMLRPC_FOUND 0)
  endif()
endif()

# Lookup the libraries needed for the components requested.
if(XMLRPC_FOUND)
  # Use the newer EXECUTE_PROCESS command if it is available.
  if(COMMAND EXECUTE_PROCESS)
    execute_process(
      COMMAND ${XMLRPC_C_CONFIG} ${XMLRPC_FIND_COMPONENTS} --libs
      OUTPUT_VARIABLE XMLRPC_C_CONFIG_LIBS
      OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE XMLRPC_C_CONFIG_RESULT
      )
  else()
    exec_program(${XMLRPC_C_CONFIG} ARGS "${XMLRPC_FIND_COMPONENTS} --libs"
      OUTPUT_VARIABLE XMLRPC_C_CONFIG_LIBS
      RETURN_VALUE XMLRPC_C_CONFIG_RESULT
      )
  endif()

  # Parse the library names and directories.
  if("${XMLRPC_C_CONFIG_RESULT}" STREQUAL "0")
    string(REGEX REPLACE " +" ";"
      XMLRPC_C_CONFIG_LIBS "${XMLRPC_C_CONFIG_LIBS}")

    # Look for -L flags for directories and -l flags for library names.
    set(XMLRPC_LIBRARY_DIRS)
    set(XMLRPC_LIBRARY_NAMES)
    foreach(flag ${XMLRPC_C_CONFIG_LIBS})
      if("${flag}" MATCHES "^-L(.+)")
        file(TO_CMAKE_PATH "${CMAKE_MATCH_1}" DIR)
        list(APPEND XMLRPC_LIBRARY_DIRS "${DIR}")
      elseif("${flag}" MATCHES "^-l(.+)")
        list(APPEND XMLRPC_LIBRARY_NAMES "${CMAKE_MATCH_1}")
      endif()
    endforeach()

    # Search for each library needed using the directories given.
    foreach(name ${XMLRPC_LIBRARY_NAMES})
      # Look for this library.
      find_library(XMLRPC_${name}_LIBRARY
        NAMES ${name}
        HINTS ${XMLRPC_LIBRARY_DIRS}
        )
      mark_as_advanced(XMLRPC_${name}_LIBRARY)

      # If any library is not found then the whole package is not found.
      if(NOT XMLRPC_${name}_LIBRARY)
        set(XMLRPC_FOUND 0)
      endif()

      # Build an ordered list of all the libraries needed.
      set(XMLRPC_LIBRARIES ${XMLRPC_LIBRARIES} "${XMLRPC_${name}_LIBRARY}")
    endforeach()
  else()
    message("Error running ${XMLRPC_C_CONFIG}: [${XMLRPC_C_CONFIG_RESULT}]")
    set(XMLRPC_FOUND 0)
  endif()
endif()

# Report the results.
if(NOT XMLRPC_FOUND)
  set(XMLRPC_DIR_MESSAGE
    "XMLRPC was not found. Make sure the entries XMLRPC_* are set.")
  if(NOT XMLRPC_FIND_QUIETLY)
    message(STATUS "${XMLRPC_DIR_MESSAGE}")
  else()
    if(XMLRPC_FIND_REQUIRED)
      message(FATAL_ERROR "${XMLRPC_DIR_MESSAGE}")
    endif()
  endif()
endif()
