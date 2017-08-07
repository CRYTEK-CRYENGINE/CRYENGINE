# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


# This file is included in CMakeSystemSpecificInformation.cmake if
# the KDevelop3 extra generator has been selected.

find_program(CMAKE_KDEVELOP3_EXECUTABLE NAMES kdevelop DOC "The KDevelop3 executable")

if(CMAKE_KDEVELOP3_EXECUTABLE)
   set(CMAKE_OPEN_PROJECT_COMMAND "${CMAKE_KDEVELOP3_EXECUTABLE} <PROJECT_FILE>" )
endif()

