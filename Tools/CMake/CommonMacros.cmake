function(add_prefix list_name prefix)
	set( ${list_name}_TMP )
	foreach(s ${${list_name}})
		list(APPEND ${list_name}_TMP "${prefix}${s}")
	endforeach()
	set( ${list_name} ${${list_name}_TMP} PARENT_SCOPE)
endfunction()

function(add_suffix list_name suffix)
	set( ${list_name}_TMP )
	foreach(s ${${list_name}})
		list(APPEND ${list_name}_TMP "${s}${suffix}")
	endforeach()
	set( ${list_name} ${${list_name}_TMP} PARENT_SCOPE)
endfunction()

function(make_library list_name path)
	set( ${list_name}_TMP )
	foreach(s ${${list_name}})
		if (WIN32 OR WIN64 OR DURANGO)
			list(APPEND ${list_name}_TMP "${path}${s}.lib")
		else()
			list(APPEND ${list_name}_TMP "${path}lib${s}.a")
		endif()
	endforeach()
	set( ${list_name} ${${list_name}_TMP} PARENT_SCOPE)
endfunction()

macro(set_libpath_flag)
	if (WIN64 OR WIN32 OR DURANGO)
		set(LIBPATH_FLAG " /LIBPATH:")
	else()
		set(LIBPATH_FLAG " -L")
	endif()
endmacro()

function(USE_MSVC_PRECOMPILED_HEADER TargetProject PrecompiledHeader PrecompiledSource)
  if(NOT ${CMAKE_GENERATOR} MATCHES "Visual Studio")
    # Now only support precompiled headers for the Visual Studio projects
    return()
  endif()

	if (OPTION_UNITY_BUILD AND UBERFILES AND UNITY_${TargetProject})
	return()
  endif()
  
  if (OPTION_PCH AND MSVC)
		#message("Enable PCH for ${TargetProject}")
		if (WIN32 OR DURANGO)
			if(${CMAKE_GENERATOR} MATCHES "Visual Studio")
				# Inside Visual Studio
				set(PCH_FILE "$(IntDir)$(TargetName).pch")
			else()
				get_filename_component(PCH_NAME "${PrecompiledSource}" NAME_WE)
				set(PCH_FILE "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${TargetProject}.dir/${PCH_NAME}.pch")
			endif()

			#set_target_properties(${TargetProject} PROPERTIES COMPILE_FLAGS "/Yu\"${PrecompiledHeader}\" /Fp\"${PCH_FILE}\"")
			set_source_files_properties("${PrecompiledSource}" PROPERTIES COMPILE_FLAGS " /Yc\"${PrecompiledHeader}\" /Fp\"${PCH_FILE}\" ")
			# Disable Precompiled Header on all C files

			get_target_property(TARGET_SOURCES ${TargetProject} SOURCES)
			foreach(sourcefile ${TARGET_SOURCES})
				if ("${sourcefile}" MATCHES ".*\\.\\cpp$")
				  if (NOT "${sourcefile}" STREQUAL "${PrecompiledSource}")
					set_property(SOURCE "${sourcefile}" APPEND_STRING PROPERTY COMPILE_FLAGS " /Yu\"${PrecompiledHeader}\" /Fp\"${PCH_FILE}\" ")
				  endif()
				endif()
			endforeach(sourcefile)
		endif()
		
		if (ORBIS)
			#set_target_properties(${TargetProject} PROPERTIES COMPILE_FLAGS "/Yu\"${PrecompiledHeader}\"")
			#set_source_files_properties(${PrecompiledSource} PROPERTIES COMPILE_FLAGS "/Yc\"${PrecompiledHeader}\"")
		endif()
	endif()
endfunction()

MACRO(EXCLUDE_FILE_FROM_MSVC_PRECOMPILED_HEADER)
	if (MSVC)
		if (WIN32 OR DURANGO)
			set_property( SOURCE ${ARGN} APPEND PROPERTY COMPILE_FLAGS "/Y-")
		endif()
	endif()
ENDMACRO(EXCLUDE_FILE_FROM_MSVC_PRECOMPILED_HEADER)

# Organize projects into solution folders
macro(set_solution_folder folder target)
	if(TARGET ${target})
		if (NOT "${folder}" MATCHES "^Projects" AND NOT "${folder}" MATCHES "^Project")
			set_property(TARGET ${target} PROPERTY FOLDER "${VS_FOLDER_PREFIX}/${folder}")
		else()
			set_property(TARGET ${target} PROPERTY FOLDER "${folder}")
		endif()
	endif()
endmacro()

# Helper macro to set default StartUp Project in Visual Studio
macro(set_solution_startup_target target)
	if ("${CMAKE_GENERATOR}" MATCHES "^Visual Studio")
		# Set startup project to launch Game.exe with this project
		set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT ${target})
	endif()
endmacro()

