start_sources()

# -- cryaisystemunittest.waf_files --
sources_platform(ALL)
add_sources("NoUberFile"
	SOURCE_GROUP "Navigation"
		"Navigation/FixedAABBTests.cpp"
		"Navigation/NavMeshTests.cpp"
)
end_sources()

CryUnitTestSuite(CryAISystemUnitTest SOLUTION_FOLDER "UnitTests")
use_scaleform()
include_directories("${CRYENGINE_DIR}/Code/CryEngine/CryAISystem")
target_compile_definitions(${THIS_PROJECT} PRIVATE _LIB CRY_IS_MONOLITHIC_BUILD _LAUNCHER)
if (NOT OPTION_STATIC_LINKING)
        target_link_libraries(${THIS_PROJECT} PRIVATE CrySystemLib CryAISystemLib)
else()
	target_link_libraries(${THIS_PROJECT} PRIVATE CrySystem CryAISystem)
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
