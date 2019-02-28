// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/File/IFileChangeMonitor.h>
#include <QTimer>

namespace ACE
{
class CFileMonitorBase : public QTimer, public IFileChangeListener
{
	Q_OBJECT

public:

	void Disable();

signals:

	void SignalReloadData();

protected:

	explicit CFileMonitorBase(int const delay, QObject* const pParent);
	virtual ~CFileMonitorBase() override;

	CFileMonitorBase() = delete;

	// IFileChangeListener
	virtual void OnFileChange(char const* szFileName, EChangeType type) override;
	// ~IFileChangeListener

	int const m_delay;
};
} // namespace ACE
