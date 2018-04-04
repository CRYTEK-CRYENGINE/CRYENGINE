start_sources()
sources_platform(ALL)
add_sources("NoUberFile"
	PROJECTS libogg_dynamic libogg_static
	SOURCE_GROUP "Root"
		"../../SDKs/libogg/src/bitwise.c"
		"../../SDKs/libogg/src/framing.c"
)

end_sources()

CryEngineStaticModule(libogg_static SOLUTION_FOLDER "Libs/Audio")
target_include_directories(${THIS_PROJECT} PUBLIC "${SDK_DIR}/libogg/include")
set_property(TARGET ${THIS_PROJECT} PROPERTY OUTPUT_NAME "ogg")

if(WIN32 OR WIN64)
	target_compile_options(${THIS_PROJECT} PRIVATE /W0)
elseif(LINUX)
	target_compile_definitions(${THIS_PROJECT} PRIVATE
		-DHAVE_DLFCN_H
		-DHAVE_INTTYPES_H
		-DHAVE_MEMORY_H
		-DHAVE_STDINT_H
		-DHAVE_STDLIB_H
		-DHAVE_STRINGS_H
		-DHAVE_STRING_H
		-DHAVE_SYS_STAT_H
		-DHAVE_SYS_TYPES_H
		-DHAVE_UNISTD_H
		-DSTDC_HEADERS
	)
	target_compile_options(${THIS_PROJECT} PRIVATE -w)
elseif (ANDROID)
	target_compile_options(${THIS_PROJECT} PRIVATE -w)
	target_compile_definitions(${THIS_PROJECT} PRIVATE
		-DHAVE_DLFCN_H
		-DHAVE_INTTYPES_H
		-DHAVE_MEMORY_H
		-DHAVE_STDINT_H
		-DHAVE_STDLIB_H
		-DHAVE_STRINGS_H
		-DHAVE_STRING_H
		-DHAVE_SYS_STAT_H
		-DHAVE_SYS_TYPES_H
		-DHAVE_UNISTD_H
		-DSTDC_HEADERS
	)
	target_link_libraries(${THIS_PROJECT} PRIVATE m)
endif()
