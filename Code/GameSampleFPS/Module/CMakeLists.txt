## Don't forget to make sure GameSampleFPS asset folders/cmake-files/configurations have the same 'GameSampleFPS' name 
## Or whatever your chosen project is called.

## Using default CPP template code
## Typically your files would all be in the same directory as this CMakeLists.txt file
set(CODE_DIR "${CRYENGINE_DIR}/Code/GameTemplates/cpp/FirstPersonShooter/Code")

#START-FILE-LIST
start_sources()
sources_platform(ALL)
add_sources("CryGameSampleFPS_uber_Player.cpp"
	SOURCE_GROUP "Components"
		"${CODE_DIR}/Components/Bullet.h"
		"${CODE_DIR}/Components/SpawnPoint.h"
		"${CODE_DIR}/Components/SpawnPoint.cpp"
		"${CODE_DIR}/Components/Player.h"
		"${CODE_DIR}/Components/Player.cpp"
)

add_sources("NoUberFile"
	SOURCE_GROUP "DLL"
		"${CODE_DIR}/StdAfx.h"
		"${CODE_DIR}/StdAfx.cpp"
		"${CODE_DIR}/GamePlugin.h"
		"${CODE_DIR}/GamePlugin.cpp"
)
end_sources()
#END-FILE-LIST

CryGameModule(CryGameSampleFPS PCH "${CODE_DIR}/StdAfx.cpp" SOLUTION_FOLDER "Projects/GameSampleFPS")

target_include_directories( ${THIS_PROJECT} PRIVATE 
	${CRYENGINE_DIR}/Code/CryEngine/CryCommon	## General library of useful stuff, base interfaces etc.
	${CRYENGINE_DIR}/Code/CryEngine/CryAction	## CryMannequin etc. includes
	${CRYENGINE_DIR}/Code/CryPlugins/CryDefaultEntities/Module ## DefaultEntity component includes
)

if (WIN32 OR WIN64)
	target_link_libraries( ${THIS_PROJECT} PRIVATE shell32 Gdi32  )
elseif(DURANGO)
	target_link_libraries( ${THIS_PROJECT} PRIVATE ixmlhttprequest2 Ws2_32 )
	configure_durango_game(
		"app_id" "GameSampleFPS.app"
		"package_name" "GameSampleFPS"
		"display_name" "CRYENGINE Game SampleFPS"
		"publisher_name" "Crytek"
		"description" "CRYENGINE Sample FPS Project"
		"foreground_text" "light"
		"background_color" "#6495ED"
		"version" "1.0.0.0"
		"titleid" "06BFF3BF"
		"scid" "84d70100-5e91-4cf0-bcc9-7b2f06bff3bf"
		"appxmanifest" "AppxManifest.xml"
		"logo" "DurangoLogo.png"
		"small_logo" "DurangoSmallLogo.png"
		"splash_screen" "DurangoSplashScreen.png"
		"store_logo" "DurangoStoreLogo.png"
	)
endif()