// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioImplCVars.h"
#include <CrySystem/IConsole.h>

using namespace CryAudio::Impl::Fmod;

//////////////////////////////////////////////////////////////////////////
CAudioImplCVars::CAudioImplCVars()
	: m_primaryMemoryPoolSize(0)
	, m_maxChannels(0)
	, m_enableLiveUpdate(0)
	, m_enableSynchronousUpdate(1)
	, m_lowpassMinCutoffFrequency(10.0f)
	, m_distanceFactor(1.0f)
	, m_dopplerScale(1.0f)
	, m_rolloffScale(1.0f)
#if CRY_PLATFORM_DURANGO
	, m_secondaryMemoryPoolSize(0)
#endif // CRY_PLATFORM_DURANGO
#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
#endif // INCLUDE_FMOD_IMPL_PRODUCTION_CODE
{
}

//////////////////////////////////////////////////////////////////////////
CAudioImplCVars::~CAudioImplCVars()
{
}

//////////////////////////////////////////////////////////////////////////
void CAudioImplCVars::RegisterVariables()
{
#if CRY_PLATFORM_WINDOWS
	m_primaryMemoryPoolSize = 128 << 10;  // 128 MiB
	m_maxChannels = 512;
	#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
	#endif // INCLUDE_FMOD_IMPL_PRODUCTION_CODE
#elif CRY_PLATFORM_DURANGO
	m_primaryMemoryPoolSize = 128 << 10;  // 128 MiB
	m_secondaryMemoryPoolSize = 32 << 10; // 32 MiB
	m_maxChannels = 512;
	#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
	#endif // INCLUDE_FMOD_IMPL_PRODUCTION_CODE
#elif CRY_PLATFORM_ORBIS
	m_primaryMemoryPoolSize = 128 << 10;  // 128 MiB
	m_maxChannels = 512;
	#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
	#endif // INCLUDE_FMOD_IMPL_PRODUCTION_CODE
#elif CRY_PLATFORM_MAC
	m_primaryMemoryPoolSize = 128 << 10;  // 128 MiB
	m_maxChannels = 512;
	#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
	#endif // INCLUDE_FMOD_IMPL_PRODUCTION_CODE
#elif CRY_PLATFORM_LINUX
	m_primaryMemoryPoolSize = 128 << 10;  // 128 MiB
	m_maxChannels = 512;
	#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
	#endif // INCLUDE_FMOD_IMPL_PRODUCTION_CODE
#elif defined(CRY_PLATFORM_IOS)
	m_primaryMemoryPoolSize = 8 << 10;    // 8 MiB
	m_maxChannels = 512;
	#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
	#endif // INCLUDE_FMOD_IMPL_PRODUCTION_CODE
#elif CRY_PLATFORM_ANDROID
	m_primaryMemoryPoolSize = 32 << 10;   // 32 MiB
	m_maxChannels = 512;
	#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
	#endif // INCLUDE_FMOD_IMPL_PRODUCTION_CODE
#else
	#error "Undefined platform."
#endif

	REGISTER_CVAR2("s_FmodPrimaryPoolSize", &m_primaryMemoryPoolSize, m_primaryMemoryPoolSize, VF_REQUIRE_APP_RESTART,
	               "Specifies the size (in KiB) of the memory pool to be used by the Fmod audio system implementation.\n"
	               "Usage: s_FmodPrimaryPoolSize [0/...]\n"
	               "Default PC: 131072 (128 MiB), XboxOne: 131072 (128 MiB), PS4: 131072 (128 MiB), Mac: 131072 (128 MiB), Linux: 131072 (128 MiB), iOS: 8192 (8 MiB), Android: 32768 (32 MiB)\n");

	REGISTER_CVAR2("s_FmodMaxChannels", &m_maxChannels, m_maxChannels, VF_REQUIRE_APP_RESTART,
	               "Sets the maximum number of channels.\n"
	               "Usage: s_FmodMaxChannels [0/...]\n"
	               "Default PC: 512, XboxOne: 512, PS4: 512, Mac: 512, Linux: 512, iOS: 512, Android: 512\n");

	REGISTER_CVAR2("s_FmodEnableLiveUpdate", &m_enableLiveUpdate, m_enableLiveUpdate, VF_REQUIRE_APP_RESTART,
	               "Enables Fmod Studio to run with LiveUpdate enabled. Needs implementation restart.\n"
	               "Usage: s_FmodEnableLiveUpdate [0/1]\n"
	               "Default PC: 0, XboxOne: 0, PS4: 0, Mac: 0, Linux: 0, iOS: 0, Android: 0\n");

	REGISTER_CVAR2("s_FmodEnableSynchronousUpdate", &m_enableSynchronousUpdate, m_enableSynchronousUpdate, VF_REQUIRE_APP_RESTART,
	               "Enable synchronous processing and perform all processing on the calling thread instead.\n"
	               "Usage: s_FmodEnableSynchronousUpdate [0/1]\n"
	               "Default PC: 1, XboxOne: 1, PS4: 1, Mac: 1, Linux: 1, iOS: 1, Android: 1\n");

	REGISTER_CVAR2("s_FmodLowpassMinCutoffFrequency", &m_lowpassMinCutoffFrequency, m_lowpassMinCutoffFrequency, VF_REQUIRE_APP_RESTART,
	               "Sets the minimum LPF cutoff frequency upon full audio object occlusion.\n"
	               "Usage: s_FmodLowpassMinCutoffFrequency [10/...]\n"
	               "Default PC: 10, XboxOne: 10, PS4: 10, Mac: 10, Linux: 10, iOS: 10, Android: 10\n");

	REGISTER_CVAR2("s_FmodDistanceFactor", &m_distanceFactor, m_distanceFactor, VF_REQUIRE_APP_RESTART,
	               "Sets the relative distance factor to FMOD's units.\n"
	               "The distance factor is the FMOD 3D engine relative distance factor, compared to 1.0 meters.\n"
	               "Another way to put it is that it equates to \"how many units per meter does your engine have\".\n"
	               "For example, if you are using feet then \"scale\" would equal 3.28.\n"
	               "Usage: s_FmodDistanceFactor [0/...]\n"
	               "Default PC: 1, XboxOne: 1, PS4: 1, Mac: 1, Linux: 1, iOS: 1, Android: 1\n");

	REGISTER_CVAR2("s_FmodDopplerScale", &m_dopplerScale, m_dopplerScale, VF_REQUIRE_APP_RESTART,
	               "Sets the scaling factor for doppler shift.\n"
	               "The doppler scale is a general scaling factor for how much the pitch varies due to doppler shifting in 3D sound.\n"
	               "Doppler is the pitch bending effect when a sound comes towards the listener or moves away from it,\n"
	               "much like the effect you hear when a train goes past you with its horn sounding.\n"
	               "With \"dopplerscale\" you can exaggerate or diminish the effect.\n"
	               "FMOD's effective speed of sound at a doppler factor of 1.0 is 340 m/s.\n"
	               "Usage: s_FmodDopplerScale [0/...]\n"
	               "Default PC: 1, XboxOne: 1, PS4: 1, Mac: 1, Linux: 1, iOS: 1, Android: 1\n");

	REGISTER_CVAR2("s_FmodRolloffScale", &m_rolloffScale, m_rolloffScale, VF_REQUIRE_APP_RESTART,
	               "Sets the scaling factor for 3D sound rolloff or attenuation for FMOD_3D_INVERSEROLLOFF based sounds only (which is the default type).\n"
	               "Volume for a sound set to FMOD_3D_INVERSEROLLOFF will scale at mindistance / distance.\n"
	               "This gives an inverse attenuation of volume as the source gets further away (or closer).\n"
	               "Setting this value makes the sound drop off faster or slower.\n"
	               "The higher the value, the faster volume will attenuate, and conversely the lower the value, the slower it will attenuate.\n"
	               "For example a rolloff factor of 1 will simulate the real world, where as a value of 2 will make sounds attenuate 2 times quicker.\n"
	               "Usage: s_FmodRolloffScale [0/...]\n"
	               "Default PC: 1, XboxOne: 1, PS4: 1, Mac: 1, Linux: 1, iOS: 1, Android: 1\n");

#if CRY_PLATFORM_DURANGO
	REGISTER_CVAR2("s_FmodSecondaryPoolSize", &m_secondaryMemoryPoolSize, m_secondaryMemoryPoolSize, VF_REQUIRE_APP_RESTART,
	               "Specifies the size (in KiB) of the memory pool to be used by the Fmod audio system implementation.\n"
	               "Usage: s_FmodSecondaryPoolSize [0/...]\n"
	               "Default PC: 0, XboxOne: 32768 (32 MiB), PS4: 0, Mac: 0, Linux: 0, iOS: 0, Android: 0\n");
#endif // CRY_PLATFORM_DURANGO

#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
#endif // INCLUDE_FMOD_IMPL_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CAudioImplCVars::UnregisterVariables()
{
	IConsole* const pConsole = gEnv->pConsole;

	if (pConsole != nullptr)
	{
		pConsole->UnregisterVariable("s_FmodPrimaryPoolSize");
		pConsole->UnregisterVariable("s_FmodMaxChannels");
		pConsole->UnregisterVariable("s_FmodEnableLiveUpdate");
		pConsole->UnregisterVariable("s_FmodEnableSynchronousUpdate");
		pConsole->UnregisterVariable("s_FmodLowpassMinCutoffFrequency");
		pConsole->UnregisterVariable("s_FmodDistanceFactor");
		pConsole->UnregisterVariable("s_FmodDopplerScale");
		pConsole->UnregisterVariable("s_FmodRolloffScale");

#if CRY_PLATFORM_DURANGO
		pConsole->UnregisterVariable("s_FmodSecondaryPoolSize");
#endif // CRY_PLATFORM_DURANGO

#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
#endif // INCLUDE_FMOD_IMPL_PRODUCTION_CODE
	}
}
