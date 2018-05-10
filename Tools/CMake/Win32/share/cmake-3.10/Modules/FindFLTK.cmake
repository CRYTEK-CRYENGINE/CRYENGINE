# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindFLTK
# --------
#
# Find the native FLTK includes and library
#
#
#
# By default FindFLTK.cmake will search for all of the FLTK components
# and add them to the FLTK_LIBRARIES variable.
#
# ::
#
#    You can limit the components which get placed in FLTK_LIBRARIES by
#    defining one or more of the following three options:
#
#
#
# ::
#
#      FLTK_SKIP_OPENGL, set to true to disable searching for opengl and
#                        the FLTK GL library
#      FLTK_SKIP_FORMS, set to true to disable searching for fltk_forms
#      FLTK_SKIP_IMAGES, set to true to disable searching for fltk_images
#
#
#
# ::
#
#      FLTK_SKIP_FLUID, set to true if the fluid binary need not be present
#                       at build time
#
#
#
# The following variables will be defined:
#
# ::
#
#      FLTK_FOUND, True if all components not skipped were found
#      FLTK_INCLUDE_DIR, where to find include files
#      FLTK_LIBRARIES, list of fltk libraries you should link against
#      FLTK_FLUID_EXECUTABLE, where to find the Fluid tool
#      FLTK_WRAP_UI, This enables the FLTK_WRAP_UI command
#
#
#
# The following cache variables are assigned but should not be used.
# See the FLTK_LIBRARIES variable instead.
#
# ::
#
#      FLTK_BASE_LIBRARY   = the full path to fltk.lib
#      FLTK_GL_LIBRARY     = the full path to fltk_gl.lib
#      FLTK_FORMS_LIBRARY  = the full path to fltk_forms.lib
#      FLTK_IMAGES_LIBRARY = the full path to fltk_images.lib

if(NOT FLTK_SKIP_OPENGL)
  find_package(OpenGL)
endif()

#  Platform dependent libraries required by FLTK
if(WIN32)
  if(NOT CYGWIN)
    if(BORLAND)
      set( FLTK_PLATFORM_DEPENDENT_LIBS import32 )
    else()
      set( FLTK_PLATFORM_DEPENDENT_LIBS wsock32 comctl32 )
    endif()
  endif()
endif()

if(UNIX)
  include(${CMAKE_CURRENT_LIST_DIR}/FindX11.cmake)
  find_library(FLTK_MATH_LIBRARY m)
  set( FLTK_PLATFORM_DEPENDENT_LIBS ${X11_LIBRARIES} ${FLTK_MATH_LIBRARY})
endif()

if(APPLE)
  set( FLTK_PLATFORM_DEPENDENT_LIBS  "-framework Carbon -framework Cocoa -framework ApplicationServices -lz")
endif()

# If FLTK_INCLUDE_DIR is already defined we assigne its value to FLTK_DIR
if(FLTK_INCLUDE_DIR)
  set(FLTK_DIR ${FLTK_INCLUDE_DIR})
endif()


# If FLTK has been built using CMake we try to find everything directly
set(FLTK_DIR_STRING "directory containing FLTKConfig.cmake.  This is either the root of the build tree, or PREFIX/lib/fltk for an installation.")

# Search only if the location is not already known.
if(NOT FLTK_DIR)
  # Get the system search path as a list.
  file(TO_CMAKE_PATH "$ENV{PATH}" FLTK_DIR_SEARCH2)

  # Construct a set of paths relative to the system search path.
  set(FLTK_DIR_SEARCH "")
  foreach(dir ${FLTK_DIR_SEARCH2})
    set(FLTK_DIR_SEARCH ${FLTK_DIR_SEARCH} "${dir}/../lib/fltk")
  endforeach()
  string(REPLACE "//" "/" FLTK_DIR_SEARCH "${FLTK_DIR_SEARCH}")

  #
  # Look for an installation or build tree.
  #
  find_path(FLTK_DIR FLTKConfig.cmake
    # Look for an environment variable FLTK_DIR.
    HINTS
      ENV FLTK_DIR

    # Look in places relative to the system executable search path.
    ${FLTK_DIR_SEARCH}

    PATHS
    # Look in standard UNIX install locations.
    /usr/local/lib/fltk
    /usr/lib/fltk
    /usr/local/fltk
    /usr/X11R6/include

    # Help the user find it if we cannot.
    DOC "The ${FLTK_DIR_STRING}"
    )
