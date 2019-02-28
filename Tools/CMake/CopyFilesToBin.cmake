# Copy required additional files from 3rdparty to the binary folder
set(DEPLOY_FILES  CACHE INTERNAL "List of files to deploy before running")

if (OPTION_ENGINE OR OPTION_SANDBOX OR OPTION_SHADERCACHEGEN)
	set (BinaryFileList_Win64
		"${WINDOWS_SDK}/Debuggers/x64/srcsrv/dbghelp.dll"
		"${WINDOWS_SDK}/Debuggers/x64/srcsrv/dbgcore.dll"
		"${WINDOWS_SDK}/bin/x64/d3dcompiler_47.dll"
		)

	set (BinaryFileList_Win32
		"${WINDOWS_SDK}/Debuggers/x86/srcsrv/dbghelp.dll"
		"${WINDOWS_SDK}/Debuggers/x86/srcsrv/dbgcore.dll"
		"${WINDOWS_SDK}/bin/x86/d3dcompiler_47.dll"
		)

	if(EXISTS "${SDK_DIR}/AMD/AGS Lib")
		set (BinaryFileList_Win64 ${BinaryFileList_Win64}
			"${SDK_DIR}/AMD/AGS Lib/lib/amd_ags_x64.dll"
			)

	    set (BinaryFileList_Win32 ${BinaryFileList_Win32} 
			"${SDK_DIR}/AMD/AGS Lib/lib/amd_ags_x86.dll"
			)
	endif()
endif()

file(TO_CMAKE_PATH "${DURANGO_SDK}" DURANGO_SDK_CMAKE)
set (BinaryFileList_Durango
	"${DURANGO_SDK_CMAKE}/xdk/symbols/d3dcompiler_47.dll"
	)


set (BinaryFileList_LINUX64
	${SDK_DIR}/ncurses/lib/libncursesw.so.6
	)


macro(add_optional_runtime_files)

	if (OPTION_ENABLE_BROFILER)
		set (BinaryFileList_Win64 ${BinaryFileList_Win64} "${SDK_DIR}/Brofiler/ProfilerCore64.dll")
		set (BinaryFileList_Win32 ${BinaryFileList_Win32} "${SDK_DIR}/Brofiler/ProfilerCore32.dll")
	endif()

	if(OPTION_ENABLE_CRASHRPT)
		set (BinaryFileList_Win64 ${BinaryFileList_Win64} "${SDK_DIR}/CrashRpt/1403/bin/x64/crashrpt_lang.ini")
		set (BinaryFileList_Win64 ${BinaryFileList_Win64} "${SDK_DIR}/CrashRpt/1403/bin/x64/CrashSender1403.exe")
		set (BinaryFileList_Win32 ${BinaryFileList_Win32} "${SDK_DIR}/CrashRpt/1403/bin/x86/crashrpt_lang.ini")
		set (BinaryFileList_Win32 ${BinaryFileList_Win32} "${SDK_DIR}/CrashRpt/1403/bin/x86/CrashSender1403.exe")
	endif()

	if (PLUGIN_VR_OCULUS OR AUDIO_OCULUS_HRTF)
		set (BinaryFileList_Win64 ${BinaryFileList_Win64} "${SDK_DIR}/audio/oculus/wwise/x64/bin/plugins/OculusSpatializerWwise.dll")
		set (BinaryFileList_Win32 ${BinaryFileList_Win32} "${SDK_DIR}/audio/oculus/wwise/Win32/bin/plugins/OculusSpatializerWwise.dll")
	endif()

	if (PLUGIN_VR_OPENVR)
		set (BinaryFileList_Win64 ${BinaryFileList_Win64} "${SDK_DIR}/OpenVR/bin/win64/*.*")
	endif()

	if (PLUGIN_VR_OSVR)
		set (BinaryFileList_Win64 ${BinaryFileList_Win64} "${SDK_DIR}/OSVR/dll/*.dll")
	endif()

	if (OPTION_SANDBOX)
		if (CMAKE_BUILD_TYPE)
			set (BinaryFileList_Win64 ${BinaryFileList_Win64}
				"${SDK_DIR}/Qt/5.6/msvc2015_64/Qt/bin/icudt*.dll"
			)
			set (BinaryFileList_Win64_Profile ${BinaryFileList_Win64_Profile}
				"${SDK_DIR}/XT_13_4/bin_vc14/*[^Dd].dll"
				"${SDK_DIR}/Qt/5.6/msvc2015_64/Qt/bin/icu[^Dd][^Tt]*[^Dd].dll"
				"${SDK_DIR}/Qt/5.6/msvc2015_64/Qt/bin/[^Ii]*[^Dd].dll"
			)
			set (BinaryFileList_Win64_Debug ${BinaryFileList_Win64_Debug}
				"${SDK_DIR}/XT_13_4/bin_vc14/*[Dd].dll"
				"${SDK_DIR}/XT_13_4/bin_vc14/*[Dd].pdb"
				"${SDK_DIR}/Qt/5.6/msvc2015_64/Qt/bin/*[Dd].dll"
				"${SDK_DIR}/Qt/5.6/msvc2015_64/Qt/bin/*[Dd].pdb"
			)
		else()
			set (BinaryFileList_Win64 ${BinaryFileList_Win64}
				"${SDK_DIR}/XT_13_4/bin_vc14/*.dll"
				"${SDK_DIR}/Qt/5.6/msvc2015_64/Qt/bin/*.dll"
				"${SDK_DIR}/Qt/5.6/msvc2015_64/Qt/bin/*d.pdb"
			)
		endif()
	endif()
