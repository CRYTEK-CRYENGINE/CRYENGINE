start_sources()

sources_platform(LINUX)

add_sources("NoUberFile"
	SOURCE_GROUP "Header Files"
		"StdAfx.h"
		"assert.h"
	SOURCE_GROUP "Source Files"
		"Assert.cpp"
		"Main.cpp"
)
end_sources()

if(LINUX)
	if(OPTION_DEDICATED_SERVER)
		CryDedicatedServer(LinuxLauncher SOLUTION_FOLDER "Launcher")
	else()
		CryLauncher(LinuxLauncher SOLUTION_FOLDER "Launcher")
	endif()
	target_include_directories(${THIS_PROJECT} PRIVATE "${CRYENGINE_DIR}/Code/CryEngine/CrySystem")
	target_link_libraries( ${THIS_PROJECT} PRIVATE SDL2 dl pthread menuw formw ncursesw jsmn)
	if (ORBIS OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		target_compile_options(${THIS_PROJECT} PRIVATE
			-Wno-unknown-pragmas
			-Wno-unused-variable
			-Wno-unused-value
			-Wno-unused-function
			-Wno-empty-body
			-Wno-reorder
			-Wno-switch
			-Wno-format
			-Wno-unused-private-field
			-Wno-parentheses
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
			-Wno-unused-result
			-Wno-maybe-uninitialized
			-Wno-unused-const-variable
			-Wno-multichar
			-Wno-char-subscripts
			-Wno-parentheses
			-Wno-comment
			-Wno-address
			-Wno-format
			-Wno-switch
		)
    endif()
else()
	CryFileContainer(LinuxLauncher SOLUTION_FOLDER "Launcher")
endif()
