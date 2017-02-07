
#options

#Renderer modules
if(NOT (ORBIS OR ANDROID))
	OPTION(RENDERER_DX11 "Renderer for DirectX 11" ON)
endif()

if (WIN32 OR WIN64)
	if (NOT MSVC_VERSION EQUAL 1900)
		message(STATUS "MSVC 14.0 is not being used - CryRenderD3D12 cannot be built.")
	else()
		OPTION(RENDERER_DX12 "Renderer for DirectX 12" ON)
	endif()
endif()

if(LINUX OR ANDROID)
	set(RENDERER_OPENGL ON)
elseif (NOT (DURANGO OR ORBIS))
	OPTION(RENDERER_OPENGL "Renderer for OpenGL" ON)
endif()

if(ORBIS)
	OPTION(RENDERER_GNM "Use GNM renderer for Orbis" ON)
	if(NOT RENDERER_GNM)
		SET(RENDERER_DX11 ON)
	endif()
endif()

#Audio modules
if (ANDROID)
	# SDL_mixer is only supported audio on these platforms
	set(AUDIO_SDL_MIXER ON)
elseif (NOT (WIN32 OR WIN64 OR LINUX))
	# Wwise is only supported audio on these platforms
	set(AUDIO_WWISE ON)
else()
	if (EXISTS ${SDK_DIR}/Audio/fmod)
		include(${CRYENGINE_DIR}/Tools/CMake/modules/fmod.cmake)
		option(AUDIO_FMOD "FMod sound system support" ON)
	endif()
	if (EXISTS ${SDK_DIR}/Audio/portaudio AND EXISTS ${SDK_DIR}/Audio/libsndfile AND (WIN32 OR WIN64))
		include(${CRYENGINE_DIR}/Tools/CMake/modules/PortAudio.cmake)
		option(AUDIO_PORTAUDIO "PortAudio sound system support" ON)
	endif()
	if(WIN32)
		option(AUDIO_SDL_MIXER "SDL_mixer sound system support" ON)
	endif()
	if (EXISTS ${SDK_DIR}/Audio/wwise)
		option(AUDIO_WWISE "Wwise Sound System support" ON)
	endif()
endif()

#Physics modules
option(PHYSICS_CRYPHYSICS "Enable" ON)
if (WIN64)
	option(PHYSICS_PHYSX "Enable" OFF)
endif()
if (PHYSICS_PHYSX)
	include(${CRYENGINE_DIR}/Tools/CMake/modules/PhysX.cmake)
endif()

#rc
if(WIN32)
	if (NOT METADATA_COMPANY)
		set(METADATA_COMPANY "Crytek GmbH")
	endif()
	set(METADATA_COMPANY ${METADATA_COMPANY} CACHE STRING "Company name for executable metadata")

	if (NOT METADATA_COPYRIGHT)
		string(TIMESTAMP year "%Y")
		set(METADATA_COPYRIGHT "(C) ${year} ${METADATA_COMPANY}")
	endif()
	set(METADATA_COPYRIGHT ${METADATA_COPYRIGHT} CACHE STRING "Copyright string for executable metadata")	

	if (NOT VERSION)
		set(VERSION "1.0.0.0")
	endif()
	set(METADATA_VERSION ${VERSION} CACHE STRING "Version number for executable metadata" FORCE)
	string(REPLACE . , METADATA_VERSION_COMMA ${METADATA_VERSION})
endif(WIN32)


if (MSVC_VERSION EQUAL 1900) # Visual Studio 2015
	set(MSVC_LIB_PREFIX vc140)
elseif (MSVC_VERSION EQUAL 1800) # Visual Studio 2013
	set(MSVC_LIB_PREFIX vc120)
elseif (MSVC_VERSION EQUAL 1700) # Visual Studio 2012
	set(MSVC_LIB_PREFIX "vc110")
else()
	set(MSVC_LIB_PREFIX "")
endif()


include_directories("${CRYENGINE_DIR}/Code/CryEngine/CryCommon")
include_directories("${CRYENGINE_DIR}/Code/Libs/yasli")
include_directories("${SDK_DIR}/yasli")
include_directories("${SDK_DIR}/boost")

