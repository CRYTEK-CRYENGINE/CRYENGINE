start_sources()

# -- reflectionunittest.waf_files --
sources_platform(ALL)
add_sources("NoUberFile"
	SOURCE_GROUP "Source Files"
		"../CryReflectionBase/ReflectionBase.cpp"
		"../CryReflectionBase/ReflectionBase.h"
		"Reflection.cpp"
)
end_sources()
CryUnitTestSuite(CryReflectionUnitTest SOLUTION_FOLDER "UnitTests")
if (ORBIS OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_options(${THIS_PROJECT} PRIVATE
		-Wno-unknown-pragmas
		-Wno-reorder
		-Wno-parentheses
		-Wno-unused-variable
		-Wno-format
		-Wno-c++11-narrowing
		-Wno-switch
		-Wno-unused-private-field
		-Wno-unused-value
		-Wno-empty-body
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
		-Wno-maybe-uninitialized
		-Wno-strict-aliasing
		-Wno-multichar
		-Wno-char-subscripts
		-Wno-return-type
		-Wno-parentheses
		-Wno-comment
		-Wno-address
		-Wno-format
		-Wno-switch
		-Wno-stringop-truncation
	)
endif()
use_scaleform()
include_directories("${CRYENGINE_DIR}/Code/CryEngine/CrySystem")
include_directories("${CRYENGINE_DIR}/Code/CryEngine/CryReflection")
include_directories("${CRYENGINE_DIR}/Code/CryEngine/UnitTests/CryReflectionBase")
target_compile_definitions(${THIS_PROJECT} PRIVATE _LIB CRY_IS_MONOLITHIC_BUILD _LAUNCHER)
if (NOT OPTION_STATIC_LINKING)
	target_link_libraries(${THIS_PROJECT} PRIVATE CrySystemLib CryReflectionLib)
else()
	target_link_libraries(${THIS_PROJECT} PRIVATE CrySystem CryReflection)
endif()
