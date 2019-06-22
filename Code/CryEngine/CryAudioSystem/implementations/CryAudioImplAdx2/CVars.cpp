// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "CVars.h"

#include <CrySystem/ConsoleRegistration.h>

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
//////////////////////////////////////////////////////////////////////////
void SetVoiceAllocationMethod(ICVar* const pCvar)
{
	pCvar->Set(crymath::clamp(pCvar->GetIVal(), 0, 1));
}

//////////////////////////////////////////////////////////////////////////
void CCVars::RegisterVariables()
{
	REGISTER_CVAR2("s_Adx2CuePoolSize", &m_cuePoolSize, m_cuePoolSize, VF_REQUIRE_APP_RESTART,
	               "Sets the number of preallocated cue instances.\n"
	               "Usage: s_Adx2CuePoolSize [0/...]\n"
	               "Default PC: 256, XboxOne: 256, PS4: 256, Mac: 256, Linux: 256, iOS: 256, Android: 256\n");

	REGISTER_CVAR2("s_Adx2MaxVirtualVoices", &m_maxVirtualVoices, m_maxVirtualVoices, VF_REQUIRE_APP_RESTART,
	               "Specifies the maximum number of voices for which voice control is performed simultaneously.\n"
	               "Usage: s_Adx2MaxVirtualVoices [0/...]\n"
	               "Default PC: 16\n");

	REGISTER_CVAR2("s_AdxMaxVoiceLimitGroups", &m_maxVoiceLimitGroups, m_maxVoiceLimitGroups, VF_REQUIRE_APP_RESTART,
	               "Specifies the maximum number of Voice Limit Groups that can be created.\n"
	               "Usage: s_AdxMaxVoiceLimitGroups [0/...]\n"
	               "Default PC: 16\n");

	REGISTER_CVAR2("s_Adx2MaxCategories", &m_maxCategories, m_maxCategories, VF_REQUIRE_APP_RESTART,
	               "Specifies the maximum number of categories that can be created.\n"
	               "Usage: s_Adx2MaxCategories [0/...]\n"
	               "Default PC: 16\n");

	REGISTER_CVAR2("s_Adx2CategoriesPerPlayback", &m_categoriesPerPlayback, m_categoriesPerPlayback, VF_REQUIRE_APP_RESTART,
	               "Specifies the number of categories that can be referenced per playback.\n"
	               "Usage: s_Adx2CategoriesPerPlayback [0/...]\n"
	               "Default PC: 4\n");

	REGISTER_CVAR2("s_Adx2MaxTracks", &m_maxTracks, m_maxTracks, VF_REQUIRE_APP_RESTART,
	               "Specifies the total number of Tracks in a sequence that can be played back simultaneously.\n"
	               "Usage: s_Adx2MaxTracks [0/...]\n"
	               "Default PC: 32\n");

	REGISTER_CVAR2("s_Adx2MaxTrackItems", &m_maxTrackItems, m_maxTrackItems, VF_REQUIRE_APP_RESTART,
	               "Specifies the total number of events in a sequence that can be played back simultaneously.\n"
	               "Usage: s_Adx2MaxTrackItems [0/...]\n"
	               "Default PC: 32\n");

	REGISTER_CVAR2("s_Adx2MaxFaders", &m_maxFaders, m_maxFaders, VF_REQUIRE_APP_RESTART,
	               "Specifies the maximum number of faders used in the Atom library.\n"
	               "Usage: s_Adx2MaxFaders [0/...]\n"
	               "Default PC: 4\n");

	REGISTER_CVAR2("s_Adx2NumVoices", &m_numVoices, m_numVoices, VF_REQUIRE_APP_RESTART,
	               "Specifies the number of voices.\n"
	               "Usage: s_Adx2NumVoices [0/...]\n"
	               "Default PC: 8\n");

	REGISTER_CVAR2("s_Adx2MaxChannels", &m_maxChannels, m_maxChannels, VF_REQUIRE_APP_RESTART,
	               "Specifies the maximum number of channels that can be processed by a DSP.\n"
	               "Usage: s_Adx2MaxChannels [0/...]\n"
	               "Default PC: 2\n");

	REGISTER_CVAR2("s_Adx2MaxSamplingRate", &m_maxSamplingRate, m_maxSamplingRate, VF_REQUIRE_APP_RESTART,
	               "Specifies the maximum sampling rate that can be processed by a DSP.\n"
	               "Usage: s_Adx2MaxSamplingRate [0/...]\n"
	               "Default PC: 48000\n");

	REGISTER_CVAR2("s_Adx2NumBuses", &m_numBuses, m_numBuses, VF_REQUIRE_APP_RESTART,
	               "Specifies the number of buses.\n"
	               "Usage: s_Adx2NumBuses [0/...]\n"
	               "Default PC: 8\n");

	REGISTER_CVAR2("s_Adx2OutputChannels", &m_outputChannels, m_outputChannels, VF_REQUIRE_APP_RESTART,
	               "Specifies the number of output channels.\n"
	               "Usage: s_Adx2OutputChannels [0/...]\n"
	               "Default PC: 6\n");

	REGISTER_CVAR2("s_Adx2OutputSamplingRate", &m_outputSamplingRate, m_outputSamplingRate, VF_REQUIRE_APP_RESTART,
	               "Specifies the sampling rate.\n"
	               "Usage: s_Adx2OutputSamplingRate [0/...]\n"
	               "Default PC: 48000\n");

	REGISTER_CVAR2("s_Adx2MaxStreams", &m_maxStreams, m_maxStreams, VF_REQUIRE_APP_RESTART,
	               "Specifies the instantaneous maximum number of streamings.\n"
	               "Usage: s_Adx2MaxStreams [0/...]\n"
	               "Default PC: 8\n");

	REGISTER_CVAR2("s_Adx2MaxFiles", &m_maxFiles, m_maxFiles, VF_REQUIRE_APP_RESTART,
	               "Specifies the maximum number of files to open simultaneously.\n"
	               "Usage: s_Adx2MaxFiles [0/...]\n"
	               "Default PC: 32\n");

	REGISTER_CVAR2_CB("s_Adx2VoiceAllocationMethod", &m_voiceAllocationMethod, m_voiceAllocationMethod, VF_REQUIRE_APP_RESTART,
	                  "Specifies the method used when an AtomEx Player allocates voices.\n"
	                  "Usage: s_Adx2VoiceAllocationMethod [0/1]\n"
	                  "Default PC: 0\n"
	                  "0: Voice allocation is tried once.\n"
	                  "1: Voice allocation is tried as many times as needed.\n",
	                  SetVoiceAllocationMethod);

	REGISTER_CVAR2("s_Adx2MaxPitch", &m_maxPitch, m_maxPitch, VF_REQUIRE_APP_RESTART,
	               "Specifies the upper limit of the pitch change applied in the Atom library.\n"
	               "Usage: s_Adx2MaxPitch [0/...]\n"
	               "Default PC: 2400\n");

	REGISTER_CVAR2("s_Adx2VelocityTrackingThreshold", &m_velocityTrackingThreshold, m_velocityTrackingThreshold, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "An object has to change its velocity by at least this amount to issue an \"absolute_velocity\" parameter update request to the audio system.\n"
	               "Usage: s_Adx2VelocityTrackingThreshold [0/...]\n"
	               "Default: 0.1 (10 cm/s)\n");

	REGISTER_CVAR2("s_Adx2PositionUpdateThresholdMultiplier", &m_positionUpdateThresholdMultiplier, m_positionUpdateThresholdMultiplier, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "An object's distance to the listener is multiplied by this value to determine the position update threshold.\n"
	               "Usage: s_Adx2PositionUpdateThresholdMultiplier [0/...]\n"
	               "Default: 0.02\n");

	REGISTER_CVAR2("s_Adx2MaxVelocity", &m_maxVelocity, m_maxVelocity, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "The maximum velocity that will be normalized to 1.0 of the AISCA-Control \"absolute_velocity\".\n"
	               "For instance if this value is set to 100, and an object has a speed of 20, the corresponding AISAC-Control value will be 0.2\n"
	               "Usage: s_Adx2MaxVelocity [0/...]\n"
	               "Default: 100\n");

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	REGISTER_CVAR2("s_Adx2DebugListFilter", &m_debugListFilter, 448, VF_CHEAT | VF_CHEAT_NOCHECK | VF_BITFIELD,
	               "Defines which lists to show when list filtering is enabled in the debug draw of the audio system.\n"
	               "Usage: s_Adx2DebugListFilter [0ab...] (flags can be combined)\n"
	               "Default: abc\n"
	               "0: Draw nothing.\n"
	               "a: Draw cue instances.\n"
	               "b: Draw GameVariable values.\n"
	               "c: Draw Category values.\n"
	               );
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CCVars::UnregisterVariables()
{
	IConsole* const pConsole = gEnv->pConsole;

	if (pConsole != nullptr)
	{
		pConsole->UnregisterVariable("s_Adx2CuePoolSize");
		pConsole->UnregisterVariable("s_Adx2MaxVirtualVoices");
		pConsole->UnregisterVariable("s_AdxMaxVoiceLimitGroups");
		pConsole->UnregisterVariable("s_Adx2MaxCategories");
		pConsole->UnregisterVariable("s_Adx2CategoriesPerPlayback");
		pConsole->UnregisterVariable("s_Adx2MaxTracks");
		pConsole->UnregisterVariable("s_Adx2MaxTrackItems");
		pConsole->UnregisterVariable("s_Adx2MaxFaders");
		pConsole->UnregisterVariable("s_Adx2NumVoices");
		pConsole->UnregisterVariable("s_Adx2MaxChannels");
		pConsole->UnregisterVariable("s_Adx2MaxSamplingRate");
		pConsole->UnregisterVariable("s_Adx2NumBuses");
		pConsole->UnregisterVariable("s_Adx2OutputChannels");
		pConsole->UnregisterVariable("s_Adx2OutputSamplingRate");
		pConsole->UnregisterVariable("s_Adx2MaxStreams");
		pConsole->UnregisterVariable("s_Adx2MaxFiles");
		pConsole->UnregisterVariable("s_Adx2VoiceAllocationMethod");
		pConsole->UnregisterVariable("s_Adx2MaxPitch");
		pConsole->UnregisterVariable("s_Adx2VelocityTrackingThreshold");
		pConsole->UnregisterVariable("s_Adx2PositionUpdateThresholdMultiplier");
		pConsole->UnregisterVariable("s_Adx2MaxVelocity");

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
		pConsole->UnregisterVariable("s_Adx2DebugListFilter");
#endif    // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
	}
}
} // namespace Adx2
} // namespace Impl
} // namespace CryAudio
