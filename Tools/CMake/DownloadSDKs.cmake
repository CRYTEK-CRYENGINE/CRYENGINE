## To ignore warning "variable not used" that happens if we skip SDK extraction/etc.
## And to remove caching of this option
set(actual_mode "${CMAKE_MODE}")
unset(CMAKE_MODE CACHE)

if (NOT EXISTS "${CRYENGINE_DIR}/Tools/branch_bootstrap/bootstrap.exe" AND NOT EXISTS "${CRYENGINE_DIR}/Code/SDKs")
	set(GIT_REPO      "CRYTEK/CRYENGINE")
	set(GIT_TAG       "5.5.2")
	set(SDK_ARCHIVE   "CRYENGINE_v5.5.2_SDKs")
	
	## Update download_sdk config using CMake variables
	set(SCRIPT_DIR    "${TOOLS_CMAKE_DIR}/download_sdks")
	configure_file(${SCRIPT_DIR}/base.json ${SCRIPT_DIR}/sdk_config.json)

	message(STATUS "Downloading/Extracting SDKs...")
	
	## Downloads or re-extracts this zip, uses sdk_config.json for target URL/zip params.
	execute_process(COMMAND "download_sdks.exe" ${actual_mode}
	WORKING_DIRECTORY "${SCRIPT_DIR}"
	RESULT_VARIABLE RES)

	## PERSONAL TODO: Switch if linux mode, assume python 3 is in path.
	## Don't have GUI linux box to develop this atm.
		#execute_process(COMMAND "python" "download_sdks.py"
		#WORKING_DIRECTORY "${SCRIPT_DIR}"
		#RESULT_VARIABLE RES)
	
	if(NOT ${RES} EQUAL 0)
		message(FATAL_ERROR "Failed to download/extract SDKs!")
	endif()
else()
	message(STATUS "SDKs folder already exists, no need for download/extraction.")
endif()