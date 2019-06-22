// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "CVars.h"
#include <CrySystem/ConsoleRegistration.h>

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
//////////////////////////////////////////////////////////////////////////
void CCVars::RegisterVariables()
{
	REGISTER_CVAR2("s_FmodEventPoolSize", &m_eventPoolSize, m_eventPoolSize, VF_REQUIRE_APP_RESTART,
	               "Sets the number of preallocated events.\n"
	               "Usage: s_FmodEventPoolSize [0/...]\n"
	               "Default: 256\n");

	REGISTER_CVAR2("s_FmodVelocityTrackingThreshold", &m_velocityTrackingThreshold, m_velocityTrackingThreshold, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "An object has to change its velocity by at least this amount to issue an \"absolute_velocity\" parameter update request to the audio system.\n"
	               "Usage: s_FmodVelocityTrackingThreshold [0/...]\n"
	               "Default: 0.1 (10 cm/s)\n");

	REGISTER_CVAR2("s_FmodPositionUpdateThresholdMultiplier", &m_positionUpdateThresholdMultiplier, m_positionUpdateThresholdMultiplier, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "An object's distance to the listener is multiplied by this value to determine the position update threshold.\n"
	               "Usage: s_FmodPositionUpdateThresholdMultiplier [0/...]\n"
	               "Default: 0.02\n");

	REGISTER_CVAR2("s_FmodMaxChannels", &m_maxChannels, m_maxChannels, VF_REQUIRE_APP_RESTART,
	               "Sets the maximum number of channels.\n"
	               "Usage: s_FmodMaxChannels [0/...]\n"
	               "Default: 512,\n");

	REGISTER_CVAR2("s_FmodEnableSynchronousUpdate", &m_enableSynchronousUpdate, m_enableSynchronousUpdate, VF_REQUIRE_APP_RESTART,
	               "Enable synchronous processing and perform all processing on the calling thread instead.\n"
	               "Usage: s_FmodEnableSynchronousUpdate [0/1]\n"
	               "Default: 1\n");

	REGISTER_CVAR2("s_FmodLowpassMinCutoffFrequency", &m_lowpassMinCutoffFrequency, m_lowpassMinCutoffFrequency, VF_REQUIRE_APP_RESTART,
	               "Sets the minimum LPF cutoff frequency upon full object occlusion.\n"
	               "Usage: s_FmodLowpassMinCutoffFrequency [10/...]\n"
	               "Default: 10\n");

	REGISTER_CVAR2("s_FmodDistanceFactor", &m_distanceFactor, m_distanceFactor, VF_REQUIRE_APP_RESTART,
	               "Sets the relative distance factor to FMOD's units.\n"
	               "The distance factor is the FMOD 3D engine relative distance factor, compared to 1.0 meters.\n"
	               "Another way to put it is that it equates to \"how many units per meter does your engine have\".\n"
	               "For example, if you are using feet then \"scale\" would equal 3.28.\n"
	               "Usage: s_FmodDistanceFactor [0/...]\n"
	               "Default: 1\n");

	REGISTER_CVAR2("s_FmodDopplerScale", &m_dopplerScale, m_dopplerScale, VF_REQUIRE_APP_RESTART,
	               "Sets the scaling factor for doppler shift.\n"
	               "The doppler scale is a general scaling factor for how much the pitch varies due to doppler shifting in 3D sound.\n"
	               "Doppler is the pitch bending effect when a sound comes towards the listener or moves away from it,\n"
	               "much like the effect you hear when a train goes past you with its horn sounding.\n"
	               "With \"dopplerscale\" you can exaggerate or diminish the effect.\n"
	               "FMOD's effective speed of sound at a doppler factor of 1.0 is 340 m/s.\n"
	               "Usage: s_FmodDopplerScale [0/...]\n"
	               "Default: 1\n");

	REGISTER_CVAR2("s_FmodRolloffScale", &m_rolloffScale, m_rolloffScale, VF_REQUIRE_APP_RESTART,
	               "Sets the scaling factor for 3D sound rolloff or attenuation for FMOD_3D_INVERSEROLLOFF based sounds only (which is the default type).\n"
	               "Volume for a sound set to FMOD_3D_INVERSEROLLOFF will scale at mindistance / distance.\n"
	               "This gives an inverse attenuation of volume as the source gets further away (or closer).\n"
	               "Setting this value makes the sound drop off faster or slower.\n"
	               "The higher the value, the faster volume will attenuate, and conversely the lower the value, the slower it will attenuate.\n"
	               "For example a rolloff factor of 1 will simulate the real world, where as a value of 2 will make sounds attenuate 2 times quicker.\n"
	               "Usage: s_FmodRolloffScale [0/...]\n"
	               "Default: 1\n");

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	REGISTER_CVAR2("s_FmodEnableLiveUpdate", &m_enableLiveUpdate, m_enableLiveUpdate, VF_REQUIRE_APP_RESTART,
	               "Enables Fmod Studio to run with LiveUpdate enabled. Needs implementation restart.\n"
	               "Usage: s_FmodEnableLiveUpdate [0/1]\n"
	               "Default: 0\n");

	REGISTER_CVAR2("s_FmodDebugListFilter", &m_debugListFilter, 448, VF_CHEAT | VF_CHEAT_NOCHECK | VF_BITFIELD,
	               "Defines which lists to show when list filtering is enabled in the debug draw of the audio system.\n"
	               "Usage: s_FmodDebugListFilter [0ab...] (flags can be combined)\n"
	               "Default: abc\n"
	               "0: Draw nothing.\n"
	               "a: Draw event instances.\n"
	               "b: Draw active snapshots.\n"
	               "c: Draw VCA values.\n"
	               );
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CCVars::UnregisterVariables()
{
	IConsole* const pConsole = gEnv->pConsole;

	if (pConsole != nullptr)
	{
		pConsole->UnregisterVariable("s_FmodEventPoolSize");
		pConsole->UnregisterVariable("s_FmodVelocityTrackingThreshold");
		pConsole->UnregisterVariable("s_FmodPositionUpdateThresholdMultiplier");
		pConsole->UnregisterVariable("s_FmodMaxChannels");
		pConsole->UnregisterVariable("s_FmodEnableSynchronousUpdate");
		pConsole->UnregisterVariable("s_FmodLowpassMinCutoffFrequency");
		pConsole->UnregisterVariable("s_FmodDistanceFactor");
		pConsole->UnregisterVariable("s_FmodDopplerScale");
		pConsole->UnregisterVariable("s_FmodRolloffScale");

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
		pConsole->UnregisterVariable("s_FmodEnableLiveUpdate");
		pConsole->UnregisterVariable("s_FmodDebugListFilter");
#endif    // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
	}
}
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio
