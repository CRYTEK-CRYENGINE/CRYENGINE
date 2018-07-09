# Mandatory plugin, contains entities required by the engine
if (OPTION_ENGINE)
	add_subdirectory (CryDefaultEntities/Module)
	add_subdirectory(CrySensorSystem/Module)
endif()

# Example plugin
if (PLUGIN_FPSPLUGIN)
	add_subdirectory(CryFramesPerSecond/Module)
endif()
if (PLUGIN_USERANALYTICS)
	add_subdirectory(CryUserAnalytics/Module)
endif()
# VR plugins
if (PLUGIN_VR_OCULUS)
	add_subdirectory(VR/CryOculusVR/Module)
endif()
if (PLUGIN_VR_OPENVR)	
	add_subdirectory(VR/CryOpenVR/Module)
endif()	
if (PLUGIN_VR_OSVR)	
	add_subdirectory(VR/CryOSVR/Module)
endif()

# UQS: Optional plugin; option PLUGIN_CRYUQS to enable/disable it resides in its own sub directory
add_subdirectory(CryUQS)

add_subdirectory(CryHTTP/Module)
add_subdirectory(CryGamePlatform/Module)
add_subdirectory(CryPerceptionSystem/Module)