set(GCC_COMMON_FLAGS
	-Wall
	-Werror

	-ffast-math
	-flax-vector-conversions
	-fvisibility=hidden
	-fPIC
	-fno-exceptions

	-Wno-unknown-warning
)

set(GCC_CPP_COMMON_FLAGS
	-fno-rtti
	-std=c++11
	-Wno-invalid-offsetof
	-Wno-aligned-new
	-Wno-conversion-null
	-Wno-unused-result
	-Wno-reorder
	-Wno-delete-non-virtual-dtor # Needed to provide virtual dispatch to allow strings to be modified on CryCommon types
	-Wno-class-memaccess
)

string(REPLACE ";" " " GCC_COMMON_FLAGS "${GCC_COMMON_FLAGS}")
string(REPLACE ";" " " GCC_CPP_COMMON_FLAGS "${GCC_CPP_COMMON_FLAGS}")

set(CMAKE_CXX_FLAGS "${GCC_COMMON_FLAGS} ${GCC_CPP_COMMON_FLAGS}" CACHE STRING "C++ Flags" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -D_DEBUG -DDEBUG" CACHE STRING "C++ Debug Flags" FORCE)
set(CMAKE_CXX_FLAGS_PROFILE "-O2 -D_PROFILE -DNDEBUG" CACHE STRING "C++ Profile Flags" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -D_RELEASE -DNDEBUG" CACHE STRING "C++ Release Flags" FORCE)

set(CMAKE_C_FLAGS "${GCC_COMMON_FLAGS}" CACHE STRING "C Flags" FORCE)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}" CACHE STRING "C Debug Flags" FORCE)
set(CMAKE_C_FLAGS_PROFILE "${CMAKE_CXX_FLAGS_PROFILE}" CACHE STRING "C Profile Flags" FORCE)
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}" CACHE STRING "C Release Flags" FORCE)

set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "Libraries linked by default with all C++ applications." FORCE)

set(CMAKE_LINK_FLAGS "{$CMAKE_LINK_FLAGS} -Wl, --gc-sections")


set(CMAKE_SHARED_LINKER_FLAGS_PROFILE ${CMAKE_SHARED_LINKER_FLAGS_DEBUG} CACHE STRING "Linker Library Profile Flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_PROFILE ${CMAKE_EXE_LINKER_FLAGS_DEBUG} CACHE STRING "Linker Executable Profile Flags" FORCE)

function (wrap_whole_archive project target source)
	set(${target} "-Wl,--whole-archive;${${source}};-Wl,--no-whole-archive" PARENT_SCOPE)
endfunction()
