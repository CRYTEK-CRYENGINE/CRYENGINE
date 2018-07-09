// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "FileMonitorBase.h"

namespace ACE
{
class CFileMonitorSystem final : public CFileMonitorBase
{
	Q_OBJECT

public:

	explicit CFileMonitorSystem(int const delay, QObject* const pParent);

	CFileMonitorSystem() = delete;

	void Enable();
	void EnableDelayed();

private:

	string        m_monitorFolder;
	QTimer* const m_delayTimer;
};
} // namespace ACE
