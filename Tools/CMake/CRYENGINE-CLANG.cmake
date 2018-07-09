set(CLANG_COMMON_FLAGS
	-Werror
	-Wall

	-Wno-c++11-extensions
	-Wno-c++11-narrowing
	-Wno-char-subscripts
	-Wno-comment
	-Wno-conversion-null
	-Wno-deprecated-writable-strings
	-Wno-dynamic-class-memaccess
	-Wno-empty-body
	-Wno-format
	-Wno-format-security
	-Wno-invalid-offsetof
	-Wno-multichar
	-Wno-narrowing
	-Wno-null-character
	-Wno-overloaded-virtual
	-Wno-parentheses
	-Wno-reorder
	-Wno-self-assign
	-Wno-sign-compare
	-Wno-sizeof-pointer-memaccess
	-Wno-sometimes-uninitialized
	-Wno-switch
	-Wno-tautological-compare
	-Wno-unknown-pragmas
	-Wno-unneeded-internal-declaration
	-Wno-unused-function
	-Wno-unused-private-field
	-Wno-unused-result
	-Wno-unused-value
	-Wno-unused-variable

	-fvisibility-inlines-hidden
	-fvisibility=hidden
	-fPIC

	-D_HAS_C9X
	-Wno-write-strings
	-Wno-logical-op-parentheses
	-Wno-null-conversion
	-Wno-overloaded-virtual
	-Wno-shift-sign-overflow

	-fdata-sections
	-ffunction-sections
	-fno-omit-frame-pointer
	-fno-strict-aliasing
	-funwind-tables
	-gfull
	-ffast-math
	-fno-rtti

	# clang 3.8 -> 5.0 upgrade
	-Wno-unknown-warning-option      # Allows multiple versions of clang to be used
	-Wno-dangling-else
	-Wno-null-dereference
	-Wno-undefined-var-template
	-Wno-delete-non-virtual-dtor
	-Wno-unused-lambda-capture
	-Wno-deprecated-declarations
	-Wno-address-of-packed-member
	-Wno-expansion-to-defined
)

if(NOT ANDROID)
  set(CLANG_COMMON_FLAGS ${CLANG_COMMON_FLAGS} 
    -msse2
    -Wunknown-attributes
    -fno-exceptions
  )
endif()

if(ANDROID)
  set(CLANG_COMMON_FLAGS ${CLANG_COMMON_FLAGS}
	-Wno-deprecated
	-Wno-nonportable-include-path		# This will be caught be submissions tests.
    -fno-exceptions
    -fms-extensions
	-D_MSC_EXTENSIONS=1
  )
endif()

set(CLANG_CPP_COMMON_FLAGS
	-std=c++11
)

string(REPLACE ";" " " CLANG_COMMON_FLAGS "${CLANG_COMMON_FLAGS}")
string(REPLACE ";" " " CLANG_CPP_COMMON_FLAGS "${CLANG_CPP_COMMON_FLAGS}")

if (NOT (ORBIS AND ${CMAKE_GENERATOR} MATCHES "Visual Studio"))
	# HACK: Do not apply this to Orbis in Visual Studio; it breaks .c file compilation
	set(CMAKE_CXX_FLAGS "${CLANG_COMMON_FLAGS} ${CLANG_CPP_COMMON_FLAGS}" CACHE STRING "C++ Common Flags" FORCE)
else()
	set(CMAKE_CXX_FLAGS "${CLANG_COMMON_FLAGS} ${CLANG_CPP_COMMON_FLAGS}" CACHE STRING "C++ Common Flags" FORCE)
endif()

message(STATUS "CMAKE_CXX_FLAGS = ${CMAKE_CXX_FLAGS}" )

# Set MSVC option for Visual Studio properties to be displayed correctly
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -D_DEBUG -DDEBUG" CACHE STRING "C++ Debug Flags" FORCE)
set(CMAKE_CXX_FLAGS_PROFILE "-O3 -D_PROFILE -DNDEBUG" CACHE STRING "C++ Profile Flags" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -D_RELEASE -DNDEBUG" CACHE STRING "C++ Release Flags" FORCE)

set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "Libraries linked by defalut with all C++ applications." FORCE)

set(CMAKE_C_FLAGS "${CLANG_COMMON_FLAGS}" CACHE STRING "C Common Flags" FORCE)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}" CACHE STRING "C Debug Flags" FORCE)
set(CMAKE_C_FLAGS_PROFILE "${CMAKE_CXX_FLAGS_PROFILE}" CACHE STRING "C Profile Flags" FORCE)
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}" CACHE STRING "C Release Flags" FORCE)

#set(CMAKE_LINK_FLAGS "{$CMAKE_LINK_FLAGS}") #-Wl,-dead_strip

set(CMAKE_SHARED_LINKER_FLAGS_PROFILE ${CMAKE_SHARED_LINKER_FLAGS_DEBUG} CACHE STRING "Linker Library Profile Flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_PROFILE ${CMAKE_EXE_LINKER_FLAGS_DEBUG} CACHE STRING "Linker Executable Profile Flags" FORCE)

function (wrap_whole_archive project target source)
	set(${target} "-Wl,--whole-archive;${${source}};-Wl,--no-whole-archive" PARENT_SCOPE)
endfunction()
