// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "LevelEditor.h"

// LevelEditor
#include "LevelFileUtils.h"
#include "NewLevelDialog.h"
#include "OpenLevelDialog.h"
#include "SaveLevelDialog.h"
#include "LevelAssetType.h"

// EditorQt
#include "GameEngine.h"
#include "GameExporter.h"
#include "LevelIndependentFileMan.h"
#include "AlignTool.h"
#include "IEditorImpl.h"
#include "CryEdit.h"
#include "CryEditDoc.h"
#include "ViewManager.h"
#include "QT/QtMainFrame.h"
#include "QT/QToolTabManager.h"
#include "Commands/QCommandAction.h"
#include "PickObjectTool.h"
#include "EditMode/VertexSnappingModeTool.h"
#include "LevelExplorer.h"
#include "Util/Clipboard.h"
#include "Grid.h"
#include "QtViewPane.h"

// EditorCommon
#include "QtUtil.h"
#include "EditorFramework/Inspector.h"
#include <Notifications/NotificationCenterTrayWidget.h>
#include <EditorFramework/PreferencesDialog.h>
#include <Preferences/GeneralPreferences.h>
#include "Objects/ObjectLoader.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/Browser/AssetBrowser.h"
#include "Controls/DockableDialog.h"
#include "FilePathUtil.h"
#include "ThreadingUtils.h"
#include "EditorStyleHelper.h"
#include "Util/FileUtil.h"

// CryCommon
#include <CrySandbox/ScopedVariableSetter.h>
#include <CrySystem/IProjectManager.h>

// Qt
#include <QMenu>
#include <QtGlobal>
#include <QMenuBar>
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QTimer>

#include "Controls/QuestionDialog.h"

// Register viewpanes defined in EditorCommon
REGISTER_VIEWPANE_FACTORY_AND_MENU(CNotificationCenterDockable, "Notification Center", "Advanced", true, "Advanced")

class CLevelEditorInspector : public CInspector
{
public:
	CLevelEditorInspector()
		: CInspector(CEditorMainFrame::GetInstance()->GetLevelEditor())
	{
		GetIEditorImpl()->GetObjectManager()->EmitPopulateInspectorEvent();
	}
};

REGISTER_VIEWPANE_FACTORY_AND_MENU(CLevelEditorInspector, "Properties", "Tools", false, "Level Editor")

namespace Private_LevelEditor
{

std::vector<string> CollectCryLevels()
{
	std::vector<string> levels;

	SDirectoryEnumeratorHelper enumerator;
	enumerator.ScanDirectoryRecursive("", "levels", "*.cry", levels);

	// It seems ScanDirectoryRecursive can return *.cry* files. Remove them from the list, if any.
	// Also do not convert already converted levels.
	levels.erase(std::remove_if(levels.begin(), levels.end(), [](const string& x)
	{
		return stricmp(PathUtil::GetExt(x.c_str()), "cry") != 0
			|| GetISystem()->GetIPak()->IsFileExist(PathUtil::ReplaceExtension(x, "level"), ICryPak::eFileLocation_OnDisk);
	}), levels.end());

	return levels;
}

void DeleteObsoleteMetadataFile(const string& level)
{
	const string filename(string().Format("%s.cryasset", level.c_str()));
	const string absPath(PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), filename));
	PathUtil::RemoveFile(absPath);
}

bool UnpakCryFile(ICryPak* pCryPak, const string& cryFilename, const string& destinationFolder, std::function<void(float)> progress)
{
	// Relative to the project root directory.
	const string cryPakPath = PathUtil::Make(PathUtil::GetGameFolder(), cryFilename);
	if (!pCryPak->OpenPack(cryPakPath))
	{
		CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_WARNING, "Could not open pak %s", cryFilename.c_str());
		return false;
	}

	PathUtil::Unpak(cryPakPath, destinationFolder, std::move(progress));
	pCryPak->ClosePack(cryPakPath);
	return true;
}

void UpdateLevels(const std::vector<string>& levels)
{
	size_t countOfImported = 0;

	CProgressNotification notif(QObject::tr("Importing levels"), "", true);

	ICryPak* const pCryPak = GetISystem()->GetIPak();

	for (size_t i = 0, n = levels.size(); i < n; ++i)
	{
		// Relative to the game root folder.
		const string& oldFilePath = levels[i];

		CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_COMMENT, "Updating level: %s", oldFilePath.c_str());

		const char* const szOldFilename = PathUtil::GetFile(oldFilePath.c_str());
		const string levelFolder(PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), PathUtil::GetDirectory(oldFilePath)));

		notif.SetMessage(QtUtil::ToQString(szOldFilename));

		auto updateProgressBar = [&notif, i, n](float unpakProgress)
		{
			const float minBound = float(i) / n;
			const float maxBound = float(i + 1) / n;

			// lerp
			const float totalProgress = minBound + (maxBound - minBound) * unpakProgress;
			notif.SetProgress(totalProgress);
		};

		const CLevelEditor* const pLevelEditor = static_cast<const CLevelEditor* const>(GetIEditorImpl()->GetLevelEditor());
		if (!UnpakCryFile(pCryPak, oldFilePath, levelFolder, updateProgressBar) || !pLevelEditor->ConvertEditorXmlToLevelAssetType(levelFolder, szOldFilename))
		{
			CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_WARNING, "Could not import level %s", szOldFilename);
			continue;
		}
		DeleteObsoleteMetadataFile(oldFilePath);
		CFileUtil::BackupFile(PathUtil::Make(levelFolder, szOldFilename));

		++countOfImported;
		notif.SetProgress(float(i + 1) / n);
	}

	if (countOfImported)
	{
		CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_WARNING, "%i levels have been updated to be compatible with the new version of CRYENGINE.\n", countOfImported);
	}
}

class CUpdateLevelsDialog : public CQuestionDialog
{
public:
	static QDialogButtonBox::StandardButton SWarning()
	{
		CUpdateLevelsDialog dialog;

		const QString link("http://docs.cryengine.com/pages/viewpage.action?pageId=29800625#CRYENGINE5.5.0Preview(s)-NewLevelFileFormat");

		const QString text =
		  tr("<p>"
		     "This change fully integrates the levels into the CRYENGINE Sandbox asset system and allows for a proper collaborative workflow.<br>"
		     "To benefit from this the editor will have to convert all levels of this project.<br>"
		     "Before you continue you should <span style = \"color:%1\">quit the editor and manually backup your project</span>.<br>"
		     "Please refer to the CRYENGINE documentation for more details:"
		     "<a style = \"color:%2;\" href=\"%3\"> New Level File Format </a>"
		     "</p>")
		  .arg(GetStyleHelper()->warningColor().name())
		  .arg(GetStyleHelper()->selectedIconTint().name())
		  .arg(link);

		dialog.SetupWarning(tr("New Level File Format"), text, QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::No);
		dialog.m_buttons->button(QDialogButtonBox::Yes)->setText(tr("Convert all levels"));
		dialog.m_buttons->button(QDialogButtonBox::No)->setText(tr("Quit"));
		dialog.m_infoLabel->setOpenExternalLinks(true);

		return dialog.Execute();
	}
};

};

CLevelEditor::CLevelEditor()
	: CEditor(nullptr)
	, m_findWindow(nullptr)
	, m_assetBrowser(nullptr)
{
	auto cmdManager = GetIEditorImpl()->GetCommandManager();
	ICommandManager* pCmdMgr = GetIEditorImpl()->GetICommandManager();
}

CLevelEditor::~CLevelEditor()
{
	if (m_assetBrowser)
	{
		delete m_assetBrowser;
		m_assetBrowser = nullptr;
	}

	if (m_findWindow)
	{
		delete m_findWindow;
		m_findWindow = nullptr;
	}
}

