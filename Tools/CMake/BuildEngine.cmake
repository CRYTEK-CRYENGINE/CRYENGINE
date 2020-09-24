
#options

option(PLUGIN_SCHEMATYC "Enables compilation of the Schematyc plugin (currently Schematyc2.dll)" OFF)
option(PLUGIN_SCHEMATYC_EXPERIMENTAL "Enables compilation of the Experimental Schematyc plugin (Schematyc.dll)" ON)

option(OPTION_PAKTOOLS "Build .pak encryption tools" OFF)
option(OPTION_RC "Include RC in the build" ${WINDOWS})

option(OPTION_DOXYGEN_EXAMPLES "Build Doxygen examples with the engine" OFF)

if (WINDOWS)
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

#The remote console is useful in development, but it is a potential security vulnerability, therefore opt-in
option(OPTION_REMOTE_CONSOLE "Allows remote console connection" OFF)

if(NOT ANDROID) # Unit test is disabled for Android until fully supported
	if(EXISTS "${SDK_DIR}/googletest_CE_Support")
		option(OPTION_UNIT_TEST "Unit Tests" ON)
	elseif(OPTION_UNIT_TEST)
		message(STATUS "Google Test not found in ${SDK_DIR}/googletest_CE_Support - disabling unit tests.")

		# Disables the OPTION_UNIT_TEST option but also updates the message in the cache that is then used in the GUI as a tooltip.
		set(OPTION_UNIT_TEST OFF CACHE BOOL "OPTION_UNIT_TEST was previously set but Google Test is not available. Check bootstrap settings." FORCE)
	endif()
endif()

#Plugins
if(WINDOWS)

	# Oculus SDK 1.40.0 does not compile starting with Visual Studio 2019 16.3.0 (vc142)
	if (MSVC_VERSION LESS 1923)
		if(EXISTS "${SDK_DIR}/OculusSDK")
			option(PLUGIN_VR_OCULUS "Oculus support" ON)
		else()
			option(PLUGIN_VR_OCULUS "Oculus support" OFF)
		endif()
	else()
		message(WARNING "Disabling Oculus VR Plugin because Oculus SDK 1.40.0 does not compile starting with Visual Studio 2019 16.3.0 (vc142)")
		unset(PLUGIN_VR_OCULUS CACHE)
	endif()

	option(OPTION_CRYMONO "C# support" OFF)

	if (OPTION_CRYMONO)
		option(OPTION_CRYMONO_SWIG "Expose C++ API to C# with SWIG" ON)
	endif()
	if(EXISTS "${SDK_DIR}/OSVR")
		option(PLUGIN_VR_OSVR "OSVR support" ON)
	else()
		option(PLUGIN_VR_OSVR "OSVR support" OFF)
	endif()

	option(OPTION_PHYSDBGR "Include standalone physics debugger in the build" OFF)
endif()


if(WINDOWS)
	option(PLUGIN_VR_EMULATOR "VR emulation support" ON)
endif()


if(WINDOWS OR LINUX)
	if(EXISTS "${SDK_DIR}/OpenVR")
		option(PLUGIN_VR_OPENVR "OpenVR support" ON)
	else()
		option(PLUGIN_VR_OPENVR "OpenVR support" OFF)
	endif()
endif()

option(OPTION_UNSIGNED_PAKS_IN_RELEASE "Allow unsigned PAK files to be used for release builds" ON)

if(WINDOWS AND EXISTS "${CRYENGINE_DIR}/Code/Sandbox/EditorQt")
	option(OPTION_SANDBOX "Enable Sandbox" ON)
	if (EXISTS "${SDK_DIR}/SubstanceEngines")
		option(OPTION_SANDBOX_SUBSTANCE "Enable Sandbox Substance Integration" ON)
	endif()
endif()
if(NOT OPTION_SANDBOX OR NOT EXISTS "${SDK_DIR}/SubstanceEngines")
	unset(OPTION_SANDBOX_SUBSTANCE CACHE)
endif()

if(WINDOWS)
	option(OPTION_ENABLE_BROFILER "Enable Brofiler profiler support" ON)
endif()

