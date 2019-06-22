// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"
#include <EditorFramework/Editor.h>
#include <CrySystem/ISystem.h>

class QAction;
class QCommandAction;

namespace ACE
{
class CSystemControlsWidget;
class CPropertiesWidget;
class CMiddlewareDataWidget;
class CContextWidget;
class CAsset;
class CControl;
class CFileMonitorSystem;
class CFileMonitorMiddleware;

class CMainWindow final : public CDockableEditor, public ISystemEventListener
{
	Q_OBJECT

public:

	CMainWindow(CMainWindow const&) = delete;
	CMainWindow(CMainWindow&&) = delete;
	CMainWindow& operator=(CMainWindow const&) = delete;
	CMainWindow& operator=(CMainWindow&&) = delete;

	CMainWindow();
	virtual ~CMainWindow() override;

	// CDockableEditor
	virtual void        Initialize() override;
	virtual char const* GetEditorName() const override { return g_szEditorName; }
	// ~CDockableEditor

	// IPane
	virtual IViewPaneClass::EDockingDirection GetDockingDirection() const override { return IViewPaneClass::DOCK_FLOAT; }
	// ~IPane

	void ReloadMiddlewareData();

protected slots:

	void OnSystemControlsWidgetDestruction(QObject* const pObject);
	void OnPropertiesWidgetDestruction(QObject* const pObject);
	void OnMiddlewareDataWidgetDestruction(QObject* const pObject);
	void OnContextWidgetDestruction(QObject* const pObject);

private slots:

	void OnPreferencesDialog();

private:

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener

	// QWidget
	virtual void keyPressEvent(QKeyEvent* pEvent) override;
	virtual void closeEvent(QCloseEvent* pEvent) override;
	// ~QWidget

	bool OnReload();
	bool OnRefresh();
	bool OnSave();

	// CEditor
	virtual void CreateDefaultLayout(CDockableContainer* pSender) override;
	virtual bool CanQuit(std::vector<string>& unsavedChanges) override;
	// ~CEditor

	void                   InitMenu();
	void                   UpdateState();
	void                   RegisterActions();
	void                   RegisterWidgets();
	void                   Reload(bool const hasImplChanged);
	void                   SaveBeforeImplChange();
	void                   ReloadSystemData();
	bool                   TryClose();

	Assets                 GetSelectedAssets();

	CSystemControlsWidget* CreateSystemControlsWidget();
	CPropertiesWidget*     CreatePropertiesWidget();
	CMiddlewareDataWidget* CreateMiddlewareDataWidget();
	CContextWidget*        CreateContextWidget();

	QCommandAction*           m_pSaveAction;
	QCommandAction*           m_pRefreshAction;
	QCommandAction*           m_pReloadAction;
	QAction*                  m_pPreferencesAction;
	CFileMonitorSystem* const m_pMonitorSystem;
	bool                      m_isModified;
	bool                      m_isReloading;
};
} // namespace ACE
