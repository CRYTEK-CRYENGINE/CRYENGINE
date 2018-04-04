start_sources()
# -- timidity.waf_files --
sources_platform(ALL)
add_sources("NoUberFile"
	PROJECTS timidity
	SOURCE_GROUP "Root"
		"../../SDKs/SDL_mixer/timidity/common.c"
		"../../SDKs/SDL_mixer/timidity/common.h"
		"../../SDKs/SDL_mixer/timidity/config.h"
		"../../SDKs/SDL_mixer/timidity/ctrlmode.c"
		"../../SDKs/SDL_mixer/timidity/ctrlmode.h"
		"../../SDKs/SDL_mixer/timidity/filter.c"
		"../../SDKs/SDL_mixer/timidity/filter.h"
		"../../SDKs/SDL_mixer/timidity/instrum.c"
		"../../SDKs/SDL_mixer/timidity/instrum.h"
		"../../SDKs/SDL_mixer/timidity/mix.c"
		"../../SDKs/SDL_mixer/timidity/mix.h"
		"../../SDKs/SDL_mixer/timidity/output.c"
		"../../SDKs/SDL_mixer/timidity/output.h"
		"../../SDKs/SDL_mixer/timidity/playmidi.c"
		"../../SDKs/SDL_mixer/timidity/playmidi.h"
		"../../SDKs/SDL_mixer/timidity/readmidi.c"
		"../../SDKs/SDL_mixer/timidity/readmidi.h"
		"../../SDKs/SDL_mixer/timidity/resample.c"
		"../../SDKs/SDL_mixer/timidity/resample.h"
		"../../SDKs/SDL_mixer/timidity/sdl_a.c"
		"../../SDKs/SDL_mixer/timidity/sdl_c.c"
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
		"../../SDKs/SDL_mixer/native_midi/native_midi_win32.c"
)

# -- SDL_mixer.waf_files --
sources_platform(ALL)
add_sources("NoUberFile"
	PROJECTS sdl_mixer_dynamic sdl_mixer_static
	SOURCE_GROUP "Root"
		"../../SDKs/SDL_mixer/SDL_mixer.h"
		"../../SDKs/SDL_mixer/dynamic_flac.c"
		"../../SDKs/SDL_mixer/dynamic_flac.h"
		"../../SDKs/SDL_mixer/dynamic_fluidsynth.c"
		"../../SDKs/SDL_mixer/dynamic_fluidsynth.h"
		"../../SDKs/SDL_mixer/dynamic_mod.c"
		"../../SDKs/SDL_mixer/dynamic_mod.h"
		"../../SDKs/SDL_mixer/dynamic_modplug.c"
		"../../SDKs/SDL_mixer/dynamic_modplug.h"
		"../../SDKs/SDL_mixer/dynamic_mp3.c"
		"../../SDKs/SDL_mixer/dynamic_mp3.h"
		"../../SDKs/SDL_mixer/dynamic_ogg.c"
		"../../SDKs/SDL_mixer/dynamic_ogg.h"
		"../../SDKs/SDL_mixer/effect_position.c"
		"../../SDKs/SDL_mixer/effect_stereoreverse.c"
		"../../SDKs/SDL_mixer/effects_internal.c"
		"../../SDKs/SDL_mixer/effects_internal.h"
		"../../SDKs/SDL_mixer/fluidsynth.c"
		"../../SDKs/SDL_mixer/fluidsynth.h"
		"../../SDKs/SDL_mixer/load_aiff.c"
		"../../SDKs/SDL_mixer/load_aiff.h"
		"../../SDKs/SDL_mixer/load_flac.c"
		"../../SDKs/SDL_mixer/load_flac.h"
		"../../SDKs/SDL_mixer/load_mp3.c"
		"../../SDKs/SDL_mixer/load_mp3.h"
		"../../SDKs/SDL_mixer/load_ogg.c"
		"../../SDKs/SDL_mixer/load_ogg.h"
		"../../SDKs/SDL_mixer/load_voc.c"
		"../../SDKs/SDL_mixer/load_voc.h"
		"../../SDKs/SDL_mixer/mixer.c"
		"../../SDKs/SDL_mixer/music.c"
		"../../SDKs/SDL_mixer/music_cmd.c"
		"../../SDKs/SDL_mixer/music_cmd.h"
		"../../SDKs/SDL_mixer/music_flac.c"
		"../../SDKs/SDL_mixer/music_flac.h"
		"../../SDKs/SDL_mixer/music_mad.c"
		"../../SDKs/SDL_mixer/music_mad.h"
		"../../SDKs/SDL_mixer/music_mod.c"
		"../../SDKs/SDL_mixer/music_mod.h"
		"../../SDKs/SDL_mixer/music_modplug.c"
		"../../SDKs/SDL_mixer/music_modplug.h"
		"../../SDKs/SDL_mixer/music_ogg.c"
		"../../SDKs/SDL_mixer/music_ogg.h"
		"../../SDKs/SDL_mixer/wavestream.c"
		"../../SDKs/SDL_mixer/wavestream.h"
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
	flac_static
	libogg_static
	libmikmod_static
	libvorbis_static
	libvorbisfile
)
target_compile_definitions(${THIS_PROJECT} PRIVATE
	-DOGG_MUSIC
	-DMID_MUSIC
	-DMOD_MUSIC
	-DFLAC_MUSIC
	-DMODPLUG_MUSIC
	-DUSE_TIMIDITY_MIDI
)

if(WIN32 OR WIN64)
	target_compile_definitions(${THIS_PROJECT} PRIVATE
		-DWAV_MUSIC
		-DMP3_MUSIC
		-DMODPLUG_DYNAMIC="libmodplug-1.dll"
		-DMP3_DYNAMIC="smpeg2.dll"
		-DUSE_NATIVE_MIDI
	)
	target_link_libraries(${THIS_PROJECT} PUBLIC Winmm native_midi)
	target_include_directories(${THIS_PROJECT} PUBLIC "${SDK_DIR}/SDL_mixer/VisualC/external/include")
	target_compile_options(${THIS_PROJECT} PRIVATE /W0)
elseif(LINUX OR ANDROID)
	target_link_libraries(${THIS_PROJECT} PRIVATE libmodplug_static)
	target_compile_options(${THIS_PROJECT} PRIVATE -w)
endif()



##################### timidity ######################
CryEngineStaticModule(timidity SOLUTION_FOLDER "Libs/Audio")
target_include_directories(${THIS_PROJECT} PUBLIC "${SDK_DIR}/SDL_mixer/timidity")
target_link_libraries(${THIS_PROJECT} PRIVATE SDL2)

if(WIN32 OR WIN64)
	target_compile_options(${THIS_PROJECT} PRIVATE /W0)
elseif(LINUX OR ANDROID)
	target_compile_options(${THIS_PROJECT} PRIVATE -w)
endif()



#################### native_midi ####################
CryEngineStaticModule(native_midi SOLUTION_FOLDER "Libs/Audio")
target_include_directories(${THIS_PROJECT} PUBLIC "${SDK_DIR}/SDL_mixer/native_midi")
target_link_libraries(${THIS_PROJECT} PRIVATE SDL2)

if(WIN32 OR WIN64)
	target_compile_options(${THIS_PROJECT} PRIVATE /W0)
elseif(LINUX OR ANDROID)
	target_compile_options(${THIS_PROJECT} PRIVATE -w)
endif()

