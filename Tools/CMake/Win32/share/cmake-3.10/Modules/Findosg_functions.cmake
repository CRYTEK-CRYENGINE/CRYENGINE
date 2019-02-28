# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# Findosg_functions
# -----------------
#
#
#
#
#
# This CMake file contains two macros to assist with searching for OSG
# libraries and nodekits.  Please see FindOpenSceneGraph.cmake for full
# documentation.

#
# OSG_FIND_PATH
#
function(OSG_FIND_PATH module header)
   string(TOUPPER ${module} module_uc)

   # Try the user's environment request before anything else.
   find_path(${module_uc}_INCLUDE_DIR ${header}
       HINTS
            ENV ${module_uc}_DIR
            ENV OSG_DIR
            ENV OSGDIR
            ENV OSG_ROOT
            ${${module_uc}_DIR}
            ${OSG_DIR}
       PATH_SUFFIXES include
       PATHS
            /sw # Fink
            /opt/local # DarwinPorts
            /opt/csw # Blastwave
            /opt
            /usr/freeware
   )
endfunction()


#
# OSG_FIND_LIBRARY
#
function(OSG_FIND_LIBRARY module library)
   string(TOUPPER ${module} module_uc)

   find_library(${module_uc}_LIBRARY
       NAMES ${library}
       HINTS
            ENV ${module_uc}_DIR
            ENV OSG_DIR
            ENV OSGDIR
            ENV OSG_ROOT
            ${${module_uc}_DIR}
            ${OSG_DIR}
       PATH_SUFFIXES lib
       PATHS
            /sw # Fink
            /opt/local # DarwinPorts
            /opt/csw # Blastwave
            /opt
            /usr/freeware
   )

   find_library(${module_uc}_LIBRARY_DEBUG
       NAMES ${library}d
       HINTS
            ENV ${module_uc}_DIR
            ENV OSG_DIR
            ENV OSGDIR
            ENV OSG_ROOT
            ${${module_uc}_DIR}
            ${OSG_DIR}
       PATH_SUFFIXES lib
       PATHS
            /sw # Fink
            /opt/local # DarwinPorts
            /opt/csw # Blastwave
            /opt
            /usr/freeware
    )

   if(NOT ${module_uc}_LIBRARY_DEBUG)
      # They don't have a debug library
      set(${module_uc}_LIBRARY_DEBUG ${${module_uc}_LIBRARY} PARENT_SCOPE)
      set(${module_uc}_LIBRARIES ${${module_uc}_LIBRARY} PARENT_SCOPE)
   else()
      # They really have a FOO_LIBRARY_DEBUG
      set(${module_uc}_LIBRARIES
          optimized ${${module_uc}_LIBRARY}
          debug ${${module_uc}_LIBRARY_DEBUG}
          PARENT_SCOPE
      )
   endif()
endfunction()

#
# OSG_MARK_AS_ADVANCED
# Just a convenience function for calling MARK_AS_ADVANCED
#
function(OSG_MARK_AS_ADVANCED _module)
   string(TOUPPER ${_module} _module_UC)
   mark_as_advanced(${_module_UC}_INCLUDE_DIR)
   mark_as_advanced(${_module_UC}_LIBRARY)
   mark_as_advanced(${_module_UC}_LIBRARY_DEBUG)
endfunction()
