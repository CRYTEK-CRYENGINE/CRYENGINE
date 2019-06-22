// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "CVars.h"

#include "Common.h"
#include "Impl.h"
#include <CrySystem/ConsoleRegistration.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
//////////////////////////////////////////////////////////////////////////
void SetPanningRule(ICVar* const pPanningRule)
{
	pPanningRule->Set(crymath::clamp(pPanningRule->GetIVal(), 0, 1));

	if (g_pImpl != nullptr)
	{
		g_pImpl->SetPanningRule(pPanningRule->GetIVal());
	}
}

//////////////////////////////////////////////////////////////////////////
void CCVars::RegisterVariables()
{
#if CRY_PLATFORM_WINDOWS
	m_secondaryMemoryPoolSize = 0;
	m_prepareEventMemoryPoolSize = 4 << 10;        // 4 MiB
	m_streamManagerMemoryPoolSize = 128;           // 128 KiB
	m_streamDeviceMemoryPoolSize = 3 << 10;        // 3 MiB
	m_soundEngineDefaultMemoryPoolSize = 32 << 10; // 32 MiB
	m_commandQueueMemoryPoolSize = 512;            // 512 KiB
	m_lowerEngineDefaultPoolSize = 24 << 10;       // 24 MiB
	m_enableEventManagerThread = 1;
	m_enableSoundBankManagerThread = 1;
	m_numSamplesPerFrame = 512;
	m_numRefillsInVoice = 3;
	m_panningRule = 0;                 // Speakers
	#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	m_monitorMemoryPoolSize = 256;     // 256 KiB
	m_monitorQueueMemoryPoolSize = 64; // 64 KiB
	#endif                             // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
#elif CRY_PLATFORM_DURANGO
	m_secondaryMemoryPoolSize = 32 << 10;          // 32 MiB
	m_prepareEventMemoryPoolSize = 4 << 10;        // 4 MiB
	m_streamManagerMemoryPoolSize = 128;           // 128 KiB
	m_streamDeviceMemoryPoolSize = 3 << 10;        // 3 MiB
	m_soundEngineDefaultMemoryPoolSize = 32 << 10; // 32 MiB
	m_commandQueueMemoryPoolSize = 512;            // 512 KiB
	m_lowerEngineDefaultPoolSize = 24 << 10;       // 24 MiB
	m_enableEventManagerThread = 1;
	m_enableSoundBankManagerThread = 1;
	m_numSamplesPerFrame = 512;
	m_numRefillsInVoice = 3;
	m_panningRule = 0;                 // Speakers
	#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	m_monitorMemoryPoolSize = 256;     // 256 KiB
	m_monitorQueueMemoryPoolSize = 64; // 64 KiB
	#endif                             // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
#elif CRY_PLATFORM_ORBIS
	m_secondaryMemoryPoolSize = 0;
	m_prepareEventMemoryPoolSize = 4 << 10;        // 4 MiB
	m_streamManagerMemoryPoolSize = 128;           // 128 KiB
	m_streamDeviceMemoryPoolSize = 3 << 10;        // 3 MiB
	m_soundEngineDefaultMemoryPoolSize = 32 << 10; // 32 MiB
	m_commandQueueMemoryPoolSize = 512;            // 512 KiB
	m_lowerEngineDefaultPoolSize = 24 << 10;       // 24 MiB
	m_enableEventManagerThread = 1;
	m_enableSoundBankManagerThread = 1;
	m_numSamplesPerFrame = 512;
	m_numRefillsInVoice = 3;
	m_panningRule = 0;                 // Speakers
	#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	m_monitorMemoryPoolSize = 256;     // 256 KiB
	m_monitorQueueMemoryPoolSize = 64; // 64 KiB
	#endif                             // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
#elif CRY_PLATFORM_MAC
	m_secondaryMemoryPoolSize = 0;
	m_prepareEventMemoryPoolSize = 4 << 10;        // 4 MiB
	m_streamManagerMemoryPoolSize = 128;           // 128 KiB
	m_streamDeviceMemoryPoolSize = 3 << 10;        // 3 MiB
	m_soundEngineDefaultMemoryPoolSize = 32 << 10; // 32 MiB
	m_commandQueueMemoryPoolSize = 512;            // 512 KiB
	m_lowerEngineDefaultPoolSize = 24 << 10;       // 24 MiB
	m_enableEventManagerThread = 1;
	m_enableSoundBankManagerThread = 1;
	m_numSamplesPerFrame = 512;
	m_numRefillsInVoice = 3;
	m_panningRule = 0;                 // Speakers
	#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	m_monitorMemoryPoolSize = 256;     // 256 KiB
	m_monitorQueueMemoryPoolSize = 64; // 64 KiB
	#endif                             // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
#elif CRY_PLATFORM_LINUX
	m_secondaryMemoryPoolSize = 0;
	m_prepareEventMemoryPoolSize = 4 << 10;        // 4 MiB
	m_streamManagerMemoryPoolSize = 128;           // 128 KiB
	m_streamDeviceMemoryPoolSize = 3 << 10;        // 3 MiB
	m_soundEngineDefaultMemoryPoolSize = 32 << 10; // 32 MiB
	m_commandQueueMemoryPoolSize = 512;            // 512 KiB
	m_lowerEngineDefaultPoolSize = 24 << 10;       // 24 MiB
	m_enableEventManagerThread = 1;
	m_enableSoundBankManagerThread = 1;
	m_numSamplesPerFrame = 512;
	m_numRefillsInVoice = 3;
	m_panningRule = 0;                 // Speakers
	#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	m_monitorMemoryPoolSize = 256;     // 256 KiB
	m_monitorQueueMemoryPoolSize = 64; // 64 KiB
	#endif                             // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
#elif CRY_PLATFORM_IOS
	m_secondaryMemoryPoolSize = 0;
	m_prepareEventMemoryPoolSize = 4 << 10;        // 4 MiB
	m_streamManagerMemoryPoolSize = 128;           // 128 KiB
	m_streamDeviceMemoryPoolSize = 3 << 10;        // 3 MiB
	m_soundEngineDefaultMemoryPoolSize = 32 << 10; // 32 MiB
	m_commandQueueMemoryPoolSize = 512;            // 512 KiB
	m_lowerEngineDefaultPoolSize = 24 << 10;       // 24 MiB
	m_enableEventManagerThread = 1;
	m_enableSoundBankManagerThread = 1;
	m_numSamplesPerFrame = 512;
	m_numRefillsInVoice = 3;
	m_panningRule = 1;                 // Headphones
	#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	m_monitorMemoryPoolSize = 256;     // 256 KiB
	m_monitorQueueMemoryPoolSize = 64; // 64 KiB
	#endif                             // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
#elif CRY_PLATFORM_ANDROID
	m_secondaryMemoryPoolSize = 0;
	m_prepareEventMemoryPoolSize = 4 << 10;        // 4 MiB
	m_streamManagerMemoryPoolSize = 128;           // 128 KiB
	m_streamDeviceMemoryPoolSize = 3 << 10;        // 3 MiB
	m_soundEngineDefaultMemoryPoolSize = 32 << 10; // 32 MiB
	m_commandQueueMemoryPoolSize = 512;            // 512 KiB
	m_lowerEngineDefaultPoolSize = 24 << 10;       // 24 MiB
	m_enableEventManagerThread = 1;
	m_enableSoundBankManagerThread = 1;
	m_numSamplesPerFrame = 512;
	m_numRefillsInVoice = 3;
	m_panningRule = 1;                 // Headphones
	#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	m_monitorMemoryPoolSize = 256;     // 256 KiB
	m_monitorQueueMemoryPoolSize = 64; // 64 KiB
	#endif                             // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
#else
	#error "Undefined platform."
#endif

	REGISTER_CVAR2("s_WwiseVelocityTrackingThreshold", &m_velocityTrackingThreshold, m_velocityTrackingThreshold, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "An object has to change its velocity by at least this amount to issue an \"absolute_velocity\" parameter update request to the audio system.\n"
	               "Usage: s_WwiseVelocityTrackingThreshold [0/...]\n"
	               "Default: 0.1 (10 cm/s)\n");

	REGISTER_CVAR2("s_WwisePositionUpdateThresholdMultiplier", &m_positionUpdateThresholdMultiplier, m_positionUpdateThresholdMultiplier, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "An object's distance to the listener is multiplied by this value to determine the position update threshold.\n"
	               "Usage: s_WwisePositionUpdateThresholdMultiplier [0/...]\n"
	               "Default: 0.02\n");

	REGISTER_CVAR2("s_WwiseEventPoolSize", &m_eventPoolSize, m_eventPoolSize, VF_REQUIRE_APP_RESTART,
	               "Sets the number of preallocated events.\n"
	               "Usage: s_WwiseEventPoolSize [0/...]\n"
	               "Default PC: 256, XboxOne: 256, PS4: 256, Mac: 256, Linux: 256, iOS: 256, Android: 256\n");

	REGISTER_CVAR2("s_WwiseSecondaryPoolSize", &m_secondaryMemoryPoolSize, m_secondaryMemoryPoolSize, VF_REQUIRE_APP_RESTART,
	               "Specifies the size (in KiB) of the memory pool to be used by the Wwise audio system implementation.\n"
	               "Usage: s_WwiseSecondaryPoolSize [0/...]\n"
	               "Default PC: 0, XboxOne: 32768 (32 MiB), PS4: 0, Mac: 0, Linux: 0, iOS: 0, Android: 0\n");

	REGISTER_CVAR2("s_WwisePrepareEventMemoryPoolSize", &m_prepareEventMemoryPoolSize, m_prepareEventMemoryPoolSize, VF_REQUIRE_APP_RESTART,
	               "Specifies the size (in KiB) of the Wwise prepare event memory pool.\n"
	               "Usage: s_WwisePrepareEventMemoryPoolSize [0/...]\n"
	               "Default PC: 2048 (2 MiB), XboxOne: 2048 (2 MiB), PS4: 2048 (2 MiB), Mac: 2048 (2 MiB), Linux: 2048 (2 MiB), iOS: 2048 (2 MiB), Android: 2048 (2 MiB)\n");

	REGISTER_CVAR2("s_WwiseStreamManagerMemoryPoolSize", &m_streamManagerMemoryPoolSize, m_streamManagerMemoryPoolSize, VF_REQUIRE_APP_RESTART,
	               "Specifies the size (in KiB) of the Wwise stream manager memory pool.\n"
	               "Usage: s_WwiseStreamManagerMemoryPoolSize [0/...]\n"
	               "Default PC: 64, XboxOne: 64, PS4: 64, Mac: 64, Linux: 64, iOS: 64, Android: 64\n");

	REGISTER_CVAR2("s_WwiseStreamDeviceMemoryPoolSize", &m_streamDeviceMemoryPoolSize, m_streamDeviceMemoryPoolSize, VF_REQUIRE_APP_RESTART,
	               "Specifies the size (in KiB) of the Wwise stream device memory pool.\n"
	               "Usage: s_WwiseStreamDeviceMemoryPoolSize [0/...]\n"
	               "Default PC: 2048 (2 MiB), XboxOne: 2048 (2 MiB), PS4: 2048 (2 MiB), Mac: 2048 (2 MiB), Linux: 2048 (2 MiB), iOS: 2048 (2 MiB), Android: 2048 (2 MiB)\n");

	REGISTER_CVAR2("s_WwiseSoundEngineDefaultMemoryPoolSize", &m_soundEngineDefaultMemoryPoolSize, m_soundEngineDefaultMemoryPoolSize, VF_REQUIRE_APP_RESTART,
	               "Specifies the size (in KiB) of the Wwise sound engine default memory pool.\n"
	               "Usage: s_WwiseSoundEngineDefaultMemoryPoolSize [0/...]\n"
	               "Default PC: 8192 (8 MiB), XboxOne: 8192 (8 MiB), PS4: 8192 (8 MiB), Mac: 8192 (8 MiB), Linux: 8192 (8 MiB), iOS: 8192 (8 MiB), Android: 8192 (8 MiB)\n");

	REGISTER_CVAR2("s_WwiseCommandQueueMemoryPoolSize", &m_commandQueueMemoryPoolSize, m_commandQueueMemoryPoolSize, VF_REQUIRE_APP_RESTART,
	               "Specifies the size (in KiB) of the Wwise command queue memory pool.\n"
	               "Usage: s_WwiseCommandQueueMemoryPoolSize [0/...]\n"
	               "Default PC: 256, XboxOne: 256, PS4: 256, Mac: 256, Linux: 256, iOS: 256, Android: 256\n");

	REGISTER_CVAR2("s_WwiseLowerEngineDefaultPoolSize", &m_lowerEngineDefaultPoolSize, m_lowerEngineDefaultPoolSize, VF_REQUIRE_APP_RESTART,
	               "Specifies the size (in KiB) of the Wwise lower engine memory pool.\n"
	               "Usage: s_WwiseLowerEngineDefaultPoolSize [0/...]\n"
	               "Default PC: 16384 (16 MiB), XboxOne: 16384 (16 MiB), PS4: 16384 (16 MiB), Mac: 16384 (16 MiB), Linux: 16384 (16 MiB), iOS: 16384 (16 MiB), Android: 16384 (16 MiB)\n");

	REGISTER_CVAR2("s_WwiseEnableEventManagerThread", &m_enableEventManagerThread, m_enableEventManagerThread, VF_REQUIRE_APP_RESTART,
	               "Specifies whether Wwise should initialize using its EventManager thread or not.\n"
	               "Usage: s_WwiseEnableEventManagerThread [0/1]\n"
	               "Default: 1 (on)\n");

	REGISTER_CVAR2("s_WwiseEnableSoundBankManagerThread", &m_enableSoundBankManagerThread, m_enableSoundBankManagerThread, VF_REQUIRE_APP_RESTART,
	               "Specifies whether Wwise should initialize using its SoundBankManager thread or not.\n"
	               "Usage: s_WwiseEnableSoundBankManagerThread [0/1]\n"
	               "Default: 1 (on)\n");

	REGISTER_CVAR2("s_WwiseNumSamplesPerFrame", &m_numSamplesPerFrame, m_numSamplesPerFrame, VF_REQUIRE_APP_RESTART,
	               "Specifies the number of samples per audio frame (256, 512, 1024 or 2048).\n"
	               "Usage: s_WwiseNumSamplesPerFrame [256/...]\n"
	               "Default: 512\n");

	REGISTER_CVAR2("s_WwiseNumRefillsInVoice", &m_numRefillsInVoice, m_numRefillsInVoice, VF_REQUIRE_APP_RESTART,
	               "Specifies the number of refill buffers in voice buffer.\n"
	               "Usage: s_WwiseNumRefillsInVoice [2/...]\n"
	               "Default: 3\n");

	REGISTER_CVAR2("s_WwiseChannelConfig", &m_channelConfig, m_channelConfig, VF_REQUIRE_APP_RESTART,
	               "Specifies the channel configuration.\n"
	               "Usage: s_WwiseChannelConfig [51/...]\n"
	               "Default: 0\n"
	               "0: Default speaker setup.\n"
	               "10: 1.0 speaker setup.\n"
	               "11: 1.1 speaker setup.\n"
	               "20: 2.0 speaker setup.\n"
	               "21: 2.1 speaker setup.\n"
	               "30: 3.0 speaker setup.\n"
	               "31: 3.1 speaker setup.\n"
	               "40: 4.0 speaker setup.\n"
	               "41: 4.1 speaker setup.\n"
	               "50: 5.0 speaker setup.\n"
	               "51: 5.1 speaker setup.\n"
	               "60: 6.0 speaker setup.\n"
	               "61: 6.1 speaker setup.\n"
	               "70: 7.0 speaker setup.\n"
	               "71: 7.1 speaker setup.\n");

	REGISTER_CVAR2_CB("s_WwisePanningRule", &m_panningRule, m_panningRule, VF_NULL,
	                  "Specifies the Wwise panning rule.\n"
	                  "Usage: s_WwisePanningRule [0/1]\n"
	                  "Default PC: 0, XboxOne: 0, PS4: 0, Mac: 0, Linux: 0, iOS: 1, Android: 1\n"
	                  "0: Speakers\n"
	                  "1: Headphones\n",
	                  SetPanningRule);

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	REGISTER_CVAR2("s_WwiseEnableCommSystem", &m_enableCommSystem, 0, VF_REQUIRE_APP_RESTART,
	               "Specifies whether Wwise should initialize using its Comm system or not.\n"
	               "This cvar is only available in non-release builds.\n"
	               "Usage: s_WwiseEnableCommSystem [0/1]\n"
	               "Default: 0 (off)\n");

	REGISTER_CVAR2("s_WwiseEnableOutputCapture", &m_enableOutputCapture, 0, VF_REQUIRE_APP_RESTART,
	               "Allows for capturing the output audio to a wav file.\n"
	               "This cvar is only available in non-release builds.\n"
	               "Usage: s_WwiseEnableOutputCapture [0/1]\n"
	               "Default: 0 (off)\n");

	REGISTER_CVAR2("s_WwiseMonitorMemoryPoolSize", &m_monitorMemoryPoolSize, m_monitorMemoryPoolSize, VF_REQUIRE_APP_RESTART,
	               "Specifies the size (in KiB) of the Wwise monitor memory pool.\n"
	               "Usage: s_WwiseMonitorMemoryPoolSize [0/...]\n"
	               "Default PC: 256, XboxOne: 256, PS4: 256, Mac: 256, Linux: 256, iOS: 256, Android: 256\n");

	REGISTER_CVAR2("s_WwiseMonitorQueueMemoryPoolSize", &m_monitorQueueMemoryPoolSize, m_monitorQueueMemoryPoolSize, VF_REQUIRE_APP_RESTART,
	               "Specifies the size (in KiB) of the Wwise monitor queue memory pool.\n"
	               "Usage: s_WwiseMonitorQueueMemoryPoolSize [0/...]\n"
	               "Default PC: 64, XboxOne: 64, PS4: 64, Mac: 64, Linux: 64, iOS: 64, Android: 64\n");

	REGISTER_CVAR2("s_WwiseDebugListFilter", &m_debugListFilter, 192, VF_CHEAT | VF_CHEAT_NOCHECK | VF_BITFIELD,
	               "Defines which lists to show when list filtering is enabled in the debug draw of the audio system.\n"
	               "Usage: s_WwiseDebugListFilter [0ab...] (flags can be combined)\n"
	               "Default: ab\n"
	               "0: Draw nothing.\n"
	               "a: Draw event instances.\n"
	               "b: Draw states values.\n"
	               );
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CCVars::UnregisterVariables()
{
	IConsole* const pConsole = gEnv->pConsole;

	if (pConsole != nullptr)
	{
		pConsole->UnregisterVariable("s_WwiseVelocityTrackingThreshold");
		pConsole->UnregisterVariable("s_WwisePositionUpdateThresholdMultiplier");
		pConsole->UnregisterVariable("s_WwiseEventPoolSize");
		pConsole->UnregisterVariable("s_WwiseSecondaryPoolSize");
		pConsole->UnregisterVariable("s_WwisePrepareEventMemoryPoolSize");
		pConsole->UnregisterVariable("s_WwiseStreamManagerMemoryPoolSize");
		pConsole->UnregisterVariable("s_WwiseStreamDeviceMemoryPoolSize");
		pConsole->UnregisterVariable("s_WwiseSoundEngineDefaultMemoryPoolSize");
		pConsole->UnregisterVariable("s_WwiseCommandQueueMemoryPoolSize");
		pConsole->UnregisterVariable("s_WwiseLowerEngineDefaultPoolSize");
		pConsole->UnregisterVariable("s_WwiseEnableEventManagerThread");
		pConsole->UnregisterVariable("s_WwiseEnableSoundBankManagerThread");
		pConsole->UnregisterVariable("s_WwiseNumSamplesPerFrame");
		pConsole->UnregisterVariable("s_WwiseNumRefillsInVoice");
		pConsole->UnregisterVariable("s_WwiseChannelConfig");
		pConsole->UnregisterVariable("s_WwisePanningRule");

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
		pConsole->UnregisterVariable("s_WwiseEnableCommSystem");
		pConsole->UnregisterVariable("s_WwiseEnableOutputCapture");
		pConsole->UnregisterVariable("s_WwiseMonitorMemoryPoolSize");
		pConsole->UnregisterVariable("s_WwiseMonitorQueueMemoryPoolSize");
		pConsole->UnregisterVariable("s_WwiseDebugListFilter");
#endif      // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
	}
}
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
