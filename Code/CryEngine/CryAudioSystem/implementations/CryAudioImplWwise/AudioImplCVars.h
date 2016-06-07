// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
class CAudioImplCVars
{
public:

	CAudioImplCVars();
	~CAudioImplCVars();

	void RegisterVariables();
	void UnregisterVariables();

	int m_primaryMemoryPoolSize;
	int m_secondaryMemoryPoolSize;
	int m_prepareEventMemoryPoolSize;
	int m_streamManagerMemoryPoolSize;
	int m_streamDeviceMemoryPoolSize;
	int m_soundEngineDefaultMemoryPoolSize;
	int m_commandQueueMemoryPoolSize;
	int m_lowerEngineDefaultPoolSize;

#if defined(INCLUDE_WWISE_IMPL_PRODUCTION_CODE)
	int m_enableCommSystem;
	int m_enableOutputCapture;
	int m_monitorMemoryPoolSize;
	int m_monitorQueueMemoryPoolSize;
#endif  // INCLUDE_WWISE_IMPL_PRODUCTION_CODE

	PREVENT_OBJECT_COPY(CAudioImplCVars);
};

extern CAudioImplCVars g_audioImplCVars;
}
}
}
