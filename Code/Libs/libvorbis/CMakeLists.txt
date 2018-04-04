start_sources()
# -- libvorbisfile.waf_files --
sources_platform(ALL)
add_sources("NoUberFile"
	PROJECTS libvorbisfile
	SOURCE_GROUP "Root"
		"../../SDKs/libvorbis/lib/vorbisfile.c"
		"../../SDKs/libvorbis/include/vorbis/vorbisfile.h"
)

# -- libvorbis.waf_files --
sources_platform(ALL)
add_sources("NoUberFile"
	PROJECTS libvorbis_static libvorbis_dynamic
	SOURCE_GROUP "Root"
		"../../SDKs/libvorbis/lib/analysis.c"
		"../../SDKs/libvorbis/lib/bitrate.c"
		"../../SDKs/libvorbis/lib/block.c"
		"../../SDKs/libvorbis/lib/codebook.c"
		"../../SDKs/libvorbis/lib/envelope.c"
		"../../SDKs/libvorbis/lib/floor0.c"
		"../../SDKs/libvorbis/lib/floor1.c"
		"../../SDKs/libvorbis/lib/info.c"
		"../../SDKs/libvorbis/lib/lookup.c"
		"../../SDKs/libvorbis/lib/lpc.c"
		"../../SDKs/libvorbis/lib/lsp.c"
		"../../SDKs/libvorbis/lib/mapping0.c"
		"../../SDKs/libvorbis/lib/mdct.c"
		"../../SDKs/libvorbis/lib/psy.c"
		"../../SDKs/libvorbis/lib/registry.c"
		"../../SDKs/libvorbis/lib/res0.c"
		"../../SDKs/libvorbis/lib/sharedbook.c"
		"../../SDKs/libvorbis/lib/smallft.c"
		"../../SDKs/libvorbis/lib/synthesis.c"
		"../../SDKs/libvorbis/lib/vorbisenc.c"
		"../../SDKs/libvorbis/lib/window.c"
		"../../SDKs/libvorbis/lib/backends.h"
		"../../SDKs/libvorbis/lib/bitrate.h"
		"../../SDKs/libvorbis/lib/codebook.h"
		"../../SDKs/libvorbis/include/vorbis/codec.h"
		"../../SDKs/libvorbis/lib/codec_internal.h"
		"../../SDKs/libvorbis/lib/envelope.h"
		"../../SDKs/libvorbis/lib/modes/floor_all.h"
		"../../SDKs/libvorbis/lib/books/floor/floor_books.h"
		"../../SDKs/libvorbis/lib/highlevel.h"
		"../../SDKs/libvorbis/lib/lookup.h"
		"../../SDKs/libvorbis/lib/lookup_data.h"
		"../../SDKs/libvorbis/lib/lpc.h"
		"../../SDKs/libvorbis/lib/lsp.h"
		"../../SDKs/libvorbis/lib/masking.h"
		"../../SDKs/libvorbis/lib/mdct.h"
		"../../SDKs/libvorbis/lib/misc.h"
		"../../SDKs/libvorbis/lib/os.h"
		"../../SDKs/libvorbis/lib/psy.h"
		"../../SDKs/libvorbis/lib/modes/psych_11.h"
		"../../SDKs/libvorbis/lib/modes/psych_16.h"
		"../../SDKs/libvorbis/lib/modes/psych_44.h"
		"../../SDKs/libvorbis/lib/modes/psych_8.h"
		"../../SDKs/libvorbis/lib/registry.h"
		"../../SDKs/libvorbis/lib/books/coupled/res_books_stereo.h"
		"../../SDKs/libvorbis/lib/books/uncoupled/res_books_uncoupled.h"
		"../../SDKs/libvorbis/lib/modes/residue_16.h"
		"../../SDKs/libvorbis/lib/modes/residue_44.h"
		"../../SDKs/libvorbis/lib/modes/residue_44u.h"
		"../../SDKs/libvorbis/lib/modes/residue_8.h"
		"../../SDKs/libvorbis/lib/scales.h"
		"../../SDKs/libvorbis/lib/modes/setup_11.h"
		"../../SDKs/libvorbis/lib/modes/setup_16.h"
		"../../SDKs/libvorbis/lib/modes/setup_22.h"
		"../../SDKs/libvorbis/lib/modes/setup_32.h"
		"../../SDKs/libvorbis/lib/modes/setup_44.h"
		"../../SDKs/libvorbis/lib/modes/setup_44u.h"
		"../../SDKs/libvorbis/lib/modes/setup_8.h"
		"../../SDKs/libvorbis/lib/modes/setup_X.h"
		"../../SDKs/libvorbis/lib/smallft.h"
		"../../SDKs/libvorbis/include/vorbis/vorbisenc.h"
		"../../SDKs/libvorbis/include/vorbis/vorbisfile.h"
		"../../SDKs/libvorbis/lib/window.h"
)
end_sources()



################# libvorbis_static ##################
CryEngineStaticModule(libvorbis_static SOLUTION_FOLDER "Libs/Audio")
target_include_directories(${THIS_PROJECT} PUBLIC "${SDK_DIR}/libvorbis/include")
target_link_libraries(${THIS_PROJECT} PRIVATE libogg_static)
set_property(TARGET ${THIS_PROJECT} PROPERTY OUTPUT_NAME "vorbis")

if(WIN32 OR WIN64)
	target_compile_options(${THIS_PROJECT} PRIVATE /W0)
elseif(LINUX OR ANDROID)
	target_compile_options(${THIS_PROJECT} PRIVATE -w)
	target_include_directories(${THIS_PROJECT} PUBLIC "${SDK_DIR}/libvorbis/lib")
endif()



################### libvorbisfile ###################
CryEngineStaticModule(libvorbisfile SOLUTION_FOLDER "Libs/Audio")
target_include_directories(${THIS_PROJECT} PUBLIC "${SDK_DIR}/libvorbis/include")
target_link_libraries(${THIS_PROJECT} PRIVATE libogg_static libvorbis_static)
set_property(TARGET ${THIS_PROJECT} PROPERTY OUTPUT_NAME "vorbisfile")

if(WIN32 OR WIN64)
	target_compile_options(${THIS_PROJECT} PRIVATE /W0)
elseif(LINUX OR ANDROID)
	target_compile_options(${THIS_PROJECT} PRIVATE -w)
	target_include_directories(${THIS_PROJECT} PUBLIC "${SDK_DIR}/libvorbis/lib")
endif()
