CryEngineStaticModule(CryEngine.Common SOLUTION_FOLDER "CryEngine")

set(SWIG_EXECUTABLE "${SDK_DIR}/swig/swig.exe")

FIND_PACKAGE(SWIG REQUIRED)
INCLUDE(${SWIG_USE_FILE})

INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}")

SET(CMAKE_SWIG_FLAGS "")

file(GLOB files "*.i")

file(GLOB_RECURSE native_source_files ../../../CryEngine/CryCommon/*.h)

foreach(file ${files})
	SET_SOURCE_FILES_PROPERTIES(${file} PROPERTIES CPLUSPLUS ON)
	SET_SOURCE_FILES_PROPERTIES(${file} PROPERTIES SWIG_FLAGS "-includeall")

	get_filename_component(filename "${file}" NAME_WE)

	SWIG_ADD_MODULE("${filename}.out" csharp ${file} ${native_source_files})
	SWIG_LINK_LIBRARIES("${filename}.out" CryEngine.Common)
endforeach()
