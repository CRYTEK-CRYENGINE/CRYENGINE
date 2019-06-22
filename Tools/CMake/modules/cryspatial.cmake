set(CRYSPATIAL_DIR "${SDK_DIR}/Audio/cryspatial")

if(WIN64)
	add_library(CrySpatial SHARED IMPORTED GLOBAL)
	set(CRYSPATIAL_DLL "${CRYSPATIAL_DIR}/x64/CrySpatial.dll")

elseif(DURANGO)
	add_library(CrySpatial SHARED IMPORTED GLOBAL)
	set(CRYSPATIAL_DLL "${CRYSPATIAL_DIR}/xboxone/CrySpatial.dll")

endif()

if(AUDIO_CRYSPATIAL AND CRYSPATIAL_DLL)
	deploy_runtime_files("${CRYSPATIAL_DLL}")
endif()