void CLevelEditor::customEvent(QEvent* pEvent)
{
	CEditor::customEvent(pEvent);

	if (!pEvent->isAccepted() && pEvent->type() == SandboxEvent::Command)
	{
		CommandEvent* pCommandEvent = static_cast<CommandEvent*>(pEvent);

		//Note: this could be optimized this with a hash map
		//TODO : better system of macros and registration of those commands in EditorCommon (or move all of this in Editor)
		QStringList params = QtUtil::ToQString(pCommandEvent->GetCommand()).split(' ');

		if (params.empty())
			return;

		QString command = params[0];
		params.removeFirst();

		QStringList fullCommand = command.split('.');
		QString module = fullCommand[0];
		command = fullCommand[1];

		if (module == "level")
		{
			if (command == "snap_to_grid")
			{
				const bool bEnable = (params[0])[1] != 0; // Argument is expected to be 0 or 1 enclosed by ''.
				EnableGridSnapping(bEnable);
			}
			else if (command == "toggle_snap_to_grid")
			{
				EnableGridSnapping(!IsGridSnappingEnabled());
				pEvent->setAccepted(true);
			}
			else if (command == "snap_to_angle")
			{
				const bool bEnable = (params[0])[1] != 0; // Argument is expected to be 0 or 1 enclosed by ''.
				EnableAngleSnapping(bEnable);
			}
			else if (command == "toggle_snap_to_angle")
			{
				EnableAngleSnapping(!IsAngleSnappingEnabled());
				pEvent->setAccepted(true);
			}
			else if (command == "snap_to_scale")
			{
				const bool bEnable = (params[0])[1] != 0; // Argument is expected to be 0 or 1 enclosed by ''.
				EnableScaleSnapping(bEnable);
			}
			else if (command == "toggle_snap_to_scale")
			{
				EnableScaleSnapping(!IsScaleSnappingEnabled());
				pEvent->setAccepted(true);
			}
			else if (command == "snap_to_vertex")
			{
				const bool bEnable = (params[0])[1] != 0; // Argument is expected to be 0 or 1 enclosed by ''.
				EnableVertexSnapping(bEnable);
			}
			else if (command == "toggle_snap_to_vertex")
			{
				EnableVertexSnapping(!IsVertexSnappingEnabled());
				pEvent->setAccepted(true);
			}
			else if (command == "snap_to_pivot")
			{
				const bool bEnable = (params[0])[1] != 0; // Argument is expected to be 0 or 1 enclosed by ''.
				EnablePivotSnapping(bEnable);
			}
			else if (command == "toggle_snap_to_pivot")
			{
				EnablePivotSnapping(!IsPivotSnappingEnabled());
				pEvent->setAccepted(true);
			}
			else if (command == "snap_to_terrain")
			{
				const bool bEnable = (params[0])[1] != 0; // Argument is expected to be 0 or 1 enclosed by ''.
				EnableTerrainSnapping(bEnable);
			}
			else if (command == "toggle_snap_to_terrain")
			{
				EnableTerrainSnapping(!IsTerrainSnappingEnabled());
				pEvent->setAccepted(true);
			}
			else if (command == "snap_to_geometry")
			{
				const bool bEnable = (params[0])[1] != 0; // Argument is expected to be 0 or 1 enclosed by ''.
				EnableGeometrySnapping(bEnable);
			}
			else if (command == "toggle_snap_to_geometry")
			{
				EnableGeometrySnapping(!IsGeometrySnappingEnabled());
				pEvent->setAccepted(true);
			}
			else if (command == "snap_to_surface_normal")
			{
				const bool bEnable = (params[0])[1] != 0; // Argument is expected to be 0 or 1 enclosed by ''.
				EnableSurfaceNormalSnapping(bEnable);
			}
			else if (command == "toggle_snap_to_surface_normal")
			{
				EnableSurfaceNormalSnapping(!IsSurfaceNormalSnappingEnabled());
				pEvent->setAccepted(true);
			}
			else if (command == "toggle_display_helpers")
			{
				EnableHelpersDisplay(!IsHelpersDisplayed());
				pEvent->setAccepted(true);
			}
		}
		else if (module == "asset")
		{
			if (command == "toggle_asset_browser")
			{
				OnToggleAssetBrowser();
			}
			else if (command == "show_in_asset_browser")
			{
				if (!m_assetBrowser || !m_assetBrowser->isActiveWindow())
					OnToggleAssetBrowser();

				CAssetBrowser* pAssetBrowser = m_assetBrowser->GetPaneT<CAssetBrowser>();
				pAssetBrowser->SelectAsset(params[0].toLocal8Bit());
			}
		}
		else if (module == "general")
		{
			if (command == "select_all")
			{
				GetIEditorImpl()->GetObjectManager()->SelectAll();
			}
		}
	}
}

void CLevelEditor::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	if (event == eNotify_OnEndSceneOpen || event == eNotify_OnEndSceneSave)
	{
		//While this is logically correct, this can cause issues during resize terrain (hold/fetch)
		//string currPath = GetIEditorImpl()->GetLevelPath();
		string currPath = GetIEditorImpl()->GetDocument()->GetPathName();
		if (currPath.empty())
			return;

		QString levelPath = QtUtil::ToQString(currPath);
		LevelLoaded(levelPath);
		AddRecentFile(levelPath);
	}
	else if (event == eNotify_OnMainFrameCreated)
	{
		ThreadingUtils::AsyncQueue([this]()
		{
			// Updates existing levels in CRYENGINE 5.4 format to 5.5, if any.

			using namespace Private_LevelEditor;

			std::vector<string> levels = CollectCryLevels();
			if (levels.empty())
			{
				return;
			}

			ThreadingUtils::PostOnMainThread([this, levels]()
			{
				if (CUpdateLevelsDialog::SWarning() != QDialogButtonBox::Yes)
				{
					// Quit the editor right after the initialization is done. 
					QTimer::singleShot(0, []() 
					{
						GetIEditor()->ExecuteCommand("general.exit");
					});
					return;
				}

				ThreadingUtils::AsyncQueue([this, levels]()
				{
					UpdateLevels(levels);

					// Update the Recent Files list.
					ThreadingUtils::PostOnMainThread([this]()
					{
						QStringList recent = GetRecentFiles();
						for (QString& fileName : recent)
						{
							int i = fileName.lastIndexOf(".");
							if (i > 0)
							{
								fileName = fileName.left(i + 1).append(CLevelType::GetFileExtensionStatic());
							}
						}
						SetProjectProperty("Recent Files", recent);
					});
				});
			});
		});
	}
}

