
#options

option(PLUGIN_SCHEMATYC "Enables compilation of the Schematyc plugin" ON)

option(OPTION_PAKTOOLS "Build .pak encryption tools" OFF)

if(WIN64)
	option(OPTION_RC "Include RC in the build" ON)
endif()

option(OPTION_DOXYGEN_EXAMPLES "Build Doxygen examples with the engine" OFF)

if (WIN32 OR WIN64)
	option(OPTION_ENABLE_CRASHRPT "Enable CrashRpt crash reporting library" ON)
endif()

if(NOT ANDROID AND NOT ORBIS)
	option(OPTION_SCALEFORMHELPER "Use Scaleform Helper" ON)
else()
	set(OPTION_SCALEFORMHELPER ON)
endif()

if(OPTION_DEDICATED_SERVER)
	set(OPTION_SCALEFORMHELPER OFF)
endif()

option(OPTION_DEVELOPER_CONSOLE_IN_RELEASE "Enables the developer console in Release builds" ON)

#Plugins
option(PLUGIN_FPSPLUGIN "Frames per second sample plugin" OFF)
if(WIN32 OR WIN64)
	option(PLUGIN_USERANALYTICS "Enable User Analytics" ON)
	
	if(EXISTS "${SDK_DIR}/OculusSDK")
		option(PLUGIN_VR_OCULUS "Oculus support" ON)
	else()
		option(PLUGIN_VR_OCULUS "Oculus support" OFF)
	endif()

	if(EXISTS "${SDK_DIR}/OSVR")
		option(PLUGIN_VR_OSVR "OSVR support" ON)
	else()
		option(PLUGIN_VR_OSVR "OSVR support" OFF)
	endif()

	if(EXISTS "${SDK_DIR}/OpenVR")
		option(PLUGIN_VR_OPENVR "OpenVR support" ON)
	else()
		option(PLUGIN_VR_OPENVR "OpenVR support" OFF)
	endif()

	option(OPTION_CRYMONO "C# support" OFF)
	
	if (OPTION_CRYMONO)
		option(OPTION_CRYMONO_SWIG "Expose C++ API to C# with SWIG" ON)
	endif()
	
	option(OPTION_PHYSDBGR "Include standalone physics debugger in the build" OFF)
endif()

option(OPTION_UNSIGNED_PAKS_IN_RELEASE "Allow unsigned PAK files to be used for release builds" ON)

if(WIN64 AND EXISTS "${CRYENGINE_DIR}/Code/Sandbox/EditorQt")
	option(OPTION_SANDBOX "Enable Sandbox" ON)
	if (EXISTS "${SDK_DIR}/SubstanceEngines")
		option(OPTION_SANDBOX_SUBSTANCE "Enable Sandbox Substance Integration" ON)
	endif()
	if(NOT OPTION_SANDBOX AND OPTION_SANDBOX_SUBSTANCE)
		set(OPTION_SANDBOX_SUBSTANCE OFF)
	endif()
endif()

if(WIN32 OR WIN64)
	option(OPTION_ENABLE_BROFILER "Enable Brofiler profiler support" ON)
endif()

#Renderer modules
if(NOT (ORBIS OR ANDROID OR LINUX))
	option(RENDERER_DX11 "Renderer for DirectX 11" ON)
endif()

if (WIN32 OR WIN64)
	if (MSVC_VERSION LESS 1900)
		message(STATUS "MSVC 14.0 or above is required to build CryRenderD3D12.")
	else()
		option(RENDERER_DX12 "Renderer for DirectX 12" ON)
	endif()
endif()

# Disable opengl renderer for now
set(RENDERER_OPENGL OFF)

if(ORBIS)
	option(RENDERER_GNM "Use GNM renderer for Orbis" ON)
	if(NOT RENDERER_GNM)
		set(RENDERER_DX11 ON)
	endif()
endif()

