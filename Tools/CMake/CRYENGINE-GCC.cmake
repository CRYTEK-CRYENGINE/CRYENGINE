set(GCC_COMMON_FLAGS 
	-Wall
	-Werror
	-ffast-math
	-flax-vector-conversions

	-fvisibility=hidden
	-fPIC

	-Wno-char-subscripts
	-Wno-unknown-pragmas
	-Wno-unused-variable
	-Wno-unused-value
	-Wno-parentheses
	-Wno-switch
	-Wno-unused-function
	-Wno-unused-result
	-Wno-multichar
	-Wno-format-security
	-Wno-empty-body
	-Wno-comment
	-Wno-char-subscripts
	-Wno-sign-compare
	-Wno-narrowing
	-Wno-write-strings
	-Wno-format
	-Wno-return-type

	-Wno-strict-aliasing
	-Wno-unused-but-set-variable
	-Wno-maybe-uninitialized
	-Wno-strict-overflow
	-Wno-uninitialized
	-Wno-unused-local-typedefs
	-Wno-deprecated

	-Wno-unused-result
	-Wno-sizeof-pointer-memaccess
	-Wno-array-bounds

	-Wno-address

	-fno-exceptions

	# upgrade gcc 4.9 -> 7
	-Wno-stringop-overflow
	-Wno-misleading-indentation
	-Wno-logical-not-parentheses
	-Wno-tautological-compare
)

set(GCC_CPP_COMMON_FLAGS
	-fno-rtti 
	-std=c++11
	-Wno-invalid-offsetof
	-Wno-reorder
	-Wno-conversion-null
	-Wno-overloaded-virtual
	-Wno-c++0x-compat
	-Wno-non-template-friend

	# upgrade gcc 4.9 -> 7
	-Wno-int-in-bool-context
	-Wno-aligned-new
	-Wno-ignored-attributes
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