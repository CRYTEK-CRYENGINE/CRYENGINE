start_sources()
# -- timidity.waf_files --
sources_platform(ALL)
add_sources("NoUberFile"
	PROJECTS timidity
	SOURCE_GROUP "Root"
		"../../SDKs/SDL_mixer/timidity/common.c"
		"../../SDKs/SDL_mixer/timidity/common.h"
		"../../SDKs/SDL_mixer/timidity/instrum.c"
		"../../SDKs/SDL_mixer/timidity/instrum.h"
		"../../SDKs/SDL_mixer/timidity/mix.c"
		"../../SDKs/SDL_mixer/timidity/mix.h"
		"../../SDKs/SDL_mixer/timidity/options.h"
		"../../SDKs/SDL_mixer/timidity/output.c"
		"../../SDKs/SDL_mixer/timidity/output.h"
		"../../SDKs/SDL_mixer/timidity/playmidi.c"
		"../../SDKs/SDL_mixer/timidity/playmidi.h"
		"../../SDKs/SDL_mixer/timidity/readmidi.c"
		"../../SDKs/SDL_mixer/timidity/readmidi.h"
		"../../SDKs/SDL_mixer/timidity/resample.c"
		"../../SDKs/SDL_mixer/timidity/resample.h"
		"../../SDKs/SDL_mixer/timidity/tables.c"
		"../../SDKs/SDL_mixer/timidity/tables.h"
		"../../SDKs/SDL_mixer/timidity/timidity.c"
		"../../SDKs/SDL_mixer/timidity/timidity.h"
)

# -- native_midi.waf_files --
sources_platform(ALL)
add_sources("NoUberFile"
	PROJECTS native_midi
	SOURCE_GROUP "Root"
		"../../SDKs/SDL_mixer/native_midi/native_midi.h"
		"../../SDKs/SDL_mixer/native_midi/native_midi_common.c"
		"../../SDKs/SDL_mixer/native_midi/native_midi_common.h"
		"../../SDKs/SDL_mixer/native_midi/native_midi_haiku.cpp"
		"../../SDKs/SDL_mixer/native_midi/native_midi_mac.c"
		"../../SDKs/SDL_mixer/native_midi/native_midi_macosx.c"
		"../../SDKs/SDL_mixer/native_midi/native_midi_win32.c"
)

# -- SDL_mixer.waf_files --
sources_platform(ALL)
add_sources("NoUberFile"
	PROJECTS sdl_mixer_dynamic sdl_mixer_static
	SOURCE_GROUP "Root"
		"../../SDKs/SDL_mixer/effect_position.c"
		"../../SDKs/SDL_mixer/effects_internal.c"
		"../../SDKs/SDL_mixer/effects_internal.h"
		"../../SDKs/SDL_mixer/effect_stereoreverse.c"
		"../../SDKs/SDL_mixer/load_aiff.c"
		"../../SDKs/SDL_mixer/load_aiff.h"
		"../../SDKs/SDL_mixer/load_voc.c"
		"../../SDKs/SDL_mixer/load_voc.h"
		"../../SDKs/SDL_mixer/mixer.c"
		"../../SDKs/SDL_mixer/mixer.h"
		"../../SDKs/SDL_mixer/music.c"
		"../../SDKs/SDL_mixer/music.h"
		"../../SDKs/SDL_mixer/music_cmd.c"
		"../../SDKs/SDL_mixer/music_cmd.h"
		"../../SDKs/SDL_mixer/music_flac.c"
		"../../SDKs/SDL_mixer/music_flac.h"
		"../../SDKs/SDL_mixer/music_fluidsynth.c"
		"../../SDKs/SDL_mixer/music_fluidsynth.h"
		"../../SDKs/SDL_mixer/music_mad.c"
		"../../SDKs/SDL_mixer/music_mad.h"
		"../../SDKs/SDL_mixer/music_mikmod.c"
		"../../SDKs/SDL_mixer/music_mikmod.h"
		"../../SDKs/SDL_mixer/music_modplug.c"
		"../../SDKs/SDL_mixer/music_modplug.h"
		"../../SDKs/SDL_mixer/music_mpg123.c"
		"../../SDKs/SDL_mixer/music_mpg123.h"
		"../../SDKs/SDL_mixer/music_nativemidi.c"
		"../../SDKs/SDL_mixer/music_nativemidi.h"
		"../../SDKs/SDL_mixer/music_ogg.c"
		"../../SDKs/SDL_mixer/music_ogg.h"
		"../../SDKs/SDL_mixer/music_smpeg.c"
		"../../SDKs/SDL_mixer/music_smpeg.h"
		"../../SDKs/SDL_mixer/music_timidity.c"
		"../../SDKs/SDL_mixer/music_timidity.h"
		"../../SDKs/SDL_mixer/music_wav.c"
		"../../SDKs/SDL_mixer/music_wav.h"
		"../../SDKs/SDL_mixer/playmus.c"
		"../../SDKs/SDL_mixer/playwave.c"
		"../../SDKs/SDL_mixer/SDL_mixer.h"
)