void CLevelEditor::CreateRecentFilesMenu(QMenu* pRecentFilesMenu)
{
	pRecentFilesMenu->clear();
	QStringList recentPaths = GetRecentFiles();

	for (int i = 0; i < recentPaths.size(); ++i)
	{
		const QString& path = recentPaths[i];

		string cmd;
		cmd.Format("general.open_level '%s'", QtUtil::ToString(path));
		pRecentFilesMenu->addAction(new QCommandAction(path, cmd.c_str(), pRecentFilesMenu));
	}
}

void CLevelEditor::EnableVertexSnapping(bool bEnable)
{
	bEnable ? GetIEditorImpl()->SetEditTool("EditTool.VertexSnappingMode") : GetIEditorImpl()->SetEditTool(NULL);

	VertexSnappingEnabled(bEnable);
}

void CLevelEditor::EnablePivotSnapping(bool bEnable)
{
	bEnable ? GetIEditorImpl()->PickObject(new CAlignPickCallback) : GetIEditorImpl()->SetEditTool(NULL);

	GetIEditorImpl()->EnablePivotSnapping(bEnable);

	PivotSnappingEnabled(bEnable);
}

void CLevelEditor::EnableGridSnapping(bool bEnable)
{

	gSnappingPreferences.setGridSnappingEnabled(bEnable);

	GridSnappingEnabled(bEnable);
}

void CLevelEditor::EnableAngleSnapping(bool bEnable)
{
	gSnappingPreferences.setAngleSnappingEnabled(bEnable);

	AngleSnappingEnabled(bEnable);
}

void CLevelEditor::EnableScaleSnapping(bool bEnable)
{
	gSnappingPreferences.setScaleSnappingEnabled(bEnable);

	ScaleSnappingEnabled(bEnable);
}

void CLevelEditor::EnableTerrainSnapping(bool bEnable)
{
	GetIEditorImpl()->EnableSnapToTerrain(bEnable);

	TerrainSnappingEnabled(bEnable);
}

void CLevelEditor::EnableGeometrySnapping(bool bEnable)
{
	GetIEditorImpl()->EnableSnapToGeometry(bEnable);

	GeometrySnappingEnabled(bEnable);
}

void CLevelEditor::EnableSurfaceNormalSnapping(bool bEnable)
{
	GetIEditorImpl()->EnableSnapToNormal(bEnable);

	SurfaceNormalSnappingEnabled(bEnable);
}

void CLevelEditor::EnableHelpersDisplay(bool bEnable)
{
	GetIEditorImpl()->EnableHelpersDisplay(bEnable);
	HelpersDisplayEnabled(bEnable);
	GetIEditorImpl()->GetObjectManager()->SendEvent(GetIEditor()->IsHelpersDisplayed() ? EVENT_SHOW_HELPER : EVENT_HIDE_HELPER);
}

bool CLevelEditor::IsVertexSnappingEnabled() const
{
	CEditTool* pTool = GetIEditorImpl()->GetEditTool();
	return pTool && pTool->IsKindOf(RUNTIME_CLASS(CVertexSnappingModeTool));
}

bool CLevelEditor::IsPivotSnappingEnabled() const
{
	return GetIEditorImpl()->IsPivotSnappingEnabled();
}

bool CLevelEditor::IsGridSnappingEnabled() const
{
	return gSnappingPreferences.gridSnappingEnabled();
}

bool CLevelEditor::IsAngleSnappingEnabled() const
{
	return gSnappingPreferences.angleSnappingEnabled();
}

bool CLevelEditor::IsScaleSnappingEnabled() const
{
	return gSnappingPreferences.scaleSnappingEnabled();
}

bool CLevelEditor::IsTerrainSnappingEnabled() const
{
	return GetIEditorImpl()->IsSnapToTerrainEnabled();
}

bool CLevelEditor::IsGeometrySnappingEnabled() const
{
	return GetIEditorImpl()->IsSnapToGeometryEnabled();
}

bool CLevelEditor::IsSurfaceNormalSnappingEnabled() const
{
	return GetIEditorImpl()->IsSnapToNormalEnabled();
}

bool CLevelEditor::IsHelpersDisplayed() const
{
	return GetIEditorImpl()->IsHelpersDisplayed();
}

bool CLevelEditor::IsLevelLoaded()
{
	return GetIEditor()->GetDocument() && GetIEditor()->GetGameEngine() && GetIEditor()->GetGameEngine()->IsLevelLoaded();
}

