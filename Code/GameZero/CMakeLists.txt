option(PROJECT_CRYENGINE_GAMEZERO "Build GameZero sample project" OFF)
if (NOT PROJECT_CRYENGINE_GAMEZERO)
	return()
endif()

add_subdirectory(GameDll)
