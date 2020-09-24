project(crashrpt)

# Windows-only project, should not be attempted to be build on any other platform

set(crashrpt_dir "${SDK_DIR}/CrashRpt/1403")

# Create the list of source files
start_sources()

# -- crashrpt.waf_files --
sources_platform(WINDOWS)
add_sources("NoUberFile"
	SOURCE_GROUP "Root"
		"${SDK_DIR}/CrashRpt/1403/reporting/crashrpt/CrashHandler.cpp"
		"${SDK_DIR}/CrashRpt/1403/reporting/crashrpt/CrashHandler.h"
		"${SDK_DIR}/CrashRpt/1403/reporting/crashrpt/CrashRpt.cpp"
		"${SDK_DIR}/CrashRpt/1403/reporting/crashrpt/CritSec.h"
		"${SDK_DIR}/CrashRpt/1403/reporting/crashrpt/Prefastdef.h"
		"${SDK_DIR}/CrashRpt/1403/reporting/crashrpt/resource.h"
		"${SDK_DIR}/CrashRpt/1403/reporting/crashrpt/SharedMem.cpp"
		"${SDK_DIR}/CrashRpt/1403/reporting/crashrpt/SharedMem.h"
		"${SDK_DIR}/CrashRpt/1403/reporting/crashrpt/StdAfx.cpp"
		"${SDK_DIR}/CrashRpt/1403/reporting/crashrpt/StdAfx.h"
		"${SDK_DIR}/CrashRpt/1403/reporting/crashrpt/strconv.h"
		"${SDK_DIR}/CrashRpt/1403/reporting/crashrpt/Utility.cpp"
		"${SDK_DIR}/CrashRpt/1403/reporting/crashrpt/Utility.h"
	)
end_sources()

# Define _UNICODE (use wide-char encoding)
add_definitions(-D_UNICODE)
add_definitions(-D_CRT_SECURE_NO_DEPRECATE)

add_definitions(-DCRASHRPT_LIB)

CryEngineStaticModule(crashrpt SOLUTION_FOLDER "Libs" MSVC_PERMISSIVE)
target_include_directories(${THIS_PROJECT} PUBLIC ${crashrpt_dir}/include)
target_include_directories(${THIS_PROJECT} PRIVATE ${crashrpt_dir}/thirdparty/wtl)

#Disable C4530 (warning C4530: C++ exception handler used, but unwind semantics are not enabled. Specify /EHsc)
target_compile_options(${PROJECT_NAME} PRIVATE "/wd4530")

#Disable C4996 (warning C4996: 'GetVersionExA': was declared deprecated)
target_compile_options(${PROJECT_NAME} PRIVATE "/wd4996")

#Disable mismatched exception specifications diagnosis
target_compile_options(${PROJECT_NAME} PRIVATE "/Zc:noexceptTypes-")

#Disable link warning LNK4221 (StdAfx.obj : warning LNK4221: This object file does not define any previously undefined public symbols, so it will not be used by any link operation that consumes this library)
set_target_properties(${PROJECT_NAME} PROPERTIES STATIC_LIBRARY_FLAGS "/ignore:4221")

target_link_libraries(${THIS_PROJECT} PRIVATE rpcrt4.dll version.lib)