MACRO(SET_PLATFORM_TARGET_PROPERTIES TargetProject)
	target_compile_definitions( ${THIS_PROJECT} PRIVATE "-DCODE_BASE_FOLDER=\"${CRYENGINE_DIR}/Code/\"")
	target_link_libraries( ${THIS_PROJECT} PRIVATE ${COMMON_LIBS} )

	if(OPTION_DEVELOPER_CONSOLE_IN_RELEASE)
		target_compile_definitions( ${THIS_PROJECT} PRIVATE "-DENABLE_DEVELOPER_CONSOLE_IN_RELEASE")
	endif()

	if(OPTION_UNSIGNED_PAKS_IN_RELEASE)
		target_compile_definitions( ${THIS_PROJECT} PRIVATE "-DSUPPORT_UNSIGNED_PAKS")
	endif()
		
	IF(DURANGO)
		set_target_properties_for_durango(${TargetProject})
	ENDIF(DURANGO)
	IF(ORBIS)
		set_target_properties_for_orbis(${TargetProject})
	ENDIF(ORBIS)
	if(WIN64)
		set_property(TARGET ${TargetProject} PROPERTY VS_GLOBAL_PreferredToolArchitecture x64)
	endif()
  
	if(VC_MDD_ANDROID)
		if (VC_MDD_ANDROID_PLATFORM_TOOLSET)
			set_property(TARGET ${TargetProject} PROPERTY VC_MDD_ANDROID_PLATFORM_TOOLSET "${VC_MDD_ANDROID_PLATFORM_TOOLSET}")
		endif()
		if (VC_MDD_ANDROID_USE_OF_STL)
			set_property(TARGET ${TargetProject} PROPERTY VC_MDD_ANDROID_USE_OF_STL "${VC_MDD_ANDROID_USE_OF_STL}")
		endif()
		set_property(TARGET ${TargetProject} PROPERTY VC_MDD_ANDROID_API_LEVEL "${VC_MDD_ANDROID_API_LEVEL}")
		set_property(TARGET ${TargetProject} PROPERTY VS_GLOBAL_UseMultiToolTask "True")  
	endif()
	
	if(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
		get_target_property(libout ${TargetProject} LIBRARY_OUTPUT_DIRECTORY)
		get_target_property(runout ${TargetProject} RUNTIME_OUTPUT_DIRECTORY)
		if (NOT libout)
			set(libout "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
		endif()
		if (NOT runout)
			set(runout "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
		endif()

		# Iterate Debug/Release configs and adds _DEBUG or _RELEASE
		foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
			string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )

			string(REPLACE ${BASE_OUTPUT_DIRECTORY} ${BASE_OUTPUT_DIRECTORY_${OUTPUTCONFIG}} libout_config "${libout}")
			string(REPLACE ${BASE_OUTPUT_DIRECTORY} ${BASE_OUTPUT_DIRECTORY_${OUTPUTCONFIG}} runout_config "${runout}")

			set_target_properties(${TargetProject} PROPERTIES LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${libout_config})
			set_target_properties(${TargetProject} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${runout_config})
		endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )
	endif()

	if (OPTION_SHOW_COMPILE_METRICS)
		set(MSVC_COMMON_FLAGS ${MSVC_COMMON_FLAGS} /d2cgsummary)
	endif()

	if (OPTION_LTCG)
		option(LTCG_${TargetProject} "Enable link-time code generation for ${TargetProject} (requires OPTION_LTCG)" ON)
		if (LTCG_${TargetProject})
			if (MSVC_VERSION)
				set(LTCG_FLAG -LTCG)
				get_target_property(flags ${TargetProject} LINK_FLAGS)
				if(flags)
					set_target_properties(${TargetProject} PROPERTIES LINK_FLAGS "${flags} ${LTCG_FLAG}")			
				else()
					set_target_properties(${TargetProject} PROPERTIES LINK_FLAGS "${LTCG_FLAG}")
				endif()
			else()
				target_compile_options(${TargetProject} PRIVATE -flto)
			endif()
		endif()
	endif()

	# Patch linker flags for recode, setup environment path for executables
	if (OPTION_RECODE AND EXISTS RECODE_INSTALL_PATH)
		patch_recode_linker_property(${TargetProject} LINK_FLAGS)
		foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
			string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
			patch_recode_linker_property(${TargetProject} LINK_FLAGS_${OUTPUTCONFIG})
		endforeach()
		get_target_property(type ${TargetProject} TYPE)
		if (type STREQUAL "EXECUTABLE")
			if (WIN64)
				set(platform x64)
			elseif(WIN32)
				set(platform Win32)
			endif()
			file( WRITE "${CMAKE_CURRENT_BINARY_DIR}/${TargetProject}.vcxproj.user" 
  "<?xml version=\"1.0\" encoding=\"utf-8\"?>
  <Project ToolsVersion=\"14.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">
		<PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Debug|${platform}'\">
			<DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>
			<LocalDebuggerEnvironment>PATH=$(Path);${RECODE_INSTALL_PATH}
$(LocalDebuggerEnvironment)</LocalDebuggerEnvironment>
		</PropertyGroup>
		<PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Profile|${platform}'\">
			<DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>
			<LocalDebuggerEnvironment>PATH=$(Path);${RECODE_INSTALL_PATH}
$(LocalDebuggerEnvironment)</LocalDebuggerEnvironment>
		</PropertyGroup>		
		<PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|${platform}'\">
			<DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>
			<LocalDebuggerEnvironment>PATH=$(Path);${RECODE_INSTALL_PATH}
$(LocalDebuggerEnvironment)</LocalDebuggerEnvironment>
		</PropertyGroup>
	</Project>")		
		endif()
	endif()

ENDMACRO(SET_PLATFORM_TARGET_PROPERTIES)

function(JOIN VALUES GLUE OUTPUT)
	string (REPLACE ";" "${GLUE}" _TMP_STR "${VALUES}")
	set (${OUTPUT} "${_TMP_STR}" PARENT_SCOPE)
endfunction()

macro(patch_recode_linker_flags var)
	if (OPTION_RECODE)
		set(temp ${${var}})
		string(REGEX REPLACE "/INCREMENTAL[^\\s]*(\\s|$)" "" temp "${temp}")
		string(REGEX REPLACE "/DEBUG[^\\s]*(\\s|$)" "" temp "${temp}")
		string(REGEX REPLACE "/OPT:REF[^\\s]*(\\s|$)" "" temp "${temp}")
		string(REGEX REPLACE "/OPT:ICF[^\\s]*(\\s|$)" "" temp "${temp}")
		string(REGEX REPLACE "/LTCG[^\\s]*(\\s|$)" "" temp "${temp}")
		set(${var} "${temp} /DEBUG /INCREMENTAL")
	endif()
endmacro()

macro(patch_recode_linker_property TargetProject propname)
	get_target_property(flags ${TargetProject} ${propname})
	if(flags)
		patch_recode_linker_flags(flags)
		set_target_properties(${TargetProject} PROPERTIES ${propname} ${flags})
	endif()
endmacro()

# Writes an uber file to disk
# If uber file already exist it's contents are compared and the file only is overridden if it changed, to allow incremental compilations.
function(write_uber_file_to_disk UBER_FILE input_files)
	set( uber_file_text "// Unity Build Uber File generated by CMake\n// This file is auto generated, do not manually edit it.\n\n" )

	foreach(source_file ${${input_files}} )
		set( uber_file_text "${uber_file_text}#include <${source_file}>\n" )
	endforeach()
	set( uber_file_text "${uber_file_text}\n" )
  
	if(EXISTS "${UBER_FILE}")
		#First check that existing file is exactly the same
		file(READ "${UBER_FILE}" old_uber_file_text)
		if (NOT "${uber_file_text}" STREQUAL "${old_uber_file_text}")
			FILE( WRITE "${UBER_FILE}" "${uber_file_text}")
		endif()
	else()
		FILE( WRITE "${UBER_FILE}" "${uber_file_text}")
	endif()

endfunction()

# Macro for the Unity Build, creating uber files
function(enable_unity_build UB_FILENAME SOURCE_VARIABLE_NAME)
	if(OPTION_UNITY_BUILD)
		set(files ${${SOURCE_VARIABLE_NAME}})
		
		# Generate a unique filename for the unity build translation unit
		set(unit_build_file "${CMAKE_CURRENT_BINARY_DIR}/${UB_FILENAME}")

		set( unit_source_files )
		if (MODULE_PCH_H)
			# Add Pre-compiled Header
			list(APPEND unit_source_files ${MODULE_PCH_H})
		endif()

		# Add include statement for each translation unit
		foreach(source_file ${files} )
			if ("${source_file}" MATCHES ".*\\.\\cpp$" OR 
				"${source_file}" MATCHES ".*\\.\\CPP$" OR
				"${source_file}" MATCHES ".*\\.\\C$" OR
						"${source_file}" MATCHES ".*\\.\\c$")
				if (NOT "${source_file}" STREQUAL "${MODULE_PCH}")
					# Exclude from compilation
					set_source_files_properties("${source_file}" PROPERTIES HEADER_FILE_ONLY true)
					list(APPEND unit_sources "${source_file}")
					list(APPEND unit_source_files "${CMAKE_CURRENT_SOURCE_DIR}/${source_file}")
				endif()
			endif()
		endforeach(source_file)

		write_uber_file_to_disk( "${unit_build_file}" unit_source_files )

		# Group Uber files in solution project
		source_group("UBER FILES" FILES "${unit_build_file}")
		set_source_files_properties("${unit_build_file}" PROPERTIES GENERATED true)
		set_source_files_properties("${unit_build_file}" PROPERTIES SKIP_AUTOMOC true)

		# Turn off precompiled header
		if (WIN32 OR DURANGO)
			#set_source_files_properties(${unit_build_file} PROPERTIES COMPILE_FLAGS "/Y-")
		endif()
	endif()
endfunction(enable_unity_build)

# Process source files
macro(start_sources)
	set(SOURCES)
	set(SOURCE_GROUPS)
	set(UBERFILES)
endmacro()

#Specifies a set of platforms that should build source files provided after this point. Specify ALL to build for all platforms.
#Syntax: sources_platform([OR] X Y Z [AND A B C])
#Subsequent source files will be built if "if(T)" is true for at least one token T in OR and all tokens T in AND.
#Example: sources_platform(WIN32 ANDROID AND HAS_FOO) = build if target is Windows or Android, and HAS_FOO is true.
macro(sources_platform)
	set(PLATFORM_CONDITION)
	set(multiValueArgs OR AND)
	cmake_parse_arguments(COND "" "" "${multiValueArgs}" ${ARGN})
	list(APPEND COND_OR ${COND_UNPARSED_ARGUMENTS})
	set(PLATFORM_CONDITION FALSE)
	foreach(c ${COND_OR})
		if(${c} STREQUAL "ALL")
			set(c TRUE)
		endif()
		if(${c})
			set(PLATFORM_CONDITION TRUE)
		endif()
	endforeach()
	foreach(c ${COND_AND})
		if(${c} STREQUAL "ALL")
			set(c TRUE)
		endif()
		if(NOT ${c})
			set(PLATFORM_CONDITION FALSE)
		endif()
	endforeach()	
endmacro()

macro(add_files)
	foreach(p ${UB_PROJECTS})
		list(APPEND ${p}_SOURCES ${ARGN})
	endforeach()
	list(APPEND SOURCES ${ARGN})
endmacro()

macro(add_to_uberfile uberfile)
	if(OPTION_UNITY_BUILD AND NOT "${uberfile}" STREQUAL "NoUberFile")
		list(APPEND ${uberfile} ${ARGN})
	endif()		
	add_files(${ARGN})
endmacro()

#Usage: add_sources(uberFileName [PROJECTS proj1 proj2 ...] (SOURCE_GROUP "GroupName" file1 file2...)+
#Use "NoUberFile" to signify source files which should not be compiled with others.
#PROJECTS is optional, but should be used when building multiple projects with distinct sets of source files from one directory. If a project is never referenced in PROJECTS, it will use all sources given in this directory.
#Added files will be built if building for a platform matching the latest sources_platform call. If the files should not be built, they are allowed to be missing on the filesystem.
macro(add_sources name)
	set(multiValueArgs PROJECTS SOURCE_GROUP)
	cmake_parse_arguments(UB "" "" "${multiValueArgs}" ${ARGN})

	if(NOT "${name}" STREQUAL "NoUberFile")
		list(APPEND UBERFILES ${name})
		set(${name}_PROJECTS ${UB_PROJECTS})
	endif()

	#Parse source groups manually to avoid conflating multiple groups
	set(CURRENT_SOURCE_GROUP)
	set(EXPECTING_FILE FALSE)
	set(EXPECTING_GROUP_NAME FALSE)
	foreach(ARG ${ARGN})
		if(${ARG} STREQUAL "PROJECTS")
			set(EXPECTING_FILE FALSE)
		elseif(${ARG} STREQUAL "SOURCE_GROUP")
			set(EXPECTING_GROUP_NAME TRUE)
		elseif(EXPECTING_GROUP_NAME)
			set(CURRENT_SOURCE_GROUP ${ARG})
			string(REPLACE " " "_" CURRENT_SOURCE_GROUP_VAR ${CURRENT_SOURCE_GROUP})
			list(FIND SOURCE_GROUPS ${CURRENT_SOURCE_GROUP_VAR} GROUP_INDEX)			
			if(GROUP_INDEX EQUAL -1)
				list(APPEND SOURCE_GROUPS ${CURRENT_SOURCE_GROUP_VAR})
				set(SOURCE_GROUP_${CURRENT_SOURCE_GROUP_VAR})
			endif()
			set(EXPECTING_GROUP_NAME FALSE)
			set(EXPECTING_FILE TRUE)
		elseif(EXPECTING_FILE)
			if(NOT CURRENT_SOURCE_GROUP)
				message(FATAL_ERROR "No source group name defined")
			endif()
			list(APPEND SOURCE_GROUP_${CURRENT_SOURCE_GROUP_VAR} ${ARG})
			if(NOT ${CURRENT_SOURCE_GROUP} STREQUAL "Root")
				source_group(${CURRENT_SOURCE_GROUP} FILES ${ARG})
			else()
				source_group("" FILES ${ARG})
			endif()
			# .mm files are Objective-C; disable those from build on non-Apple
			if(NOT (${PLATFORM_CONDITION}) OR (NOT APPLE AND ${ARG} MATCHES ".*\\.\\mm$"))
				if (EXISTS ${ARG})
					set_source_files_properties(${ARG} PROPERTIES HEADER_FILE_ONLY TRUE)
					add_files(${ARG})
				endif()
			else()
				add_to_uberfile(${name} ${ARG})
			endif()
		endif()
	endforeach()
endmacro()

# Automatically add sources in current CMakeLists directory, respecting directory structure
macro(add_sources_recursive_search)
	set(_src_root_path "${CMAKE_CURRENT_SOURCE_DIR}")
	file(
		GLOB_RECURSE _source_list 
		LIST_DIRECTORIES false
		"${_src_root_path}/*.cpp"
		"${_src_root_path}/*.h"
	)

	foreach(_source IN ITEMS ${_source_list})
		get_filename_component(_source_path "${_source}" PATH)
		file(RELATIVE_PATH _source_path_rel "${_src_root_path}" "${_source_path}")
		string(REPLACE "/" "\\" _group_path "${_source_path_rel}")
		source_group("${_group_path}" FILES "${_source}")
		file(RELATIVE_PATH _source_rel "${_src_root_path}" "${_source}")
		
		if (_group_path STREQUAL "")
			set(_group_path "Root")
		endif()
		
		string(REPLACE "\\" "_" _group_path "${_group_path}")
		
		set(_group_path "${_group_path}.cpp")
		
		list(FIND UBERFILES ${_group_path} GROUP_INDEX)			
		if(GROUP_INDEX EQUAL -1)
			list(APPEND UBERFILES "${_group_path}")
			set(${_group_path}_PROJECTS ${UB_PROJECTS})
		endif()
		
		add_to_uberfile(${_group_path} ${_source_rel})
	endforeach()
endmacro()

macro(get_source_group output group)
	string(REPLACE " " "_" group_var ${group})	
	set(${output} ${SOURCE_GROUP_${group_var}})
endmacro()

macro(end_sources)
endmacro()

macro(generate_uber_files)
	if(UBERFILES)
		option(UNITY_${THIS_PROJECT} "Enable unity building for ${THIS_PROJECT} (requires OPTION_UNITY_BUILD)" ON)
	endif()
	if(OPTION_UNITY_BUILD AND UBERFILES AND UNITY_${THIS_PROJECT})
		list(REMOVE_DUPLICATES UBERFILES)
		foreach(u ${UBERFILES})
			set(UB_PROJECTS ${${u}_PROJECTS})
			if(UB_PROJECTS)
				list(REMOVE_DUPLICATES UB_PROJECTS)
			endif()
			list(LENGTH UB_PROJECTS projcount)
			if(projcount GREATER 1)
				set(UB_PROJECTS_TEMP ${UB_PROJECTS})
				foreach(UB_PROJECTS ${UB_PROJECTS_TEMP})
					if(${u})
						enable_unity_build(${UB_PROJECTS}_${u} ${u})
						add_files(${UB_PROJECTS}_${u})
					endif()
				endforeach()
			else()
				if(${u})
					enable_unity_build(${u} ${u})
					add_files(${u})
				endif()
			endif()
		endforeach()
	endif()
endmacro()

# Shared settings for specific module types
macro(force_static_crt)
	target_compile_definitions(${THIS_PROJECT} PUBLIC -D_MT)
	if(MSVC)
		target_compile_options(${THIS_PROJECT} PUBLIC /MT$<$<CONFIG:Debug>:d>)
	endif()
endmacro()

macro(read_settings)
	set(options DISABLE_MFC FORCE_STATIC FORCE_SHARED FORCE_SHARED_WIN EDITOR_COMPILE_SETTINGS)
	set(oneValueArgs SOLUTION_FOLDER PCH OUTDIR)
	set(multiValueArgs FILE_LIST INCLUDES LIBS DEFINES)
	cmake_parse_arguments(MODULE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
	if (MODULE_PCH)
		string(REPLACE ".cpp" ".h" MODULE_PCH_HEADER_FILE ${MODULE_PCH})
		get_filename_component(MODULE_PCH_H ${MODULE_PCH_HEADER_FILE} NAME)
	endif()
	if (MODULE_FORCE_SHARED_WIN AND (WIN32 OR WIN64))
		set(MODULE_FORCE_SHARED TRUE)
	endif()
	if (MODULE_FORCE_SHARED AND (ORBIS))
		set(MODULE_FORCE_SHARED FALSE)
	endif()
endmacro()

macro(prepare_project)
	set(THIS_PROJECT ${target} PARENT_SCOPE)
	set(THIS_PROJECT ${target})
	include_directories( "${CMAKE_CURRENT_SOURCE_DIR}" )
	project(${target})
	read_settings(${ARGN})
	generate_uber_files()
	if(NOT ${THIS_PROJECT}_SOURCES)
		set(${THIS_PROJECT}_SOURCES ${SOURCES})
	endif()	
endmacro()

macro(apply_compile_settings)
	if (MODULE_PCH)
		USE_MSVC_PRECOMPILED_HEADER( ${THIS_PROJECT} ${MODULE_PCH_H} ${MODULE_PCH} )
		set_property(TARGET ${THIS_PROJECT} APPEND PROPERTY AUTOMOC_MOC_OPTIONS -b${MODULE_PCH_H})
	endif()
	if (MODULE_OUTDIR)
		set_property(TARGET ${THIS_PROJECT} PROPERTY LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${MODULE_OUTDIR}")
		set_property(TARGET ${THIS_PROJECT} PROPERTY RUNTIME_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${MODULE_OUTDIR}") 
	endif()
	SET_PLATFORM_TARGET_PROPERTIES( ${THIS_PROJECT} )	
	if(MODULE_SOLUTION_FOLDER)
		set_solution_folder("${MODULE_SOLUTION_FOLDER}" ${THIS_PROJECT})
	endif()

	if (WIN32 AND NOT OPTION_DEDICATED_SERVER)
		target_compile_options(${THIS_PROJECT} PRIVATE $<$<CONFIG:Release>:-DPURE_CLIENT>)
	endif()
	
	if (DEFINED PROJECT_BUILD_CRYENGINE AND NOT PROJECT_BUILD_CRYENGINE)
		# If option to not build engine modules is selected they are excluded from the build
		set_target_properties(${THIS_PROJECT} PROPERTIES EXCLUDE_FROM_ALL TRUE)
		set_target_properties(${THIS_PROJECT} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD TRUE)
	endif()

	get_target_property(target_type ${THIS_PROJECT} TYPE)
	if (target_type MATCHES "EXECUTABLE")
		target_compile_options(${THIS_PROJECT} PRIVATE -DCRY_IS_APPLICATION)
	endif()
endmacro()

function(CryEngineModule target)
	prepare_project(${ARGN})
	
	if ((OPTION_STATIC_LINKING OR MODULE_FORCE_STATIC) AND NOT MODULE_FORCE_SHARED)
		# Static modules must not contain resource files
		remove_files_from_list( ${THIS_PROJECT}_SOURCES ".*\\.rc$")
	endif()

	if (MODULE_FORCE_SHARED)
		add_library(${THIS_PROJECT} SHARED ${${THIS_PROJECT}_SOURCES})
	elseif (MODULE_FORCE_STATIC)
		add_library(${THIS_PROJECT} STATIC ${${THIS_PROJECT}_SOURCES})
	else()
		add_library(${THIS_PROJECT} ${${THIS_PROJECT}_SOURCES})
	endif()
	if (MODULE_EDITOR_COMPILE_SETTINGS)
		set_editor_module_flags()
	else()
		set(MODULES ${MODULES} ${THIS_PROJECT} CACHE INTERNAL "List of engine being built" FORCE)	
	endif()
	apply_compile_settings()
	if ((OPTION_STATIC_LINKING OR MODULE_FORCE_STATIC) AND NOT MODULE_FORCE_SHARED)
		target_compile_definitions(${THIS_PROJECT} PRIVATE _LIB -DCRY_IS_MONOLITHIC_BUILD)
	endif()
	add_metadata()
	if (ANDROID AND NOT OPTION_STATIC_LINKING AND NOT MODULE_FORCE_STATIC) 
		set(SHARED_MODULES ${SHARED_MODULES} ${THIS_PROJECT} CACHE INTERNAL "Shared modules for APK creation" FORCE)
		target_link_libraries(${THIS_PROJECT} PRIVATE m log c android)
	endif()

	if(OPTION_DEDICATED_SERVER)
		target_compile_definitions( ${THIS_PROJECT} PRIVATE "-DDEDICATED_SERVER")
	endif()

	if (NOT DEFINED PROJECT_BUILD_CRYENGINE OR PROJECT_BUILD_CRYENGINE)
		install(TARGETS ${target} LIBRARY DESTINATION bin RUNTIME DESTINATION bin ARCHIVE DESTINATION lib)
	endif()
endfunction()

function(CryGameModule target)
	prepare_project(${ARGN})
	add_library(${THIS_PROJECT} ${${THIS_PROJECT}_SOURCES})
	apply_compile_settings()
	if (NOT game_folder)
		set(game_folder ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "Game folder used for resource files on Windows" FORCE)
	endif()
	set(GAME_MODULES ${GAME_MODULES} ${THIS_PROJECT} CACHE INTERNAL "List of game modules being built" FORCE)
	if (OPTION_STATIC_LINKING)
		target_compile_definitions(${THIS_PROJECT} PRIVATE _LIB -DCRY_IS_MONOLITHIC_BUILD)
	elseif(ANDROID)
		set(SHARED_MODULES ${SHARED_MODULES} ${THIS_PROJECT} CACHE INTERNAL "Shared modules for APK creation" FORCE)
		target_link_libraries(${THIS_PROJECT} PRIVATE m log c android)
	endif()
	add_metadata()

	if(OPTION_DEDICATED_SERVER)
		target_compile_definitions( ${THIS_PROJECT} PRIVATE "-DDEDICATED_SERVER")
	endif()

	if (NOT DEFINED PROJECT_BUILD_CRYENGINE OR PROJECT_BUILD_CRYENGINE)
		install(TARGETS ${target} LIBRARY DESTINATION bin RUNTIME DESTINATION bin ARCHIVE DESTINATION lib)
	endif()
endfunction()

function(CreateDynamicModule target)
	prepare_project(${ARGN})
	add_library(${THIS_PROJECT} SHARED ${${THIS_PROJECT}_SOURCES})
	if (MODULE_EDITOR_COMPILE_SETTINGS)
		set_editor_module_flags()
	endif()
	apply_compile_settings()
endfunction()

function(CryEngineStaticModule target)
	prepare_project(${ARGN})
	add_library(${THIS_PROJECT} STATIC ${${THIS_PROJECT}_SOURCES})
	set(MODULE_FORCE_STATIC TRUE)
	target_compile_definitions(${THIS_PROJECT} PRIVATE -D_LIB)
	if (OPTION_STATIC_LINKING)
		target_compile_definitions(${THIS_PROJECT} PRIVATE -DCRY_IS_MONOLITHIC_BUILD)
	endif()
	if(OPTION_DEDICATED_SERVER)
		target_compile_definitions( ${THIS_PROJECT} PRIVATE "-DDEDICATED_SERVER")
	endif()
	if (MODULE_EDITOR_COMPILE_SETTINGS)
		set_editor_module_flags()
	endif()
	apply_compile_settings()
endfunction()

function(CryLauncher target)
	prepare_project(${ARGN})
	if(ANDROID)
		add_library(${target} SHARED ${${THIS_PROJECT}_SOURCES})
		target_link_libraries(${THIS_PROJECT} PRIVATE m log c android)
		configure_android_launcher(${target})		
	elseif(WIN32)
		add_executable(${THIS_PROJECT} WIN32 ${${THIS_PROJECT}_SOURCES})
	else()
		add_executable(${target} ${${THIS_PROJECT}_SOURCES})
	endif()
	if(ORBIS)
		set_property(TARGET ${target} PROPERTY OUTPUT_NAME "GameLauncher.elf")	
	elseif(NOT ANDROID)
		set_property(TARGET ${THIS_PROJECT} PROPERTY OUTPUT_NAME "GameLauncher")	
	endif()

	if(OPTION_STATIC_LINKING)
		use_scaleform()
		target_compile_definitions(${THIS_PROJECT} PRIVATE _LIB -DCRY_IS_MONOLITHIC_BUILD)
		if (WIN32)
			set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS_PROFILE " /NODEFAULTLIB:libcpmt.lib")
			set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS_RELEASE " /NODEFAULTLIB:libcpmt.lib")
		endif()
		set(MODULES_LIST ${GAME_MODULES} ${MODULES})
		wrap_whole_archive(${target} WRAPPED_MODULES MODULES_LIST)
		target_link_libraries(${THIS_PROJECT} PRIVATE ${WRAPPED_MODULES})
	endif()
	add_metadata(WindowsIcon.ico)
	apply_compile_settings()	

	if(NOT ANDROID)
		if (NOT DEFINED PROJECT_BUILD_CRYENGINE OR PROJECT_BUILD_CRYENGINE)
			install(TARGETS ${target} RUNTIME DESTINATION bin ARCHIVE DESTINATION lib)
		endif()
	endif()
endfunction()

function(CryDedicatedServer target)
	prepare_project(${ARGN})
	if(WIN32)
		add_executable(${THIS_PROJECT} WIN32 ${${THIS_PROJECT}_SOURCES})
		set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS " /SUBSYSTEM:WINDOWS")
	else()
		add_executable(${THIS_PROJECT} ${${THIS_PROJECT}_SOURCES})
	endif()
	set_property(TARGET ${THIS_PROJECT} PROPERTY OUTPUT_NAME "Game_Server")
	add_metadata(WindowsServerIcon.ico)

	if(OPTION_STATIC_LINKING)
		use_scaleform()
		target_compile_definitions(${THIS_PROJECT} PRIVATE _LIB -DCRY_IS_MONOLITHIC_BUILD)
		if (WIN32)
			set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS_PROFILE " /NODEFAULTLIB:libcpmt.lib")
			set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS_RELEASE " /NODEFAULTLIB:libcpmt.lib")
		endif()
		set(MODULES_LIST ${GAME_MODULES} ${MODULES})
		wrap_whole_archive(${target} WRAPPED_MODULES MODULES_LIST)
		target_link_libraries(${THIS_PROJECT} PRIVATE ${WRAPPED_MODULES})
	endif()

	if(OPTION_DEDICATED_SERVER) 
		target_compile_definitions( ${THIS_PROJECT} PRIVATE "-DDEDICATED_SERVER") 
	endif() 
	
	apply_compile_settings()	
