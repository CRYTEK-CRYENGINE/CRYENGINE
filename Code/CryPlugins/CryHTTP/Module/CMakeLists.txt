if ((WIN32 OR ORBIS) AND OPTION_ENGINE)
	option(PLUGIN_HTTP "Enable HTTP support" ON)
else()
	option(PLUGIN_HTTP "Enable HTTP support" OFF)
endif()
	
if (NOT PLUGIN_HTTP)
	return()
endif()

start_sources()
sources_platform(ALL)
add_sources("CryHTTP_uber_0.cpp"
	SOURCE_GROUP "Root"
		"resource.h"
		"StdAfx.cpp"
		"StdAfx.h"
)

sources_platform(WIN32 LINUX APPLE)
add_sources("CryHTTP_cURL_uber_0.cpp"
	SOURCE_GROUP "Implementation"
		"HttpImplementation_cURL.cpp"
		"HttpImplementation_cURL.h"
)

sources_platform(ORBIS)
add_sources("CryHTTP_sce_uber_0.cpp"
	SOURCE_GROUP "Implementation"
		"HttpImplementation_SCE.cpp"
		"HttpImplementation_SCE.h"
)

add_sources("NoUberFile"
	SOURCE_GROUP "Root"
		"StdAfx.cpp"
)

end_sources()

CryEngineModule(CryHTTP PCH "StdAfx.cpp" SOLUTION_FOLDER "CryPlugins")

target_include_directories(${THIS_PROJECT}
PRIVATE 
	"${CRYENGINE_DIR}/Code/CryEngine/CryCommon"
PUBLIC
	../Interface
)

if(WIN32 OR LINUX OR APPLE)
	target_link_libraries( ${THIS_PROJECT} PRIVATE curl )
endif()