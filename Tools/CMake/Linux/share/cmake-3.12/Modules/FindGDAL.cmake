# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindGDAL
# --------
#
#
#
# Locate gdal
#
# This module accepts the following environment variables:
#
# ::
#
#     GDAL_DIR or GDAL_ROOT - Specify the location of GDAL
#
#
#
# This module defines the following CMake variables:
#
# ::
#
#     GDAL_FOUND - True if libgdal is found
#     GDAL_LIBRARY - A variable pointing to the GDAL library
#     GDAL_INCLUDE_DIR - Where to find the headers

#
# $GDALDIR is an environment variable that would
# correspond to the ./configure --prefix=$GDAL_DIR
# used in building gdal.
#
# Created by Eric Wing. I'm not a gdal user, but OpenSceneGraph uses it
# for osgTerrain so I whipped this module together for completeness.
# I actually don't know the conventions or where files are typically
# placed in distros.
# Any real gdal users are encouraged to correct this (but please don't
# break the OS X framework stuff when doing so which is what usually seems
# to happen).

# This makes the presumption that you are include gdal.h like
#
#include "gdal.h"

find_path(GDAL_INCLUDE_DIR gdal.h
  HINTS
    ENV GDAL_DIR
    ENV GDAL_ROOT
  PATH_SUFFIXES
     include/gdal
     include/GDAL
     include
)

if(UNIX)
    # Use gdal-config to obtain the library version (this should hopefully
    # allow us to -lgdal1.x.y where x.y are correct version)
    # For some reason, libgdal development packages do not contain
    # libgdal.so...
    find_program(GDAL_CONFIG gdal-config
        HINTS
          ENV GDAL_DIR
          ENV GDAL_ROOT
        PATH_SUFFIXES bin
    )

    if(GDAL_CONFIG)
        exec_program(${GDAL_CONFIG} ARGS --libs OUTPUT_VARIABLE GDAL_CONFIG_LIBS)

        if(GDAL_CONFIG_LIBS)
            # treat the output as a command line and split it up
            separate_arguments(args NATIVE_COMMAND "${GDAL_CONFIG_LIBS}")

            # only consider libraries whose name matches this pattern
            set(name_pattern "[gG][dD][aA][lL]")

            # consider each entry as a possible library path, name, or parent directory
            foreach(arg IN LISTS args)
                # library name
                if("${arg}" MATCHES "^-l(.*)$")
                    set(lib "${CMAKE_MATCH_1}")

                    # only consider libraries whose name matches the expected pattern
                    if("${lib}" MATCHES "${name_pattern}")
                        list(APPEND _gdal_lib "${lib}")
                    endif()
                # library search path
                elseif("${arg}" MATCHES "^-L(.*)$")
                    list(APPEND _gdal_libpath "${CMAKE_MATCH_1}")
                # assume this is a full path to a library
                elseif(IS_ABSOLUTE "${arg}" AND EXISTS "${arg}")
                    # extract the file name
                    get_filename_component(lib "${arg}" NAME)

                    # only consider libraries whose name matches the expected pattern
                    if(NOT "${lib}" MATCHES "${name_pattern}")
                        continue()
                    endif()

                    # extract the file directory
                    get_filename_component(dir "${arg}" DIRECTORY)

                    # remove library prefixes/suffixes
                    string(REGEX REPLACE "^(${CMAKE_SHARED_LIBRARY_PREFIX}|${CMAKE_STATIC_LIBRARY_PREFIX})" "" lib "${lib}")
                    string(REGEX REPLACE "(${CMAKE_SHARED_LIBRARY_SUFFIX}|${CMAKE_STATIC_LIBRARY_SUFFIX})$" "" lib "${lib}")

                    # use the file name and directory as hints
                    list(APPEND _gdal_libpath "${dir}")
                    list(APPEND _gdal_lib "${lib}")
                endif()
            endforeach()
        endif()
    endif()
endif()

find_library(GDAL_LIBRARY
  NAMES ${_gdal_lib} gdal gdal_i gdal1.5.0 gdal1.4.0 gdal1.3.2 GDAL
  HINTS
     ENV GDAL_DIR
     ENV GDAL_ROOT
     ${_gdal_libpath}
  PATH_SUFFIXES lib
)

include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GDAL DEFAULT_MSG GDAL_LIBRARY GDAL_INCLUDE_DIR)

set(GDAL_LIBRARIES ${GDAL_LIBRARY})
set(GDAL_INCLUDE_DIRS ${GDAL_INCLUDE_DIR})