if(WIN64)
	option(RENDERER_VULKAN "Renderer for Vulkan API" ON)
	if (RENDERER_VULKAN AND NOT EXISTS "${SDK_DIR}/VulkanSDK")
		message(STATUS "Vulkan SDK not found in ${SDK_DIR}/VulkanSDK - disabling Vulkan renderer.")
		
		# Disables the RENDERER_VULKAN option but also updates the message in the cache that is then used in the GUI as a tooltip.
		set(RENDERER_VULKAN OFF CACHE BOOL "Disabled Vulkan renderer due to absent Vulkan SDK. Must reside in ${SDK_DIR}/VulkanSDK" FORCE)
	endif()
endif()

# Audio
function(try_to_enable_fmod)
if (NOT ORBIS AND NOT DURANGO)
	if (DEFINED AUDIO_FMOD)
		if (AUDIO_FMOD)
			if (EXISTS "${SDK_DIR}/Audio/fmod")
				message(STATUS "Fmod SDK found in ${SDK_DIR}/Audio/fmod - enabling Fmod support.")
				
				# This is to update only the message in the cache that is then used in the GUI as a tooltip.
				option(AUDIO_FMOD "Fmod SDK found in ${SDK_DIR}/Audio/fmod." ON)
				
				include(${TOOLS_CMAKE_DIR}/modules/fmod.cmake)
			else()
				message(STATUS "Fmod SDK not found in ${SDK_DIR}/Audio/fmod - disabling Fmod support.")
				
				# Disables the AUDIO_FMOD option but also updates the message in the cache that is then used in the GUI as a tooltip.
				option(AUDIO_FMOD "Fmod SDK not found in ${SDK_DIR}/Audio/fmod." OFF)
			endif()
		else()
			if (EXISTS "${SDK_DIR}/Audio/fmod")
				message(STATUS "Fmod SDK found in ${SDK_DIR}/Audio/fmod but AUDIO_FMOD option turned OFF")
				
				# This is to update only the message in the cache that is then used in the GUI as a tooltip.
				option(AUDIO_FMOD "Fmod SDK found in ${SDK_DIR}/Audio/fmod but AUDIO_FMOD option turned OFF." OFF)
			else()
				message(STATUS "Fmod SDK not found in ${SDK_DIR}/Audio/fmod and AUDIO_FMOD option turned OFF")
				
				# This is to update only the message in the cache that is then used in the GUI as a tooltip.
				option(AUDIO_FMOD "Fmod SDK not found in ${SDK_DIR}/Audio/fmod and AUDIO_FMOD option turned OFF." OFF)
			endif()
		endif()
	else()
		# If this option is not in the cache yet, set it depending on whether the SDK is present or not.
		if (EXISTS "${SDK_DIR}/Audio/fmod")
			message(STATUS "Fmod SDK found in ${SDK_DIR}/Audio/fmod - enabling Fmod support.")
			set(AUDIO_FMOD ON CACHE BOOL "Fmod SDK found in ${SDK_DIR}/Audio/fmod." FORCE)
			include(${TOOLS_CMAKE_DIR}/modules/fmod.cmake)
		else()
			message(STATUS "Fmod SDK not found in ${SDK_DIR}/Audio/fmod - disabling Fmod support.")
			set(AUDIO_FMOD OFF CACHE BOOL "Fmod SDK not found in ${SDK_DIR}/Audio/fmod." FORCE)
		endif()
	endif()
else()
	message(STATUS "Disabling Fmod support due to unsupported platform.")
	set(AUDIO_FMOD OFF CACHE BOOL "Fmod disabled due to unsupported platform." FORCE)
endif()
endfunction(try_to_enable_fmod)