endfunction()

function(CryConsoleApplication target)
	prepare_project(${ARGN})
	add_executable(${THIS_PROJECT} ${${THIS_PROJECT}_SOURCES})
	set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS " /SUBSYSTEM:CONSOLE")
	apply_compile_settings()	
endfunction()

function(CryWindowsApplication target)
	prepare_project(${ARGN})
	add_executable(${THIS_PROJECT} ${${THIS_PROJECT}_SOURCES})
	set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS " /SUBSYSTEM:WINDOWS")
	apply_compile_settings()	
endfunction()

function(CryFileContainer target)
	set(THIS_PROJECT ${target} PARENT_SCOPE)
	set(THIS_PROJECT ${target})
	project(${target})

	read_settings(${ARGN})
	if(NOT ${THIS_PROJECT}_SOURCES)
		set(${THIS_PROJECT}_SOURCES ${SOURCES})
	endif()	

	add_custom_target( ${THIS_PROJECT} SOURCES ${${THIS_PROJECT}_SOURCES})
	if(MODULE_SOLUTION_FOLDER)
		set_solution_folder("${MODULE_SOLUTION_FOLDER}" ${THIS_PROJECT})
	endif()
endfunction()

macro(set_editor_module_flags)
	target_include_directories( ${THIS_PROJECT} PRIVATE
		"${CRYENGINE_DIR}/Code/Sandbox/Plugins/EditorCommon"
		"${CRYENGINE_DIR}/Code/Sandbox/EditorInterface"
		"${CRYENGINE_DIR}/Code/CryEngine/CryCommon" 
		"${CRYENGINE_DIR}/Code/CryEngine/CryCommon/3rdParty"
		"${SDK_DIR}/boost"
	)
	target_compile_definitions( ${THIS_PROJECT} PRIVATE
		-DWIN32
		-DCRY_ENABLE_RC_HELPER
		-DIS_EDITOR_BUILD
		-DQT_FORCE_ASSERT
		-DUSE_PYTHON_SCRIPTING 
		-DNO_WARN_MBCS_MFC_DEPRECATION
	)
	if(NOT MODULE_DISABLE_MFC AND NOT MODULE_FORCE_STATIC)
		target_compile_definitions( ${THIS_PROJECT} PRIVATE -D_AFXDLL)
	endif()
	target_link_libraries( ${THIS_PROJECT} PRIVATE BoostPython python27)
	use_qt()
	target_compile_options(${THIS_PROJECT} PRIVATE /EHsc /GR /bigobj /wd4251 /wd4275)
	target_include_directories(${THIS_PROJECT} PRIVATE "${CRYENGINE_DIR}/Code/Sandbox/Libs/CryQt")
	target_link_libraries(${THIS_PROJECT} PRIVATE CryQt)
	set_property(TARGET ${THIS_PROJECT} PROPERTY EXCLUDE_FROM_DEFAULT_BUILD_RELEASE TRUE)
