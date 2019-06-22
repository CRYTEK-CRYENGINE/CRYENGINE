cmake_policy(SET CMP0011 NEW) # Included scripts do automatic cmake_policy PUSH and POP if this is not set.
cmake_policy(SET CMP0048 NEW) # Set VERSION as documented by the project() command.
cmake_policy(SET CMP0053 NEW) # Use the simpler variable expansion and escape sequence evaluation rules.
cmake_policy(SET CMP0054 NEW) # Only interpret if() arguments as variables or keywords when unquoted if this is not set.
cmake_policy(SET CMP0063 NEW) # Honor the visibility properties for all target types.
cmake_policy(SET CMP0067 NEW) # Honor language standard in try_compile() source-file signature.

set_property(GLOBAL PROPERTY DEBUG_CONFIGURATIONS Debug Profile)

# Turn on the ability to create folders to organize projects (.vcproj)
# It creates "CMakePredefinedTargets" folder by default and adds CMake
# defined projects like INSTALL.vcproj and ZERO_CHECK.vcproj
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(NOT DEFINED CRYENGINE_DIR)
	set(CRYENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

if(NOT DEFINED PROJECT_DIR)
	set(PROJECT_DIR "${CMAKE_SOURCE_DIR}")
endif()
	
set(TOOLS_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

#Fix slashes on paths
string(REPLACE "\\" "/" CRYENGINE_DIR "${CRYENGINE_DIR}")
string(REPLACE "\\" "/" PROJECT_DIR "${PROJECT_DIR}")
string(REPLACE "\\" "/" TOOLS_CMAKE_DIR "${TOOLS_CMAKE_DIR}")

set(CMAKE_MODULE_PATH "${TOOLS_CMAKE_DIR}/modules")

if(NOT DEFINED SDK_DIR)
	set(SDK_DIR "${CRYENGINE_DIR}/Code/SDKs")
endif()

message(STATUS "CMAKE_GENERATOR = ${CMAKE_GENERATOR}")
message(STATUS "CMAKE_CONFIGURATION_TYPES = ${CMAKE_CONFIGURATION_TYPES}")
message(STATUS "OPTION_PROFILE = ${OPTION_PROFILE}")
message(STATUS "OPTION_PCH = ${OPTION_PCH}")
message(STATUS "CRYENGINE_DIR = ${CRYENGINE_DIR}")
message(STATUS "SDK_DIR = ${SDK_DIR}")
message(STATUS "PROJECT_DIR = ${PROJECT_DIR}")
message(STATUS "TOOLS_CMAKE_DIR = ${TOOLS_CMAKE_DIR}")

# Bootstrap support
if(EXISTS "${TOOLS_CMAKE_DIR}/Bootstrap.cmake")
	include("${TOOLS_CMAKE_DIR}/Bootstrap.cmake")
	if(OPTION_AUTO_BOOTSTRAP)
		set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "bootstrap.dat")
	endif()
elseif(EXISTS "${TOOLS_CMAKE_DIR}/DownloadSDKs.cmake")
	include("${TOOLS_CMAKE_DIR}/DownloadSDKs.cmake")
endif()