function(try_to_enable_portaudio)
if (WIN32 OR WIN64)
	if (DEFINED AUDIO_PORTAUDIO)
		if (AUDIO_PORTAUDIO)
			if (EXISTS "${SDK_DIR}/Audio/portaudio" AND EXISTS "${SDK_DIR}/Audio/libsndfile")
				message(STATUS "PortAudio and libsndfile SDKs found in ${SDK_DIR}/Audio/portaudio and ${SDK_DIR}/Audio/libsndfile - enabling PortAudio support.")
				
				# This is to update only the message in the cache that is then used in the GUI as a tooltip.
				option(AUDIO_PORTAUDIO "PortAudio and libsndfile SDKs found in ${SDK_DIR}/Audio/portaudio and ${SDK_DIR}/Audio/libsndfile." ON)
				
				include(${TOOLS_CMAKE_DIR}/modules/PortAudio.cmake)
			else()
				message(STATUS "PortAudio or libsndfile SDK not found in ${SDK_DIR}/Audio/portaudio and ${SDK_DIR}/Audio/libsndfile - disabling PortAudio support.")
				
				# Disables the AUDIO_PORTAUDIO option but also updates the message in the cache that is then used in the GUI as a tooltip.
				option(AUDIO_PORTAUDIO "PortAudio or libsndfile SDK not found in ${SDK_DIR}/Audio/portaudio and ${SDK_DIR}/Audio/libsndfile." OFF)
			endif()
		else()
			if (EXISTS "${SDK_DIR}/Audio/portaudio" AND EXISTS "${SDK_DIR}/Audio/libsndfile")
				message(STATUS "PortAudio and libsndfile SDKs found in ${SDK_DIR}/Audio/portaudio and ${SDK_DIR}/Audio/libsndfile but AUDIO_PORTAUDIO option turned OFF")
				
				# This is to update only the message in the cache that is then used in the GUI as a tooltip.
				option(AUDIO_PORTAUDIO "PortAudio and libsndfile SDKs found in ${SDK_DIR}/Audio/portaudio and ${SDK_DIR}/Audio/libsndfile but AUDIO_PORTAUDIO option turned OFF." OFF)
			else()
				message(STATUS "PortAudio or libsndfile SDK not found in ${SDK_DIR}/Audio/portaudio and ${SDK_DIR}/Audio/libsndfile and AUDIO_PORTAUDIO option turned OFF")
				
				# This is to update only the message in the cache that is then used in the GUI as a tooltip.
				option(AUDIO_PORTAUDIO "PortAudio or libsndfile SDK not found in ${SDK_DIR}/Audio/portaudio and ${SDK_DIR}/Audio/libsndfile and AUDIO_PORTAUDIO option turned OFF." OFF)
			endif()
		endif()
	else()
		# If this option is not in the cache yet, set it depending on whether the SDK is present or not.
		if (EXISTS "${SDK_DIR}/Audio/portaudio" AND EXISTS "${SDK_DIR}/Audio/libsndfile")
			message(STATUS "PortAudio and libsndfile SDKs found in ${SDK_DIR}/Audio/portaudio and ${SDK_DIR}/Audio/libsndfile - enabling PortAudio support.")
			set(AUDIO_PORTAUDIO ON CACHE BOOL "PortAudio and libsndfile SDKs found in ${SDK_DIR}/Audio/portaudio and ${SDK_DIR}/Audio/libsndfile." FORCE)
			include(${TOOLS_CMAKE_DIR}/modules/PortAudio.cmake)
		else()
			message(STATUS "PortAudio or libsndfile SDK not found in ${SDK_DIR}/Audio/portaudio and ${SDK_DIR}/Audio/libsndfile - disabling PortAudio support.")
			set(AUDIO_PORTAUDIO OFF CACHE BOOL "PortAudio or libsndfile SDK not found in ${SDK_DIR}/Audio/portaudio and ${SDK_DIR}/Audio/libsndfile." FORCE)
		endif()
	endif()
else()
	message(STATUS "Disabling PortAudio support due to unsupported platform.")
	set(AUDIO_PORTAUDIO OFF CACHE BOOL "PortAudio disabled due to unsupported platform." FORCE)
endif()
endfunction(try_to_enable_portaudio)

function(try_to_enable_sdl_mixer)
if (ANDROID OR WIN32 OR WIN64)
	# We build SDL_mixer ourselves for these platforms.
	option(AUDIO_SDL_MIXER "SDL_mixer support" ON)
	
	if (AUDIO_SDL_MIXER)
		# Supported platforms and the user enabled the AUDIO_SDL_MIXER option.
		message(STATUS "Enabling SDL_mixer support.")
	else()
		# Supported platforms but the user disabled the AUDIO_SDL_MIXER option.
		message(STATUS "SDL_mixer support manually disabled.")
	endif()