#Renderer modules
if(NOT (ORBIS OR ANDROID OR LINUX))
	option(RENDERER_DX11 "Renderer for DirectX 11" ON)
endif()

if (WINDOWS)
	if (MSVC_VERSION LESS 1900)
		message(STATUS "MSVC 14.0 or above is required to build CryRenderD3D12.")
	else()
		option(RENDERER_DX12 "Renderer for DirectX 12" ON)
	endif()
endif()

if(ORBIS)
	set(RENDERER_GNM ON) # TODO: remove after cleanup
	option(GNM_VALIDATION "Enable GNM validation" ON)
endif()

if(WINDOWS OR LINUX OR ANDROID)
	option(RENDERER_VULKAN "Renderer for Vulkan API" ON)
	if (RENDERER_VULKAN AND NOT EXISTS "${SDK_DIR}/VulkanSDK")
		message(STATUS "Vulkan SDK not found in ${SDK_DIR}/VulkanSDK - disabling Vulkan renderer.")

		# Disables the RENDERER_VULKAN option but also updates the message in the cache that is then used in the GUI as a tooltip.
		set(RENDERER_VULKAN OFF CACHE BOOL "Disabled Vulkan renderer due to absent Vulkan SDK. Must reside in ${SDK_DIR}/VulkanSDK" FORCE)
	endif()
endif()

# Audio
# Occlusion is enabled by default
option(AUDIO_USE_OCCLUSION "Enable" ON)

function(try_to_enable_fmod)
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
endfunction(try_to_enable_fmod)

function(try_to_enable_portaudio)
if (WINDOWS)
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
if (LINUX OR ANDROID OR WINDOWS)
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

function(try_to_enable_dynamic_response_system)
if (DEFINED PLUGIN_DRS)
	option(PLUGIN_DRS "Dynamic Response System" ON)
	
	if (PLUGIN_DRS)
		message(STATUS "Enabling Dynamic Response System.")
	else()
		message(STATUS "Dynamic Response System manually disabled.")
	endif()
else()
	message(STATUS "Dynamic Response System not in CMake Cache, forcing default ON.")
	set(PLUGIN_DRS ON CACHE BOOL "Enabling Dynamic Response System." FORCE)
endif()
endfunction(try_to_enable_dynamic_response_system)

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

function(try_to_enable_adx2)
if (WINDOWS)
	if (DEFINED AUDIO_ADX2)
		if (AUDIO_ADX2)
			if (EXISTS "${SDK_DIR}/Audio/adx2")
				message(STATUS "ADX2 SDK found in ${SDK_DIR}/Audio/adx2 - enabling ADX2 support.")
				
				# This is to update only the message in the cache that is then used in the GUI as a tooltip.
				option(AUDIO_ADX2 "ADX2 SDK found in ${SDK_DIR}/Audio/adx2." ON)
			else()
				message(STATUS "ADX2 SDK not found in ${SDK_DIR}/Audio/adx2 - disabling ADX2 support.")
				
				# Disables the AUDIO_ADX2 option but also updates the message in the cache that is then used in the GUI as a tooltip.
				option(AUDIO_ADX2 "ADX2 SDK not found in ${SDK_DIR}/Audio/adx2." OFF)
			endif()
		else()
			if (EXISTS "${SDK_DIR}/Audio/adx2")
				message(STATUS "ADX2 SDK found in ${SDK_DIR}/Audio/adx2 but AUDIO_ADX2 option turned OFF")
				
				# This is to update only the message in the cache that is then used in the GUI as a tooltip.
				option(AUDIO_ADX2 "ADX2 SDK found in ${SDK_DIR}/Audio/adx2 but AUDIO_ADX2 option turned OFF." OFF)
			else()
				message(STATUS "ADX2 SDK not found in ${SDK_DIR}/Audio/adx2 and AUDIO_ADX2 option turned OFF")
				
				# This is to update only the message in the cache that is then used in the GUI as a tooltip.
				option(AUDIO_ADX2 "ADX2 SDK not found in ${SDK_DIR}/Audio/adx2 and AUDIO_ADX2 option turned OFF." OFF)
			endif()
		endif()
	else()
		# If this option is not in the cache yet, set it depending on whether the SDK is present or not.
		if (EXISTS "${SDK_DIR}/Audio/adx2")
			message(STATUS "ADX2 SDK found in ${SDK_DIR}/Audio/adx2 - enabling ADX2 support.")
			set(AUDIO_ADX2 ON CACHE BOOL "ADX2 SDK found in ${SDK_DIR}/Audio/adx2." FORCE)
		else()
			message(STATUS "ADX2 SDK not found in ${SDK_DIR}/Audio/adx2 - disabling ADX2 support.")
			set(AUDIO_ADX2 OFF CACHE BOOL "ADX2 SDK not found in ${SDK_DIR}/Audio/adx2." FORCE)
		endif()
	endif()
