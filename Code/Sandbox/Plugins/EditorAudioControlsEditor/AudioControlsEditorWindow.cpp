// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioControlsEditorWindow.h"
#include "AudioControlsEditorPlugin.h"
#include "QAudioControlEditorIcons.h"
#include "ATLControlsModel.h"
#include <CryAudio/IAudioSystem.h>
#include "AudioControlsEditorUndo.h"
#include "ATLControlsPanel.h"
#include "InspectorPanel.h"
#include "AudioSystemPanel.h"
#include <DockTitleBarWidget.h>
#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include <CryString/CryPath.h>
#include "ImplementationManager.h"
#include "QtUtil.h"

// File watching
#include <FileSystem/FileSystem_Snapshot.h>

#include <QPushButton>
#include <QApplication>
#include <QPainter>
#include <QMessageBox>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QSplitter>
#include <QKeyEvent>
#include <QDir>

namespace ACE
{

class CAudioFileMonitor final : public FileSystem::ISubTreeMonitor
{
public:
	CAudioFileMonitor(CAudioControlsEditorWindow& window)
		: m_window(window) {}

	virtual void Activated(const FileSystem::SnapshotPtr& initialSnapshot) override {}

	virtual void Update(const FileSystem::SSubTreeMonitorUpdate& update) override
	{
		if (!update.root.subDirectoryChanges.empty())
		{
			m_window.ReloadMiddlewareData();
		}
	}

private:
	CAudioControlsEditorWindow& m_window;
};

CAudioControlsEditorWindow::CAudioControlsEditorWindow()
{
	memset(m_allowedTypes, true, sizeof(m_allowedTypes));
	m_pMonitor = std::make_shared<CAudioFileMonitor>(*this);

	setWindowTitle(tr("Audio Controls Editor"));
	resize(972, 674);

	// Menu
	QMenuBar* pMenuBar = new QMenuBar(this);
	pMenuBar->setGeometry(QRect(0, 0, 972, 21));

	QMenu* pFileMenu = new QMenu(pMenuBar);
	pFileMenu->setTitle(tr("&File"));
	pMenuBar->addAction(pFileMenu->menuAction());

	QAction* pSaveAction = new QAction(this);
	pSaveAction->setIcon(QIcon(":/Icons/Save_Icon.png"));
	pSaveAction->setText(tr("Save All"));
	connect(pSaveAction, &QAction::triggered, this, &CAudioControlsEditorWindow::Save);
	pFileMenu->addAction(pSaveAction);

	QAction* pReloadAction = new QAction(this);
	pReloadAction->setIcon(QIcon(":/Icons/Load_Icon.png"));
	pReloadAction->setText(tr("Reload"));
	connect(pReloadAction, &QAction::triggered, this, &CAudioControlsEditorWindow::Reload);
	pFileMenu->addAction(pReloadAction);
	setMenuBar(pMenuBar);

	m_pATLModel = CAudioControlsEditorPlugin::GetATLModel();
	IAudioSystemEditor* pAudioSystemImpl = CAudioControlsEditorPlugin::GetAudioSystemEditorImpl();
	if (pAudioSystemImpl)
	{
		m_pATLControlsPanel = new CATLControlsPanel(m_pATLModel, CAudioControlsEditorPlugin::GetControlsTree());
		m_pInspectorPanel = new CInspectorPanel(m_pATLModel);
		m_pAudioSystemPanel = new CAudioSystemPanel();

		Update();
		connect(m_pATLControlsPanel, &CATLControlsPanel::SelectedControlChanged, [&]()
			{
				m_pInspectorPanel->SetSelectedControls(m_pATLControlsPanel->GetSelectedControls());
		  });
		connect(m_pATLControlsPanel, &CATLControlsPanel::SelectedControlChanged, this, &CAudioControlsEditorWindow::UpdateFilterFromSelection);
		connect(m_pATLControlsPanel, &CATLControlsPanel::ControlTypeFiltered, this, &CAudioControlsEditorWindow::FilterControlType);
		connect(CAudioControlsEditorPlugin::GetImplementationManger(), &CImplementationManager::ImplementationChanged, this, &CAudioControlsEditorWindow::Update);
		connect(m_pAudioSystemPanel, &CAudioSystemPanel::ImplementationSettingsChanged, this, &CAudioControlsEditorWindow::Update);

		GetIEditor()->RegisterNotifyListener(this);

		QSplitter* pSplitter = new QSplitter(this);
		pSplitter->setHandleWidth(0);
		pSplitter->addWidget(m_pATLControlsPanel);
		pSplitter->addWidget(m_pInspectorPanel);
		pSplitter->addWidget(m_pAudioSystemPanel);
		setCentralWidget(pSplitter);
	}

	const uint errorCodeMask = CAudioControlsEditorPlugin::GetLoadingErrorMask();
	if (errorCodeMask & EErrorCode::eErrorCode_UnkownPlatform)
	{
		QMessageBox::warning(this, tr("Audio Controls Editor"), tr("Audio Preloads reference an unknown platform.\nSaving will permanently erase this data."));
	}
}

CAudioControlsEditorWindow::~CAudioControlsEditorWindow()
{
	GetIEditor()->UnregisterNotifyListener(this);
}

void CAudioControlsEditorWindow::StartWatchingFolder(const QString& folderPath)
{
	// Fix the path so that it is a path relative to the engine folder. The file monitor system expects it that way.
	QString enginePath = QDir::cleanPath(QtUtil::ToQStringSafe(PathUtil::GetEnginePath()));
	QString finalPath = QDir::cleanPath(folderPath);
	if (QDir::isAbsolutePath(finalPath))
	{
		int index = finalPath.indexOf(enginePath, 0, Qt::CaseInsensitive);
		if (index == 0)
		{
			finalPath = finalPath.right(finalPath.size() - enginePath.size());
		}
		else
		{
			return;   // it's a path outside the engine folder
		}
	}
	else
	{
		QString absolutePath = enginePath + QDir::separator() + finalPath;
		QDir path(absolutePath);
		if (!path.exists())
		{
			return;   // it's a relative path that doesn't exist
		}
	}

	m_filter.directories.push_back(finalPath);
	FileSystem::CEnumerator* pEnumerator = GetIEditor()->GetFileSystemEnumerator();
	m_watchingHandles.push_back(pEnumerator->StartSubTreeMonitor(m_filter, m_pMonitor));
}

void CAudioControlsEditorWindow::keyPressEvent(QKeyEvent* pEvent)
{

	uint16 mod = pEvent->modifiers();
	if (pEvent->key() == Qt::Key_S && pEvent->modifiers() == Qt::ControlModifier)
	{
		Save();
	}
	else if (pEvent->key() == Qt::Key_Z && (pEvent->modifiers() & Qt::ControlModifier))
	{
		if (pEvent->modifiers() & Qt::ShiftModifier)
		{
			GetIEditor()->Redo();
		}
		else
		{
			GetIEditor()->Undo();
		}
	}
	QMainWindow::keyPressEvent(pEvent);
}

void CAudioControlsEditorWindow::closeEvent(QCloseEvent* pEvent)
{
	if (m_pATLModel && m_pATLModel->IsDirty())
	{
		QMessageBox messageBox;
		messageBox.setText(tr("There are unsaved changes."));
		messageBox.setInformativeText(tr("Do you want to save your changes?"));
		messageBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
		messageBox.setDefaultButton(QMessageBox::Save);
		messageBox.setWindowTitle("Audio Controls Editor");
		switch (messageBox.exec())
		{
		case QMessageBox::Save:
			QApplication::setOverrideCursor(Qt::WaitCursor);
			Save();
			QApplication::restoreOverrideCursor();
			pEvent->accept();
			break;
		case QMessageBox::Discard:
			{
				ACE::IAudioSystemEditor* pAudioSystemEditorImpl = CAudioControlsEditorPlugin::GetAudioSystemEditorImpl();
				if (pAudioSystemEditorImpl)
				{
					pAudioSystemEditorImpl->Reload(false);
				}
				CAudioControlsEditorPlugin::ReloadModels();

				pEvent->accept();
			}
			break;
		default:
			pEvent->ignore();
			break;
		}
	}
	else
	{
		pEvent->accept();
	}
}

void CAudioControlsEditorWindow::Reload()
{
	bool bReload = true;
	if (m_pATLModel && m_pATLModel->IsDirty())
	{
		QMessageBox messageBox;
		messageBox.setText(tr("If you reload you will lose all your unsaved changes."));
		messageBox.setInformativeText(tr("Are you sure you want to reload?"));
		messageBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
		messageBox.setDefaultButton(QMessageBox::No);
		messageBox.setWindowTitle("Audio Controls Editor");
		bReload = (messageBox.exec() == QMessageBox::Yes);
	}

	if (bReload)
	{
		CAudioControlsEditorPlugin::ReloadModels();
		Update();
	}
}

void CAudioControlsEditorWindow::Update()
{
	m_pATLControlsPanel->Reload();
	UpdateFilterFromSelection();
	m_pInspectorPanel->SetSelectedControls(m_pATLControlsPanel->GetSelectedControls());
	IAudioSystemEditor* pAudioSystemImpl = CAudioControlsEditorPlugin::GetAudioSystemEditorImpl();
	if (pAudioSystemImpl)
	{
		IImplementationSettings* pSettings = pAudioSystemImpl->GetSettings();
		if (pSettings)
		{
			FileSystem::CEnumerator* pEnumerator = GetIEditor()->GetFileSystemEnumerator();
			for (auto handle : m_watchingHandles)
			{
				pEnumerator->StopSubTreeMonitor(handle);
			}
			m_watchingHandles.clear();
			m_filter = FileSystem::SFileFilter();

			StartWatchingFolder(QtUtil::ToQString(pSettings->GetProjectPath()));
			StartWatchingFolder(QtUtil::ToQString(pSettings->GetSoundBanksPath()));
		}
	}
}

void CAudioControlsEditorWindow::Save()
{
	bool bPreloadsChanged = m_pATLModel->IsTypeDirty(eACEControlType_Preload);
	CAudioControlsEditorPlugin::SaveModels();
	UpdateAudioSystemData();

	// if preloads have been modified, ask the user if s/he wants to refresh the audio system
	if (bPreloadsChanged)
	{
		QMessageBox messageBox;
		messageBox.setText(tr("Preload requests have been modified. \n\nFor the new data to be loaded the audio system needs to be refreshed, this will stop all currently playing audio. Do you want to do this now?. \n\nYou can always refresh manually at a later time through the Audio menu."));
		messageBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
		messageBox.setDefaultButton(QMessageBox::No);
		messageBox.setWindowTitle("Audio Controls Editor");
		if (messageBox.exec() == QMessageBox::Yes)
		{
			SAudioRequest oAudioRequestData;
			char const* sLevelName = GetIEditor()->GetLevelName();

			if (_stricmp(sLevelName, "Untitled") == 0)
			{
				// Rather pass NULL to indicate that no level is loaded!
				sLevelName = NULL;
			}

			SAudioManagerRequestData<eAudioManagerRequestType_RefreshAudioSystem> oAMData(sLevelName);
			oAudioRequestData.flags = eAudioRequestFlags_PriorityHigh | eAudioRequestFlags_ExecuteBlocking;
			oAudioRequestData.pData = &oAMData;
			gEnv->pAudioSystem->PushRequest(oAudioRequestData);
		}
	}
	m_pATLModel->ClearDirtyFlags();
}

void CAudioControlsEditorWindow::UpdateFilterFromSelection()
{
	bool bAllSameType = true;
	EACEControlType selectedType = eACEControlType_NumTypes;
	std::vector<ACE::CID> ids = m_pATLControlsPanel->GetSelectedControls();
	size_t size = ids.size();
	for (size_t i = 0; i < size; ++i)
	{
		CATLControl* pControl = m_pATLModel->GetControlByID(ids[i]);
		if (pControl)
		{
			if (selectedType == eACEControlType_NumTypes)
			{
				selectedType = pControl->GetType();
			}
			else if (selectedType != pControl->GetType())
			{
				bAllSameType = false;
			}
		}
	}

	bool bSelectedFolder = (selectedType == eACEControlType_NumTypes);
	for (int i = 0; i < eACEControlType_NumTypes; ++i)
	{
		EACEControlType type = (EACEControlType)i;
		bool bAllowed = m_allowedTypes[type] && (bSelectedFolder || (bAllSameType && selectedType == type));
		m_pAudioSystemPanel->SetAllowedControls((EACEControlType)i, bAllowed);
	}
}

void CAudioControlsEditorWindow::UpdateAudioSystemData()
{
	SAudioRequest audioRequest;
	audioRequest.flags = eAudioRequestFlags_PriorityHigh;

	string levelPath = CRY_NATIVE_PATH_SEPSTR "levels" CRY_NATIVE_PATH_SEPSTR;
	levelPath += GetIEditor()->GetLevelName();
	SAudioManagerRequestData<eAudioManagerRequestType_ReloadControlsData> data(gEnv->pAudioSystem->GetConfigPath(), levelPath.c_str());
	audioRequest.pData = &data;
	gEnv->pAudioSystem->PushRequest(audioRequest);
}

void CAudioControlsEditorWindow::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	if (event == eNotify_OnEndSceneSave)
	{
		CAudioControlsEditorPlugin::ReloadScopes();
		m_pInspectorPanel->Reload();
	}
}

void CAudioControlsEditorWindow::FilterControlType(EACEControlType type, bool bShow)
{
	m_allowedTypes[type] = bShow;
	if (type == eACEControlType_Switch)
	{
		// need to keep states and switches filtering in sync as we don't have a separate filtering for states, only for switches
		m_allowedTypes[eACEControlType_State] = bShow;
	}
	m_pAudioSystemPanel->SetAllowedControls(type, bShow);
}

void CAudioControlsEditorWindow::ReloadMiddlewareData()
{
	IAudioSystemEditor* pAudioSystemImpl = CAudioControlsEditorPlugin::GetAudioSystemEditorImpl();
	if (pAudioSystemImpl)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_COMMENT, "[Audio Controls Editor] Reloading audio implementation data");
		pAudioSystemImpl->Reload();
	}
	m_pInspectorPanel->Reload();
	m_pAudioSystemPanel->Reset();
}
}