if (WIN32)
	link_libraries (Ntdll)
endif (WIN32)

if (OPTION_SCALEFORMHELPER AND NOT (OPTION_ENGINE OR OPTION_SHADERCACHEGEN))
	add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CrySystem/Scaleform" "${CMAKE_BINARY_DIR}/CryEngine/CrySystem/Scaleform")
endif()

if (OPTION_ENGINE OR OPTION_SHADERCACHEGEN)
	add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CrySystem" "${CMAKE_BINARY_DIR}/CryEngine/CrySystem")
	add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CryCommon" "${CMAKE_BINARY_DIR}/CryEngine/CryCommon")
	add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/RenderDll/XRenderD3D9" "${CMAKE_BINARY_DIR}/CryEngine/RenderDll/XRenderD3D9")
endif()

#engine
if (OPTION_ENGINE)
	add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/Cry3DEngine" "${CMAKE_BINARY_DIR}/CryEngine/Cry3DEngine")
	add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CryAction" "${CMAKE_BINARY_DIR}/CryEngine/CryAction")
	add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CryAISystem" "${CMAKE_BINARY_DIR}/CryEngine/CryAISystem")
	add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CryAnimation" "${CMAKE_BINARY_DIR}/CryEngine/CryAnimation")
	add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CryAudioSystem/Common" "${CMAKE_BINARY_DIR}/CryEngine/CryAudioSystem/Common")
	add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CryAudioSystem" "${CMAKE_BINARY_DIR}/CryEngine/CryAudioSystem")
	add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CryDynamicResponseSystem" "${CMAKE_BINARY_DIR}/CryEngine/CryDynamicResponseSystem")
	add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CryEntitySystem" "${CMAKE_BINARY_DIR}/CryEngine/CryEntitySystem")
	add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CryFont" "${CMAKE_BINARY_DIR}/CryEngine/CryFont")
	add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CryInput" "${CMAKE_BINARY_DIR}/CryEngine/CryInput")
	add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CryMovie" "${CMAKE_BINARY_DIR}/CryEngine/CryMovie")
	add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CryNetwork" "${CMAKE_BINARY_DIR}/CryEngine/CryNetwork")
	add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CrySchematyc" "${CMAKE_BINARY_DIR}/CryEngine/CrySchematyc")
	add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CryScriptSystem" "${CMAKE_BINARY_DIR}/CryEngine/CryScriptSystem")

	if (WIN32)
		add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CryLiveCreate" "${CMAKE_BINARY_DIR}/CryEngine/CryLiveCreate")
	endif (WIN32)

	#physics
	if (PHYSICS_CRYPHYSICS)
		add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CryPhysics" "${CMAKE_BINARY_DIR}/CryEngine/CryPhysics")
	endif()
	if (PHYSICS_PHYSX)
		add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CryPhysicsSystem/CryPhysX" "${CMAKE_BINARY_DIR}/CryEngine/CryPhysicsSystem/CryPhysX")
	endif()

	#audio
	if (AUDIO_FMOD)
		add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CryAudioSystem/implementations/CryAudioImplFmod" "${CMAKE_BINARY_DIR}/CryEngine/CryAudioSystem/implementations/CryAudioImplFmod")
	endif (AUDIO_FMOD)
	if (AUDIO_PORTAUDIO)
		add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CryAudioSystem/implementations/CryAudioImplPortAudio" "${CMAKE_BINARY_DIR}/CryEngine/CryAudioSystem/implementations/CryAudioImplPortAudio")
	endif (AUDIO_PORTAUDIO)
	if (AUDIO_SDL_MIXER)
		add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CryAudioSystem/implementations/CryAudioImplSDLMixer" "${CMAKE_BINARY_DIR}/CryEngine/CryAudioSystem/implementations/CryAudioImplSDLMixer")
		add_subdirectory ("${CRYENGINE_DIR}/Code/Libs/flac" "${CMAKE_BINARY_DIR}/Libs/flac")
		add_subdirectory ("${CRYENGINE_DIR}/Code/Libs/libmikmod" "${CMAKE_BINARY_DIR}/Libs/libmikmod")
		add_subdirectory ("${CRYENGINE_DIR}/Code/Libs/libmodplug" "${CMAKE_BINARY_DIR}/Libs/libmodplug")
		add_subdirectory ("${CRYENGINE_DIR}/Code/Libs/libogg" "${CMAKE_BINARY_DIR}/Libs/libogg")
		add_subdirectory ("${CRYENGINE_DIR}/Code/Libs/libvorbis" "${CMAKE_BINARY_DIR}/Libs/libvorbis")
		add_subdirectory ("${CRYENGINE_DIR}/Code/Libs/SDL_mixer" "${CMAKE_BINARY_DIR}/Libs/SDL_mixer")
		add_subdirectory ("${CRYENGINE_DIR}/Code/Libs/smpeg" "${CMAKE_BINARY_DIR}/Libs/smpeg")
	endif (AUDIO_SDL_MIXER)
	if (AUDIO_WWISE)
		add_subdirectory ("${CRYENGINE_DIR}/Code/CryEngine/CryAudioSystem/implementations/CryAudioImplWwise" "${CMAKE_BINARY_DIR}/CryEngine/CryAudioSystem/implementations/CryAudioImplWwise")
	endif (AUDIO_WWISE)

	#libs
	add_subdirectory ("${CRYENGINE_DIR}/Code/Libs/bigdigits" "${CMAKE_BINARY_DIR}/Libs/bigdigits")
	if (WIN32)
		add_subdirectory ("${CRYENGINE_DIR}/Code/Libs/curl" "${CMAKE_BINARY_DIR}/Libs/curl")
	endif ()
	add_subdirectory ("${CRYENGINE_DIR}/Code/Libs/freetype" "${CMAKE_BINARY_DIR}/Libs/freetype")
	add_subdirectory ("${CRYENGINE_DIR}/Code/Libs/lua" "${CMAKE_BINARY_DIR}/Libs/lua")

	#extensions
	#add_subdirectory ("${CRYENGINE_DIR}/Code/CryExtensions/CrySchematyc" "${CMAKE_BINARY_DIR}/CryExtensions/CrySchematyc")

	#plugins
	# Mandatory plugin, contains entities required by the engine
	add_subdirectory ("${CRYENGINE_DIR}/Code/CryPlugins/CryDefaultEntities/Module" "${CMAKE_BINARY_DIR}/CryPlugins/CryDefaultEntities/Module")
