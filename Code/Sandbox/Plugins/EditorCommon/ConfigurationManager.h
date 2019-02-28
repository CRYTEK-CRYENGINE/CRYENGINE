// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include "dll_string.h"

struct ICVar;

class EDITOR_COMMON_API CConfigurationManager
{
public:
	CConfigurationManager();
	void                           Init();
	const std::vector<dll_string>& GetPlatformNames() const { return m_platformNames; }

private:
	std::vector<dll_string> m_platformNames;

	ICVar*                  m_pPlatformsCvar;
};

