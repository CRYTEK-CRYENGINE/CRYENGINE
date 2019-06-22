// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct ICVar;
struct IConsoleCmdArgs;

namespace CryAudio
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

#if CRY_PLATFORM_DURANGO
	int m_fileCacheManagerSize = 384 << 10;
#endif // CRY_PLATFORM_DURANGO

	int    m_objectPoolSize = 256;
	int    m_triggerInstancePoolSize = 512;
	int    m_ignoreWindowFocus = 0;
	int    m_poolAllocationMode = 0;
	ICVar* m_pListeners = nullptr;

#if defined(CRY_AUDIO_USE_OCCLUSION)
	int   m_occlusionCollisionTypes = 0;
	int   m_occlusionSetFullOnMaxHits = 0;
	int   m_occlusionInitialRayCastMode = 1;
	int   m_occlusionAccumulate = 1;
	float m_occlusionMaxDistance = 500.0f;
	float m_occlusionMinDistance = 0.1f;
	float m_occlusionMaxSyncDistance = 10.0f;
	float m_occlusionHighDistance = 10.0f;
	float m_occlusionMediumDistance = 80.0f;
	float m_occlusionListenerPlaneSize = 1.0f;
	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	int   m_occlusionGlobalType = 0;
	#endif // CRY_AUDIO_USE_DEBUG_CODE
#endif   // CRY_AUDIO_USE_OCCLUSION

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	float  m_debugDistance = 0.0f;
	int    m_loggingOptions = 0;
	int    m_drawDebug = 0;
	int    m_fileCacheManagerDebugFilter = 0;
	int    m_hideInactiveObjects = 1;
	ICVar* m_pDebugFilter = nullptr;
#endif // CRY_AUDIO_USE_DEBUG_CODE
};

extern CCVars g_cvars;
} // namespace CryAudio
