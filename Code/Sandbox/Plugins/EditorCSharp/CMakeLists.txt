start_sources()
sources_platform(ALL)
add_sources("Uber_CSharpEditor.cpp"
	SOURCE_GROUP "Main"
		"CSharpEditorPlugin.h"
		"CSharpEditorPlugin.cpp"
		"StdAfx.cpp"
		"StdAfx.h"
		"CSharpAssetType.h"
		"CSharpAssetType.cpp"
		"CSharpOutputTextEdit.h"
		"EditorCSharp.qrc"
		"CSharpOutputWindow.cpp"
		"CSharpOutputWindow.h"
)

end_sources()

CryEditorPlugin(EditorCSharp PCH "StdAfx.cpp" SOLUTION_FOLDER "Sandbox/Plugins")

target_include_directories(${THIS_PROJECT} PRIVATE ../../EditorQt)
target_include_directories(${THIS_PROJECT} PRIVATE ../../EditorQt/Include)

target_link_libraries(${THIS_PROJECT} PRIVATE Sandbox)


