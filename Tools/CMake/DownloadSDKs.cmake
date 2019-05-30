if (NOT EXISTS "${CMAKE_SOURCE_DIR}/Tools/branch_bootstrap/bootstrap.exe" AND NOT EXISTS "${CMAKE_SOURCE_DIR}/Code/SDKs")
	set(SDK_ARCHIVE   "CRYENGINE_v5.5.0_SDKs.zip")
	set(GIT_TAG       "5.5.0_preview4")
	include(FetchContent)
	FetchContent_Declare(
		CRYENGINE
		URL "https://github.com/CRYTEK/CRYENGINE/releases/download/${GIT_TAG}/${SDK_ARCHIVE}"
		SOURCE_DIR ${SDK_DIR}
	)
	FetchContent_GetProperties(CRYENGINE)
	if(NOT CRYENGINE_POPULATED)
		message(STATUS "Downloading SDKs...")
		FetchContent_Populate(CRYENGINE)
	endif(NOT CRYENGINE_POPULATED)
endif(NOT EXISTS "${CMAKE_SOURCE_DIR}/Tools/branch_bootstrap/bootstrap.exe" AND NOT EXISTS "${CMAKE_SOURCE_DIR}/Code/SDKs")