end_sources()

# A hack for Nsight Tegra to prevent c files compiling with -std-c++11 flag
unset (CMAKE_CXX_STANDARD )

################# sdl_mixer_static ##################
CryEngineStaticModule(sdl_mixer_static SOLUTION_FOLDER "Libs/Audio")
target_include_directories(${THIS_PROJECT} PUBLIC "${SDK_DIR}/SDL_mixer")
target_link_libraries(${THIS_PROJECT} PRIVATE
	SDL2
	timidity
	libogg_static
	libmikmod_static
	libvorbis_static
	libvorbisfile
)
target_compile_definitions(${THIS_PROJECT} PRIVATE
	-DMUSIC_OGG
	-DMUSIC_MOD_MIKMOD
	-DMUSIC_MOD_MODPLUG
	-DMUSIC_MID_TIMIDITY
)


if(WINDOWS)
	target_compile_definitions(${THIS_PROJECT} PRIVATE
		-DMUSIC_WAV
		-DMODPLUG_DYNAMIC="libmodplug-1.dll"
		-DMUSIC_MP3_SMPEG="smpeg2.dll"
		-DMUSIC_MID_NATIVE
		)
    target_link_libraries(${THIS_PROJECT}      PUBLIC Winmm native_midi smpeg)
	target_include_directories(${THIS_PROJECT} PUBLIC "${SDK_DIR}/SDL_mixer/VisualC/external/include")
	target_compile_options(${THIS_PROJECT}     PRIVATE /W0)
elseif(LINUX OR ANDROID)
	target_link_libraries(${THIS_PROJECT}      PRIVATE libmodplug_static)
	target_compile_options(${THIS_PROJECT}     PRIVATE -w)
endif()



##################### timidity ######################
CryEngineStaticModule(timidity SOLUTION_FOLDER "Libs/Audio")
target_include_directories(${THIS_PROJECT} PUBLIC "${SDK_DIR}/SDL_mixer/timidity")
target_link_libraries(${THIS_PROJECT} PRIVATE SDL2)

if(WINDOWS)
	target_compile_options(${THIS_PROJECT} PRIVATE /W0)
elseif(LINUX OR ANDROID)
	target_compile_options(${THIS_PROJECT} PRIVATE -w)
endif()



#################### native_midi ####################
CryEngineStaticModule(native_midi SOLUTION_FOLDER "Libs/Audio")
target_include_directories(${THIS_PROJECT} PUBLIC "${SDK_DIR}/SDL_mixer/native_midi")
target_link_libraries(${THIS_PROJECT} PRIVATE SDL2)

if(WINDOWS)
	target_compile_options(${THIS_PROJECT} PRIVATE /W0)
elseif(LINUX OR ANDROID)
	target_compile_options(${THIS_PROJECT} PRIVATE -w)
endif()

