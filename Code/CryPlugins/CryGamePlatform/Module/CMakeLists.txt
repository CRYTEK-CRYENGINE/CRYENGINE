if ((((WIN32 OR LINUX OR APPLE) AND EXISTS "${SDK_DIR}/Steamworks") OR ORBIS) AND OPTION_ENGINE)
	option(PLUGIN_GAMEPLATFORM "Enable Game platform integration plug-in for Steam and PSN" ON)
else()
	option(PLUGIN_GAMEPLATFORM "Enable Game platform integration plug-in for Steam and PSN" OFF)
endif()
	
if (NOT PLUGIN_GAMEPLATFORM)
	return()
endif()

start_sources()
sources_platform(ALL)
add_sources("CryGamePlatform_uber_0.cpp"
	SOURCE_GROUP "Root"
		"resource.h"
		"StdAfx.cpp"
		"StdAfx.h"
)

add_sources("CryGamePlatform_uber_1.cpp"
	SOURCE_GROUP "Interface"
		"../Interface/IGamePlatform.h"
		"../Interface/IPlatformAchievement.h"
		"../Interface/IPlatformLeaderboards.h"
		"../Interface/IPlatformLobby.h"
		"../Interface/IPlatformMatchmaking.h"
		"../Interface/IPlatformNetworking.h"
		"../Interface/IPlatformRemoteStorage.h"
		"../Interface/IPlatformServer.h"
		"../Interface/IPlatformStatistics.h"
		"../Interface/IPlatformUser.h"
		"../Interface/IPlatformUserGeneratedContent.h"
)

sources_platform(WIN32 LINUX APPLE)
add_sources("CryGamePlatform_Steam_uber_0.cpp"
	SOURCE_GROUP "Root"
		"Steam/SteamPlatform.cpp"
		"Steam/SteamPlatform.h"
	SOURCE_GROUP "Profile"
		"Steam/SteamStatistics.cpp"
		"Steam/SteamStatistics.h"
		"Steam/SteamUser.cpp"
		"Steam/SteamUser.h"
		"Steam/SteamAchievement.cpp"
		"Steam/SteamAchievement.h"
		"Steam/SteamLeaderboards.cpp"
		"Steam/SteamLeaderboards.h"
	SOURCE_GROUP "Multiplayer"
		"Steam/SteamMatchmaking.cpp"
		"Steam/SteamMatchmaking.h"
		"Steam/SteamNetworking.cpp"
		"Steam/SteamNetworking.h"
		"Steam/SteamUserLobby.cpp"
		"Steam/SteamUserLobby.h"
		"Steam/SteamServer.cpp"
		"Steam/SteamServer.h"
	SOURCE_GROUP "RemoteStorage"
		"Steam/SteamRemoteFile.cpp"
		"Steam/SteamRemoteFile.h"
		"Steam/SteamRemoteStorage.cpp"
		"Steam/SteamRemoteStorage.h"
		"Steam/SteamSharedRemoteFile.cpp"
		"Steam/SteamSharedRemoteFile.h"
	SOURCE_GROUP "UserGeneratedContent"
		"Steam/UserGeneratedContent.cpp"
		"Steam/UserGeneratedContent.h"
		"Steam/UserGeneratedContentManager.cpp"
		"Steam/UserGeneratedContentManager.h"
)

sources_platform(ORBIS)
add_sources("CryGamePlatform_PSN_uber_0.cpp"
	SOURCE_GROUP "Root"
		"PSN/PSNPlatform.cpp"
		"PSN/PSNPlatform.h"
	SOURCE_GROUP "Profile"
		"PSN/PSNStatistics.cpp"
		"PSN/PSNStatistics.h"
		"PSN/PSNUser.cpp"
		"PSN/PSNUser.h"
		"PSN/PSNAchievement.cpp"
		"PSN/PSNAchievement.h"
		"PSN/PSNLeaderboards.cpp"
		"PSN/PSNLeaderboards.h"
)

add_sources("NoUberFile"
	SOURCE_GROUP "Root"
		"StdAfx.cpp"
)

end_sources()

CryEngineModule(CryGamePlatform PCH "StdAfx.cpp" SOLUTION_FOLDER "CryPlugins")

target_include_directories(${THIS_PROJECT}
PRIVATE 
	"${CRYENGINE_DIR}/Code/CryEngine/CryCommon"
PUBLIC
	../Interface
)

if(WIN32 OR LINUX OR APPLE)
	include(${TOOLS_CMAKE_DIR}/modules/Steamworks.cmake)

	target_link_libraries(${THIS_PROJECT} PRIVATE Steamworks)
elseif(ORBIS)
	target_link_libraries(${THIS_PROJECT} PRIVATE sceNpAuth_stub_weak sceSigninDialog_stub_weak)
endif()