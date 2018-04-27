// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "FileMonitorBase.h"

namespace ACE
{
class CFileMonitorMiddleware final : public CFileMonitorBase
{
	Q_OBJECT

public:

	explicit CFileMonitorMiddleware(int const delay, QObject* const pParent);

	CFileMonitorMiddleware() = delete;

	void Enable();

private:

	std::vector<char const*> m_monitorFolders;
};
} // namespace ACE
