// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
class CImpl;

class CCVars final
{
public:

	CCVars() = default;
	CCVars(CCVars const&) = delete;
	CCVars(CCVars&&) = delete;
	CCVars& operator=(CCVars const&) = delete;
	CCVars& operator=(CCVars&&) = delete;

	void    RegisterVariables();
	void    UnregisterVariables();

	int   m_cuePoolSize = 256;
	int   m_maxVirtualVoices = 16;
	int   m_maxVoiceLimitGroups = 16;
	int   m_maxCategories = 16;
	int   m_categoriesPerPlayback = 4;
	int   m_maxTracks = 32;
	int   m_maxTrackItems = 32;
	int   m_maxFaders = 4;
	int   m_numVoices = 8;
	int   m_maxChannels = 2;
	int   m_maxSamplingRate = 48000;
	int   m_numBuses = 8;
	int   m_outputChannels = 6;
	int   m_outputSamplingRate = 48000;
	int   m_maxStreams = 8;
	int   m_maxFiles = 32;
	int   m_voiceAllocationMethod = 0;

	float m_maxPitch = 2400.0f;
	float m_velocityTrackingThreshold = 0.1f;
	float m_positionUpdateThresholdMultiplier = 0.02f;
	float m_maxVelocity = 100.0f;

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	int m_debugListFilter = 0;
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
};

extern CCVars g_cvars;
} // namespace Adx2
} // namespace Impl
} // namespace CryAudio