bool CLevelEditor::ConvertEditorXmlToLevelAssetType(const string& levelFolder, const char* const szName) const
{
	// Rename level.editor_xml to a new *.level
	const string oldFilename = PathUtil::Make(levelFolder, "level.editor_xml");
	const string newFilename = PathUtil::Make(levelFolder, PathUtil::ReplaceExtension(szName, CLevelType::GetFileExtensionStatic()));
	if (PathUtil::MoveFileAllowOverwrite(oldFilename, newFilename))
	{
		CAssetType* pType = GetIEditorImpl()->GetAssetManager()->FindAssetType("Level");
		CRY_ASSERT(pType);

		const string path = PathUtil::RemoveSlash(levelFolder);

		const string cryassetPath = string().Format("%s.%s.cryasset", path, pType->GetFileExtension());
		CAsset asset(pType->GetTypeName(), CryGUID::Create(), PathUtil::GetFile(cryassetPath));
		CEditableAsset editAsset(asset);
		editAsset.SetMetadataFile(cryassetPath);
		
		const string rootFolder = PathUtil::GetParentDirectory(path);
		editAsset.AddFile(PathUtil::AbsoluteToRelativePath(newFilename, rootFolder));
		
		editAsset.WriteToFile();

		return true;
	}

	return false;
}

bool CLevelEditor::OnNew()
{
	bool isProceed = GetIEditorImpl()->GetDocument()->CanClose();
	if (!isProceed)
	{
		return false;
	}

	if (!GetIEditorImpl()->GetLevelIndependentFileMan()->PromptChangedFiles())
	{
		return false;
	}

	CSaveLevelDialog levelSaveDialog(QString(tr("Create New Level")));
	QString lastLoadedLevelName(GetIEditorImpl()->GetDocument()->GetLastLoadedLevelName());
	if (!lastLoadedLevelName.isEmpty())
	{
		levelSaveDialog.SelectLevelFile(lastLoadedLevelName);
	}
	if (levelSaveDialog.exec() != QDialog::Accepted)
	{
		return true;
	}

	CNewLevelDialog settingsDialog;
	if (settingsDialog.exec() != QDialog::Accepted)
	{
		return true;
	}

	const static CAssetType* const pLevelType = GetIEditorImpl()->GetAssetManager()->FindAssetType("Level");
	CRY_ASSERT(pLevelType);

	// Convert "Levels/LevelName/LevelName.cry" to "Levels/LevelName.cry.cryasset"
	const QString absoluteFilePath = levelSaveDialog.GelAcceptedAbsoluteLevelFile();
	const QString levelFilePath = LevelFileUtils::ConvertAbsoluteToGamePath(absoluteFilePath);
	const QString levelPath = QFileInfo(levelFilePath).path();
	const string cryassetPath = QtUtil::ToString(QString("%1.%2.cryasset").arg(levelPath).arg(QtUtil::ToQString(pLevelType->GetFileExtension())));

	// levelSaveDialog has got the confirmation to overwrite the level.
	CAsset* pAsset = CAssetManager::GetInstance()->FindAssetForMetadata(cryassetPath);
	size_t filesDeleted = 0;

	// If new level path does not point to a level
	if (!pAsset)
	{
		QDir dirToDelete(QFileInfo(absoluteFilePath).path());
		if (dirToDelete.exists())
		{
			const QString question = tr("All existing contents of the folder %1 will be deleted.\nDo you really want to continue?").arg(levelPath);
			if (CQuestionDialog::SQuestion(tr("New Level"), question) != QDialogButtonBox::Yes)
			{
				return true;
			}

			QDir dirToDelete(QFileInfo(absoluteFilePath).path());

			// Make sure none of files to be deleted are read only.
			QDirIterator iterator(dirToDelete, QDirIterator::Subdirectories);
			while (iterator.hasNext())
			{
				QFileInfo fileInfo(iterator.next());
				if (!fileInfo.isWritable())
				{
					CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_WARNING, "File is read-only: %s", QtUtil::ToString(fileInfo.absoluteFilePath()));
					return true;
				}
			}

			if (!dirToDelete.removeRecursively())
			{
				auto messageText = tr("Failed to remove some files of the existing folder:\n%1").arg(levelPath);
				CQuestionDialog::SWarning(tr("New Level failed"), messageText);
				return true;
			}
		}
	}
	else if (!pLevelType->DeleteAssetFiles(*pAsset, false, filesDeleted))
	{
		if (filesDeleted)
		{
			auto messageText = tr("Failed to remove some files of the level that has to be overwritten:\n%1").arg(levelPath);
			CQuestionDialog::SWarning(tr("New Level failed"), messageText);
		}
		return true;
	}

	pLevelType->Create(cryassetPath, &settingsDialog.GetResult());

	return true;
}

bool CLevelEditor::OnOpen()
{
	bool isProceed = GetIEditorImpl()->GetDocument()->CanClose();
	if (!isProceed)
	{
		return true;
	}
	// We need to ensure that the level path exists before calling the dialog, else the filesystem model will be initialized
	// with an invalid path
	if (!LevelFileUtils::EnsureLevelPathsValid())
	{
		return true;
	}
	COpenLevelDialog levelOpenDialog;
	QString lastLoadedLevelName(GetIEditorImpl()->GetDocument()->GetLastLoadedLevelName());
	if (!lastLoadedLevelName.isEmpty())
	{
		levelOpenDialog.SelectLevelFile(lastLoadedLevelName);
	}
	if (levelOpenDialog.exec() == QDialog::Accepted)
	{
		auto filename = levelOpenDialog.GetAcceptedLevelFile().toStdString();//will be relative to working directory/project root
		CCryEditApp::GetInstance()->DiscardLevelChanges();
		CCryEditApp::GetInstance()->LoadLevel(filename.c_str());
	}
	return true;
}