else()
	message(STATUS "Disabling SDL_mixer support due to unsupported platform.")
	set(AUDIO_SDL_MIXER OFF CACHE BOOL "SDL_mixer disabled due to unsupported platform." FORCE)
endif()
endfunction(try_to_enable_sdl_mixer)

function(try_to_enable_wwise)
if (DEFINED AUDIO_WWISE)
	if (AUDIO_WWISE)
		if (EXISTS "${SDK_DIR}/Audio/wwise")
			message(STATUS "Wwise SDK found in ${SDK_DIR}/Audio/wwise - enabling Wwise support.")
			
			# This is to update only the message in the cache that is then used in the GUI as a tooltip.
			option(AUDIO_WWISE "Wwise SDK found in ${SDK_DIR}/Audio/wwise." ON)
		else()
			message(STATUS "Wwise SDK not found in ${SDK_DIR}/Audio/wwise - disabling Wwise support.")
			
			# Disables the AUDIO_WWISE option but also updates the message in the cache that is then used in the GUI as a tooltip.
			option(AUDIO_WWISE "Wwise SDK not found in ${SDK_DIR}/Audio/wwise." OFF)
		endif()
	else()
		if (EXISTS "${SDK_DIR}/Audio/wwise")
			message(STATUS "Wwise SDK found in ${SDK_DIR}/Audio/wwise but AUDIO_WWISE option turned OFF")
			
			# This is to update only the message in the cache that is then used in the GUI as a tooltip.
			option(AUDIO_WWISE "Wwise SDK found in ${SDK_DIR}/Audio/wwise but AUDIO_WWISE option turned OFF." OFF)
		else()
			message(STATUS "Wwise SDK not found in ${SDK_DIR}/Audio/wwise and AUDIO_WWISE option turned OFF")
			
			# This is to update only the message in the cache that is then used in the GUI as a tooltip.
			option(AUDIO_WWISE "Wwise SDK not found in ${SDK_DIR}/Audio/wwise and AUDIO_WWISE option turned OFF." OFF)
		endif()
	endif()
else()
	# If this option is not in the cache yet, set it depending on whether the SDK is present or not.
	if (EXISTS "${SDK_DIR}/Audio/wwise")
		message(STATUS "Wwise SDK found in ${SDK_DIR}/Audio/wwise - enabling Wwise support.")
		set(AUDIO_WWISE ON CACHE BOOL "Wwise SDK found in ${SDK_DIR}/Audio/wwise." FORCE)
	else()
		message(STATUS "Wwise SDK not found in ${SDK_DIR}/Audio/wwise - disabling Wwise support.")
		set(AUDIO_WWISE OFF CACHE BOOL "Wwise SDK not found in ${SDK_DIR}/Audio/wwise." FORCE)
	endif()
endif()
endfunction(try_to_enable_wwise)

