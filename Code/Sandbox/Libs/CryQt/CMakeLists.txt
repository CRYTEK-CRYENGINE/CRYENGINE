start_sources()
sources_platform(ALL)
add_sources("NoUberFile"
	SOURCE_GROUP "QToolWindowManager"
		"QToolWindowManager/QToolWindowManagerCommon.cpp"
		"QToolWindowManager/QToolWindowManagerCommon.h"
		"QToolWindowManager/QToolWindowManager.cpp"
		"QToolWindowManager/QToolWindowManager.h"
		"QToolWindowManager/IToolWindowArea.h"
		"QToolWindowManager/QToolWindowArea.cpp"
		"QToolWindowManager/IToolWindowWrapper.h"
		"QToolWindowManager/QToolWindowArea.h"
		"QToolWindowManager/QToolWindowWrapper.cpp"
		"QToolWindowManager/QToolWindowWrapper.h"
		"QToolWindowManager/QToolWindowCustomWrapper.cpp"
		"QToolWindowManager/QToolWindowCustomWrapper.h"
		"QToolWindowManager/QCustomWindowFrame.cpp"
		"QToolWindowManager/QCustomWindowFrame.h"
		"QToolWindowManager/IToolWindowDragHandler.h"
		"QToolWindowManager/QToolWindowDragHandlerDropTargets.cpp"
		"QToolWindowManager/QToolWindowDragHandlerDropTargets.h"
		"QToolWindowManager/QToolWindowDragHandlerNinePatch.cpp"
		"QToolWindowManager/QToolWindowDragHandlerNinePatch.h"
		"QToolWindowManager/QToolWindowTabBar.cpp"
		"QToolWindowManager/QToolWindowTabBar.h"
		"QToolWindowManager/QToolWindowRollupBarArea.cpp"
		"QToolWindowManager/QToolWindowRollupBarArea.h"
	SOURCE_GROUP "Resource Files"
		"QToolWindowManager/QToolWindowManager.qrc"
		"QToolWindowManager/gfx/base_window.png"
		"QToolWindowManager/gfx/dock_bottom.png"
		"QToolWindowManager/gfx/dock_left.png"
		"QToolWindowManager/gfx/dock_right.png"
		"QToolWindowManager/gfx/dock_top.png"
		"QToolWindowManager/gfx/drag_handle.png"
		"QToolWindowManager/gfx/hsplit_top.png"
		"QToolWindowManager/gfx/hsplit_bottom.png"
		"QToolWindowManager/gfx/vsplit_left.png"
		"QToolWindowManager/gfx/vsplit_right.png"
		"QToolWindowManager/gfx/minimize.png"
		"QToolWindowManager/gfx/maximize.png"
		"QToolWindowManager/gfx/restore.png"
		"QToolWindowManager/gfx/close.png"
	SOURCE_GROUP "Root"
		"stdafx.cpp"
		"stdafx.h"
		"CryQtAPI.h"
		"CryQtCompatibility.h"
		"CryIcon.cpp"
		"CryIcon.h"
		"QCollapsibleFrame.cpp"
		"QCollapsibleFrame.h"
		"QRollupBar.cpp"
		"QRollupBar.h"
		"QScrollableBox.cpp"
		"QScrollableBox.h"
)
end_sources()


CreateDynamicModule(CryQt PCH "stdafx.cpp" SOLUTION_FOLDER "Sandbox/Libs")

set_property(TARGET ${THIS_PROJECT} PROPERTY EXCLUDE_FROM_DEFAULT_BUILD_RELEASE TRUE)

target_compile_definitions(${THIS_PROJECT} PRIVATE -DCryQT_EXPORTS -DWIN32)
target_link_libraries(${THIS_PROJECT} PRIVATE dwmapi gdi32 Ole32 qtmain$<$<CONFIG:Debug>:d>)
set_target_properties(${THIS_PROJECT} PROPERTIES DEBUG_POSTFIX "d")
set_property(TARGET ${THIS_PROJECT} PROPERTY EXCLUDE_FROM_DEFAULT_BUILD_RELEASE TRUE)
use_qt()

# Wrapping with shiboken requires Run-Time Type Information to be enabled.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR")

add_subdirectory("ShibokenWrapper")