bool CLevelEditor::OnSave()
{
	string level = GetIEditorImpl()->GetDocument()->GetPathName();
	if (!level.empty())
	{
		GetIEditor()->GetDocument()->DoSave(level, true);
		SaveCryassetFile(level);
	}
	return true;
}

bool CLevelEditor::OnSaveAs()
{
	string openlevel = GetIEditorImpl()->GetDocument()->GetPathName();
	if (openlevel.empty())
		return true;

	CSaveLevelDialog levelSaveDialog;
	QString lastLoadedLevelName(GetIEditorImpl()->GetDocument()->GetLastLoadedLevelName());
	if (!lastLoadedLevelName.isEmpty())
	{
		levelSaveDialog.SelectLevelFile(lastLoadedLevelName);
	}
	if (levelSaveDialog.exec() == QDialog::Accepted)
	{
		auto filename = levelSaveDialog.GetAcceptedLevelFile().toStdString();
		GetIEditorImpl()->GetDocument()->DoSave(filename.c_str(), true);
		SaveCryassetFile(GetIEditorImpl()->GetDocument()->GetPathName());
	}
	return true;
}

bool CLevelEditor::OnDelete()
{
	// If Edit tool active cannot delete object.
	if (GetIEditorImpl()->GetEditTool())
	{
		if (GetIEditorImpl()->GetEditTool()->OnKeyDown(GetIEditorImpl()->GetViewManager()->GetView(0), Qt::Key_Delete, 0, 0))
			return true;
	}

	string strAsk;
	int nSelectedObjects = GetIEditorImpl()->GetObjectManager()->GetSelection()->GetCount();
	if (nSelectedObjects != 0)
	{
		GetIEditorImpl()->GetIUndoManager()->Begin();
		GetIEditorImpl()->GetObjectManager()->DeleteSelection();
		GetIEditorImpl()->GetIUndoManager()->Accept("Delete Selection");
		GetIEditorImpl()->SetModifiedFlag();
	}

	return true;
}

bool CLevelEditor::OnDuplicate()
{
	CCryEditApp::GetInstance()->OnEditDuplicate();
	return true;
}

bool CLevelEditor::OnFind()
{
	if (!m_findWindow)
	{
		m_findWindow = new CDockableDialog("level_editor_find", "Level Explorer");
		m_findWindow->SetHideOnClose();
		m_findWindow->SetTitle(tr("Find Objects"));
		auto levelExplorer = m_findWindow->GetPaneT<CLevelExplorer>();
		levelExplorer->SetSyncSelection(false);
		levelExplorer->SetModelType(CLevelExplorer::Objects);
		levelExplorer->ShowActiveLayerWidget(false);
		m_findWindow->Popup();
		m_findWindow->SetPosCascade();
		levelExplorer->GrabFocusSearchBar();
	}
	else
	{
		m_findWindow->Popup();
		m_findWindow->GetPaneT<CLevelExplorer>()->GrabFocusSearchBar();
	}
	return true;
}

void CLevelEditor::OnToggleAssetBrowser()
{
	if (!m_assetBrowser)
	{
		m_assetBrowser = new CDockableDialog("Quick Asset Browser", "Asset Browser");
		m_assetBrowser->SetHideOnClose();
		m_assetBrowser->SetTitle(tr("Quick Asset Browser"));
		m_assetBrowser->Popup();
		m_assetBrowser->SetPosCascade();
		m_assetBrowser->GetPaneT<CAssetBrowser>()->GrabFocusSearchBar();
	}
	else if (m_assetBrowser->window() && m_assetBrowser->window()->isActiveWindow())
	{
		m_assetBrowser->hide();
	}
	else
	{
		m_assetBrowser->Popup();
		m_assetBrowser->GetPaneT<CAssetBrowser>()->GrabFocusSearchBar();
	}
}

void CLevelEditor::SaveCryassetFile(const string& levelPath)
{
	const string localLevelPath(PathUtil::ToGamePath(levelPath));
	CAsset* pAsset = GetIEditor()->GetAssetManager()->FindAssetForFile(localLevelPath);
	if (pAsset)
	{
		CEditableAsset editAsset(*pAsset);
		CLevelType::UpdateDependencies(editAsset);
		editAsset.WriteToFile();
		return;
	}

	CAssetType* pType = GetIEditorImpl()->GetAssetManager()->FindAssetType("Level");
	CRY_ASSERT(pType);

	const string path = PathUtil::RemoveSlash(PathUtil::GetPathWithoutFilename(localLevelPath));
	const string cryassetPath = string().Format("%s.%s.cryasset", path, pType->GetFileExtension());
	CAsset asset(pType->GetTypeName(), CryGUID::Create(), PathUtil::GetFile(cryassetPath));
	CEditableAsset editAsset(asset);
	editAsset.SetMetadataFile(cryassetPath);
	editAsset.AddFile(localLevelPath);
	CLevelType::UpdateDependencies(editAsset);
	editAsset.WriteToFile();
}

