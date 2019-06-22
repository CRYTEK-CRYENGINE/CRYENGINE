// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
class CCVars final
{
public:

	CCVars(CCVars const&) = delete;
	CCVars(CCVars&&) = delete;
	CCVars& operator=(CCVars const&) = delete;
	CCVars& operator=(CCVars&&) = delete;

	CCVars() = default;

	void RegisterVariables();
	void UnregisterVariables();

	float m_velocityTrackingThreshold = 0.1f;
	float m_positionUpdateThresholdMultiplier = 0.02f;
	int   m_eventPoolSize = 256;
	int   m_secondaryMemoryPoolSize = 0;
	int   m_prepareEventMemoryPoolSize = 0;
	int   m_streamManagerMemoryPoolSize = 0;
	int   m_streamDeviceMemoryPoolSize = 0;
	int   m_soundEngineDefaultMemoryPoolSize = 0;
	int   m_commandQueueMemoryPoolSize = 0;
	int   m_lowerEngineDefaultPoolSize = 0;
	int   m_enableEventManagerThread = 0;
	int   m_enableSoundBankManagerThread = 0;
	int   m_numSamplesPerFrame = 0;
	int   m_numRefillsInVoice = 0;
	int   m_channelConfig = 0;
	int   m_panningRule = 0;

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	int m_enableCommSystem = 0;
	int m_enableOutputCapture = 0;
	int m_monitorMemoryPoolSize = 0;
	int m_monitorQueueMemoryPoolSize = 0;
	int m_debugListFilter = 0;
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
};

extern CCVars g_cvars;
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