endmacro()

function(CryEditor target)
	prepare_project(${ARGN})
	if(WIN32)
		add_executable(${THIS_PROJECT} WIN32 ${${THIS_PROJECT}_SOURCES})
	else()
		add_executable(${THIS_PROJECT} ${${THIS_PROJECT}_SOURCES})
	endif()
	set_editor_module_flags()
	add_metadata()
	target_compile_options(${THIS_PROJECT} PRIVATE /Zm200)
	target_compile_definitions(${THIS_PROJECT} PRIVATE -DSANDBOX_EXPORTS -DPLUGIN_IMPORTS -DEDITOR_COMMON_IMPORTS)
	target_link_libraries(${THIS_PROJECT} PRIVATE EditorCommon)
	set_property(TARGET ${THIS_PROJECT} PROPERTY ENABLE_EXPORTS TRUE)
	set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS " /SUBSYSTEM:WINDOWS")
	apply_compile_settings()
endfunction()

function(CryEditorPlugin target)
	prepare_project(${ARGN})
	add_library(${THIS_PROJECT} ${${THIS_PROJECT}_SOURCES})
	set_editor_module_flags()
	target_compile_options(${THIS_PROJECT} PRIVATE /Zm200)
	target_compile_definitions(${THIS_PROJECT} PRIVATE -DSANDBOX_IMPORTS -DPLUGIN_IMPORTS -DEDITOR_COMMON_IMPORTS -DNOT_USE_CRY_MEMORY_MANAGER)
	set_property(TARGET ${THIS_PROJECT} PROPERTY LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/EditorPlugins")
	set_property(TARGET ${THIS_PROJECT} PROPERTY RUNTIME_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/EditorPlugins")
	target_link_libraries(${THIS_PROJECT} PRIVATE EditorCommon)
	apply_compile_settings()	
endfunction()

macro(set_rc_flags)
	# Forcing static crt gets Linker Tools Error LNK2038. 
	# https://msdn.microsoft.com/en-us/library/2kzt1wy3.aspx All modules passed to a given invocation of the linker must have been compiled with the same run-time library compiler option.
	#
	# force_static_crt()
	
	target_compile_definitions( ${THIS_PROJECT} PRIVATE
		-DWIN32
		-DRESOURCE_COMPILER
		-DFORCE_STANDARD_ASSERT
		-DNOT_USE_CRY_MEMORY_MANAGER
	)
	target_include_directories( ${THIS_PROJECT} PRIVATE 
		"${CRYENGINE_DIR}/Code/CryEngine/CryCommon" 
		"${CRYENGINE_DIR}/Code/CryEngine/CryCommon/3rdParty"
		"${SDK_DIR}/boost"
		"${CRYENGINE_DIR}/Code/Sandbox/Plugins/EditorCommon" 
	)
endmacro()

macro(set_pipeline_flags)
	force_static_crt()
	target_compile_definitions( ${THIS_PROJECT} PRIVATE
		-DWIN32
		-DRESOURCE_COMPILER
		-DFORCE_STANDARD_ASSERT
		-DNOT_USE_CRY_MEMORY_MANAGER
	)
	target_include_directories( ${THIS_PROJECT} PRIVATE 
		"${CRYENGINE_DIR}/Code/CryEngine/CryCommon" 
		"${CRYENGINE_DIR}/Code/CryEngine/CryCommon/3rdParty"
	)
endmacro()

function(CryPipelineModule target)
	prepare_project(${ARGN})
	add_library(${THIS_PROJECT} ${${THIS_PROJECT}_SOURCES})
	set_rc_flags()
	set_property(TARGET ${THIS_PROJECT} PROPERTY LIBRARY_OUTPUT_DIRECTORY Tools/rc)
	set_property(TARGET ${THIS_PROJECT} PROPERTY RUNTIME_OUTPUT_DIRECTORY Tools/rc)
	if(WIN32)
		set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS " /SUBSYSTEM:CONSOLE")
	endif()
	set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS_DEBUG " /NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:msvcprtd.lib")
	apply_compile_settings()	
