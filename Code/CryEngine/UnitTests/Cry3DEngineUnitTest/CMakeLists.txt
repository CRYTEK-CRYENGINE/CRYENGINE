start_sources()

# -- cry3dengineunittest.waf_files --
sources_platform(ALL)
add_sources("NoUberFile"
	SOURCE_GROUP "Source Files"
		"ParticleSystemUnitTests.cpp"
)
end_sources()

CryUnitTestSuite(Cry3DEngineUnitTest SOLUTION_FOLDER "UnitTests")
use_scaleform()
include_directories("${CRYENGINE_DIR}/Code/CryEngine/Cry3DEngine")
target_compile_definitions(${THIS_PROJECT} PRIVATE _LIB CRY_IS_MONOLITHIC_BUILD _LAUNCHER)
if (NOT OPTION_STATIC_LINKING)
	target_link_libraries(${THIS_PROJECT} PRIVATE CrySystemLib Cry3DEngineLib)
else()
	target_link_libraries(${THIS_PROJECT} PRIVATE CrySystem Cry3DEngine)
endif()

if (ORBIS OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_options(${THIS_PROJECT} PRIVATE
		-Wno-reorder
		-Wno-unknown-pragmas
		-Wno-unused-variable
		-Wno-parentheses
		-Wno-unused-private-field
		-Wno-switch
		-Wno-empty-body
		-Wno-format
		-Wno-logical-op-parentheses
		-Wno-multichar
		-Wno-invalid-offsetof
		-Wno-unused-function
		-Wno-unused-value
		-Wno-sometimes-uninitialized
	)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(${THIS_PROJECT} PRIVATE
		-Wno-unused-function
		-Wno-unused-value
		-Wno-unused-variable
		-Wno-uninitialized
		-Wno-sign-compare
		-Wno-ignored-attributes
		-Wno-int-in-bool-context
		-Wno-unused-but-set-variable
		-Wno-misleading-indentation
		-Wno-strict-aliasing
		-Wno-maybe-uninitialized
		-Wno-unknown-pragmas
		-Wno-unused-const-variable
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
