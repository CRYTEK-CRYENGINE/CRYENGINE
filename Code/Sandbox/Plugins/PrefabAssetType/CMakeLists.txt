## PERSONAL CRYTEK: Skipping compilation because it quite obviuosly doesn't work. Missing files and so on. Ugh.
return()

start_sources()

# -- prefabassettype.waf_files --
sources_platform(ALL)
add_sources("NoUberFile"
	SOURCE_GROUP "Root"
		"PrefabAssetType.cpp"
		"PrefabAssetType.h"
		"PrefabPlugin.cpp"
		"PrefabPlugin.h"
		"StdAfx.h"
		"StdAfx.cpp"
)
end_sources()


CryEditorPlugin(PrefabAssetType PCH "StdAfx.cpp" SOLUTION_FOLDER "Sandbox/Plugins")

# PrefabManager stuff
target_include_directories(${THIS_PROJECT} PRIVATE ../../EditorQt)	
target_include_directories(${THIS_PROJECT} PRIVATE ../../EditorQt/Include)
target_include_directories(${THIS_PROJECT} PRIVATE ../MFCToolsPlugin)
target_link_libraries(${THIS_PROJECT} PRIVATE Sandbox)
target_link_libraries(${THIS_PROJECT} PRIVATE MFCToolsPlugin)
