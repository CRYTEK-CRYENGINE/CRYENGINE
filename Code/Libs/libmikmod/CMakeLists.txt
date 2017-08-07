start_sources()
sources_platform(ALL)
add_sources("NoUberFile"
	PROJECTS libmikmod_static
	SOURCE_GROUP "Root"
		"../../SDKs/libmikmod/drivers/drv_ds.c"
		"../../SDKs/libmikmod/drivers/drv_aiff.c"
		"../../SDKs/libmikmod/drivers/drv_nos.c"
		"../../SDKs/libmikmod/drivers/drv_openal.c"
		"../../SDKs/libmikmod/drivers/drv_sdl.c"
		"../../SDKs/libmikmod/drivers/drv_raw.c"
		"../../SDKs/libmikmod/drivers/drv_stdout.c"
		"../../SDKs/libmikmod/drivers/drv_wav.c"
		"../../SDKs/libmikmod/drivers/drv_win.c"
		"../../SDKs/libmikmod/drivers/drv_xaudio2.c"
		"../../SDKs/libmikmod/loaders/load_669.c"
		"../../SDKs/libmikmod/loaders/load_amf.c"
		"../../SDKs/libmikmod/loaders/load_asy.c"
		"../../SDKs/libmikmod/loaders/load_dsm.c"
		"../../SDKs/libmikmod/loaders/load_far.c"
		"../../SDKs/libmikmod/loaders/load_gdm.c"
		"../../SDKs/libmikmod/loaders/load_gt2.c"
		"../../SDKs/libmikmod/loaders/load_it.c"
		"../../SDKs/libmikmod/loaders/load_imf.c"
		"../../SDKs/libmikmod/loaders/load_m15.c"
		"../../SDKs/libmikmod/loaders/load_med.c"
		"../../SDKs/libmikmod/loaders/load_mod.c"
		"../../SDKs/libmikmod/loaders/load_mtm.c"
		"../../SDKs/libmikmod/loaders/load_okt.c"
		"../../SDKs/libmikmod/loaders/load_s3m.c"
		"../../SDKs/libmikmod/loaders/load_stm.c"
		"../../SDKs/libmikmod/loaders/load_stx.c"
		"../../SDKs/libmikmod/loaders/load_ult.c"
		"../../SDKs/libmikmod/loaders/load_umx.c"
		"../../SDKs/libmikmod/loaders/load_uni.c"
		"../../SDKs/libmikmod/loaders/load_xm.c"
		"../../SDKs/libmikmod/mmio/mmalloc.c"
		"../../SDKs/libmikmod/mmio/mmerror.c"
		"../../SDKs/libmikmod/mmio/mmio.c"
		"../../SDKs/libmikmod/depackers/mmcmp.c"
		"../../SDKs/libmikmod/depackers/pp20.c"
		"../../SDKs/libmikmod/depackers/s404.c"
		"../../SDKs/libmikmod/depackers/xpk.c"
		"../../SDKs/libmikmod/playercode/mdriver.c"
		"../../SDKs/libmikmod/playercode/mdreg.c"
		"../../SDKs/libmikmod/playercode/mloader.c"
		"../../SDKs/libmikmod/playercode/mlreg.c"
		"../../SDKs/libmikmod/playercode/mlutil.c"
		"../../SDKs/libmikmod/playercode/mplayer.c"
		"../../SDKs/libmikmod/playercode/munitrk.c"
		"../../SDKs/libmikmod/playercode/mwav.c"
		"../../SDKs/libmikmod/playercode/npertab.c"
		"../../SDKs/libmikmod/playercode/sloader.c"
		"../../SDKs/libmikmod/playercode/virtch.c"
		"../../SDKs/libmikmod/playercode/virtch2.c"
		"../../SDKs/libmikmod/playercode/virtch_common.c"
		"../../SDKs/libmikmod/posix/strcasecmp.c"
	)
end_sources()


################### libmikmod_static ###################
CryEngineStaticModule(libmikmod_static SOLUTION_FOLDER "Libs/Audio")
target_include_directories(${THIS_PROJECT} PUBLIC
	"${SDK_DIR}/libmikmod/include"
)
target_compile_definitions(${THIS_PROJECT} PUBLIC
	-DMIKMOD_STATIC
)
target_compile_definitions(${THIS_PROJECT} PRIVATE
	-DHAVE_FCNTL_H
	-DDRV_AIFF
	-DDRV_WAV
	-DDRV_RAW
)

set_property(TARGET ${THIS_PROJECT} PROPERTY OUTPUT_NAME "mikmod")
if(WIN32 OR WIN64)
	target_compile_definitions(${THIS_PROJECT} PRIVATE
		-DDRV_DS
		-DDRV_WIN
		-DHAVE_MALLOC_H
		-DHAVE_LIMITS_H
	)
	target_compile_options(${THIS_PROJECT} PRIVATE /W0)
	target_link_libraries(${THIS_PROJECT} PRIVATE Winmm)
elseif(LINUX)
	target_compile_definitions(${THIS_PROJECT} PRIVATE
		-DDRV_OSS
		-DDRV_PIPE
		-DDRV_PULSEAUDIO
		-DDRV_ALSA

		-DSTDC_HEADERS
		-DHAVE_SINF
		-DHAVE_SETENV
		-DHAVE_STDINT_H
		-DHAVE_LIMITS_H
		-DHAVE_UNISTD_H
	)
	target_compile_options(${THIS_PROJECT} PRIVATE -w)
elseif(ANDROID)
	target_compile_definitions(${THIS_PROJECT} PRIVATE
		-DDRV_OSS
		-DDRV_PIPE
		-DDRV_PULSEAUDIO
		-DDRV_ALSA

		-DSTDC_HEADERS
		-DHAVE_SINF
		-DHAVE_SETENV
		-DHAVE_STDINT_H
		-DHAVE_LIMITS_H
		-DHAVE_UNISTD_H
	)
	target_compile_options(${THIS_PROJECT} PRIVATE -w)
endif()
