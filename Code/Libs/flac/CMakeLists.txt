start_sources()
sources_platform(ALL)
add_sources("NoUberFile"
	PROJECTS flac_static
	SOURCE_GROUP "Root"
		"../../SDKs/flac/src/libFLAC/bitmath.c"
		"../../SDKs/flac/src/libFLAC/bitreader.c"
		"../../SDKs/flac/src/libFLAC/bitwriter.c"
		"../../SDKs/flac/src/libFLAC/cpu.c"
		"../../SDKs/flac/src/libFLAC/crc.c"
		"../../SDKs/flac/src/libFLAC/fixed.c"
		"../../SDKs/flac/src/libFLAC/fixed_intrin_sse2.c"
		"../../SDKs/flac/src/libFLAC/fixed_intrin_ssse3.c"
		"../../SDKs/flac/src/libFLAC/float.c"
		"../../SDKs/flac/src/libFLAC/format.c"
		"../../SDKs/flac/src/libFLAC/lpc.c"
		"../../SDKs/flac/src/libFLAC/lpc_intrin_avx2.c"
		"../../SDKs/flac/src/libFLAC/lpc_intrin_sse.c"
		"../../SDKs/flac/src/libFLAC/lpc_intrin_sse2.c"
		"../../SDKs/flac/src/libFLAC/lpc_intrin_sse41.c"
		"../../SDKs/flac/src/libFLAC/md5.c"
		"../../SDKs/flac/src/libFLAC/memory.c"
		"../../SDKs/flac/src/libFLAC/metadata_iterators.c"
		"../../SDKs/flac/src/libFLAC/metadata_object.c"
		"../../SDKs/flac/src/libFLAC/ogg_decoder_aspect.c"
		"../../SDKs/flac/src/libFLAC/ogg_encoder_aspect.c"
		"../../SDKs/flac/src/libFLAC/ogg_helper.c"
		"../../SDKs/flac/src/libFLAC/ogg_mapping.c"
		"../../SDKs/flac/src/libFLAC/stream_decoder.c"
		"../../SDKs/flac/src/libFLAC/stream_encoder.c"
		"../../SDKs/flac/src/libFLAC/stream_encoder_framing.c"
		"../../SDKs/flac/src/libFLAC/stream_encoder_intrin_avx2.c"
		"../../SDKs/flac/src/libFLAC/stream_encoder_intrin_sse2.c"
		"../../SDKs/flac/src/libFLAC/stream_encoder_intrin_ssse3.c"
		"../../SDKs/flac/src/libFLAC/window.c"
)

sources_platform(WIN32 WIN64)
add_sources("NoUberFile"
	PROJECTS flac_static
	SOURCE_GROUP "Root"
		"../../SDKs/flac/src/share/win_utf8_io/win_utf8_io.c"
)
end_sources()


################### flac_static ###################
CryEngineStaticModule(flac_static SOLUTION_FOLDER "Libs/Audio")
target_include_directories(${THIS_PROJECT} PUBLIC
	"${SDK_DIR}/flac/include"
	"${SDK_DIR}/flac/src/libFLAC/include"
)
target_compile_definitions(${THIS_PROJECT} PUBLIC
	-DFLAC__ALIGN_MALLOC_DATA
	-DFLAC__HAS_OGG
)
target_compile_definitions(${THIS_PROJECT} PRIVATE
	-DVERSION="1.3.1"
)
target_link_libraries(${THIS_PROJECT} PRIVATE libogg_static)

if(WIN32 OR WIN64)
	target_compile_definitions(${THIS_PROJECT} PUBLIC -DFLAC__NO_DLL)
	target_compile_options(${THIS_PROJECT} PRIVATE /W0)
elseif(LINUX OR ANDROID)
	target_compile_definitions(${THIS_PROJECT} PRIVATE
		-DHAVE_STDINT_H
		-DHAVE_LROUND
		-DHAVE_SYS_PARAM_H
	)
	target_compile_options(${THIS_PROJECT} PRIVATE -w)
endif()