else()
	message(STATUS "Disabling ADX2 support due to unsupported platform.")
	set(AUDIO_ADX2 OFF CACHE BOOL "ADX2 disabled due to unsupported platform." FORCE)
endif()
endfunction(try_to_enable_adx2)

function(try_to_enable_dsp_spatializer)
if(WINDOWS OR DURANGO)
	if (DEFINED AUDIO_DSP_SPATIALIZER)
		option(AUDIO_DSP_SPATIALIZER "audio DSP Spatializer." ON)

		if (AUDIO_DSP_SPATIALIZER)
			message(STATUS "Enabling audio DSP Spatializer.")
		else()
			message(STATUS "audio DSP Spatializer manually disabled.")
		endif()
	endif()
else()
	message(STATUS "Disabling audio DSP Spatializer due to unsupported platform.")
	set(AUDIO_DSP_SPATIALIZER OFF CACHE BOOL "audio DSP Spatializer disabled due to unsupported platform." FORCE)
endif()
endfunction(try_to_enable_dsp_spatializer)

# We need to do this as currently we can't have multiple modules sharing the same GUID in monolithic builds.
# Can be changed back to include all supported middlewares once that has been rectified.
if(OPTION_STATIC_LINKING)
	if(ORBIS)
		try_to_enable_fmod()
	else()
		try_to_enable_sdl_mixer()
	endif()
else()
	try_to_enable_fmod()
	try_to_enable_portaudio()
	try_to_enable_sdl_mixer()
	try_to_enable_wwise()
	try_to_enable_adx2()
endif()

try_to_enable_dynamic_response_system()
try_to_enable_dsp_spatializer()
# ~Audio

#Physics modules
option(PHYSICS_CRYPHYSICS "Enable" ON)
if (WINDOWS)
	option(PHYSICS_PHYSX "Enable" OFF)
endif()
if (PHYSICS_PHYSX)
	include("${TOOLS_CMAKE_DIR}/modules/PhysX.cmake")
endif()

include_directories("${CRYENGINE_DIR}/Code/CryEngine/CryCommon")
include_directories("${CRYENGINE_DIR}/Code/CryEngine/CryCommon/3rdParty")
include_directories("${SDK_DIR}/boost")
include_directories("${CRYENGINE_DIR}/Code/CryEngine/CrySchematyc/Core/Interface")

#if (WINDOWS)
#	link_libraries (Ntdll)
#endif (WINDOWS)

if (OPTION_SCALEFORMHELPER AND NOT (OPTION_ENGINE OR OPTION_SHADERCACHEGEN))
	add_subdirectory ("Code/CryEngine/CrySystem/Scaleform")
endif()

if (OPTION_ENGINE OR OPTION_SHADERCACHEGEN OR OPTION_DOXYGEN_EXAMPLES)
	add_subdirectory ("Code/CryEngine/CryCommon")
endif()
	
