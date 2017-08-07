# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


# This file is included by cmGlobalGenerator::EnableLanguage.
# It is included before the compiler has been determined.

include(Platform/${CMAKE_SYSTEM_NAME}-Initialize OPTIONAL)

set(CMAKE_SYSTEM_SPECIFIC_INITIALIZE_LOADED 1)
