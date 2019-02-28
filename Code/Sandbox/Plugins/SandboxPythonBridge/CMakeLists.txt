start_sources()

sources_platform(ALL)
add_sources("NoUberFile"
	SOURCE_GROUP "Root"
			"SandboxPythonBridge.cpp"
			"SandboxPythonBridgeCommands.cpp"
			"SandboxPythonBridgePlugin.cpp"
			"SandboxPythonBridgePlugin.h"
			"StdAfx.cpp"
			"StdAfx.h"
)

end_sources()

CryEditorPlugin(SandboxPythonBridge PCH "StdAfx.cpp" SOLUTION_FOLDER "Sandbox/Plugins")

set(PYSIDE_SDK_SRC "${SDK_DIR}/Qt/5.6/msvc2015_64/PySide/PySide2/")
set(PYSIDE_INCLUDE "${PYSIDE_SDK_SRC}include/")
target_include_directories(${THIS_PROJECT} PRIVATE "${PYSIDE_INCLUDE}shiboken2" "${PYSIDE_INCLUDE}PySide2" "${PYSIDE_INCLUDE}PySide2/QtWidgets" "${PYSIDE_INCLUDE}PySide2/QtGui"  "${PYSIDE_INCLUDE}PySide2/QtCore"  "${QT_DIR}/include/QtANGLE")

set_libpath_flag()
set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS_DEBUG " ${LIBPATH_FLAG}\"${PYSIDE_SDK_SRC}\"")
set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS_PROFILE " ${LIBPATH_FLAG}\"${PYSIDE_SDK_SRC}\"")
set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS_RELEASE " ${LIBPATH_FLAG}\"${PYSIDE_SDK_SRC}\"")

target_link_libraries(${THIS_PROJECT} PRIVATE pyside2-python2.7$<$<CONFIG:Debug>:-dbg> shiboken2-python2.7$<$<CONFIG:Debug>:-dbg>)
