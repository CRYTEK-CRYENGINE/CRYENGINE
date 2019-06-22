// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MainWindow.h"

#include "AudioControlsEditorPlugin.h"
#include "AssetsManager.h"
#include "ImplManager.h"
#include "PreferencesDialog.h"
#include "SystemControlsWidget.h"
#include "PropertiesWidget.h"
#include "MiddlewareDataWidget.h"
#include "ContextWidget.h"
#include "FileMonitorMiddleware.h"
#include "FileMonitorSystem.h"
#include "Common/IImpl.h"
#include "Common/ModelUtils.h"

#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include <Commands/QCommandAction.h>
#include <QtUtil.h>
#include <Controls/QuestionDialog.h>

#include <QAction>
#include <QGuiApplication>
#include <QKeyEvent>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
void OnBeforeReload()
{
	if (g_pSystemControlsWidget != nullptr)
	{
		g_pSystemControlsWidget->OnBeforeReload();
	}

	if (g_pIImpl != nullptr)
	{
		g_pIImpl->OnBeforeReload();
	}

	if (g_pPropertiesWidget != nullptr)
	{
		g_pPropertiesWidget->OnBeforeReload();
	}

	if (g_pContextWidget != nullptr)
	{
		g_pContextWidget->OnBeforeReload();
	}
}

//////////////////////////////////////////////////////////////////////////
void OnAfterReload()
{
	if (g_pSystemControlsWidget != nullptr)
	{
		g_pSystemControlsWidget->OnAfterReload();
	}

	if (g_pIImpl != nullptr)
	{
		g_pIImpl->OnAfterReload();
	}

	if (g_pPropertiesWidget != nullptr)
	{
		g_pPropertiesWidget->OnAfterReload();
	}

	if (g_pContextWidget != nullptr)
	{
		g_pContextWidget->OnAfterReload();
	}
}

//////////////////////////////////////////////////////////////////////////
CMainWindow::CMainWindow()
	: m_pSaveAction(nullptr)
	, m_pRefreshAction(nullptr)
	, m_pReloadAction(nullptr)
	, m_pPreferencesAction(nullptr)
	, m_pMonitorSystem(new CFileMonitorSystem(1000, this))
	, m_isModified(false)
	, m_isReloading(false)
{
	g_pMainWindow = this;
	g_pFileMonitorMiddleware = new CFileMonitorMiddleware(500, this);

	setAttribute(Qt::WA_DeleteOnClose);
	setObjectName(GetEditorName());
	RegisterActions();

	ModelUtils::InitIcons();

	if (g_assetsManager.IsLoading())
	{
		// The middleware is being swapped out therefore we must not
		// reload it and must not call signals!
		CAudioControlsEditorPlugin::ReloadData(EReloadFlags::ReloadSystemControls);
	}
	else
	{
		CAudioControlsEditorPlugin::ReloadData(EReloadFlags::ReloadSystemControls | EReloadFlags::ReloadImplData | EReloadFlags::SendSignals);
	}

	m_isModified = g_assetsManager.IsDirty();

	RegisterWidgets();

	if (g_pIImpl != nullptr)
	{
		g_assetsManager.UpdateAllConnectionStates();
	}

	QObject::connect(m_pMonitorSystem, &CFileMonitorSystem::SignalReloadData, this, &CMainWindow::ReloadSystemData);
	QObject::connect(g_pFileMonitorMiddleware, &CFileMonitorMiddleware::SignalReloadData, this, &CMainWindow::ReloadMiddlewareData);

	g_assetsManager.SignalIsDirty.Connect([this](bool const isDirty)
		{
			m_isModified = isDirty;
			m_pSaveAction->setEnabled(isDirty);
		}, reinterpret_cast<uintptr_t>(this));

	g_implManager.SignalOnBeforeImplChange.Connect(this, &CMainWindow::SaveBeforeImplChange);
	g_implManager.SignalOnAfterImplChange.Connect([this]()
		{
			UpdateState();
			Reload(true);
		}, reinterpret_cast<uintptr_t>(this));

	GetISystem()->GetISystemEventDispatcher()->RegisterListener(this, "CAudioControlsEditorMainWindow");
}

