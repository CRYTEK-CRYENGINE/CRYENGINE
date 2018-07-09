if (NOT DEFINED PYSIDE_DIR)
	get_filename_component(PYSIDE_DIR ${QT_DIR} DIRECTORY)
	set(PYSIDE_DIR "${PYSIDE_DIR}/PySide/PySide2/")
endif()

set(SHIBOKEN_EXE "${PYSIDE_DIR}shiboken2.exe")

if (NOT DEFINED QT_API)
	set(QT_API "${QT_VERSION_MAJOR}.${QT_VERSION_MINOR}")
endif()

set(PYSIDE_INCLUDE_PATH "${PYSIDE_DIR}include/PySide2/")
set(SHIBOKEN_INCLUDE_PATH "${PYSIDE_DIR}include/shiboken2/")
set(QT_INCLUDE_PATH "${QT_DIR}/include/")

SET (WRAPPER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/Generated/")
FILE (REMOVE_RECURSE "${WRAPPER_OUTPUT_DIR}")

get_filename_component (PARENT_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" DIRECTORY)

set (TYPESYSTEM_PATHS "${PYSIDE_DIR}typesystems\;${PYSIDE_INCLUDE_PATH}")
set(ENV{PATH} "${QT_DIR}/bin;$ENV{PATH}")
SET(SHIBOKEN_ARGS
	"--generator-set=shiboken"
	"--enable-parent-ctor-heuristic"
	"--enable-pyside-extensions"
	"--enable-return-value-heuristic"
	"--use-isnull-as-nb_nonzero"
	"--avoid-protected-hack"
	"${CMAKE_CURRENT_SOURCE_DIR}/global.h"
	"--include-paths=${PARENT_SOURCE_DIRECTORY}\;${PYSIDE_INCLUDE_PATH}\;${QT_INCLUDE_PATH}"
	"--typesystem-paths=${TYPESYSTEM_PATHS}"
	"--output-directory=${WRAPPER_OUTPUT_DIR}"
	"${CMAKE_CURRENT_SOURCE_DIR}/typesystem_cryqt.xml"
	"--api-version=5.6")

message( STATUS "Running PySide Shiboken ${SHIBOKEN_EXE}")
execute_process ( COMMAND "${SHIBOKEN_EXE}" ${SHIBOKEN_ARGS} WORKING_DIRECTORY "${QT_DIR}/bin" OUTPUT_VARIABLE shiboken_output ERROR_VARIABLE shiboken_error)
message( STATUS "Done Running PySide Shiboken")

if (DEFINED SHOW_SHIBOKEN_OUTPUT)
	message("${shiboken_output}")
	message("${shiboken_error}")
endif()

FILE(GLOB shiboken_generated_sources "${WRAPPER_OUTPUT_DIR}/_CryQt/*.*")

start_sources()
sources_platform(ALL)
add_sources("NoUberFile"
	SOURCE_GROUP "Generated"
	${shiboken_generated_sources}
)
end_sources()

CreateDynamicModule(CryQtShibokenWrapper SOLUTION_FOLDER "Sandbox/Libs")
target_compile_definitions(${THIS_PROJECT} PRIVATE -DWIN32)
target_include_directories(${THIS_PROJECT} PRIVATE "${SHIBOKEN_INCLUDE_PATH}" "${PYSIDE_INCLUDE_PATH}" "${PYSIDE_INCLUDE_PATH}/QtGui" "${PYSIDE_INCLUDE_PATH}/QtCore" "${PYSIDE_INCLUDE_PATH}/QtWidgets" "${QT_INCLUDE_PATH}/QtANGLE")
target_link_libraries(${THIS_PROJECT} PRIVATE dwmapi gdi32 Ole32 qtmain$<$<CONFIG:Debug>:d>)
target_link_libraries(${THIS_PROJECT} PRIVATE CryQt)
set_property(TARGET ${THIS_PROJECT} PROPERTY EXCLUDE_FROM_DEFAULT_BUILD_RELEASE TRUE)

target_link_libraries(${THIS_PROJECT} PRIVATE python27)

# Must include all sub-directories of CryQt, since generated files include header of wraped code by name only.
FILE(GLOB_RECURSE all_headers "${PARENT_SOURCE_DIRECTORY}/*.h")
set_property(DIRECTORY . PROPERTY CMAKE_CONFIGURE_DEPENDS ${all_headers})

FOREACH(header ${all_headers})
        GET_FILENAME_COMPONENT(dir_path "${header}" DIRECTORY)
        SET(dir_list ${dir_list} "${dir_path}")
ENDFOREACH()
LIST(REMOVE_DUPLICATES dir_list)

target_include_directories(${THIS_PROJECT} PRIVATE ${dir_list})

set_libpath_flag()
set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS_DEBUG " ${LIBPATH_FLAG}\"${PYSIDE_DIR}\"")
set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS_PROFILE " ${LIBPATH_FLAG}\"${PYSIDE_DIR}\"")
set_property(TARGET ${THIS_PROJECT} APPEND_STRING PROPERTY LINK_FLAGS_RELEASE " ${LIBPATH_FLAG}\"${PYSIDE_DIR}\"")

if (NOT DEFINED PYSIDE_LIB)
	set(PYSIDE_LIB pyside2-python2.7$<$<CONFIG:Debug>:-dbg>)
endif()

if (NOT DEFINED SHIBOKEN_LIB)
	set(SHIBOKEN_LIB shiboken2-python2.7$<$<CONFIG:Debug>:-dbg>)
endif()

target_link_libraries(${THIS_PROJECT} PRIVATE "${PYSIDE_LIB}" "${SHIBOKEN_LIB}")

use_qt()

set_target_properties(${THIS_PROJECT} PROPERTIES SUFFIX ".pyd")
set_target_properties(${THIS_PROJECT} PROPERTIES OUTPUT_NAME "_CryQt")
set_target_properties(${THIS_PROJECT} PROPERTIES DEBUG_POSTFIX "_d")

if(MSVC)
  # Force to always compile with W0
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W0" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W0")
  endif()
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")

add_custom_command(TARGET ${THIS_PROJECT} POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy "\"${CMAKE_CURRENT_SOURCE_DIR}/CryQt.py\"" "\"$<TARGET_FILE_DIR:${THIS_PROJECT}>\"")

