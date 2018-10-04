if (NOT EXISTS "${CMAKE_SOURCE_DIR}/Tools/branch_bootstrap/bootstrap.exe" AND NOT EXISTS "${CMAKE_SOURCE_DIR}/Code/SDKs")
	set(SDK_ARCHIVE   "CRYENGINE_v5.5.0_SDKs.zip")
	set(GIT_TAG       "5.5.0")

	if(EXISTS "${CMAKE_SOURCE_DIR}/${SDK_ARCHIVE}")
		message(STATUS "Using pre-downloaded SDKs: ${SDK_ARCHIVE}")
	else()
		message(STATUS "Downloading SDKs...")
		file(DOWNLOAD "https://github.com/CRYTEK/CRYENGINE/releases/download/${GIT_TAG}/${SDK_ARCHIVE}"
			"${CMAKE_CURRENT_SOURCE_DIR}/${SDK_ARCHIVE}" SHOW_PROGRESS)
	endif()

	message("Extracting ${SDK_ARCHIVE} to ${CMAKE_SOURCE_DIR}/Code/SDKs/...")
	file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/Code/SDKs")
	execute_process(COMMAND "${CMAKE_COMMAND}" "-E" "tar" "xzf" "${CMAKE_SOURCE_DIR}/${SDK_ARCHIVE}" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/Code/SDKs")
	message("Download and extraction of SDKs completed.")
endif()
