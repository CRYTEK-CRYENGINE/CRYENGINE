if (NOT WIN32)
	return()
endif()

start_sources()
sources_platform(ALL)
add_sources("NoUberFile"
	PROJECTS PhysDebugger
	SOURCE_GROUP "Stubs"
		"CryCore/Platform/CryEndian.h"
		"CryCore/Platform/platform.h"
		"CryCore/Platform/platform_impl.inl"
		"CryMath/Cry_Geo.h"
		"CryMath/GeomQuery.h"
		"CryMemory/CrySizer.h"
		"CryNetwork/ISerialize.h"
		"CrySystem/IEngineModule.h"
		"CrySystem/ILog.h"
		"CrySystem/ISystem.h"
		"CryThreading/CryThread.h"
		"CryThreading/IThreadManager.h"
		"DummyEditor.h"
	SOURCE_GROUP "Resource Files"
		"PhysDebugger.rc"
		"Resource.h"
	SOURCE_GROUP "Editor"
		"../../Sandbox/EditorQt/PhysTool.cpp"
		"../../Sandbox/EditorQt/PhysTool.h"
	SOURCE_GROUP "Root"
		"PhysDebugger.cpp"
		"physics.cpp"
)
set_source_files_properties("../../Sandbox/EditorQt/PhysTool.cpp" PROPERTIES COMPILE_FLAGS "/FIDummyEditor.h")
end_sources()

CryWindowsApplication(PhysDebugger SOLUTION_FOLDER "PhysDebugger")
target_include_directories(${THIS_PROJECT} BEFORE PRIVATE .)
target_compile_options(${THIS_PROJECT} PRIVATE $<$<CONFIG:Profile>:/GL /O2 /Ot> $<$<CONFIG:Release>:/GL /O2 /Ot> $<$<CONFIG:Debug>:/RTCu> )
target_link_libraries(${THIS_PROJECT} PRIVATE CryPhysicsLib opengl32 glu32 Shlwapi)
set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS_PROFILE " /LTCG:incremental")
set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS_RELEASE " /LTCG:incremental")