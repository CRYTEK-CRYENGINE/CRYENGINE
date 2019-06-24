if (NOT ORBIS)
	return()
endif()

start_sources()

# -- CryGamePlatform_PSN.waf_files --
sources_platform(ALL)
add_sources("CryGamePlatform_PSN_uber_0.cpp"
	SOURCE_GROUP "Root"
		"resource.h"
		"StdAfx.cpp"
		"StdAfx.h"
)

add_sources("CryGamePlatform_PSN_uber_1.cpp"
	SOURCE_GROUP "Interface"
		"../../Interface/IGamePlatform.h"
		"../../Interface/IPlatformAccount.h"
		"../../Interface/IPlatformAchievement.h"
		"../../Interface/IPlatformBase.h"
		"../../Interface/IPlatformLeaderboards.h"
		"../../Interface/IPlatformLobby.h"
		"../../Interface/IPlatformMatchmaking.h"
		"../../Interface/IPlatformNetworking.h"
		"../../Interface/IPlatformRemoteStorage.h"
		"../../Interface/IPlatformServer.h"
		"../../Interface/IPlatformService.h"
		"../../Interface/IPlatformStatistics.h"
		"../../Interface/IPlatformUser.h"
		"../../Interface/IPlatformUserGeneratedContent.h"
		"../../Interface/PlatformTypes.h"
		"../../Interface/PlatformIdentifier.h"
		"../../Interface/PlatformUserIdentifier.h"
)

add_sources("CryGamePlatform_PSN_uber_2.cpp"
	SOURCE_GROUP "Root"
		"PSNService.cpp"
		"PSNService.h"
		"PSNTypes.h"
	SOURCE_GROUP "Profile"
		"PSNStatistics.cpp"
		"PSNStatistics.h"
		"PSNUserIdentifier.h"
		"PSNAccount.h"
		"PSNAccount.cpp"
		"PSNAchievement.cpp"
		"PSNAchievement.h"
		"PSNLeaderboards.cpp"
		"PSNLeaderboards.h"
)

add_sources("NoUberFile"
	SOURCE_GROUP "Root"
		"StdAfx.cpp"
)

end_sources()

CryEngineModule(CryGamePlatformPSN PCH "StdAfx.cpp" SOLUTION_FOLDER "CryPlugins")

target_include_directories(${THIS_PROJECT}
PRIVATE 
	"${CRYENGINE_DIR}/Code/CryEngine/CryCommon"
PUBLIC
	../../Interface
	../../Common
)

target_link_libraries(${THIS_PROJECT} PRIVATE sceNpAuth_stub_weak sceSigninDialog_stub_weak)

target_compile_options(${THIS_PROJECT} PRIVATE
	-Wno-reorder
	-Wno-parentheses
	-Wno-unknown-pragmas
	-Wno-format
	-Wno-switch
	-Wno-multichar
	-Wno-unused-private-field
	-Wno-empty-body
	-Wno-unused-value
	-Wno-unused-function
)