endmacro()

macro(deploy_runtime_file source destination)
	if(USE_CONFIG)
		# HACK: This works on the assumption that any individual file is only used by *one* configuration, or *all* configurations. CMake will generate errors otherwise.
		list(APPEND DEPLOY_FILES "$<$<CONFIG:${USE_CONFIG}>:${source}>")
		list(APPEND DEPLOY_FILES "${source}")
		list(APPEND DEPLOY_FILES "${destination}")
	else()
		list(APPEND DEPLOY_FILES "${source}")
		list(APPEND DEPLOY_FILES "${source}")
		list(APPEND DEPLOY_FILES "${destination}")
	endif()
	set(DEPLOY_FILES "${DEPLOY_FILES}" CACHE INTERNAL "List of files to deploy before running")
endmacro()

macro(set_base_outdir)
	set(base_outdir ${BASE_OUTPUT_DIRECTORY})
	if(NOT ${CMAKE_GENERATOR} MATCHES "Visual Studio")
		string( TOUPPER ${CMAKE_BUILD_TYPE} BUILD_CONFIG )
		set(base_outdir ${BASE_OUTPUT_DIRECTORY_${BUILD_CONFIG}})
	endif()
	if(OUTPUT_DIRECTORY_SUFFIX)
		set(base_outdir "${base_outdir}_${OUTPUT_DIRECTORY_SUFFIX}")
	endif()
	if(OPTION_DEDICATED_SERVER)
		set(base_outdir "${base_outdir}_dedicated")
	endif()
endmacro()

macro(deploy_runtime_files fileexpr)
	file(GLOB FILES_TO_COPY "${fileexpr}")
	foreach(FILE_PATH ${FILES_TO_COPY})
		get_filename_component(FILE_NAME "${FILE_PATH}" NAME)

		set_base_outdir()
		# If another argument was passed files are deployed to the subdirectory
		if (${ARGC} GREATER 1)
			deploy_runtime_file("${FILE_PATH}" "${base_outdir}/${ARGV1}/${FILE_NAME}")
			install(FILES "${FILE_PATH}" DESTINATION "bin/${ARGV1}")
		else ()
			deploy_runtime_file("${FILE_PATH}" "${base_outdir}/${FILE_NAME}")
			install(FILES "${FILE_PATH}" DESTINATION bin)
		endif ()
	endforeach()
endmacro()