endfunction()

# WAF features
function(use_qt_modules)
	set(_QT_MODULES ${ARGN})
	foreach(MODULE ${_QT_MODULES})
		#find_package(Qt5${MODULE} REQUIRED)
		target_include_directories(${THIS_PROJECT} PRIVATE "${QT_DIR}/include/Qt${MODULE}")
		target_link_libraries(${THIS_PROJECT} PRIVATE Qt5${MODULE}$<$<CONFIG:Debug>:d>)
	endforeach()
endfunction()

macro(use_qt)
	#custom_enable_QT5()
	set(CMAKE_PREFIX_PATH "${QT_DIR}")
	set_property(TARGET ${THIS_PROJECT} PROPERTY AUTOMOC TRUE)
	set_property(TARGET ${THIS_PROJECT} PROPERTY AUTORCC TRUE)

	target_compile_definitions(${THIS_PROJECT} PRIVATE -DQT_GUI_LIB -DQT_NO_EMIT -DQT_WIDGETS_LIB)
	target_include_directories(${THIS_PROJECT} PRIVATE "${QT_DIR}/include")
	set_libpath_flag()
	set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS " ${LIBPATH_FLAG}\"${QT_DIR}/lib\"")

	set(QT_MODULES Core Gui OpenGL Widgets)
	use_qt_modules(${QT_MODULES})
