if (NOT PROJECT_BUILD)
	option(OPTION_ENGINE "Build CRYENGINE" ON)
else()
	option(OPTION_ENGINE "Build CRYENGINE" OFF)
endif()

option(OPTION_PROFILE "Enable Profiling" ON)
option(OPTION_UNITY_BUILD "Enable Unity Build" ON)

option(OPTION_PCH "Enable Precompiled Headers" ON)

option(OPTION_GEOM_CACHES "Enable Geom Cache" ON)

if(ORBIS OR ANDROID)
	set(OPTION_STATIC_LINKING TRUE)
else()
	option(OPTION_STATIC_LINKING "Link all CryEngine modules as static libs to single exe" OFF)
endif()

option(OPTION_STATIC_LINKING_WITH_GAME_AS_DLL "Build game as DLL in a monolithic CryEngine build" OFF)

option(OPTION_RUNTIME_CVAR_OVERRIDES "Use runtime CVar overrides to allow for multiple projects in the same solution without having to recompile" OFF)

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

if(OPTION_GEOM_CACHES)
	list(APPEND global_defines USE_GEOM_CACHES=1)
endif()

if (WINDOWS OR LINUX)
	option(OPTION_DEDICATED_SERVER "Build engine in Dedicated Server mode" OFF)
endif()

option(OPTION_LTCG "Enable link-time code generation/optimization" OFF)
set(OPTION_PGO "Off" CACHE STRING "Enable profile-guided optimization")
set_property(CACHE OPTION_PGO PROPERTY STRINGS "Off" "Generate" "Use")

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	option(OPTION_PGO_INSTRUMENT "Use instrumentation instead of sampling for PGO (more expensive, but better results)" OFF)
endif()

if (MSVC_VERSION)
option(OPTION_SHOW_COMPILE_METRICS "Show MSVC compilation metrics" OFF)
endif()

if (MSVC_VERSION GREATER 1900 AND "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}" VERSION_GREATER_EQUAL "10.0.16299.0")
	option(OPTION_MSVC_PERMISSIVE_MINUS "Enable Visual Studio C++ /permissive- compile option for the standards-conforming compiler behavior" ON)
endif()


option(OPTION_RELEASE_PROFILING "Enable basic profiling in Release builds" OFF)
option(OPTION_RELEASE_LOGGING "Enable logging in Release builds" ON)

if (WINDOWS)
	option (OPTION_MEMREPLAY_USES_DETOURS "Use the Detours library to try capturing more allocations by hooking into malloc, etc." ON)
else()
	set (OPTION_MEMREPLAY_USES_DETOURS FALSE)
endif ()