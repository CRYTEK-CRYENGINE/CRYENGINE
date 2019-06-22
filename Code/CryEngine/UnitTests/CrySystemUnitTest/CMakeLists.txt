start_sources()

# -- systemunittest.waf_files --
sources_platform(ALL)
add_sources("NoUberFile"
	SOURCE_GROUP "Source Files"
        "xml.cpp"
		"ArchiveHostTests.cpp"
		"JobTests.cpp"
)
end_sources()

CryUnitTestSuite(CrySystemUnitTest SOLUTION_FOLDER "UnitTests")

use_scaleform()
include_directories("${CRYENGINE_DIR}/Code/CryEngine/CrySystem")
target_compile_definitions(${THIS_PROJECT} PRIVATE _LIB CRY_IS_MONOLITHIC_BUILD _LAUNCHER)
if (NOT OPTION_STATIC_LINKING)
	target_link_libraries(${THIS_PROJECT} PRIVATE CrySystemLib)
else()
	target_link_libraries(${THIS_PROJECT} PRIVATE CrySystem)
endif()

if (ORBIS OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_options(${THIS_PROJECT} PRIVATE
		-Wno-unused-variable
		-Wno-parentheses
		-Wno-reorder
		-Wno-unused-value
		-Wno-unknown-pragmas
		-Wno-format
		-Wno-unused-private-field
		-Wno-multichar
		-Wno-empty-body
		-Wno-unused-const-variable
		-Wno-switch
		-Wno-unused-function
		-Wno-char-subscripts
		-Wno-c++11-narrowing
		-Wno-address-of-packed-member
		-Wno-invalid-offsetof
		-Wno-dynamic-class-memaccess
		-Wno-comment
	)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(${THIS_PROJECT} PRIVATE
		-Wno-unused-function
		-Wno-unused-value
		-Wno-unused-variable
		-Wno-sign-compare
		-Wno-ignored-attributes
		-Wno-int-in-bool-context
		-Wno-unused-but-set-variable
		-Wno-misleading-indentation
		-Wno-unknown-pragmas
		-Wno-unused-const-variable
		-Wno-strict-aliasing
		-Wno-maybe-uninitialized
		-Wno-multichar
		-Wno-char-subscripts
		-Wno-parentheses
		-Wno-comment
		-Wno-address
		-Wno-format
		-Wno-switch
		-Wno-stringop-truncation
	)
endif()
