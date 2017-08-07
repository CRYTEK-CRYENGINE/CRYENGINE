cmake_minimum_required(VERSION 3.2)

project(MaxPluginTools)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Bin")
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Bin")

set(maxSdkVersions "2015;2016;2017")

SET(maxSdkPath_2015 "${CMAKE_CURRENT_SOURCE_DIR}/../../../SDKs/maxsdk/max17/")
set(maxPlugingPath_2015 "$ENV{ADSK_3DSMAX_x64_2015}plugins")
STRING(REGEX REPLACE "\\\\" "/" maxPlugingPath_2015 ${maxPlugingPath_2015})

SET(maxSdkPath_2016 "${CMAKE_CURRENT_SOURCE_DIR}/../../../SDKs/maxsdk/max18/")
set(maxPlugingPath_2016 "$ENV{ADSK_3DSMAX_x64_2016}plugins")
STRING(REGEX REPLACE "\\\\" "/" maxPlugingPath_2016 ${maxPlugingPath_2016})

SET(maxSdkPath_2017 "${CMAKE_CURRENT_SOURCE_DIR}/../../../SDKs/maxsdk/max19/")
set(maxPlugingPath_2017 "$ENV{ADSK_3DSMAX_x64_2017}plugins")
STRING(REGEX REPLACE "\\\\" "/" maxPlugingPath_2017 ${maxPlugingPath_2017})

# Disables the "ZERO_CHECK" target, and trying to regenerate on build. (We want to manually regenerate)
set(CMAKE_SUPPRESS_REGENERATION true)

MACRO(SORT_SOURCES sourceFiles)
	# Group source files in the same way that they are found in the folders
	FOREACH (file ${sourceFiles})
		file (RELATIVE_PATH relative_file "${CMAKE_CURRENT_SOURCE_DIR}" ${file}) # Make relative to CMakeLists
		get_filename_component(dir_path ${relative_file} DIRECTORY) # Get folder path
		string (REPLACE "/" "\\" fixed "${dir_path}") # On windows the dir_path will have forward slashes and subgroups use backslashes, so convert them.
		source_group("${fixed}" FILES ${file}) # Put in group
	ENDFOREACH()
ENDMACRO()

MACRO(LINK_MAX_SDK projectName sdkDirectory)
	# Remove trailing slashes from path name
	STRING(REGEX REPLACE "(\/)+$" "" cleanedDirectoryPath "${sdkDirectory}")
	
	# Clean backslashes from directory path
	target_include_directories(${projectName} PUBLIC "${cleanedDirectoryPath}/include/")
	
	set(sdkLibs "mesh;gfx;maxutil;core;paramblk2;geom;mnmath;edmodel;poly;menus;Maxscrpt;gup;assetmanagement;graphicsUtility;fragmentGraph;viewSystem;maxGraphicsObjects;bmm;manipsys;graphicsDriver;dataBridge")
	
	FOREACH(sdkLib ${sdkLibs})
		target_link_libraries(${projectName} "${cleanedDirectoryPath}/lib/x64/Release/${sdkLib}.lib")
	ENDFOREACH()
	
	# Projects linking against max sdk must be unicode
	add_definitions(-DUNICODE)
	add_definitions(-D_UNICODE)
ENDMACRO()

# Macro for declaring a plugin in the current directory
MACRO(MAX_PLUGIN pluginName utilityLibDependencies)
	FOREACH(maxSDK ${maxSdkVersions})
		set(targetName "${pluginName}_${maxSDK}")
	
		FILE (GLOB_RECURSE sourceFiles "*.h" "*.cpp" "*.rc")
		add_library(${targetName} SHARED ${sourceFiles})
		set_target_properties(${targetName} PROPERTIES SUFFIX ".dlu")
		SORT_SOURCES("${sourceFiles}")
		set_target_properties(${targetName} PROPERTIES FOLDER ${pluginName})
		
		FOREACH(dependency ${utilityLibDependencies})
			set(dependencyName "${dependency}_${maxSDK}")
			target_link_libraries(${targetName} ${dependencyName})
			get_target_property(publicIncludeDirectories ${dependencyName} INCLUDE_DIRECTORIES)
			target_include_directories(${targetName} PUBLIC ${publicIncludeDirectories})
		ENDFOREACH()

		LINK_MAX_SDK(${targetName} "${maxSdkPath_${maxSDK}}")
		
		# Copy from bin directory to max plugins folder on build completion
		add_custom_command(TARGET ${targetName} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${targetName}>" "${maxPlugingPath_${maxSDK}}/$<TARGET_FILE_NAME:${targetName}>")
		
	ENDFOREACH()
ENDMACRO()

MACRO(UTILITY_LIB libName)
	FOREACH(maxSDK ${maxSdkVersions})
		set(targetName "${libName}_${maxSDK}")
		
		FILE (GLOB_RECURSE sourceFiles "*.h" "*.cpp" "*.rc")
		add_library(${targetName} STATIC ${sourceFiles})
		target_include_directories(${targetName} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
		SORT_SOURCES("${sourceFiles}")
		set_target_properties(${targetName} PROPERTIES FOLDER ${libName})

		LINK_MAX_SDK (${targetName} "${maxSdkPath_${maxSDK}}")
	ENDFOREACH()
ENDMACRO()

# Add the utilities lib first, since the plugins depend on it.
add_subdirectory("MaxPluginUtilities")

# Include the plugin subdirectories.
add_subdirectory("Abnormals")
add_subdirectory("GeomDecal")
add_subdirectory("SuperArray")
add_subdirectory("FlowPaint")
