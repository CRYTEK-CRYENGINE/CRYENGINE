// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include <string>

class CRenderPassBase
{
public:
	void SetLabel(const char* label)  { m_label = label; }
	const char* GetLabel() const      { return m_label.c_str(); }

protected:
	std::string m_label;
};