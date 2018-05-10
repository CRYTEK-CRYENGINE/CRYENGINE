start_sources()
sources_platform(ALL)
add_sources("NoUberFile"
	SOURCE_GROUP "Source Files"
		"AudioSystemTests.cpp"
)
end_sources()

CryUnitTestSuite(CryAudioSystemUnitTest SOLUTION_FOLDER "UnitTests")
use_scaleform()
include_directories("${CRYENGINE_DIR}/Code/CryEngine/CryAudioSystem")
include_directories("${CRYENGINE_DIR}/Code/CryEngine/CryAudioSystem/Common")
target_compile_definitions(${THIS_PROJECT} PRIVATE _LIB CRY_IS_MONOLITHIC_BUILD _LAUNCHER)
if (NOT OPTION_STATIC_LINKING)
	target_link_libraries(${THIS_PROJECT} PRIVATE CrySystemLib CryAudioSystemLib)
else()
	target_link_libraries(${THIS_PROJECT} PRIVATE CrySystem CryAudioSystem)
endif()

if (ORBIS OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_options(${THIS_PROJECT} PRIVATE
		-Wno-unknown-pragmas
		-Wno-unused-variable
		-Wno-reorder
		-Wno-parentheses
		-Wno-format
		-Wno-switch
		-Wno-unused-value
		-Wno-comment
		-Wno-empty-body
		-Wno-format-security
		-Wno-unused-function
		-Wno-multichar
		-Wno-unused-private-field
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
	)
endif()
