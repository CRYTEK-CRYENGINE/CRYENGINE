start_sources()

sources_platform(LINUX)

add_sources("NoUberFile"
	SOURCE_GROUP "Header Files"
		"StdAfx.h"
		"assert.h"
	SOURCE_GROUP "Source Files"
		"Assert.cpp"
		"EngineSymbols.cpp"
		"Main.cpp"
)
end_sources()

if(LINUX)
	CryLauncher(LinuxLauncher SOLUTION_FOLDER "Launcher")
	target_include_directories(${THIS_PROJECT} PRIVATE "${CRYENGINE_DIR}/Code/CryEngine/CrySystem")
	target_link_libraries( ${THIS_PROJECT} PRIVATE SDL2 dl pthread menuw formw ncursesw jsmn)
else()
	CryFileContainer(LinuxLauncher SOLUTION_FOLDER "Launcher")
endif()