function(try_to_enable_oculus_hrtf)
if(WIN32 OR WIN64)
	if (DEFINED AUDIO_OCULUS_HRTF)
		if (AUDIO_OCULUS_HRTF)
			if (EXISTS "${SDK_DIR}/Audio/oculus")
				message(STATUS "Oculus audio SDK found in ${SDK_DIR}/Audio/oculus - enabling Oculus HRTF support.")
				
				# This is to update only the message in the cache that is then used in the GUI as a tooltip.
				option(AUDIO_OCULUS_HRTF "Oculus audio SDK found in ${SDK_DIR}/Audio/oculus." ON)
			else()
				message(STATUS "Oculus audio SDK not found in ${SDK_DIR}/Audio/oculus - disabling Oculus HRTF support.")
				
				# Disables the AUDIO_OCULUS_HRTF option but also updates the message in the cache that is then used in the GUI as a tooltip.
				option(AUDIO_OCULUS_HRTF "Oculus audio SDK not found in ${SDK_DIR}/Audio/oculus." OFF)
			endif()
		else()
			if (EXISTS "${SDK_DIR}/Audio/oculus")
				message(STATUS "Oculus audio SDK found in ${SDK_DIR}/Audio/oculus but AUDIO_OCULUS_HRTF option turned OFF")
				
				# This is to update only the message in the cache that is then used in the GUI as a tooltip.
				option(AUDIO_OCULUS_HRTF "Oculus audio SDK found in ${SDK_DIR}/Audio/oculus but AUDIO_OCULUS_HRTF option turned OFF." OFF)
			else()
				message(STATUS "Oculus audio SDK not found in ${SDK_DIR}/Audio/oculus and AUDIO_OCULUS_HRTF option turned OFF")
				
				# This is to update only the message in the cache that is then used in the GUI as a tooltip.
				option(AUDIO_OCULUS_HRTF "Oculus audio SDK not found in ${SDK_DIR}/Audio/oculus and AUDIO_OCULUS_HRTF option turned OFF." OFF)
			endif()
		endif()
	else()
		# If this option is not in the cache yet, set it depending on whether the SDK is present or not.
		if (EXISTS "${SDK_DIR}/Audio/oculus")
			message(STATUS "Oculus audio SDK found in ${SDK_DIR}/Audio/oculus - enabling Oculus HRTF support.")
			set(AUDIO_OCULUS_HRTF ON CACHE BOOL "Oculus audio SDK found in ${SDK_DIR}/Audio/oculus." FORCE)
		else()
			message(STATUS "Oculus audio SDK not found in ${SDK_DIR}/Audio/oculus - disabling Oculus HRTF support.")
			set(AUDIO_OCULUS_HRTF OFF CACHE BOOL "Oculus audio SDK not found in ${SDK_DIR}/Audio/oculus." FORCE)
		endif()
	endif()
else()
	message(STATUS "Disabling Oculus HRTF support due to unsupported platform.")
	set(AUDIO_OCULUS_HRTF OFF CACHE BOOL "Oculus HRTF disabled due to unsupported platform." FORCE)
endif()
endfunction(try_to_enable_oculus_hrtf)

try_to_enable_fmod()
try_to_enable_portaudio()
try_to_enable_sdl_mixer()
try_to_enable_wwise()
try_to_enable_oculus_hrtf()
# ~Audio

#Physics modules
option(PHYSICS_CRYPHYSICS "Enable" ON)
if (WIN64)
	option(PHYSICS_PHYSX "Enable" OFF)
endif()
if (PHYSICS_PHYSX)
	include("${TOOLS_CMAKE_DIR}/modules/PhysX.cmake")
endif()

include_directories("${CRYENGINE_DIR}/Code/CryEngine/CryCommon")
include_directories("${CRYENGINE_DIR}/Code/CryEngine/CryCommon/3rdParty")
include_directories("${SDK_DIR}/boost")
include_directories("${CRYENGINE_DIR}/Code/CryEngine/CrySchematyc/Core/Interface")

if (WIN32)
	link_libraries (Ntdll)
endif (WIN32)

if (OPTION_SCALEFORMHELPER AND NOT (OPTION_ENGINE OR OPTION_SHADERCACHEGEN))
	add_subdirectory ("Code/CryEngine/CrySystem/Scaleform")
endif()

if (OPTION_ENGINE OR OPTION_SHADERCACHEGEN OR OPTION_DOXYGEN_EXAMPLES)
	add_subdirectory ("Code/CryEngine/CryCommon")
endif()
	
if (OPTION_ENGINE OR OPTION_SHADERCACHEGEN)
	add_subdirectory ("Code/CryEngine/CrySystem")
	add_subdirectory ("Code/CryEngine/CryReflection")
	add_subdirectory ("Code/CryEngine/RenderDll/XRenderD3D9")
	
	# Shaders custom project
	if(EXISTS "${CRYENGINE_DIR}/Engine/Shaders")
		add_subdirectory(Engine/Shaders)
	endif()
endif()

