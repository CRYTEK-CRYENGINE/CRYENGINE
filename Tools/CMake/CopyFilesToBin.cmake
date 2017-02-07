# Copy required additional files from 3rdparty to the binary folder
set(DEPLOY_FILES  CACHE INTERNAL "List of files to deploy before running")

set (BinaryFileList_Win64
	"${SDK_DIR}/Microsoft Windows SDK/10/Debuggers/x64/dbghelp.dll"
	"${SDK_DIR}/Microsoft Windows SDK/10/bin/x64/d3dcompiler_47.dll"
	)

set (BinaryFileList_Win32
	"${SDK_DIR}/Microsoft Windows SDK/10/Debuggers/x86/dbghelp.dll"
	"${SDK_DIR}/Microsoft Windows SDK/10/bin/x86/d3dcompiler_47.dll"
	)

set (BinaryFileList_LINUX64
	${SDK_DIR}/ncurses/lib/libncursesw.so.6
	)


if (OPTION_ENABLE_BROFILER)
	set (BinaryFileList_Win64 ${BinaryFileList_Win64};${SDK_DIR}/Brofiler/ProfilerCore64.dll)
	set (BinaryFileList_Win32 ${BinaryFileList_Win32};${SDK_DIR}/Brofiler/ProfilerCore32.dll)
endif()
if(OPTION_ENABLE_CRASHRPT)
	set (BinaryFileList_Win64 ${BinaryFileList_Win64};${SDK_DIR}/CrashRpt/1403/bin/x64/crashrpt_lang.ini)
	set (BinaryFileList_Win64 ${BinaryFileList_Win64};${SDK_DIR}/CrashRpt/1403/bin/x64/CrashSender1403.exe)
	set (BinaryFileList_Win32 ${BinaryFileList_Win32};${SDK_DIR}/CrashRpt/1403/bin/x86/crashrpt_lang.ini)
	set (BinaryFileList_Win32 ${BinaryFileList_Win32};${SDK_DIR}/CrashRpt/1403/bin/x86/CrashSender1403.exe)
endif()
if (OPTION_CRYMONO)
	set (BinaryFileList_Win64 ${BinaryFileList_Win64};${SDK_DIR}/Mono/bin/x64/mono-2.0.dll)
	set (BinaryFileList_Win32 ${BinaryFileList_Win32};${SDK_DIR}/Mono/bin/x86/mono-2.0.dll)
endif()
if (PLUGIN_VR_OCULUS)
	set (BinaryFileList_Win64 ${BinaryFileList_Win64};${SDK_DIR}/audio/oculus/wwise/bin/plugins/OculusSpatializer.dll)