//////////////////////////////////////////////////////////////////////////
CMainWindow::~CMainWindow()
{
	g_implManager.SignalOnBeforeImplChange.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_implManager.SignalOnAfterImplChange.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_assetsManager.SignalIsDirty.DisconnectById(reinterpret_cast<uintptr_t>(this));
	GetISystem()->GetISystemEventDispatcher()->RemoveListener(this);
	g_pMainWindow = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::Initialize()
{
	CDockableEditor::Initialize();

	InitMenu();
	UpdateState();
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::UpdateState()
{
	bool const middleWareFound = g_pIImpl != nullptr;

	m_pSaveAction->setEnabled(middleWareFound && m_isModified);
	m_pReloadAction->setEnabled(middleWareFound);
	m_pRefreshAction->setEnabled(middleWareFound);
	m_pPreferencesAction->setEnabled(middleWareFound);

	setWindowTitle(QString("%1 (%2)").arg(g_szEditorName).arg(middleWareFound ? g_implInfo.name.c_str() : "Warning: No middleware implementation!"));
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::InitMenu()
{
	AddToMenu({ CEditor::MenuItems::FileMenu, CEditor::MenuItems::Save, CEditor::MenuItems::EditMenu });
	CAbstractMenu* const pMenuEdit = GetMenu(MenuItems::EditMenu);

	pMenuEdit->AddCommandAction(m_pReloadAction);
	pMenuEdit->AddCommandAction(m_pRefreshAction);

	int const section = pMenuEdit->GetNextEmptySection();
	m_pPreferencesAction = pMenuEdit->CreateAction(tr("Preferences..."), section);
	QObject::connect(m_pPreferencesAction, &QAction::triggered, this, &CMainWindow::OnPreferencesDialog);
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::RegisterActions()
{
	m_pReloadAction = RegisterAction("general.reload", &CMainWindow::OnReload);
	m_pRefreshAction = RegisterAction("general.refresh", &CMainWindow::OnRefresh);
	m_pSaveAction = RegisterAction("general.save", &CMainWindow::OnSave);

	// Create and get refresh action so we can provide extra context through text and icon
	// This action will benefit from sharing the general.refresh shortcut
	m_pRefreshAction->setText("Refresh Audio System");
	m_pRefreshAction->setIcon(CryIcon("icons:Audio/Refresh_Audio.ico"));
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::RegisterWidgets()
{
	EnableDockingSystem();

	RegisterDockableWidget("Audio System Controls", [&]() { return CreateSystemControlsWidget(); }, true, false);
	RegisterDockableWidget("Properties", [&]() { return CreatePropertiesWidget(); }, true, false);
	RegisterDockableWidget("Contexts", [&]() { return CreateContextWidget(); }, true, false);
	RegisterDockableWidget("Middleware Data", [&]() { return CreateMiddlewareDataWidget(); }, true, false);
}

//////////////////////////////////////////////////////////////////////////
CSystemControlsWidget* CMainWindow::CreateSystemControlsWidget()
{
	auto pSystemControlsWidget = new CSystemControlsWidget(this);

	if (g_pSystemControlsWidget == nullptr)
	{
		g_pSystemControlsWidget = pSystemControlsWidget;
	}

	QObject::connect(pSystemControlsWidget, &QObject::destroyed, this, &CMainWindow::OnSystemControlsWidgetDestruction);

	return pSystemControlsWidget;
}

//////////////////////////////////////////////////////////////////////////
CPropertiesWidget* CMainWindow::CreatePropertiesWidget()
{
	auto pPropertiesWidget = new CPropertiesWidget(this);

	if (g_pPropertiesWidget == nullptr)
	{
		g_pPropertiesWidget = pPropertiesWidget;
	}

	QObject::connect(pPropertiesWidget, &QObject::destroyed, this, &CMainWindow::OnPropertiesWidgetDestruction);

	g_pPropertiesWidget->OnSetSelectedAssets(GetSelectedAssets(), !m_isReloading);
	return pPropertiesWidget;
}

//////////////////////////////////////////////////////////////////////////
CMiddlewareDataWidget* CMainWindow::CreateMiddlewareDataWidget()
{
	auto pMiddlewareDataWidget = new CMiddlewareDataWidget(this);

	if (g_pMiddlewareDataWidget == nullptr)
	{
		g_pMiddlewareDataWidget = pMiddlewareDataWidget;
	}

	QObject::connect(pMiddlewareDataWidget, &QObject::destroyed, this, &CMainWindow::OnMiddlewareDataWidgetDestruction);

	return pMiddlewareDataWidget;
}

//////////////////////////////////////////////////////////////////////////
CContextWidget* CMainWindow::CreateContextWidget()
{
	auto pContextWidget = new CContextWidget(this);

	if (g_pContextWidget == nullptr)
	{
		g_pContextWidget = pContextWidget;
	}

	QObject::connect(pContextWidget, &QObject::destroyed, this, &CMainWindow::OnContextWidgetDestruction);

	return pContextWidget;
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::OnSystemControlsWidgetDestruction(QObject* const pObject)
{
	auto pWidget = static_cast<CSystemControlsWidget*>(pObject);

	if (g_pSystemControlsWidget == pWidget)
	{
		g_pSystemControlsWidget = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::OnPropertiesWidgetDestruction(QObject* const pObject)
{
	auto pWidget = static_cast<CPropertiesWidget*>(pObject);

	if (g_pPropertiesWidget == pWidget)
	{
		g_pPropertiesWidget = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::OnMiddlewareDataWidgetDestruction(QObject* const pObject)
{
	auto pWidget = static_cast<CMiddlewareDataWidget*>(pObject);

	if (g_pMiddlewareDataWidget == pWidget)
	{
		g_pMiddlewareDataWidget = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::OnContextWidgetDestruction(QObject* const pObject)
{
	auto pWidget = static_cast<CContextWidget*>(pObject);

	if (g_pContextWidget == pWidget)
	{
		g_pContextWidget = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::CreateDefaultLayout(CDockableContainer* pSender)
{
	CRY_ASSERT_MESSAGE(pSender != nullptr, "Dockable container is null pointer during %s", __FUNCTION__);

	pSender->SpawnWidget("Audio System Controls");
	QWidget* pPropertiesWidget = pSender->SpawnWidget("Properties", QToolWindowAreaReference::VSplitRight);
	QWidget* pMiddlewareWidget = pSender->SpawnWidget("Middleware Data", QToolWindowAreaReference::VSplitRight);
	pSender->SetSplitterSizes(pMiddlewareWidget, { 1, 1, 1 });
	QWidget* pContextWidget = pSender->SpawnWidget("Contexts", pPropertiesWidget, QToolWindowAreaReference::HSplitBottom);
	pSender->SetSplitterSizes(pContextWidget, { 2, 1 });
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::closeEvent(QCloseEvent* pEvent)
{
	if (TryClose())
	{
		pEvent->accept();
	}
	else
	{
		pEvent->ignore();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::keyPressEvent(QKeyEvent* pEvent)
{
	if ((g_pSystemControlsWidget != nullptr) && (!g_pSystemControlsWidget->IsEditing()))
	{
		if ((pEvent->key() == Qt::Key_S) && (pEvent->modifiers() == Qt::ControlModifier))
		{
			OnSave();
		}
		else if ((pEvent->key() == Qt::Key_Z) && (pEvent->modifiers() & Qt::ControlModifier))
		{
			if (pEvent->modifiers() & Qt::ShiftModifier)
			{
				// GetIEditor()->GetIUndoManager()->Redo();
			}
			else
			{
				// GetIEditor()->GetIUndoManager()->Undo();
			}
		}
	}

	QWidget::keyPressEvent(pEvent);
}

//////////////////////////////////////////////////////////////////////////
bool CMainWindow::OnReload()
{
	Reload(false);
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::Reload(bool const hasImplChanged)
{
	m_pMonitorSystem->Disable();
	g_pFileMonitorMiddleware->Disable();

	bool shouldReload = true;

	if (m_isModified)
	{
		auto const messageBox = new CQuestionDialog();
		messageBox->SetupQuestion(tr(GetEditorName()), tr("If you reload you will lose all your unsaved changes.\nAre you sure you want to reload?"), QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::No);
		shouldReload = (messageBox->Execute() == QDialogButtonBox::Yes);
	}

	if (shouldReload)
	{
		m_isReloading = true;
		QGuiApplication::setOverrideCursor(Qt::WaitCursor);

		if (!hasImplChanged)
		{
			OnBeforeReload();
		}

		if (g_assetsManager.IsLoading())
		{
			// The middleware is being swapped out therefore we must not
			// reload it and must not call signals!
			CAudioControlsEditorPlugin::ReloadData(EReloadFlags::ReloadSystemControls);
		}
		else
		{
			CAudioControlsEditorPlugin::ReloadData(EReloadFlags::ReloadSystemControls | EReloadFlags::ReloadImplData | EReloadFlags::SendSignals);
		}

		if (g_pSystemControlsWidget != nullptr)
		{
			g_pSystemControlsWidget->Reset();
		}

		if (!hasImplChanged)
		{
			if (g_pPropertiesWidget != nullptr)
			{
				g_pPropertiesWidget->OnSetSelectedAssets(GetSelectedAssets(), false);
				g_pPropertiesWidget->Reset();
			}
		}

		if (g_pIImpl != nullptr)
		{
			g_assetsManager.UpdateAllConnectionStates();
		}

		if (!hasImplChanged)
		{
			OnAfterReload();
		}

		QGuiApplication::restoreOverrideCursor();
		m_isReloading = false;
	}

	m_pMonitorSystem->Enable();
	g_pFileMonitorMiddleware->Enable();
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::SaveBeforeImplChange()
{
	if (m_isModified)
	{
		auto const messageBox = new CQuestionDialog();
		messageBox->SetupQuestion(tr(GetEditorName()), tr("Middleware implementation changed.\nThere are unsaved changes.\nDo you want to save before reloading?"), QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::No);

		if (messageBox->Execute() == QDialogButtonBox::Yes)
		{
			g_assetsManager.ClearDirtyFlags();
			OnSave();
		}
	}

	g_assetsManager.ClearDirtyFlags();
}

//////////////////////////////////////////////////////////////////////////
bool CMainWindow::OnSave()
{
	QGuiApplication::setOverrideCursor(Qt::WaitCursor);
	m_pMonitorSystem->Disable();
	CAudioControlsEditorPlugin::SaveData();
	gEnv->pAudioSystem->ReloadControlsData();
	m_pMonitorSystem->EnableDelayed();
	QGuiApplication::restoreOverrideCursor();

	// if preloads have been modified, ask the user if s/he wants to refresh the audio system
	if (g_assetsManager.IsTypeDirty(EAssetType::Preload))
	{
		auto const messageBox = new CQuestionDialog();
		messageBox->SetupQuestion(tr(GetEditorName()), tr("Preload requests have been modified. \n\nFor the new data to be loaded the audio system needs to be refreshed, this will stop all currently playing audio. Do you want to do this now?. \n\nYou can always refresh manually at a later time through the Audio menu."), QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::No);

		if (messageBox->Execute() == QDialogButtonBox::Yes)
		{
			OnRefresh();
		}
	}

	bool const shouldReload = g_assetsManager.ShouldReloadAfterSave();
	g_assetsManager.ClearDirtyFlags();

	if (shouldReload)
	{
		Reload(false);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CMainWindow::OnRefresh()
{
	GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_AUDIO_REFRESH, 0, 0);
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_AUDIO_LANGUAGE_CHANGED:
		{
			ReloadMiddlewareData();
			break;
		}
	default:
		{
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::ReloadSystemData()
{
	m_pMonitorSystem->Disable();
	bool shouldReload = true;

	if (m_isModified)
	{
		auto const messageBox = new CQuestionDialog();
		char const* const szMessageText = "External changes have been made to audio controls files.\nIf you reload you will lose all your unsaved changes.\nAre you sure you want to reload?";
		messageBox->SetupQuestion(tr(GetEditorName()), tr(szMessageText), QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::No);
		shouldReload = (messageBox->Execute() == QDialogButtonBox::Yes);
	}

	if (shouldReload)
	{
		g_assetsManager.ClearDirtyFlags();
		Reload(false);
	}
	else
	{
		m_pMonitorSystem->Enable();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::ReloadMiddlewareData()
{
	g_pFileMonitorMiddleware->Disable();
	QGuiApplication::setOverrideCursor(Qt::WaitCursor);
	OnBeforeReload();

	CAudioControlsEditorPlugin::ReloadData(EReloadFlags::ReloadImplData | EReloadFlags::BackupConnections);

	if (g_pPropertiesWidget != nullptr)
	{
		g_pPropertiesWidget->Reset();
	}

	OnAfterReload();
	QGuiApplication::restoreOverrideCursor();
	g_pFileMonitorMiddleware->Enable();
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::OnPreferencesDialog()
{
	auto const pPreferencesDialog = new CPreferencesDialog(this);

	QObject::connect(pPreferencesDialog, &CPreferencesDialog::SignalOnBeforeImplSettingsChange, [&]()
		{
			OnBeforeReload();
		});

	QObject::connect(pPreferencesDialog, &CPreferencesDialog::SignalOnAfterImplSettingsChanged, [&]()
		{
			if (g_pPropertiesWidget != nullptr)
			{
			  g_pPropertiesWidget->Reset();
			}

			OnAfterReload();
			g_pFileMonitorMiddleware->Enable();
		});

	pPreferencesDialog->exec();
}

//////////////////////////////////////////////////////////////////////////
Assets CMainWindow::GetSelectedAssets()
{
	Assets assets;

	if (g_pSystemControlsWidget != nullptr)
	{
		assets = g_pSystemControlsWidget->GetSelectedAssets();
	}

	return assets;
}

//////////////////////////////////////////////////////////////////////////
bool CMainWindow::TryClose()
{
	bool canClose = true;

	if (m_isModified)
	{
		if (!GetIEditor()->IsMainFrameClosing())
		{
			QString const title = tr("Closing %1").arg(GetEditorName());
			auto const button = CQuestionDialog::SQuestion(title, tr("There are unsaved changes.\nDo you want to save your changes?"), QDialogButtonBox::Save | QDialogButtonBox::Discard | QDialogButtonBox::Cancel, QDialogButtonBox::Cancel);

			switch (button)
			{
			case QDialogButtonBox::Save:
				{
					OnSave();
					break;
				}
			case QDialogButtonBox::Discard:
				{
					CAudioControlsEditorPlugin::ReloadData(EReloadFlags::ReloadSystemControls | EReloadFlags::SendSignals);
					break;
				}
			case QDialogButtonBox::Cancel: // Intentional fall-through.
			default:
				{
					canClose = false;
					break;
				}
			}
		}
	}

	return canClose;
}

//////////////////////////////////////////////////////////////////////////
bool CMainWindow::CanQuit(std::vector<string>& unsavedChanges)
{
	bool canQuit = true;

	if (m_isModified)
	{
		for (char const* const modifiedLibrary : g_assetsManager.GetModifiedLibraries())
		{
			string const reason = QtUtil::ToString(tr("'%1' has unsaved modifications.").arg(modifiedLibrary));
			unsavedChanges.emplace_back(reason);
		}

		canQuit = false;
	}

	return canQuit;
}
} // namespace ACE