#engine
if (OPTION_ENGINE)
	add_subdirectory ("Code/CryEngine/Cry3DEngine")
	add_subdirectory ("Code/CryEngine/CryAction")
	add_subdirectory ("Code/CryEngine/CryAISystem")
	add_subdirectory ("Code/CryEngine/CryAnimation")
	add_subdirectory ("Code/CryEngine/CryAudioSystem/Common")
	add_subdirectory ("Code/CryEngine/CryAudioSystem")
	add_subdirectory ("Code/CryEngine/CryDynamicResponseSystem")
	add_subdirectory ("Code/CryEngine/CryEntitySystem")
	add_subdirectory ("Code/CryEngine/CryFont")
	add_subdirectory ("Code/CryEngine/CryInput")
	add_subdirectory ("Code/CryEngine/CryMovie")
	add_subdirectory ("Code/CryEngine/CryNetwork")
	#add_subdirectory ("Code/CryEngine/CryReflection")
	add_subdirectory ("Code/CryEngine/CrySchematyc")
	add_subdirectory ("Code/CryEngine/CrySchematyc2")
	add_subdirectory ("Code/CryEngine/CryScriptSystem")
	add_subdirectory ("Code/CryEngine/CryFlowGraph")

	if (WIN32)
		add_subdirectory ("Code/CryEngine/CryLiveCreate")
	endif (WIN32)

	#physics
	if (PHYSICS_CRYPHYSICS)
		add_subdirectory ("Code/CryEngine/CryPhysics")
	endif()
	if (PHYSICS_PHYSX)
		add_subdirectory ("Code/CryEngine/CryPhysicsSystem/CryPhysX")
	endif()

	#audio
	if (AUDIO_FMOD)
		add_subdirectory ("Code/CryEngine/CryAudioSystem/implementations/CryAudioImplFmod")
	endif (AUDIO_FMOD)
	if (AUDIO_PORTAUDIO)
		add_subdirectory ("Code/CryEngine/CryAudioSystem/implementations/CryAudioImplPortAudio")
	endif (AUDIO_PORTAUDIO)
	if (AUDIO_SDL_MIXER)
		add_subdirectory ("Code/CryEngine/CryAudioSystem/implementations/CryAudioImplSDLMixer")
		add_subdirectory ("Code/Libs/flac")
		add_subdirectory ("Code/Libs/libmikmod")
		add_subdirectory ("Code/Libs/libmodplug")
		add_subdirectory ("Code/Libs/libogg")
		add_subdirectory ("Code/Libs/libvorbis")
		add_subdirectory ("Code/Libs/SDL_mixer")
		add_subdirectory ("Code/Libs/smpeg")
	endif (AUDIO_SDL_MIXER)
	if (AUDIO_WWISE)
		add_subdirectory ("Code/CryEngine/CryAudioSystem/implementations/CryAudioImplWwise")
	endif (AUDIO_WWISE)

	#libs
	add_subdirectory ("Code/Libs/bigdigits")
	
	if(PLUGIN_VR_OCULUS)
		add_subdirectory("Code/Libs/oculus")
	endif()
	
	if (WIN32)
		add_subdirectory ("Code/Libs/curl")
	endif ()
	add_subdirectory ("Code/Libs/freetype")
	add_subdirectory ("Code/Libs/lua")
endif()

if (OPTION_SCALEFORMHELPER OR OPTION_ENGINE OR OPTION_SHADERCACHEGEN)
	add_subdirectory ("Code/Libs/zlib")
	add_subdirectory ("Code/Libs/expat")
	if (WIN32 OR WIN64)
		add_subdirectory ("Code/Libs/png16")
	endif ()
endif()

if (OPTION_ENGINE OR OPTION_SHADERCACHEGEN OR OPTION_PAKTOOLS)
	add_subdirectory ("Code/Libs/tomcrypt")
endif()

if (OPTION_ENGINE OR OPTION_SHADERCACHEGEN)
	add_subdirectory ("Code/Libs/md5")
	add_subdirectory ("Code/Libs/lz4")
	add_subdirectory ("Code/Libs/jsmn")
	add_subdirectory ("Code/Libs/lzma")
	add_subdirectory ("Code/Libs/lzss")
	add_subdirectory ("Code/Libs/tiff")
endif()
