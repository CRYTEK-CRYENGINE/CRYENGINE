// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QMainWindow>
#include <IEditor.h>
#include "AudioAssetsManager.h"
#include <QFileSystemWatcher>
#include <QtViewPane.h>
#include <qobjectdefs.h>
#include <FileSystem/FileSystem_SubTreeMonitor.h>
#include <FileSystem/FileSystem_Enumerator.h>
#include <FileSystem/FileSystem_FileFilter.h>

class QSplitter;

namespace ACE
{
class CAudioAssetsManager;
class CAudioAssetsExplorer;
class CInspectorPanel;
class CAudioSystemPanel;
class CAudioControl;

class CAudioControlsEditorWindow : public CDockableWindow, public IEditorNotifyListener
{
	Q_OBJECT
public:
	CAudioControlsEditorWindow();
	virtual ~CAudioControlsEditorWindow() override;
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event) override;

	//////////////////////////////////////////////////////////
	// CDockableWindow implementation
	virtual const char*                       GetPaneTitle() const override        { return "Audio Controls Editor"; };
	virtual IViewPaneClass::EDockingDirection GetDockingDirection() const override { return IViewPaneClass::DOCK_FLOAT; }
	//////////////////////////////////////////////////////////

	void ReloadMiddlewareData();

private:
	void Reload();
	void Save();
	void FilterControlType(EItemType type, bool bShow);
	void Update();
	void CheckErrorMask();

protected:
	virtual void keyPressEvent(QKeyEvent* pEvent) override;
	virtual void closeEvent(QCloseEvent* pEvent) override;

private:
	void UpdateAudioSystemData();
	void StartWatchingFolder(const QString& folderPath);

	CAudioAssetsManager*                                m_pAssetsManager;
	CAudioAssetsExplorer*                               m_pExplorer;
	CInspectorPanel*                                    m_pInspectorPanel;
	CAudioSystemPanel*                                  m_pAudioSystemPanel;
	FileSystem::SubTreeMonitorPtr                       m_pMonitor;
	std::vector<FileSystem::CEnumerator::MonitorHandle> m_watchingHandles;
	FileSystem::SFileFilter                             m_filter;
	bool       m_allowedTypes[EItemType::eItemType_NumTypes];

	QSplitter* m_pSplitter;

};
} // namespace ACE
