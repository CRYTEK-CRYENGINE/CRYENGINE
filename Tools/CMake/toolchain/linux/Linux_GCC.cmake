include ("${CMAKE_CURRENT_LIST_DIR}/../../CrossPlatformSetup.cmake")
set(CMAKE_CONFIGURATION_TYPES Debug Profile Release)
set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "Reset the configurations to what we need" FORCE)

set(BUILD_CPU_ARCHITECTURE x64)
set(BUILD_PLATFORM Linux64)
set(LINUX TRUE)
set(OUTPUT_DIRECTORY_NAME "linux_x64_gcc")

if(EXISTS "${CRYENGINE_DIR}/linux_bootstrap")
	set(LINUX_BOOTSTRAP_FOLDER "${CRYENGINE_DIR}/linux_bootstrap/Code/SDKs")
	message(STATUS "Changing SDK_DIR from ${SDK_DIR} to ${LINUX_BOOTSTRAP_FOLDER}")
	set(SDK_DIR "${LINUX_BOOTSTRAP_FOLDER}")
endif()

# QtCreator requires selection of a "kit", which includes these, so don't force them.
if(NOT CMAKE_C_COMPILER)
	set(CMAKE_C_COMPILER gcc-7)
endif()

if(NOT CMAKE_CXX_COMPILER)
	set(CMAKE_CXX_COMPILER g++-7)
endif()

message(STATUS "CMAKE_C_COMPILER = ${CMAKE_C_COMPILER}")
message(STATUS "CMAKE_CXX_COMPILER = ${CMAKE_CXX_COMPILER}")

add_definitions(-DLINUX64)

include ("${CMAKE_CURRENT_LIST_DIR}/../../CRYENGINE-GCC.cmake")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DLINUX -D__linux__" CACHE STRING "C Common Flags" FORCE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLINUX -D__linux__" CACHE STRING "C++ Common Flags" FORCE)
