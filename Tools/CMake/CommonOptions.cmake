if (NOT PROJECT_BUILD)
	option(OPTION_ENGINE "Build CRYENGINE" ON)
else()
	option(OPTION_ENGINE "Build CRYENGINE" OFF)
endif()

option(OPTION_PROFILE "Enable Profiling" ON)
option(OPTION_UNITY_BUILD "Enable Unity Build" ON)

option(OPTION_PCH "Enable Precompiled Headers" ON)

if(ORBIS OR ANDROID)
	set(OPTION_STATIC_LINKING TRUE)
else()
	option(OPTION_STATIC_LINKING "Link all CryEngine modules as static libs to single exe" OFF)
endif()

if(OPTION_ENGINE)
	option(OPTION_UQS_SCHEMATYC_SUPPORT "Enable UQS functions in Schematyc" ON)
else()
	option(OPTION_UQS_SCHEMATYC_SUPPORT "Enable UQS functions in Schematyc" OFF)
endif()
	
if(OPTION_UQS_SCHEMATYC_SUPPORT)
	list(APPEND global_defines UQS_SCHEMATYC_SUPPORT=1)
else()
	list(APPEND global_defines UQS_SCHEMATYC_SUPPORT=0)
endif()
