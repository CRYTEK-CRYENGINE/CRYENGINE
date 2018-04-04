option(PROJECT_CRYENGINE_GAMESDK "Build GameSDK sample project" ON)
if (NOT PROJECT_CRYENGINE_GAMESDK)
	return()
endif()

add_subdirectory(GameDll)