endmacro()

macro(remove_files_from_list filelist filepattern)
	#message(STATUS " remove_files_from_list ${filelist} ${filepattern}")
	set(FilesToRemove "")
	foreach (sourcefile ${${filelist}})
		if ("${sourcefile}" MATCHES ${filepattern})
			list(APPEND FilesToRemove "${sourcefile}" )
			#message(STATUS " FILE TO REMOVE: ${sourcefile}")
		endif()
	endforeach()
	foreach (sourcefile ${FilesToRemove}) 
		list(REMOVE_ITEM ${filelist} "${sourcefile}" )
	endforeach() 
endmacro()

# For Windows, an argument may be provided to specify the location of an icon for the executable
macro(add_metadata)
	if (WIN32 OR WIN64)
		get_target_property(project_type ${THIS_PROJECT} TYPE)
		set(valid_types EXECUTABLE MODULE_LIBRARY SHARED_LIBRARY)
		if(NOT ${project_type} IN_LIST valid_types)
			return()
		endif()
		
		set(icon_name ${ARGN})
		if (NOT PRODUCT_NAME)
			set(PRODUCT_NAME ${THIS_PROJECT})
		endif()
		file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${THIS_PROJECT}.autogen.rc"
			"// Microsoft Visual C++ generated resource script.\n"
			"//\n"
			"#include \"resource.h\"\n"
			"\n"
			"#define APSTUDIO_READONLY_SYMBOLS\n"
			"/////////////////////////////////////////////////////////////////////////////\n"
			"//\n"
			"// Generated from the TEXTINCLUDE 2 resource.\n"
			"//\n"
			"#include \"winres.h\"\n"
			"\n"
			"/////////////////////////////////////////////////////////////////////////////\n"
			"#undef APSTUDIO_READONLY_SYMBOLS\n"
			"\n"
			)
		if (FALSE)
			file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/${THIS_PROJECT}.autogen.rc"
				"/////////////////////////////////////////////////////////////////////////////\n"
				"// Neutral resources\n"
				"\n"
				"#if !defined(AFX_RESOURCE_DLL) || defined(AFX_TARG_NEU)\n"
				"LANGUAGE LANG_NEUTRAL, SUBLANG_NEUTRAL\n"
				"#pragma code_page(1252)\n"
				"\n"
				"/////////////////////////////////////////////////////////////////////////////\n"
				"//\n"
				"// Cursor\n"
				"//\n"
				"\n"
				"\"${project.cursor_resource_name}\"   CURSOR                  \"${project.cursor_name}\"\n"
				"\n"
				"#endif    // Neutral resources\n"
				"/////////////////////////////////////////////////////////////////////////////\n"
			)
		endif()
		file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/${THIS_PROJECT}.autogen.rc"
			"		"
			"/////////////////////////////////////////////////////////////////////////////\n"
			"// English (United States) resources\n"
			"\n"
			"#if !defined(AFX_RESOURCE_DLL) || defined(AFX_TARG_ENU)\n"
			"LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US\n"
			"#pragma code_page(1252)\n"
			"\n"
			"#ifdef APSTUDIO_INVOKED\n"
			"/////////////////////////////////////////////////////////////////////////////\n"
			"//\n"
			"// TEXTINCLUDE\n"
			"//\n"
			"\n"
			"1 TEXTINCLUDE \n"
			"BEGIN\n"
			"    \"resource.h\\0\"\n"
			"END\n"
			"\n"
			"2 TEXTINCLUDE \n"
			"BEGIN\n"
			"    \"#include \"\"winres.h\"\"\\r\\n\"\n"
			"    \"\\0\"\n"
			"END\n"
			"\n"
			"3 TEXTINCLUDE \n"
			"BEGIN\n"
			"    \"\\r\\n\"\n"
			"    \"\\0\"\n"
			"END\n"
			"\n"
			"#endif    // APSTUDIO_INVOKED\n"
			"\n"
		)
		if (icon_name AND EXISTS "${game_folder}/../Resources/${icon_name}")
			file(COPY "${game_folder}/../Resources/${icon_name}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
			file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/${THIS_PROJECT}.autogen.rc"
				"// Icon with lowest ID value placed first to ensure application icon\n"
				"// remains consistent on all systems.\n"
				"IDI_ICON                ICON                    \"${icon_name}\"\n"
			)
		endif()
		file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/${THIS_PROJECT}.autogen.rc"
			"#endif    // English (United States) resources\n"
			"/////////////////////////////////////////////////////////////////////////////\n"
			"\n"
			"\n"
			"/////////////////////////////////////////////////////////////////////////////\n"
			"// German (Germany) resources\n"
			"\n"
			"#if !defined(AFX_RESOURCE_DLL) || defined(AFX_TARG_DEU)\n"
			"LANGUAGE LANG_GERMAN, SUBLANG_GERMAN\n"
			"#pragma code_page(1252)\n"
			"\n"
			"/////////////////////////////////////////////////////////////////////////////\n"
			"//\n"
			"// Version\n"
			"//\n"
			"\n"
			"VS_VERSION_INFO VERSIONINFO\n"
			" FILEVERSION ${METADATA_VERSION_COMMA}\n"
			" PRODUCTVERSION ${METADATA_VERSION}\n"
			" FILEFLAGSMASK 0x17L\n"
			"#ifdef _DEBUG\n"
			" FILEFLAGS 0x1L\n"
			"#else\n"
			" FILEFLAGS 0x0L\n"
			"#endif\n"
			" FILEOS 0x4L\n"
			" FILETYPE 0x2L\n"
			" FILESUBTYPE 0x0L\n"
			"BEGIN\n"
			"    BLOCK \"StringFileInfo\"\n"
			"    BEGIN\n"
			"        BLOCK \"000904b0\"\n"
			"        BEGIN\n"
			"            VALUE \"CompanyName\", \"${METADATA_COMPANY}\"\n"
			"            VALUE \"FileVersion\", \"${METADATA_VERSION_COMMA}\"\n"
			"            VALUE \"LegalCopyright\", \"${METADATA_COPYRIGHT}\"\n"
			"            VALUE \"ProductName\", \"${PRODUCT_NAME}\"\n"
			"            VALUE \"ProductVersion\", \"${METADATA_VERSION}\"\n"
			"        END\n"
			"    END\n"
			"    BLOCK \"VarFileInfo\"\n"
			"    BEGIN\n"
			"        VALUE \"Translation\", 0x9, 1200\n"
			"    END\n"
			"END\n"
			"\n"
			"#endif    // German (Germany) resources\n"
			"/////////////////////////////////////////////////////////////////////////////\n"
			"\n"
			"\n"
			"\n"
			"#ifndef APSTUDIO_INVOKED\n"
			"/////////////////////////////////////////////////////////////////////////////\n"
			"//\n"
			"// Generated from the TEXTINCLUDE 3 resource.\n"
			"//\n"
			"\n"
			"\n"
			"/////////////////////////////////////////////////////////////////////////////\n"
			"#endif    // not APSTUDIO_INVOKED\n"
		)
	target_sources(${THIS_PROJECT} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${THIS_PROJECT}.autogen.rc")
	source_group("Resource Files" FILES "${CMAKE_CURRENT_BINARY_DIR}/${THIS_PROJECT}.autogen.rc")
	elseif(METADATA_VERSION)
		string(REPLACE "." ";" VERSION_LIST ${METADATA_VERSION}) 
		list(GET VERSION_LIST 0 VERSION_MAJOR) 
		list(GET VERSION_LIST 1 VERSION_MINOR) 
		list(GET VERSION_LIST 2 VERSION_REVISION) 
		list(GET VERSION_LIST 3 VERSION_BUILD) 

		target_compile_definitions(${THIS_PROJECT} PRIVATE EXE_VERSION_INFO_0=${VERSION_MAJOR}) 
		target_compile_definitions(${THIS_PROJECT} PRIVATE EXE_VERSION_INFO_1=${VERSION_MINOR}) 
		target_compile_definitions(${THIS_PROJECT} PRIVATE EXE_VERSION_INFO_2=${VERSION_REVISION}) 
		target_compile_definitions(${THIS_PROJECT} PRIVATE EXE_VERSION_INFO_3=${VERSION_BUILD})
	endif()
endmacro()

macro(use_scaleform)
	if (OPTION_SCALEFORMHELPER)
		target_compile_definitions(${THIS_PROJECT} PRIVATE -DCRY_FEATURE_SCALEFORM_HELPER)
		if (EXISTS ${SDK_DIR}/Scaleform)
			target_include_directories(${THIS_PROJECT} PRIVATE "${SDK_DIR}/Scaleform/Include" )
			target_compile_definitions(${THIS_PROJECT} PRIVATE -DINCLUDE_SCALEFORM_SDK $<$<CONFIG:Debug>:GFC_BUILD_DEBUG>)
			set(SCALEFORM_LIB_FOLDER "${SDK_DIR}/Scaleform/Lib")
			set(SCALEFORM_HAS_SHIPPING_LIB TRUE)
			if (WIN64)
				set(SCALEFORM_LIB_FOLDER "${SCALEFORM_LIB_FOLDER}/${MSVC_LIB_PREFIX}/Win64" )
				set(SCALEFORM_LIBS libgfx)
			elseif(WIN32)
				set(SCALEFORM_LIB_FOLDER "${SCALEFORM_LIB_FOLDER}/${MSVC_LIB_PREFIX}/Win32" )
				set(SCALEFORM_LIBS libgfx)
			elseif(DURANGO)
				set(SCALEFORM_LIB_FOLDER "${SCALEFORM_LIB_FOLDER}/${MSVC_LIB_PREFIX}/Durango" )
				set(SCALEFORM_LIBS libgfx)
			elseif(LINUX)
				set(SCALEFORM_LIB_FOLDER "${SCALEFORM_LIB_FOLDER}/linux")
				set(SCALEFORM_HAS_SHIPPING_LIB FALSE)
				set(SCALEFORM_LIBS gfx jpeg png16)
			elseif(ORBIS)
				set(SCALEFORM_LIB_FOLDER "${SCALEFORM_LIB_FOLDER}/ORBIS")
				set(SCALEFORM_LIBS gfx gfx_video)
			elseif(ANDROID)
				set(SCALEFORM_LIB_FOLDER "${SCALEFORM_LIB_FOLDER}/android-${CMAKE_ANDROID_ARCH}")
				set(SCALEFORM_HAS_SHIPPING_LIB FALSE)
				set(SCALEFORM_LIBS gfx)
			elseif(APPLE)
				set(SCALEFORM_LIB_FOLDER "${SCALEFORM_LIB_FOLDER}/mac")
				set(SCALEFORM_HAS_SHIPPING_LIB FALSE)
				set(SCALEFORM_LIBS gfx jpeg png16)
			endif()

			if(SCALEFORM_HAS_SHIPPING_LIB)
				set(SCALEFORM_RELEASE_CONFIG Shipping)
				target_compile_definitions(${THIS_PROJECT} PRIVATE "$<$<CONFIG:Release>:GFC_BUILD_SHIPPING>")
			else()
				set(SCALEFORM_RELEASE_CONFIG Release)
			endif()

			# Set linker search path
			set_libpath_flag()
			if(APPLE OR LINUX)
				set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS " ${LIBPATH_FLAG}\"${SCALEFORM_LIB_FOLDER}\"")
			endif()
			set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS_DEBUG " ${LIBPATH_FLAG}\"${SCALEFORM_LIB_FOLDER}/Debug/\"")
			set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS_PROFILE " ${LIBPATH_FLAG}\"${SCALEFORM_LIB_FOLDER}/Release/\"")
			set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS_RELEASE " ${LIBPATH_FLAG}\"${SCALEFORM_LIB_FOLDER}/${SCALEFORM_RELEASE_CONFIG}/\"")

			target_link_libraries(${THIS_PROJECT} PRIVATE ${SCALEFORM_LIBS})
		endif()
	endif()
