// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/File/IFileChangeMonitor.h>
#include <QTimer>

namespace ACE
{
class CAudioControlsEditorWindow;

class CFileMonitor : public QTimer, public IFileChangeListener
{
	Q_OBJECT

protected:

	CFileMonitor(CAudioControlsEditorWindow* const window, int const delay);

	virtual ~CFileMonitor() override;

public:

	void Disable();

protected:

	// IFileChangeListener
	virtual void OnFileChange(char const* filename, EChangeType eType) override;
	// ~IFileChangeListener

	virtual void ReloadData() {}

	CAudioControlsEditorWindow* m_window;
	int const                   m_delay;
};

class CFileMonitorSystem final : public CFileMonitor
{
	Q_OBJECT

public:

	CFileMonitorSystem(CAudioControlsEditorWindow* const window, int const delay);

	void Enable();
	void EnableDelayed();

private:

	// CFileMonitor
	virtual void ReloadData() override;
	// ~CFileMonitor

	string const m_assetFolder;
	QTimer*      m_delayTimer;
};

class CFileMonitorMiddleware final : public CFileMonitor
{
	Q_OBJECT

public:

	CFileMonitorMiddleware(CAudioControlsEditorWindow* const window, int const delay);

	void Enable();

private:

	// CFileMonitor
	virtual void ReloadData() override;
	// ~CFileMonitor

	std::vector<char const*> m_monitorFolders;
};
} // namespace ACE