endif()

  # Check if FLTK was built using CMake
  if(EXISTS ${FLTK_DIR}/FLTKConfig.cmake)
    set(FLTK_BUILT_WITH_CMAKE 1)
  endif()

  if(FLTK_BUILT_WITH_CMAKE)
    set(FLTK_FOUND 1)
    include(${FLTK_DIR}/FLTKConfig.cmake)

    # Fluid
    if(FLUID_COMMAND)
      set(FLTK_FLUID_EXECUTABLE ${FLUID_COMMAND} CACHE FILEPATH "Fluid executable")
    else()
      find_program(FLTK_FLUID_EXECUTABLE fluid PATHS
        ${FLTK_EXECUTABLE_DIRS}
        ${FLTK_EXECUTABLE_DIRS}/RelWithDebInfo
        ${FLTK_EXECUTABLE_DIRS}/Debug
        ${FLTK_EXECUTABLE_DIRS}/Release
        NO_SYSTEM_PATH)
    endif()
    # mark_as_advanced(FLTK_FLUID_EXECUTABLE)

    set(FLTK_INCLUDE_DIR ${FLTK_DIR})
    link_directories(${FLTK_LIBRARY_DIRS})

    set(FLTK_BASE_LIBRARY fltk)
    set(FLTK_GL_LIBRARY fltk_gl)
    set(FLTK_FORMS_LIBRARY fltk_forms)
    set(FLTK_IMAGES_LIBRARY fltk_images)

    # Add the extra libraries
    load_cache(${FLTK_DIR}
      READ_WITH_PREFIX
      FL FLTK_USE_SYSTEM_JPEG
      FL FLTK_USE_SYSTEM_PNG
      FL FLTK_USE_SYSTEM_ZLIB
      )

    set(FLTK_IMAGES_LIBS "")
    if(FLFLTK_USE_SYSTEM_JPEG)
      set(FLTK_IMAGES_LIBS ${FLTK_IMAGES_LIBS} fltk_jpeg)
    endif()
    if(FLFLTK_USE_SYSTEM_PNG)
      set(FLTK_IMAGES_LIBS ${FLTK_IMAGES_LIBS} fltk_png)
    endif()
    if(FLFLTK_USE_SYSTEM_ZLIB)
      set(FLTK_IMAGES_LIBS ${FLTK_IMAGES_LIBS} fltk_zlib)
    endif()
    set(FLTK_IMAGES_LIBS "${FLTK_IMAGES_LIBS}" CACHE INTERNAL
      "Extra libraries for fltk_images library.")

  else()

    # if FLTK was not built using CMake
    # Find fluid executable.
    find_program(FLTK_FLUID_EXECUTABLE fluid ${FLTK_INCLUDE_DIR}/fluid)

    # Use location of fluid to help find everything else.
    set(FLTK_INCLUDE_SEARCH_PATH "")
    set(FLTK_LIBRARY_SEARCH_PATH "")
    if(FLTK_FLUID_EXECUTABLE)
      get_filename_component(FLTK_BIN_DIR "${FLTK_FLUID_EXECUTABLE}" PATH)
      set(FLTK_INCLUDE_SEARCH_PATH ${FLTK_INCLUDE_SEARCH_PATH}
        ${FLTK_BIN_DIR}/../include ${FLTK_BIN_DIR}/..)
      set(FLTK_LIBRARY_SEARCH_PATH ${FLTK_LIBRARY_SEARCH_PATH}
        ${FLTK_BIN_DIR}/../lib)
      set(FLTK_WRAP_UI 1)
    endif()

    #
    # Try to find FLTK include dir using fltk-config
    #
    if(UNIX)
      # Use fltk-config to generate a list of possible include directories
      find_program(FLTK_CONFIG_SCRIPT fltk-config PATHS ${FLTK_BIN_DIR})
      if(FLTK_CONFIG_SCRIPT)
        if(NOT FLTK_INCLUDE_DIR)
          exec_program(${FLTK_CONFIG_SCRIPT} ARGS --cxxflags OUTPUT_VARIABLE FLTK_CXXFLAGS)
          if(FLTK_CXXFLAGS)
            string(REGEX MATCHALL "-I[^ ]*" _fltk_temp_dirs ${FLTK_CXXFLAGS})
            string(REPLACE "-I" "" _fltk_temp_dirs "${_fltk_temp_dirs}")
            foreach(_dir ${_fltk_temp_dirs})
              string(STRIP ${_dir} _output)
              list(APPEND _FLTK_POSSIBLE_INCLUDE_DIRS ${_output})
            endforeach()
          endif()
        endif()
      endif()
    endif()

    set(FLTK_INCLUDE_SEARCH_PATH ${FLTK_INCLUDE_SEARCH_PATH}
      /usr/local/fltk
      /usr/X11R6/include
      ${_FLTK_POSSIBLE_INCLUDE_DIRS}
      )

    find_path(FLTK_INCLUDE_DIR
        NAMES FL/Fl.h FL/Fl.H    # fltk 1.1.9 has Fl.H (#8376)
        PATHS ${FLTK_INCLUDE_SEARCH_PATH})

    #
    # Try to find FLTK library
    if(UNIX)
      if(FLTK_CONFIG_SCRIPT)
        exec_program(${FLTK_CONFIG_SCRIPT} ARGS --libs OUTPUT_VARIABLE _FLTK_POSSIBLE_LIBS)
        if(_FLTK_POSSIBLE_LIBS)
          get_filename_component(_FLTK_POSSIBLE_LIBRARY_DIR ${_FLTK_POSSIBLE_LIBS} PATH)
        endif()
      endif()
    endif()

    set(FLTK_LIBRARY_SEARCH_PATH ${FLTK_LIBRARY_SEARCH_PATH}
      /usr/local/fltk/lib
      /usr/X11R6/lib
      ${FLTK_INCLUDE_DIR}/lib
      ${_FLTK_POSSIBLE_LIBRARY_DIR}
      )

    find_library(FLTK_BASE_LIBRARY NAMES fltk fltkd
      PATHS ${FLTK_LIBRARY_SEARCH_PATH})
    find_library(FLTK_GL_LIBRARY NAMES fltkgl fltkgld fltk_gl
      PATHS ${FLTK_LIBRARY_SEARCH_PATH})
    find_library(FLTK_FORMS_LIBRARY NAMES fltkforms fltkformsd fltk_forms
      PATHS ${FLTK_LIBRARY_SEARCH_PATH})
    find_library(FLTK_IMAGES_LIBRARY NAMES fltkimages fltkimagesd fltk_images
      PATHS ${FLTK_LIBRARY_SEARCH_PATH})

    # Find the extra libraries needed for the fltk_images library.
    if(UNIX)
      if(FLTK_CONFIG_SCRIPT)
        exec_program(${FLTK_CONFIG_SCRIPT} ARGS --use-images --ldflags
          OUTPUT_VARIABLE FLTK_IMAGES_LDFLAGS)
        set(FLTK_LIBS_EXTRACT_REGEX ".*-lfltk_images (.*) -lfltk.*")
        if("${FLTK_IMAGES_LDFLAGS}" MATCHES "${FLTK_LIBS_EXTRACT_REGEX}")
          string(REGEX REPLACE " +" ";" FLTK_IMAGES_LIBS "${CMAKE_MATCH_1}")
          # The EXEC_PROGRAM will not be inherited into subdirectories from
          # the file that originally included this module.  Save the answer.
          set(FLTK_IMAGES_LIBS "${FLTK_IMAGES_LIBS}" CACHE INTERNAL
            "Extra libraries for fltk_images library.")
        endif()
      endif()
    endif()

  endif()

  # Append all of the required libraries together (by default, everything)
  set(FLTK_LIBRARIES)
  if(NOT FLTK_SKIP_IMAGES)
    list(APPEND FLTK_LIBRARIES ${FLTK_IMAGES_LIBRARY})
  endif()
  if(NOT FLTK_SKIP_FORMS)
    list(APPEND FLTK_LIBRARIES ${FLTK_FORMS_LIBRARY})
  endif()
  if(NOT FLTK_SKIP_OPENGL)
    list(APPEND FLTK_LIBRARIES ${FLTK_GL_LIBRARY} ${OPENGL_gl_LIBRARY})
    list(APPEND FLTK_INCLUDE_DIR ${OPENGL_INCLUDE_DIR})
    list(REMOVE_DUPLICATES FLTK_INCLUDE_DIR)
  endif()
  list(APPEND FLTK_LIBRARIES ${FLTK_BASE_LIBRARY})

include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
if(FLTK_SKIP_FLUID)
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(FLTK DEFAULT_MSG FLTK_LIBRARIES FLTK_INCLUDE_DIR)
else()
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(FLTK DEFAULT_MSG FLTK_LIBRARIES FLTK_INCLUDE_DIR FLTK_FLUID_EXECUTABLE)
endif()

if(FLTK_FOUND)
  if(APPLE)
    set(FLTK_LIBRARIES ${FLTK_PLATFORM_DEPENDENT_LIBS} ${FLTK_LIBRARIES})
  else()
    set(FLTK_LIBRARIES ${FLTK_LIBRARIES} ${FLTK_PLATFORM_DEPENDENT_LIBS})
  endif()

  # The following deprecated settings are for compatibility with CMake 1.4
  set (HAS_FLTK ${FLTK_FOUND})
  set (FLTK_INCLUDE_PATH ${FLTK_INCLUDE_DIR})
  set (FLTK_FLUID_EXE ${FLTK_FLUID_EXECUTABLE})
  set (FLTK_LIBRARY ${FLTK_LIBRARIES})
endif()

