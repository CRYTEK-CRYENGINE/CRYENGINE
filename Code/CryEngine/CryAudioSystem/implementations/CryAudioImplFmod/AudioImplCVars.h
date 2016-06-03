// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
class CAudioImplCVars
{
public:

	CAudioImplCVars();
	~CAudioImplCVars();

	void RegisterVariables();
	void UnregisterVariables();

	float m_lowpassMinCutoffFrequency;
	float m_distanceFactor;
	float m_dopplerScale;
	float m_rolloffScale;

	int   m_primaryMemoryPoolSize;
	int   m_maxChannels;
	int   m_enableLiveUpdate;
	int   m_enableSynchronousUpdate;

#if CRY_PLATFORM_DURANGO
	int m_secondaryMemoryPoolSize;
#endif  // CRY_PLATFORM_DURANGO

#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
#endif  // INCLUDE_FMOD_IMPL_PRODUCTION_CODE

	PREVENT_OBJECT_COPY(CAudioImplCVars);
};

extern CAudioImplCVars g_audioImplCVars;
}
}
}