if (OPTION_ENGINE OR OPTION_SHADERCACHEGEN)
	add_subdirectory ("Code/CryEngine/CrySystem")
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
	if (PLUGIN_DRS)
		add_subdirectory ("Code/CryEngine/CryDynamicResponseSystem")
	endif (PLUGIN_DRS)
	add_subdirectory ("Code/CryEngine/CryEntitySystem")
	add_subdirectory ("Code/CryEngine/CryFont")
	add_subdirectory ("Code/CryEngine/CryInput")
	add_subdirectory ("Code/CryEngine/CryMovie")
	add_subdirectory ("Code/CryEngine/CryNetwork")
	if (PLUGIN_SCHEMATYC_EXPERIMENTAL)
		target_compile_definitions(CrySystem PUBLIC USE_SCHEMATYC_EXPERIMENTAL=1)
		if (TARGET CrySystemLib)
			target_compile_definitions(CrySystemLib PUBLIC USE_SCHEMATYC_EXPERIMENTAL=1)
		endif()
		add_subdirectory ("Code/CryEngine/CrySchematyc")
	endif()

	if (NOT ANDROID AND PLUGIN_SCHEMATYC)
		target_compile_definitions(CrySystem PUBLIC USE_SCHEMATYC=1)
		if (TARGET CrySystemLib)
			target_compile_definitions(CrySystemLib PUBLIC USE_SCHEMATYC=1)
		endif()
		add_subdirectory ("Code/CryEngine/CrySchematyc2")
	endif()
	add_subdirectory ("Code/CryEngine/CryScriptSystem")
	add_subdirectory ("Code/CryEngine/CryFlowGraph")
	add_subdirectory ("Code/CryEngine/CryUDR")

	if (WINDOWS)
		add_subdirectory ("Code/CryEngine/CryLiveCreate")
	endif (WINDOWS)

	#mono
	if (OPTION_CRYMONO)
		target_compile_definitions(CrySystem PUBLIC USE_MONO=1)
		if (TARGET CrySystemLib)
			target_compile_definitions(CrySystemLib PUBLIC USE_MONO=1)
		endif()
	endif()

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
		if (AUDIO_DSP_SPATIALIZER)
			add_subdirectory("Code/CryEngine/CryAudioSystem/implementations/CryAudioImplFmod/plugins/CrySpatial")
		endif ()
	endif (AUDIO_FMOD)
	if (AUDIO_PORTAUDIO)
		add_subdirectory ("Code/CryEngine/CryAudioSystem/implementations/CryAudioImplPortAudio")
	endif (AUDIO_PORTAUDIO)
	if (AUDIO_SDL_MIXER)
		add_subdirectory ("Code/CryEngine/CryAudioSystem/implementations/CryAudioImplSDLMixer")
		add_subdirectory ("Code/Libs/SDL_mixer")
	endif (AUDIO_SDL_MIXER)
	if (AUDIO_WWISE)
		add_subdirectory ("Code/CryEngine/CryAudioSystem/implementations/CryAudioImplWwise")
		if (AUDIO_DSP_SPATIALIZER)
			add_subdirectory("Code/CryEngine/CryAudioSystem/implementations/CryAudioImplWwise/plugins/CrySpatial/WwisePlugin")
			add_subdirectory("Code/CryEngine/CryAudioSystem/implementations/CryAudioImplWwise/plugins/CrySpatial/SoundEnginePlugin")
		endif ()
	endif (AUDIO_WWISE)
	if (AUDIO_ADX2)
		add_subdirectory ("Code/CryEngine/CryAudioSystem/implementations/CryAudioImplAdx2")
	endif (AUDIO_ADX2)

	#libs
	add_subdirectory ("Code/Libs/bigdigits")
	add_subdirectory ("Code/Libs/mikkelsen")
	
	if(PLUGIN_VR_OCULUS)
		add_subdirectory("Code/Libs/oculus")
	endif()
		
	if (WINDOWS OR LINUX)
		add_subdirectory ("Code/Libs/curl")
	endif ()
	add_subdirectory ("Code/Libs/freetype")
	add_subdirectory ("Code/Libs/lua")

    if (OPTION_UNIT_TEST)
	    set(TEST_MODULES "" CACHE INTERNAL "List of test modules being built" FORCE)	
        set(temp ${BUILD_SHARED_LIBS})
        set(gtest_force_shared_crt TRUE CACHE INTERNAL "" FORCE)
        set(BUILD_SHARED_LIBS FALSE CACHE INTERNAL "" FORCE)
        set(INSTALL_GTEST FALSE CACHE INTERNAL "" FORCE)
        set(INSTALL_GMOCK FALSE CACHE INTERNAL "" FORCE)
	    if(ORBIS)
		    set(MSVC_TEMP ${MSVC})
		    set(MSVC FALSE)
	    endif()
	    add_subdirectory ("${SDK_DIR}/googletest_CE_Support")
		# Guard against wrongly setup bootstrap not finding gtest and gmock targets
		if (NOT (TARGET gtest AND TARGET gmock))
			message(FATAL_ERROR "Error adding google test. This could be caused by Bootstrap, please check Bootstrap.")
		endif()
        mark_as_advanced(FORCE BUILD_GTEST BUILD_GMOCK BUILD_SHARED_LIBS INSTALL_GTEST INSTALL_GMOCK gtest_build_samples gtest_build_tests gtest_disable_pthreads gtest_force_shared_crt gtest_hide_internal_symbols gmock_build_tests)
	    if(ORBIS)
		    set(MSVC ${MSVC_TEMP})
		    target_compile_definitions(gtest PRIVATE GTEST_HAS_DEATH_TEST=0)
		    target_compile_definitions(gmock PRIVATE GTEST_HAS_DEATH_TEST=0)
	    endif()
        set(BUILD_SHARED_LIBS ${temp} CACHE INTERNAL "" FORCE)
        set_solution_folder("Libs" gtest)
	    set_solution_folder("Libs" gmock)
        set_solution_folder("Libs" gtest_main)
        set_solution_folder("Libs" gmock_main)
        set(CMAKE_DEBUG_POSTFIX "" CACHE STRING "Set debug library postfix" FORCE)
	    if (DURANGO OR ORBIS)
		    set(THIS_PROJECT gtest)
		    SET_PLATFORM_TARGET_PROPERTIES(gtest)
		    set(THIS_PROJECT gmock)
		    SET_PLATFORM_TARGET_PROPERTIES(gmock)
	    endif()

		# We want to hide gtest_main and gmock_main because we are not using these
		# Only do so when these targets are defined
		if (TARGET gmock_main)
			set_property(TARGET gmock_main PROPERTY EXCLUDE_FROM_DEFAULT_BUILD TRUE)
			set_property(TARGET gmock_main PROPERTY EXCLUDE_FROM_ALL TRUE)	
		endif()
        
		if (TARGET gtest_main)
			set_property(TARGET gtest_main PROPERTY EXCLUDE_FROM_DEFAULT_BUILD TRUE)
			set_property(TARGET gtest_main PROPERTY EXCLUDE_FROM_ALL TRUE)
		endif()

	    add_subdirectory("${CRYENGINE_DIR}/Code/CryEngine/UnitTests/CryCommonUnitTest")
        add_subdirectory("${CRYENGINE_DIR}/Code/CryEngine/UnitTests/CrySystemUnitTest")
        add_subdirectory("${CRYENGINE_DIR}/Code/CryEngine/UnitTests/CryEntitySystemUnitTest")
		add_subdirectory("${CRYENGINE_DIR}/Code/CryEngine/UnitTests/CryAnimationUnitTest")
		add_subdirectory("${CRYENGINE_DIR}/Code/CryEngine/UnitTests/Cry3DEngineUnitTest")
		add_subdirectory("${CRYENGINE_DIR}/Code/CryEngine/UnitTests/CryAudioSystemUnitTest")
		add_subdirectory("${CRYENGINE_DIR}/Code/CryEngine/UnitTests/CryAISystemUnitTest")
	    if (ORBIS)
		    add_subdirectory("${CRYENGINE_DIR}/Code/CryEngine/UnitTests/CryUnitTestOrbisMain")
	    endif()
    endif()
endif()

if (OPTION_SCALEFORMHELPER OR OPTION_ENGINE OR OPTION_SHADERCACHEGEN)
	add_subdirectory ("Code/Libs/zlib")
	add_subdirectory ("Code/Libs/expat")
	if (WINDOWS)
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

if (WINDOWS)
	add_subdirectory ("Code/Libs/Detours")
endif ()

add_subdirectory ("Code/Libs/qpOASES")