endmacro()

macro(use_xt)
	if (MSVC_VERSION GREATER 1900) # Visual Studio > 2015
		set(XT_VERSION vc14)
	elseif (MSVC_VERSION EQUAL 1900) # Visual Studio 2015
		set(XT_VERSION vc14)
	elseif (MSVC_VERSION EQUAL 1800) # Visual Studio 2013
		set(XT_VERSION vc12)
	elseif (MSVC_VERSION EQUAL 1700) # Visual Studio 2012
		set(XT_VERSION vc11)
	endif()
	target_include_directories( ${THIS_PROJECT} PRIVATE "${SDK_DIR}/XT_13_4/Include" )
	set_libpath_flag()
	set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS " ${LIBPATH_FLAG}\"${SDK_DIR}/XT_13_4/lib_${XT_VERSION}\"")
endmacro()

# Function lists the subdirectories using the glob expression
function(list_subdirectories_glob globPattern result)
	file(GLOB _pathList LIST_DIRECTORIES true "${globPattern}")
	set(_dirList)
	foreach(_child ${_pathList})
		if (IS_DIRECTORY "${_child}")
			list(APPEND _dirList "${_child}")
		endif()
	endforeach()
	set(${result} ${_dirList} PARENT_SCOPE)
endfunction()

# Helper function finds and adds subdirectories containing CMakeLists.txt
function(add_subdirectories_glob globPattern)
	set(_dirs)
	list_subdirectories_glob("${globPattern}" _dirs)
	foreach(dir ${_dirs})
		if (EXISTS "${dir}/CMakeLists.txt")
			#message(STATUS "add_subdirectory ${dir}")
			add_subdirectory("${dir}")
		endif()
	endforeach()
endfunction()

function(add_subdirectories)
	add_subdirectories_glob("*")
endfunction()

function(set_visual_studio_debugger_command TARGET_NAME EXE_PATH CMD_LINE)
	if (WIN32 AND NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.vcxproj.user")
		#Configure default Visual Studio debugger settings
		file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.vcxproj.user"
			"<?xml version=\"1.0\" encoding=\"utf-8\"?>
			<Project ToolsVersion=\"14.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">
				<PropertyGroup>
					<LocalDebuggerCommand>${EXE_PATH}</LocalDebuggerCommand>
					<LocalDebuggerCommandArguments>${CMD_LINE}</LocalDebuggerCommandArguments>
					<DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>
				</PropertyGroup>
			</Project>"
		)
	endif()
endfunction()
