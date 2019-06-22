# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# CMakeFindFrameworks
# -------------------
#
# helper module to find OSX frameworks
#
# This module reads hints about search locations from variables::
#
#   CMAKE_FIND_FRAMEWORK_EXTRA_LOCATIONS - Extra directories

if(NOT CMAKE_FIND_FRAMEWORKS_INCLUDED)
  set(CMAKE_FIND_FRAMEWORKS_INCLUDED 1)
  macro(CMAKE_FIND_FRAMEWORKS fwk)
    set(${fwk}_FRAMEWORKS)
    if(APPLE)
      foreach(dir
          ~/Library/Frameworks/${fwk}.framework
          /usr/local/Frameworks/${fwk}.framework
          /Library/Frameworks/${fwk}.framework
          /System/Library/Frameworks/${fwk}.framework
          /Network/Library/Frameworks/${fwk}.framework
          ${CMAKE_FIND_FRAMEWORK_EXTRA_LOCATIONS})
        if(EXISTS ${dir})
          set(${fwk}_FRAMEWORKS ${${fwk}_FRAMEWORKS} ${dir})
        endif()
      endforeach()
    endif()
  endmacro()
endif()
