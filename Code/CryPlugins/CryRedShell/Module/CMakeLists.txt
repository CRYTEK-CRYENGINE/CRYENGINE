if ((WIN32 OR WIN64) AND PLUGIN_GAMEPLATFORM)
	option(PLUGIN_REDSHELL "Enable RedShell integration plug-in" ON)
else()
	option(PLUGIN_REDSHELL "Enable RedShell integration plug-in" OFF)
endif()
	
if (NOT PLUGIN_REDSHELL)
	return()
endif()

start_sources()
sources_platform(ALL)
add_sources("CryRedShellPlugin_uber.cpp"
	SOURCE_GROUP "Root"
		"RedShellPluginDLL.cpp"
		"resource.h"
		"StdAfx.cpp"
		"StdAfx.h"
)
end_sources()

CryEngineModule(CryRedShell FORCE_SHARED PCH "StdAfx.cpp" SOLUTION_FOLDER "CryPlugins")

target_include_directories(${THIS_PROJECT}
PRIVATE 
	"${CRYENGINE_DIR}/Code/CryEngine/CryCommon"
	"${CRYENGINE_DIR}/Code/CryPlugins"
PUBLIC
	../Interface
)

if (WIN64 OR WIN32)
	find_package(redshellsdk CONFIG PATHS "${TOOLS_CMAKE_DIR}/modules/")
	if (redshellsdk_FOUND)
		target_link_libraries(${THIS_PROJECT} PRIVATE redshellsdk)
		message(STATUS "RedShell SDK found in ${RED_SHELL_SDK_ROOT_FOLDER}")
	endif()
endif()