endif()
if (PLUGIN_VR_OPENVR)
	set (BinaryFileList_Win64 ${BinaryFileList_Win64};${SDK_DIR}/OpenVR/bin/win64/*.*)
endif()
if (PLUGIN_VR_OSVR)
	set (BinaryFileList_Win64 ${BinaryFileList_Win64};${SDK_DIR}/OSVR/dll/*.dll)
endif()
if (OPTION_SANDBOX)
	set (BinaryFileList_Win64 ${BinaryFileList_Win64}
		${SDK_DIR}/XT_13_4/bin_vc14/*.dll
		${SDK_DIR}/Qt/5.6/msvc2015_64/Qt/bin/*.dll
		${SDK_DIR}/Qt/5.6/msvc2015_64/Qt/bin/*d.pdb
	)
endif()

macro(deploy_runtime_file source destination)
	list(APPEND DEPLOY_FILES ${source})
	list(APPEND DEPLOY_FILES ${destination})
	set(DEPLOY_FILES "${DEPLOY_FILES}" CACHE INTERNAL "List of files to deploy before running")
endmacro()

macro(deploy_runtime_files fileexpr)
	file(GLOB FILES_TO_COPY ${fileexpr})
	foreach(FILE_PATH ${FILES_TO_COPY})
		get_filename_component(FILE_NAME ${FILE_PATH} NAME)

		# If another argument was passed files are deployed to the subdirectory
		if (${ARGC} GREATER 1)
			deploy_runtime_file(${FILE_PATH} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${ARGV1}/${FILE_NAME})
			install(FILES ${FILE_PATH} DESTINATION bin/${ARGV1})
		else ()
			deploy_runtime_file(${FILE_PATH} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${FILE_NAME})
			install(FILES ${FILE_PATH} DESTINATION bin)
		endif ()
	endforeach()
endmacro()

macro(deploy_runtime_dir dir output_dir)
	file(GLOB_RECURSE FILES_TO_COPY RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/${dir} ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/*)

	foreach(FILE_NAME ${FILES_TO_COPY})
		set(FILE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/${FILE_NAME})

		deploy_runtime_file(${FILE_PATH} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${output_dir}/${FILE_NAME})
	endforeach()

	install(DIRECTORY ${dir} DESTINATION bin/${output_dir})
endmacro()

macro(deploy_pyside)
	set(PYSIDE_SDK_SOURCE ${SDK_DIR}/Qt/5.6/msvc2015_64/PySide/)
	set(PYSIDE_SOURCE ${PYSIDE_SDK_SOURCE}PySide2/)

	set(PYSIDE_DLLS "pyside2-python2.7.dll" "shiboken2-python2.7.dll" "pyside2-python2.7-dbg.dll" "shiboken2-python2.7-dbg.dll")
	foreach(FILE_NAME ${PYSIDE_DLLS})
		get_filename_component (name_without_extension ${FILE_NAME} NAME_WE)
		set(PDB_NAME ${name_without_extension}.pdb)
		set(FILE_PATH ${PYSIDE_SOURCE}${FILE_NAME})
		set(PDB_PATH ${PYSIDE_SOURCE}${PDB_NAME})
		deploy_runtime_file(${FILE_PATH} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${FILE_NAME})
		install(FILES ${FILE_PATH} DESTINATION bin)
		if(EXISTS ${PDB_PATH})
			deploy_runtime_file(${PDB_PATH} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PDB_NAME})
			install(FILES ${PDB_PATH} DESTINATION bin)
		endif()
	endforeach()
	
	file(GLOB FILES_TO_COPY RELATIVE ${PYSIDE_SOURCE} ${PYSIDE_SOURCE}*.pyd ${PYSIDE_SOURCE}*.py ${PYSIDE_SOURCE}scripts/*.py)
	foreach(FILE_NAME ${FILES_TO_COPY})
		get_filename_component (name_without_extension ${FILE_NAME} NAME_WE)
		get_filename_component (dirname ${FILE_NAME} DIRECTORY)
		set(PDB_NAME ${dirname}${name_without_extension}.pdb)
		set(FILE_PATH ${PYSIDE_SOURCE}${FILE_NAME})
		set(PDB_PATH ${PYSIDE_SOURCE}${PDB_NAME})	
		deploy_runtime_file(${FILE_PATH} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/PySide2/${FILE_NAME})
		install(FILES ${FILE_PATH} DESTINATION bin/PySide2)
		if(EXISTS ${PDB_PATH})
			deploy_runtime_file(${PDB_PATH} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/PySide2/${PDB_NAME})
			install(FILES ${PDB_PATH} DESTINATION bin/PySide2)
		endif()
	endforeach()
	
	deploy_runtime_dir(${PYSIDE_SDK_SOURCE}pyside2uic pyside2uic)
	
endmacro()

macro(copy_binary_files_to_target)
  message( STATUS "copy_binary_files_to_target start ${BUILD_PLATFORM}" )
  
	set( file_list_name "BinaryFileList_${BUILD_PLATFORM}" )
	get_property( BINARY_FILE_LIST VARIABLE PROPERTY ${file_list_name} )
	deploy_runtime_files("${BINARY_FILE_LIST}")
  
	if (ORBIS)
		deploy_runtime_files(${SDK_DIR}/Orbis/target/sce_module/*.prx app/sce_module)
	endif()

	if (WIN64) 
		deploy_runtime_files("${SDK_DIR}/Microsoft Visual Studio Compiler/14.0/redist/x64/**/*.dll")
		if (OPTION_SANDBOX)
			deploy_runtime_files(${SDK_DIR}/Qt/5.6/msvc2015_64/Qt/plugins/platforms/*.dll platforms)
			deploy_runtime_files(${SDK_DIR}/Qt/5.6/msvc2015_64/Qt/plugins/imageformats/*.dll imageformats)
			deploy_runtime_files(${SDK_DIR}/Python27/*.zip)
			deploy_pyside()
		endif()
	elseif(WIN32)
		deploy_runtime_files("${SDK_DIR}/Microsoft Visual Studio Compiler/14.0/redist/x86/**/*.dll")
	endif ()

	if(DEPLOY_FILES)
		set(DEPLOY_DESTINATIONS)

		list(LENGTH DEPLOY_FILES deployFilesCount)
		math(EXPR idxRangeEnd "${deployFilesCount} - 1")

		foreach(idx RANGE 0 ${idxRangeEnd} 2)
			math(EXPR idxIncr "${idx} + 1")
			list(GET DEPLOY_FILES ${idx} source)
			list(GET DEPLOY_FILES ${idxIncr} destination)
      
			add_custom_command(OUTPUT ${destination} 
				COMMAND ${CMAKE_COMMAND} -DSOURCE=${source} -DDESTINATION=${destination} -P ${CRYENGINE_DIR}/Tools/CMake/deploy_runtime_files.cmake
				COMMENT "Deploying ${source}"
				DEPENDS ${source})

			list(APPEND DEPLOY_DESTINATIONS ${destination})
		endforeach(idx)

		add_custom_target(deployrt ALL DEPENDS ${DEPLOY_DESTINATIONS})
	endif()
  message( STATUS "copy_binary_files_to_target end" )
endmacro()

