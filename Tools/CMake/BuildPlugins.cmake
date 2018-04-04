# CryExtensions
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Code/CryExtensions")
	add_subdirectory(Code/CryExtensions)
endif()

# Mandatory plugin, contains entities required by the engine
if (OPTION_ENGINE)
	add_subdirectory (Code/CryPlugins/CryDefaultEntities/Module)
	add_subdirectory(Code/CryPlugins/CrySensorSystem/Module)
endif()

# Example plugin
if (PLUGIN_FPSPLUGIN)
	add_subdirectory(Code/CryPlugins/CryFramesPerSecond/Module)
endif()
if (PLUGIN_USERANALYTICS)
	add_subdirectory(Code/CryPlugins/CryUserAnalytics/Module)
endif()
# VR plugins
if (PLUGIN_VR_OCULUS)
	add_subdirectory(Code/Libs/oculus)
	add_subdirectory(Code/CryPlugins/VR/CryOculusVR/Module)
endif()
if (PLUGIN_VR_OPENVR)	
	add_subdirectory(Code/CryPlugins/VR/CryOpenVR/Module)
endif()	
if (PLUGIN_VR_OSVR)	
	add_subdirectory(Code/CryPlugins/VR/CryOSVR/Module)
endif()

# UQS: Optional plugin; option PLUGIN_CRYUQS to enable/disable it resides in its own sub directory
add_subdirectory(Code/CryPlugins/CryUQS)
