project(crashrpt)

# WIN32/WIN64 Only project, should not be attempted to be build on any other platform

set(crashrpt_dir ../../SDKs/CrashRpt/1403)
set(crashrpt_src_dir ${crashrpt_dir}/reporting/crashrpt)

# Create the list of source files
start_sources()
aux_source_directory( ${crashrpt_src_dir} crashrpt_SOURCES )
file( GLOB header_files ${crashrpt_src_dir}/*.h )

list(APPEND crashrpt_SOURCES ${header_files} )
end_sources()

# Define _UNICODE (use wide-char encoding)
add_definitions(-D_UNICODE)
add_definitions(-D_CRT_SECURE_NO_DEPRECATE)

add_definitions(-DCRASHRPT_LIB)

CryEngineStaticModule(crashrpt SOLUTION_FOLDER "Libs")
target_include_directories(${THIS_PROJECT} PUBLIC ${crashrpt_dir}/include)
target_include_directories(${THIS_PROJECT} PRIVATE ${crashrpt_dir}/thirdparty/wtl)

#Disable C4530 (warning C4530: C++ exception handler used, but unwind semantics are not enabled. Specify /EHsc)
target_compile_options(${PROJECT_NAME} PRIVATE "/wd4530")

#Disable C4996 (warning C4996: 'GetVersionExA': was declared deprecated)
target_compile_options(${PROJECT_NAME} PRIVATE "/wd4996")

#Disable link warning LNK4221 (StdAfx.obj : warning LNK4221: This object file does not define any previously undefined public symbols, so it will not be used by any link operation that consumes this library)
set_target_properties(${PROJECT_NAME} PROPERTIES STATIC_LIBRARY_FLAGS "/ignore:4221")

target_link_libraries(${THIS_PROJECT} PRIVATE rpcrt4.dll version.lib)
