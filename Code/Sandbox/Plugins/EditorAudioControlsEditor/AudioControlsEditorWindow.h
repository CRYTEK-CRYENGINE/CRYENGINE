// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <EditorFramework/Editor.h>
#include <IEditor.h>
#include "SystemAssetsManager.h"
#include <QFileSystemWatcher>
#include <QtViewPane.h>
#include <qobjectdefs.h>

class QAction;
class QVBoxLayout;

namespace ACE
{
class CSystemAssetsManager;
class CSystemControlsWidget;
class CPropertiesWidget;
class CMiddlewareDataWidget;
class CSystemAsset;
class CSystemControl;
class CFileMonitorSystem;
class CFileMonitorMiddleware;

class CAudioControlsEditorWindow final : public CDockableEditor, public IEditorNotifyListener
{
	Q_OBJECT

public:

	CAudioControlsEditorWindow();
	virtual ~CAudioControlsEditorWindow() override;

	// IEditorNotifyListener
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event) override;
	// ~IEditorNotifyListener

	// CDockableEditor
	virtual char const* GetEditorName() const override { return "Audio Controls Editor"; }
	// ~CDockableEditor

	// IPane
	virtual IViewPaneClass::EDockingDirection GetDockingDirection() const override { return IViewPaneClass::DOCK_FLOAT; }
	// ~IPane

	void ReloadSystemData();
	void ReloadMiddlewareData();

protected:

	// QWidget
	virtual void keyPressEvent(QKeyEvent* pEvent) override;
	virtual void closeEvent(QCloseEvent* pEvent) override;
	// ~QWidget

	// CEditor
	virtual void CreateDefaultLayout(CDockableContainer* pSender) override;
	virtual bool CanQuit(std::vector<string>& unsavedChanges) override;
	// ~CEditor

protected slots:

	void OnSystemControlsWidgetDestruction(QObject* const pObject);
	void OnPropertiesWidgetDestruction(QObject* const pObject);
	void OnMiddlewareDataWidgetDestruction(QObject* const pObject);

signals:

	void OnSelectedSystemControlChanged();
	void OnStartTextFiltering();
	void OnStopTextFiltering();

private slots:

	void OnPreferencesDialog();

private:

	void InitMenu();
	void InitToolbar(QVBoxLayout* const pWindowLayout);
	void RegisterWidgets();
	void Reload();
	void Save();
	void SaveBeforeImplementationChange();
	void CheckErrorMask();
	void UpdateAudioSystemData();
	void RefreshAudioSystem();
	void BackupTreeViewStates();
	void RestoreTreeViewStates();
	void SelectConnectedSystemControl(CSystemControl const* const pControl);
	bool TryClose();

	std::vector<CSystemAsset*> GetSelectedSystemAssets();

	CSystemControlsWidget* CreateSystemControlsWidget();
	CPropertiesWidget*     CreatePropertiesWidget();
	CMiddlewareDataWidget* CreateMiddlewareDataWidget();

	CSystemAssetsManager*                   m_pAssetsManager;
	CSystemControlsWidget*                  m_pSystemControlsWidget;
	CPropertiesWidget*                      m_pPropertiesWidget;
	CMiddlewareDataWidget*                  m_pMiddlewareDataWidget;
	QAction*                                m_pSaveAction;
	std::unique_ptr<CFileMonitorSystem>     m_pMonitorSystem;
	std::unique_ptr<CFileMonitorMiddleware> m_pMonitorMiddleware;
	bool                                    m_isModified = false;
};
} // namespace ACE
