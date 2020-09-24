if (NOT EXISTS "${CRYENGINE_DIR}/Tools/branch_bootstrap/bootstrap.exe" AND NOT EXISTS "${SDK_DIR}")
	set(SDK_ARCHIVE   "CRYENGINE_v5.6.7_SDKs.zip")
	set(GIT_TAG       "5.6.7")

	if(EXISTS "${CRYENGINE_DIR}/${SDK_ARCHIVE}")
		message(STATUS "Using pre-downloaded SDKs: ${SDK_ARCHIVE}")
	else()
		message(STATUS "Downloading SDKs...")
		file(DOWNLOAD "https://github.com/CRYTEK/CRYENGINE/releases/download/${GIT_TAG}/${SDK_ARCHIVE}"
			"${CRYENGINE_DIR}/${SDK_ARCHIVE}" SHOW_PROGRESS)
	endif()

	message("Extracting ${SDK_ARCHIVE} to ${SDK_DIR}/...")
	file(MAKE_DIRECTORY "${SDK_DIR}")
	execute_process(COMMAND "${CMAKE_COMMAND}" "-E" "tar" "xzf" "${CRYENGINE_DIR}/${SDK_ARCHIVE}" WORKING_DIRECTORY "${SDK_DIR}")
	message("Download and extraction of SDKs completed.")
endif()
