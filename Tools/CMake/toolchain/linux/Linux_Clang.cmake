set(CMAKE_CONFIGURATION_TYPES Debug Profile Release)
set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "Reset the configurations to what we need" FORCE)

set(BUILD_CPU_ARCHITECTURE x64)
set(BUILD_PLATFORM Linux64)
set(LINUX 1)
set(LINUX64 1)
set(OUTPUT_DIRECTORY_NAME "linux_x64_clang")


if(EXISTS "${CMAKE_SOURCE_DIR}/linux_bootstrap")
	set(LINUX_BOOTSTRAP_FOLDER "${CMAKE_SOURCE_DIR}/linux_bootstrap/Code/SDKs")
else()
	set(LINUX_BOOTSTRAP_FOLDER "${CMAKE_SOURCE_DIR}/Code/SDKs")
endif()

if(NOT CMAKE_C_COMPILER)
	if(EXISTS "${LINUX_BOOTSTRAP_FOLDER}/clang-5.0/bin/clang")
		set(CMAKE_C_COMPILER "${LINUX_BOOTSTRAP_FOLDER}/clang-5.0/bin/clang")
	else()
		set(CMAKE_C_COMPILER clang-5.0)
	endif()
endif()
if(NOT CMAKE_CXX_COMPILER)
	if(EXISTS "${LINUX_BOOTSTRAP_FOLDER}/clang-5.0/bin/clang++")
		set(CMAKE_CXX_COMPILER "${LINUX_BOOTSTRAP_FOLDER}/clang-5.0/bin/clang++")
	else()
		set(CMAKE_CXX_COMPILER clang++-5.0)
	endif()
endif()

message(STATUS "CMAKE_C_COMPILER = ${CMAKE_C_COMPILER}")
message(STATUS "CMAKE_CXX_COMPILER = ${CMAKE_CXX_COMPILER}")

add_definitions(-DLINUX64)

message(STATUS "BUILD_CPU_ARCHITECTURE = ${BUILD_CPU_ARCHITECTURE}")

include ("${CMAKE_CURRENT_LIST_DIR}/../../CRYENGINE-CLANG.cmake")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DLINUX -D__linux__" CACHE STRING "C Common Flags" FORCE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLINUX -D__linux__" CACHE STRING "C++ Common Flags" FORCE)