endif()

if (OPTION_SCALEFORMHELPER OR OPTION_ENGINE OR OPTION_SHADERCACHEGEN)
	add_subdirectory ("${CRYENGINE_DIR}/Code/Libs/zlib" "${CMAKE_BINARY_DIR}/Libs/zlib")
	add_subdirectory ("${CRYENGINE_DIR}/Code/Libs/expat" "${CMAKE_BINARY_DIR}/Libs/expat")
endif()

if (OPTION_ENGINE OR OPTION_SHADERCACHEGEN)
	add_subdirectory ("${CRYENGINE_DIR}/Code/Libs/tomcrypt" "${CMAKE_BINARY_DIR}/Libs/tomcrypt")
	add_subdirectory ("${CRYENGINE_DIR}/Code/Libs/md5" "${CMAKE_BINARY_DIR}/Libs/md5")
	add_subdirectory ("${CRYENGINE_DIR}/Code/Libs/lz4" "${CMAKE_BINARY_DIR}/Libs/lz4")
	add_subdirectory ("${CRYENGINE_DIR}/Code/Libs/jsmn" "${CMAKE_BINARY_DIR}/Libs/jsmn")
	add_subdirectory ("${CRYENGINE_DIR}/Code/Libs/yasli" "${CMAKE_BINARY_DIR}/Libs/yasli")
	add_subdirectory ("${CRYENGINE_DIR}/Code/Libs/lzma" "${CMAKE_BINARY_DIR}/Libs/lzma")
	add_subdirectory ("${CRYENGINE_DIR}/Code/Libs/lzss" "${CMAKE_BINARY_DIR}/Libs/lzss")
endif()

