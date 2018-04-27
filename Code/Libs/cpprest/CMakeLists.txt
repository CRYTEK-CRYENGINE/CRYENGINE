set(cpprest_dir ${SDK_DIR}/cpprest)
set(cpprest_src_dir ${cpprest_dir}/src)

# Create the list of source files
set(cpprest_SOURCES
			"${cpprest_src_dir}/pch/stdafx.cpp"
			"${cpprest_src_dir}/http/client/http_client_msg.cpp"
			"${cpprest_src_dir}/http/client/x509_cert_utilities.cpp"
			"${cpprest_src_dir}/http/common/http_helpers.cpp"
			"${cpprest_src_dir}/http/common/http_msg.cpp"
			"${cpprest_src_dir}/http/listener/http_listener.cpp"
			"${cpprest_src_dir}/http/listener/http_listener_msg.cpp"
			"${cpprest_src_dir}/http/listener/http_server_api.cpp"
			"${cpprest_src_dir}/http/oauth/oauth1.cpp"
			"${cpprest_src_dir}/http/oauth/oauth2.cpp"
			"${cpprest_src_dir}/json/json.cpp"
			"${cpprest_src_dir}/json/json_parsing.cpp"
			"${cpprest_src_dir}/json/json_serialization.cpp"
			"${cpprest_src_dir}/pplx/pplx.cpp"
			"${cpprest_src_dir}/uri/uri.cpp"
			"${cpprest_src_dir}/uri/uri_builder.cpp"
			"${cpprest_src_dir}/uri/uri_parser.cpp"
			"${cpprest_src_dir}/utilities/asyncrt_utils.cpp"
			"${cpprest_src_dir}/utilities/base64.cpp"
			"${cpprest_src_dir}/utilities/web_utilities.cpp"
			"${cpprest_src_dir}/websockets/client/ws_msg.cpp"
			"${cpprest_src_dir}/websockets/client/ws_client.cpp"
			"${cpprest_src_dir}/websockets/client/ws_client_wspp.cpp"
)
set(cpprest_SOURCES_WIN32
			"${cpprest_src_dir}/http/client/http_client_winhttp.cpp"
			"${cpprest_src_dir}/http/client/x509_cert_utilities_win32.cpp"
			"${cpprest_src_dir}/http/listener/http_server_httpsys.cpp"
			"${cpprest_src_dir}/pplx/pplxwin.cpp"
			"${cpprest_src_dir}/streams/fileio_win32.cpp"
)

file( GLOB_RECURSE header_files ${cpprest_dir}/include/*.h )

list(APPEND cpprest_SOURCES ${header_files} ${cpprest_SOURCES_WIN32} )

CryEngineStaticModule(cpprest SOLUTION_FOLDER "Libs")

target_include_directories(${THIS_PROJECT} PUBLIC
	${cpprest_dir}/include
	${cpprest_src_dir}/pch
)

target_compile_definitions(${THIS_PROJECT} PUBLIC
	-DCPPREST_EXCLUDE_WEBSOCKETS
	-D_NO_ASYNCRTIMP
)

target_compile_options(${THIS_PROJECT} PRIVATE /EHsc)

#Disable C4530 (warning C4530: C++ exception handler used, but unwind semantics are not enabled. Specify /EHsc)
#target_compile_options(${PROJECT_NAME} PRIVATE "/wd4530")

if (WIN32)
	target_link_libraries(${THIS_PROJECT} PRIVATE bcrypt crypt32 httpapi Winhttp)
	
	#Disable link warning LNK4221 (StdAfx.obj : warning LNK4221: This object file does not define any previously undefined public symbols, so it will not be used by any link operation that consumes this library)
	set_target_properties(${THIS_PROJECT} PROPERTIES STATIC_LIBRARY_FLAGS "/ignore:4221")
else()
	target_link_libraries(${THIS_PROJECT} PRIVATE boost_system crypto ssl boost_thread)
endif()