void CLevelEditor::OnCopyInternal(bool isCut)
{
	// For copy/paste we will be serializing the objects fully. That is because the copied objects
	// may have been completely deleted at the point when we paste them (for example in cut-paste).

	IObjectManager* objManager = GetIEditorImpl()->GetObjectManager();
	const CSelectionGroup* selection = objManager->GetSelection();

	if (selection->IsEmpty())
	{
		return;
	}

	CClipboard clipboard;
	// create or override previous node
	XmlNodeRef copyNode = XmlHelpers::CreateXmlNode("LevelEditorObjectCopyNode");

	CObjectArchive archive(objManager, copyNode, false);

	// serialize the objects in the selection
	CBaseObject* objIter;
	for (int i = 0; i < selection->GetCount(); ++i)
	{
		objIter = selection->GetObject(i);
		archive.SaveObject(objIter);
	}

	clipboard.Put(copyNode, "LevelEditorObjectCopyNode");

	if (isCut)
	{
		{
			CUndo undo("Delete Selection");
			objManager->DeleteSelection();
		}

		GetIEditorImpl()->SetModifiedFlag();
	}
}

bool CLevelEditor::OnCopy()
{
	OnCopyInternal();
	return true;
}

bool CLevelEditor::OnCut()
{
	OnCopyInternal(true);
	return true;
}

bool CLevelEditor::OnPaste()
{
	CClipboard clipboard;
	XmlNodeRef copyNode = clipboard.Get();
	if (!copyNode || strcmp(copyNode->getTag(), "LevelEditorObjectCopyNode") != 0)
	{
		return false;
	}

	IObjectManager* objManager = GetIEditorImpl()->GetObjectManager();

	CObjectArchive archive(objManager, copyNode, true);

	CRandomUniqueGuidProvider guidProvider;
	archive.SetGuidProvider(&guidProvider);
	archive.LoadInCurrentLayer(true);
	{
		CUndo undo("Paste Objects");

		objManager->ClearSelection();
		// instantiate the new objects
		objManager->LoadObjects(archive, true);

		//Make sure the new objects have unique names
		const CSelectionGroup* selGroup = objManager->GetSelection();

		for (int i = 0, nObj = selGroup->GetCount(); i < nObj; ++i)
		{
			CBaseObject* pObj = selGroup->GetObject(i);
			pObj->SetName(objManager->GenUniqObjectName(pObj->GetName()));
		}
	}

	return true;
}

namespace
{

namespace Private_LevelCommands
{
void PySnapToGrid(const bool bEnable)
{
	char buffer[31];
	cry_sprintf(buffer, "level.snap_to_grid '%i'", bEnable);
	CommandEvent(buffer).SendToKeyboardFocus();
}

void PyToggleSnapToGrid()
{
	CommandEvent("level.toggle_snap_to_grid").SendToKeyboardFocus();
}

void PySnapToAngle(const bool bEnable)
{
	char buffer[32];
	cry_sprintf(buffer, "level.snap_to_angle '%i'", bEnable);
	CommandEvent(buffer).SendToKeyboardFocus();
}

void PyToggleSnapToAngle()
{
	CommandEvent("level.toggle_snap_to_angle").SendToKeyboardFocus();
}

void PyToggleSnapToScale()
{
	CommandEvent("level.toggle_snap_to_scale").SendToKeyboardFocus();
}

void PySnapToVertex(const bool bEnable)
{
	char buffer[33];
	cry_sprintf(buffer, "level.snap_to_vertex '%i'", bEnable);
	CommandEvent(buffer).SendToKeyboardFocus();
}

void PyToggleSnapToVertex()
{
	CommandEvent("level.toggle_snap_to_vertex").SendToKeyboardFocus();
}

void PySnapToPivot(const bool bEnable)
{
	char buffer[32];
	cry_sprintf(buffer, "level.snap_to_pivot '%i'", bEnable);
	CommandEvent(buffer).SendToKeyboardFocus();
}

void PyToggleSnapToPivot()
{
	CommandEvent("level.toggle_snap_to_pivot").SendToKeyboardFocus();
}

void PySnapToTerrain(const bool bEnable)
{
	char buffer[34];
	cry_sprintf(buffer, "level.snap_to_terrain '%i'", bEnable);
	CommandEvent(buffer).SendToKeyboardFocus();
}

void PyToggleSnapToTerrain()
{
	CommandEvent("level.toggle_snap_to_terrain").SendToKeyboardFocus();
}

void PySnapToGeometry(const bool bEnable)
{
	char buffer[35];
	cry_sprintf(buffer, "level.snap_to_geometry '%i'", bEnable);
	CommandEvent(buffer).SendToKeyboardFocus();
}

void PyToggleSnapToGeometry()
{
	CommandEvent("level.toggle_snap_to_geometry").SendToKeyboardFocus();
}

void PySnapToSurfaceNormal(const bool bEnable)
{
	char buffer[41];
	cry_sprintf(buffer, "level.snap_to_surface_normal '%i'", bEnable);
	CommandEvent(buffer).SendToKeyboardFocus();
}

void PyToggleSnapToSurfaceNormal()
{
	CommandEvent("level.toggle_snap_to_surface_normal").SendToKeyboardFocus();
}

void PyToggle_display_helpers()
{
	CommandEvent("level.toggle_display_helpers").SendToKeyboardFocus();
}
}
}

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PySnapToGrid, level, snap_to_grid,
                                   CCommandDescription("Enable/Disable snapping to grid").Param("enable", "0: Disable, 1: Enable"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PyToggleSnapToGrid, level, toggle_snap_to_grid,
                                   CCommandDescription("Toggle snapping to grid"));
