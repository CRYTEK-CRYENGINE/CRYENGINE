start_sources()

# -- crycommonunittest.waf_files --
sources_platform(ALL)
add_sources("NoUberFile"
	SOURCE_GROUP "Source Files"
		"Utils.h"
		"Utils.cpp"
		"TypeTraitsTests.cpp"
		"TestFrameworkTest.cpp" 
		"MathTests.cpp"
		"HeapAllocatorTests.cpp"
		"StringTests.cpp"
		"StringUtilsTests.cpp"
		"ArrayTests.cpp"
		"RingBufferTests.cpp"
		"VariantTests.cpp"
		"AlignedVectorTests.cpp"
		"SmartPtrTests.cpp"
		"SmallFunctionTests.cpp"
		"PathTests.cpp"
		"GUIDTests.cpp"
		"CryTestTests.cpp"
		"FileVersionTests.cpp"
		"ThreadSafePushContainerTest.cpp"
)
end_sources()

CryUnitTestSuite(CryCommonUnitTest SOLUTION_FOLDER "UnitTests")
if (NOT OPTION_STATIC_LINKING)
	target_compile_definitions(${THIS_PROJECT} PRIVATE NOT_USE_CRY_MEMORY_MANAGER)
else()
	use_scaleform()
	target_compile_definitions(${THIS_PROJECT} PRIVATE _LIB CRY_IS_MONOLITHIC_BUILD _LAUNCHER)
	target_link_libraries(${THIS_PROJECT} PRIVATE CrySystem)
endif()

if (ORBIS OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_options(${THIS_PROJECT} PRIVATE
		-Wno-unused-variable
		-Wno-reorder
		-Wno-unknown-pragmas
		-Wno-parentheses
		-Wno-switch
		-Wno-format
		-Wno-dynamic-class-memaccess
		-Wno-unused-private-field
		-Wno-unused-value
		-Wno-invalid-offsetof
		-Wno-multichar
		-Wno-empty-body
		-Wno-unused-lambda-capture
		-Wno-unused-function
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
		-Wno-strict-aliasing
		-Wno-maybe-uninitialized
		-Wno-unused-const-variable
		-Wno-multichar
		-Wno-char-subscripts
		-Wno-parentheses
		-Wno-comment
		-Wno-address
		-Wno-format
		-Wno-switch
		-Wno-stringop-truncation
		-Wno-free-nonheap-object
	)
endif()
