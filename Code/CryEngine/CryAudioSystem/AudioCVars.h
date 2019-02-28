// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioSystem.h>

struct ICVar;
struct IConsoleCmdArgs;

namespace CryAudio
{
class CCVars final
{
public:

	CCVars() = default;
	CCVars(CCVars const&) = delete;
	CCVars& operator=(CCVars const&) = delete;

	void    RegisterVariables();
	void    UnregisterVariables();

	int   m_fileCacheManagerSize = 0;
	int   m_audioObjectPoolSize = 0;
	int   m_audioEventPoolSize = 0;
	int   m_audioStandaloneFilePoolSize = 0;
	int   m_accumulateOcclusion = 1;
	int   m_ignoreWindowFocus = 0;

	float m_occlusionMaxDistance = 500.0f;
	float m_occlusionMinDistance = 0.1f;
	float m_occlusionMaxSyncDistance = 0.0f;
	float m_occlusionHighDistance = 0.0f;
	float m_occlusionMediumDistance = 0.0f;
	float m_fullObstructionMaxDistance = 0.0f;
	float m_positionUpdateThresholdMultiplier = 0.02f;
	float m_velocityTrackingThreshold = 0.0f;
	float m_occlusionRayLengthOffset = 0.0f;
	float m_listenerOcclusionPlaneSize = 0.0f;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	float  m_debugDistance = 0.0f;
	int    m_drawAudioDebug = 0;
	int    m_fileCacheManagerDebugFilter = 0;
	int    m_hideInactiveAudioObjects = 0;
	int    m_audioObjectsRayType = 0;
	ICVar* m_pDebugFilter = nullptr;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

private:

	static void CmdExecuteTrigger(IConsoleCmdArgs* pCmdArgs);
	static void CmdStopTrigger(IConsoleCmdArgs* pCmdArgs);
	static void CmdSetParameter(IConsoleCmdArgs* pCmdArgs);
	static void CmdSetSwitchState(IConsoleCmdArgs* pCmdArgs);
};

extern CCVars g_cvars;
} // namespace CryAudio
