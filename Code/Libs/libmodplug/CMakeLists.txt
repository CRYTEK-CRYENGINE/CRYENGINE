
start_sources()
sources_platform(ALL)
add_sources("NoUberFile"
	PROJECTS libmodplug_static
	SOURCE_GROUP "Root"
		"../../SDKs/libmodplug/src/fastmix.cpp"
		"../../SDKs/libmodplug/src/load_669.cpp"
		"../../SDKs/libmodplug/src/load_abc.cpp"
		"../../SDKs/libmodplug/src/load_amf.cpp"
		"../../SDKs/libmodplug/src/load_ams.cpp"
		"../../SDKs/libmodplug/src/load_dbm.cpp"
		"../../SDKs/libmodplug/src/load_dmf.cpp"
		"../../SDKs/libmodplug/src/load_dsm.cpp"
		"../../SDKs/libmodplug/src/load_far.cpp"
		"../../SDKs/libmodplug/src/load_it.cpp"
		"../../SDKs/libmodplug/src/load_j2b.cpp"
		"../../SDKs/libmodplug/src/load_mdl.cpp"
		"../../SDKs/libmodplug/src/load_med.cpp"
		"../../SDKs/libmodplug/src/load_mid.cpp"
		"../../SDKs/libmodplug/src/load_mod.cpp"
		"../../SDKs/libmodplug/src/load_mt2.cpp"
		"../../SDKs/libmodplug/src/load_mtm.cpp"
		"../../SDKs/libmodplug/src/load_okt.cpp"
		"../../SDKs/libmodplug/src/load_pat.cpp"
		"../../SDKs/libmodplug/src/load_pat.h"
		"../../SDKs/libmodplug/src/load_psm.cpp"
		"../../SDKs/libmodplug/src/load_ptm.cpp"
		"../../SDKs/libmodplug/src/load_s3m.cpp"
		"../../SDKs/libmodplug/src/load_stm.cpp"
		"../../SDKs/libmodplug/src/load_ult.cpp"
		"../../SDKs/libmodplug/src/load_umx.cpp"
		"../../SDKs/libmodplug/src/load_wav.cpp"
		"../../SDKs/libmodplug/src/load_xm.cpp"
		"../../SDKs/libmodplug/src/mmcmp.cpp"
		"../../SDKs/libmodplug/src/modplug.cpp"
		"../../SDKs/libmodplug/src/modplug.h"
		"../../SDKs/libmodplug/src/snd_dsp.cpp"
		"../../SDKs/libmodplug/src/sndfile.cpp"
		"../../SDKs/libmodplug/src/snd_flt.cpp"
		"../../SDKs/libmodplug/src/snd_fx.cpp"
		"../../SDKs/libmodplug/src/sndmix.cpp"
		"../../SDKs/libmodplug/src/tables.h"
)
end_sources()


if(LINUX OR ANDROID)

CryEngineStaticModule(libmodplug_static SOLUTION_FOLDER "Libs/Audio")
target_include_directories(${THIS_PROJECT} PUBLIC
	"${SDK_DIR}/libmodplug/src"
	"${SDK_DIR}/libmodplug/src/libmodplug"
)
target_compile_definitions(${THIS_PROJECT} PUBLIC -DMODPLUG_HEADER="modplug.h")
target_compile_options(${THIS_PROJECT} PRIVATE -w)
set_property(TARGET ${THIS_PROJECT} PROPERTY OUTPUT_NAME "modplug")

if(LINUX OR ANDROID)
	target_compile_definitions(${THIS_PROJECT} PRIVATE
		-DHAVE_DLFCN_H
		-DHAVE_INTTYPES_H
		-DHAVE_MALLOC_H
		-DHAVE_MEMORY_H
		-DHAVE_SETENV
		-DHAVE_SINF
		-DHAVE_STDINT_H
		-DHAVE_STDLIB_H
		-DHAVE_STRINGS_H
		-DHAVE_STRING_H
		-DHAVE_SYS_STAT_H
		-DHAVE_SYS_TYPES_H
		-DHAVE_UNISTD_H
		-DSTDC_HEADERS
	)
endif()

endif()
