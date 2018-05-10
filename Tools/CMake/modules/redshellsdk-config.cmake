if (RED_SHELL_SDK_INCLUDE_DIRS)
	return()
endif()

set(RED_SHELL_SDK_ROOT_FOLDER  "${SDK_DIR}/RedShellSDK")
set(RED_SHELL_SDK_INCLUDE_DIRS ${RED_SHELL_SDK_ROOT_FOLDER}/include)

if (NOT TARGET redshellsdk)
	add_library(redshellsdk SHARED IMPORTED GLOBAL)
	set(_RED_SHELL_SDK_SUPPORTED_PLATFORM FALSE)

	if(WIN64)
		set(RED_SHELL_SDK_BIN_DIRS ${RED_SHELL_SDK_ROOT_FOLDER}/bin/x64)
		set(RED_SHELL_SDK_LIBRARY_DIRS ${RED_SHELL_SDK_ROOT_FOLDER}/lib/x64)
		set(_RED_SHELL_SDK_SUPPORTED_PLATFORM TRUE)
	elseif(WIN32)
		set(RED_SHELL_SDK_BIN_DIRS ${RED_SHELL_SDK_ROOT_FOLDER}/bin/x86)
		set(RED_SHELL_SDK_LIBRARY_DIRS ${RED_SHELL_SDK_ROOT_FOLDER}/lib/x86)
		set(_RED_SHELL_SDK_SUPPORTED_PLATFORM TRUE)
	else()
		message("Unsupported platform for RedShellSDK")
	endif()

	set_property(TARGET redshellsdk APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
	set_target_properties(redshellsdk PROPERTIES
		IMPORTED_LOCATION_DEBUG "${RED_SHELL_SDK_BIN_DIRS}/Debug/RedShell.dll"
		IMPORTED_IMPLIB_DEBUG "${RED_SHELL_SDK_LIBRARY_DIRS}/Debug/RedShell.lib")

	set_property(TARGET redshellsdk APPEND PROPERTY IMPORTED_CONFIGURATIONS PROFILE)
	set_target_properties(redshellsdk PROPERTIES
		IMPORTED_LOCATION_PROFILE "${RED_SHELL_SDK_BIN_DIRS}/RedShell.dll"
		IMPORTED_IMPLIB_PROFILE "${RED_SHELL_SDK_LIBRARY_DIRS}/RedShell.lib")

	set_property(TARGET redshellsdk APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
	set_target_properties(redshellsdk PROPERTIES
		IMPORTED_LOCATION_RELEASE "${RED_SHELL_SDK_BIN_DIRS}/RedShell.dll"
		IMPORTED_IMPLIB_RELEASE "${RED_SHELL_SDK_LIBRARY_DIRS}/RedShell.lib")

	set_target_properties(redshellsdk PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${RED_SHELL_SDK_INCLUDE_DIRS}")

	# uses generator expressions; if we are compiling in debug, then this generator expression will insert /Debug to the path
	deploy_runtime_file("${RED_SHELL_SDK_BIN_DIRS}$<$<CONFIG:Debug>:/Debug>/RedShell.dll")

	find_package(PackageHandleStandardArgs)
	FIND_PACKAGE_HANDLE_STANDARD_ARGS(redshellsdk DEFAULT_MSG
		_RED_SHELL_SDK_SUPPORTED_PLATFORM)
endif()