macro(deploy_runtime_dir dir output_dir)
	file(GLOB_RECURSE FILES_TO_COPY RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/${dir}" "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/*")
	set_base_outdir()

	foreach(FILE_NAME ${FILES_TO_COPY})
		set(FILE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/${FILE_NAME}")

		deploy_runtime_file("${FILE_PATH}" "${base_outdir}/${output_dir}/${FILE_NAME}")
	endforeach()

	install(DIRECTORY "${dir}" DESTINATION "bin/${output_dir}")
endmacro()

macro(deploy_pyside_files)
	set_base_outdir()
	foreach(FILE_NAME ${PYSIDE_DLLS})
		get_filename_component (name_without_extension "${FILE_NAME}" NAME_WE)
		set(PDB_NAME "${name_without_extension}.pdb")
		set(FILE_PATH "${PYSIDE_SOURCE}${FILE_NAME}")
		set(PDB_PATH "${PYSIDE_SOURCE}${PDB_NAME}")
		deploy_runtime_file("${FILE_PATH}" "${base_outdir}/${FILE_NAME}")
		install(FILES "${FILE_PATH}" DESTINATION bin)
		if(EXISTS "${PDB_PATH}")
			deploy_runtime_file("${PDB_PATH}" "${base_outdir}/${PDB_NAME}")
			install(FILES "${PDB_PATH}" DESTINATION bin)
		endif()
	endforeach()

	foreach(FILE_NAME ${FILES_TO_COPY})
		get_filename_component (name_without_extension "${FILE_NAME}" NAME_WE)
		get_filename_component (dirname "${FILE_NAME}" DIRECTORY)
		set(PDB_NAME "${dirname}${name_without_extension}.pdb")
		set(FILE_PATH "${PYSIDE_SOURCE}${FILE_NAME}")
		set(PDB_PATH "${PYSIDE_SOURCE}${PDB_NAME}")
		deploy_runtime_file("${FILE_PATH}" "${base_outdir}/PySide2/${FILE_NAME}")
		install(FILES "${FILE_PATH}" DESTINATION bin/PySide2)
		if(EXISTS "${PDB_PATH}")
			deploy_runtime_file("${PDB_PATH}" "${base_outdir}/PySide2/${PDB_NAME}")
			install(FILES "${PDB_PATH}" DESTINATION bin/PySide2)
		endif()
	endforeach()
endmacro()

macro(deploy_pyside)
	set(PYSIDE_SDK_SOURCE "${SDK_DIR}/Qt/5.6/msvc2015_64/PySide/")
	set(PYSIDE_SOURCE "${PYSIDE_SDK_SOURCE}PySide2/")

	# Only copy debug DLLs and .pyd's if we are building in debug mode, otherwise take only the release versions.
	if(CMAKE_BUILD_TYPE)
		set(USE_CONFIG Debug)
	endif()
	set(PYSIDE_DLLS "pyside2-python2.7-dbg.dll" "shiboken2-python2.7-dbg.dll")
	file(GLOB FILES_TO_COPY RELATIVE "${PYSIDE_SOURCE}" "${PYSIDE_SOURCE}*_d.pyd")
	deploy_pyside_files()
	if(CMAKE_BUILD_TYPE)
		set(USE_CONFIG Profile)
	endif()
	set(PYSIDE_DLLS "pyside2-python2.7.dll" "shiboken2-python2.7.dll")
	file(GLOB FILES_TO_COPY RELATIVE "${PYSIDE_SOURCE}" "${PYSIDE_SOURCE}*[^_][^d].pyd")
	deploy_pyside_files()
	set(USE_CONFIG)
	set(PYSIDE_DLLS)
	file(GLOB FILES_TO_COPY RELATIVE "${PYSIDE_SOURCE}" "${PYSIDE_SOURCE}*.py" "${PYSIDE_SOURCE}scripts/*.py")
	deploy_pyside_files()

	deploy_runtime_dir("${PYSIDE_SDK_SOURCE}pyside2uic" "pyside2uic")

endmacro()

macro(copy_binary_files_to_target)
	if (DEFINED PROJECT_BUILD_CRYENGINE AND NOT PROJECT_BUILD_CRYENGINE)
		# When engine is not to be Build do not deploy anything either
		return()
	endif()

	message( STATUS "copy_binary_files_to_target start ${BUILD_PLATFORM}" )

	add_optional_runtime_files()

	set( file_list_name "BinaryFileList_${BUILD_PLATFORM}" )
	get_property( BINARY_FILE_LIST VARIABLE PROPERTY ${file_list_name} )
	foreach(b ${BINARY_FILE_LIST})
		deploy_runtime_files("${b}")
	endforeach()

	foreach(config IN ITEMS Debug Profile Release)
		set( file_list_name "BinaryFileList_${BUILD_PLATFORM}_${config}" )
		get_property( BINARY_FILE_LIST VARIABLE PROPERTY ${file_list_name} )
		set(USE_CONFIG ${config})
		foreach(b ${BINARY_FILE_LIST})
			deploy_runtime_files("${b}")
		endforeach()
		set(USE_CONFIG)
	endforeach()

	if (ORBIS)
		deploy_runtime_files("${SDK_DIR}/Orbis/target/sce_module/*.prx" "app/sce_module")
	endif()

	if (WIN64)
		if (OPTION_ENGINE)
			deploy_runtime_files("${SDK_DIR}/Microsoft Visual Studio Compiler/14.0/redist/x64/**/*.dll")
		endif()
		if (OPTION_SANDBOX)
			deploy_runtime_files("${SDK_DIR}/Qt/5.6/msvc2015_64/Qt/plugins/platforms/*.dll" "platforms")
			deploy_runtime_files("${SDK_DIR}/Qt/5.6/msvc2015_64/Qt/plugins/imageformats/*.dll" "imageformats")
			if (CMAKE_BUILD_TYPE)
				set(USE_CONFIG Debug)
				deploy_runtime_files("${SDK_DIR}/Python27/*_d.zip")
				set(USE_CONFIG Profile)
				deploy_runtime_files("${SDK_DIR}/Python27/*[^_][^d].zip")
				set(USE_CONFIG)
			else()
				deploy_runtime_files("${SDK_DIR}/Python27/*.zip")
			endif()
			deploy_pyside()
		endif()
	elseif(WIN32)
		if (OPTION_ENGINE)
			deploy_runtime_files("${SDK_DIR}/Microsoft Visual Studio Compiler/14.0/redist/x86/**/*.dll")
		endif()
	endif ()

	if(DEPLOY_FILES)
		set(DEPLOY_DESTINATIONS)

		list(LENGTH DEPLOY_FILES deployFilesCount)
		math(EXPR idxRangeEnd "${deployFilesCount} - 1")
		foreach(idx RANGE 0 ${idxRangeEnd} 3)
			math(EXPR idxIncr "${idx} + 1")
			math(EXPR idxIncr2 "${idx} + 2")
			list(GET DEPLOY_FILES ${idx} source)
			list(GET DEPLOY_FILES ${idxIncr} source_file)
			list(GET DEPLOY_FILES ${idxIncr2} destination)

			if(source MATCHES "<") # Source contains generator expression; deploy at build time
				add_custom_command(OUTPUT ${destination}
					COMMAND ${CMAKE_COMMAND} "-DSOURCE=\"${source}\"" "-DDESTINATION=\"${destination}\"" -P "${TOOLS_CMAKE_DIR}/deploy_runtime_files.cmake"
					COMMENT "Deploying ${source_file}"
					DEPENDS "${source}")
				list(APPEND DEPLOY_DESTINATIONS "${destination}")
			else()
				message(STATUS "Deploying ${source_file}")
				get_filename_component(DEST_DIR "${destination}" DIRECTORY)
				file(COPY "${source}" DESTINATION "${DEST_DIR}" NO_SOURCE_PERMISSIONS)
			endif()

		endforeach(idx)

		if (DEPLOY_DESTINATIONS)
			add_custom_target(deployrt ALL DEPENDS ${DEPLOY_DESTINATIONS})
			set_solution_folder("Deploy" deployrt)
		endif()
	endif()
  message( STATUS "copy_binary_files_to_target end" )
endmacro()

