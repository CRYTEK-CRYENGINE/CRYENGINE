start_sources()

# -- orbisunittestmain.waf_files --
sources_platform(ALL)
add_sources("NoUberFile"
	SOURCE_GROUP "Source Files"
		"main.cpp"
	SOURCE_GROUP "Header Files"
		"../Common/UnitTest.h"
)

# Pull in all sources from test modules to build a monolithic testing target
foreach(module IN LISTS TEST_MODULES)
	get_property(sources TARGET ${module} PROPERTY SOURCES)
	get_property(sourcedir TARGET ${module} PROPERTY SOURCE_DIR)
	get_property(includedir TARGET ${module} PROPERTY INCLUDE_DIRECTORIES)
	list(REMOVE_ITEM sources "stdafx.h" "stdafx.cpp" "StdAfx.h" "StdAfx.cpp")
	add_prefix(sources "${sourcedir}/")
	message( STATUS "${module}" " sources: " "${sources}" )
	add_sources("NoUberFile" SOURCE_GROUP "${module} (External)" ${sources})
	include_directories(${includedir})
endforeach(module)
end_sources()

CryLauncher(CryUnitTestOrbisMain SOLUTION_FOLDER "UnitTests")
set_property(TARGET ${THIS_PROJECT} PROPERTY OUTPUT_NAME "UnitTests.elf")
include_directories("${CRYENGINE_DIR}/Code/Libs/gtest/googletest/include")
#include_directories("../Common")
target_link_libraries(${THIS_PROJECT} PUBLIC gtest gmock)

target_include_directories(${THIS_PROJECT} PRIVATE "${SDK_DIR}")
target_link_libraries(${THIS_PROJECT} PRIVATE
	ScePm4Dump
	SceAjm_stub_weak
	ScePosix_stub_weak
	SceSysmodule_stub_weak
	SceSystemService_stub_weak
	SceDbgKeyboard_stub_weak
	SceUserService_stub_weak
	SceSha1
	SceSaveData_stub_weak
	ScePerf_stub_weak
	SceFios2_stub_weak
	ScePad_stub_weak
	SceNet_stub_weak
	SceVideoOut_stub_weak
	SceGpuAddress
	SceShaderBinary
	SceRtc_stub_weak
	SceNetCtl_stub_weak
	SceHttp_stub_weak
	SceSsl_stub_weak
	SceNpCommon_stub_weak
	SceNpManager_stub_weak
	SceNpMatching2_stub_weak
	SceNpScore_stub_weak
	SceNpTus_stub_weak
	SceNpTrophy_stub_weak
	SceNpWebApi_stub_weak
	SceCommonDialog_stub_weak
	SceInvitationDialog_stub_weak
	SceNpProfileDialog_stub_weak
	SceNpFriendListDialog_stub_weak
	SceAudioOut_stub_weak
	SceAudioIn_stub_weak)
target_link_libraries(${THIS_PROJECT} PRIVATE debug SceDbgKeyboard_stub_weak)
target_link_libraries(${THIS_PROJECT} PRIVATE debug ScePerf_stub_weak)
target_link_libraries(${THIS_PROJECT} PRIVATE debug SceRazorCpu_stub_weak)

set(data_root /data/sdk/)
file( WRITE "${CMAKE_CURRENT_BINARY_DIR}/${THIS_PROJECT}.vcxproj.user" 
  "<?xml version=\"1.0\" encoding=\"utf-8\"?>
  <Project ToolsVersion=\"4.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">
		<PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Debug|ORBIS'\">
			<LocalDebuggerWorkingDirectory>$(OutDir)</LocalDebuggerWorkingDirectory>
			<LocalDebuggerCommandArguments>-root=${data_root}</LocalDebuggerCommandArguments>
		</PropertyGroup>
		<PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Profile|ORBIS'\">
			<LocalDebuggerWorkingDirectory>$(OutDir)</LocalDebuggerWorkingDirectory>
			<LocalDebuggerCommandArguments>-root=${data_root}</LocalDebuggerCommandArguments>
		</PropertyGroup>		
		<PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|ORBIS'\">
			<LocalDebuggerWorkingDirectory>$(OutDir)</LocalDebuggerWorkingDirectory>
			<LocalDebuggerCommandArguments>-root=${data_root}</LocalDebuggerCommandArguments>
		</PropertyGroup>
	</Project>")

if (ORBIS OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_options(${THIS_PROJECT} PRIVATE
		-Wno-unknown-pragmas
		-Wno-unused-variable
		-Wno-unused-value
		-Wno-unused-function
		-Wno-empty-body
		-Wno-reorder
		-Wno-switch
		-Wno-format
		-Wno-unused-private-field
		-Wno-parentheses
		-Wno-multichar
		-Wno-deprecated-declarations
		-Wno-comment
	)
endif()
