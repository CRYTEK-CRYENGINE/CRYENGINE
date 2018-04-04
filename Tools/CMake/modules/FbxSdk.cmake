if(WIN64)
	add_library(FbxSdk SHARED IMPORTED GLOBAL)
	set_target_properties(FbxSdk PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${SDK_DIR}/FbxSdk/include")
	set_target_properties(FbxSdk PROPERTIES INTERFACE_COMPILE_DEFINITIONS FBXSDK_NEW_API=1)

	if (MSVC_VERSION GREATER 1900) # Visual Studio > 2015
		set(FBX_SUBFOLDER vs2015)
	elseif (MSVC_VERSION EQUAL 1900) # Visual Studio 2015
		set(FBX_SUBFOLDER vs2015)
	elseif (MSVC_VERSION EQUAL 1800) # Visual Studio 2013
		set(FBX_SUBFOLDER vs2013)
	elseif (MSVC_VERSION EQUAL 1700) # Visual Studio 2012
		set(FBX_SUBFOLDER vs2012)
	endif()

	set_property(TARGET FbxSdk APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
	set_target_properties(FbxSdk PROPERTIES
		IMPORTED_LOCATION_DEBUG "${SDK_DIR}/FbxSdk/lib/${FBX_SUBFOLDER}/x64/debug/libfbxsdk.dll"
		IMPORTED_IMPLIB_DEBUG "${SDK_DIR}/FbxSdk/lib/${FBX_SUBFOLDER}/x64/debug/libfbxsdk-md.lib")

	set_property(TARGET FbxSdk APPEND PROPERTY IMPORTED_CONFIGURATIONS PROFILE)
	set_target_properties(FbxSdk PROPERTIES
		IMPORTED_LOCATION_PROFILE "${SDK_DIR}/FbxSdk/lib/${FBX_SUBFOLDER}/x64/release/libfbxsdk.dll"
		IMPORTED_IMPLIB_PROFILE "${SDK_DIR}/FbxSdk/lib/${FBX_SUBFOLDER}/x64/release/libfbxsdk-md.lib")

	set_property(TARGET FbxSdk APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
	set_target_properties(FbxSdk PROPERTIES
		IMPORTED_LOCATION_RELEASE "${SDK_DIR}/FbxSdk/lib/${FBX_SUBFOLDER}/x64/release/libfbxsdk.dll"
		IMPORTED_IMPLIB_RELEASE "${SDK_DIR}/FbxSdk/lib/${FBX_SUBFOLDER}/x64/release/libfbxsdk-md.lib")
endif()
