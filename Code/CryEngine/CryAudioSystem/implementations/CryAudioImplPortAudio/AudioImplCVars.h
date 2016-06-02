// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace CryAudio
{
namespace Impl
{
namespace PortAudio
{
class CAudioImplCVars
{
public:

	CAudioImplCVars();
	~CAudioImplCVars();

	void RegisterVariables();
	void UnregisterVariables();

	int m_primaryMemoryPoolSize;

	PREVENT_OBJECT_COPY(CAudioImplCVars);
};

extern CAudioImplCVars g_audioImplCVars;
}
}
}
