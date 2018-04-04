

FILE(GLOB SOURCES "*.cmake")
FILE(GLOB_RECURSE SOURCES_toolchain "toolchain/*.cmake")
FILE(GLOB_RECURSE SOURCES_modules "modules/*.cmake")

#source_group("Tools/CMake" FILES ${Tools_CMake})
source_group("Toolchain" FILES ${SOURCES_toolchain})
source_group("Modules" FILES ${SOURCES_modules})

set( CMAKE_SOURCES
	${SOURCES}
	${SOURCES_toolchain}
	${SOURCES_modules}
)
add_custom_target( CMAKE SOURCES ${CMAKE_SOURCES})
set_property(TARGET CMAKE PROPERTY FOLDER "${VS_FOLDER_PREFIX}/CMAKE")