REGISTER_EDITOR_UI_COMMAND_DESC(level, toggle_snap_to_grid, "", "I", "icons:Viewport/viewport-snap-grid.ico")

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PySnapToAngle, level, snap_to_angle,
                                   CCommandDescription("Enable/Disable snapping to angle").Param("enable", "0: Disable, 1: Enable"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PyToggleSnapToAngle, level, toggle_snap_to_angle,
                                   CCommandDescription("Toggle snapping to angle"));
REGISTER_EDITOR_UI_COMMAND_DESC(level, toggle_snap_to_angle, "", "L", "icons:Viewport/viewport-snap-angle.ico")

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PyToggleSnapToScale, level, toggle_snap_to_scale,
                                   CCommandDescription("Toggle snapping to scale"));
REGISTER_EDITOR_UI_COMMAND_DESC(level, toggle_snap_to_scale, "", "K", "icons:Viewport/viewport-snap-scale.ico")

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PySnapToVertex, level, snap_to_vertex,
                                   CCommandDescription("Enable/Disable snapping to vertex").Param("enable", "0: Disable, 1: Enable"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PyToggleSnapToVertex, level, toggle_snap_to_vertex,
                                   CCommandDescription("Toggle snapping to vertex"));
REGISTER_EDITOR_UI_COMMAND_DESC(level, toggle_snap_to_vertex, "", "V", "icons:Viewport/viewport-snap-vertex.ico")

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PySnapToPivot, level, snap_to_pivot,
                                   CCommandDescription("Enable/Disable snapping to privot").Param("enable", "0: Disable, 1: Enable"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PyToggleSnapToPivot, level, toggle_snap_to_pivot,
                                   CCommandDescription("Toggle snapping to pivot"));
REGISTER_EDITOR_UI_COMMAND_DESC(level, toggle_snap_to_pivot, "", "P", "icons:Viewport/viewport-snap-pivot.ico")

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PySnapToTerrain, level, snap_to_terrain,
                                   CCommandDescription("Enable/Disable snapping to terrain").Param("enable", "0: Disable, 1: Enable"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PyToggleSnapToTerrain, level, toggle_snap_to_terrain,
                                   CCommandDescription("Toggle snapping to terrain"));
REGISTER_EDITOR_UI_COMMAND_DESC(level, toggle_snap_to_terrain, "", "T", "icons:common/viewport-snap-terrain.ico")

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PySnapToGeometry, level, snap_to_geometry,
                                   CCommandDescription("Enable/Disable snapping to geometry").Param("enable", "0: Disable, 1: Enable"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PyToggleSnapToGeometry, level, toggle_snap_to_geometry,
                                   CCommandDescription("Toggle snapping to geometry"));
REGISTER_EDITOR_UI_COMMAND_DESC(level, toggle_snap_to_geometry, "", "O", "icons:common/viewport-snap-geometry.ico")

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PySnapToSurfaceNormal, level, snap_to_surface_normal,
                                   CCommandDescription("Enable/Disable snapping to surface normal").Param("enable", "0: Disable, 1: Enable"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PyToggleSnapToSurfaceNormal, level, toggle_snap_to_surface_normal,
                                   CCommandDescription("Toggle snapping to surface normal"));
REGISTER_EDITOR_UI_COMMAND_DESC(level, toggle_snap_to_surface_normal, "", "N", "icons:common/viewport-snap-normal.ico")

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PyToggle_display_helpers, level, toggle_display_helpers,
                                   CCommandDescription("Toggle display of helpers in the level editor viewport"));
REGISTER_EDITOR_UI_COMMAND_DESC(level, toggle_display_helpers, "", "Ctrl+H", "icons:Viewport/viewport-helpers.ico")

