// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "LevelEditor.h"

// LevelEditor
#include "LevelAssetType.h"
#include "LevelExplorer.h"
#include "LevelFileUtils.h"
#include "NewLevelDialog.h"
#include "TagLocations.h"

// EditorQt
#include "EditMode/VertexSnappingModeTool.h"
#include "QT/QtMainFrame.h"
#include "QT/QToolTabManager.h"
#include "AlignTool.h"
#include "CryEdit.h"
#include "CryEditDoc.h"
#include "GameEngine.h"
#include "GameExporter.h"
#include "IEditorImpl.h"
#include "LevelIndependentFileMan.h"
#include "ViewManager.h"
#include "Objects/SelectionGroup.h"

#include <Util/Clipboard.h>

// EditorCommon
#include <AssetSystem/AssetManager.h>
#include <AssetSystem/Browser/AssetBrowser.h>
#include <AssetSystem/Browser/AssetBrowserDialog.h>
#include <AssetSystem/EditableAsset.h>
#include <Commands/QCommandAction.h>
#include <Controls/QuestionDialog.h>
#include <EditorFramework/InspectorLegacy.h>
#include <EditorFramework/Events.h>
#include <EditorFramework/PreferencesDialog.h>
#include <EditorStyleHelper.h>
#include <FileUtils.h>
#include <LevelEditor/Tools/PickObjectTool.h>
#include <Notifications/NotificationCenterTrayWidget.h>
#include <Objects/ObjectLoader.h>
#include <PathUtils.h>
#include <Preferences/GeneralPreferences.h>
#include <Preferences/SnappingPreferences.h>
#include <QtUtil.h>
#include <QtViewPane.h>
#include <ThreadingUtils.h>
#include <Util/FileUtil.h>
#include <IObjectManager.h>

// CryCommon
#include <CrySandbox/ScopedVariableSetter.h>
#include <CrySystem/IProjectManager.h>
#include <CrySystem/ICmdLine.h>

// Qt
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMenu>
#include <QMenuBar>
#include <QtGlobal>
#include <QVBoxLayout>

// Register viewpanes defined in EditorCommon
REGISTER_VIEWPANE_FACTORY_AND_MENU(CNotificationCenterDockable, "Notification Center", "Advanced", true, "Advanced")

class CLevelEditorInspector : public CInspectorLegacy
{
public:
	CLevelEditorInspector()
		: CInspectorLegacy(CEditorMainFrame::GetInstance()->GetLevelEditor())
	{
		GetIEditorImpl()->GetObjectManager()->EmitPopulateInspectorEvent();
	}
};

REGISTER_VIEWPANE_FACTORY_AND_MENU(CLevelEditorInspector, "Properties", "Tools", false, "Level Editor")

class CQuickAssetBrowser : public CAssetBrowser
{
public:
	CQuickAssetBrowser()
	{
		RegisterAction("asset.toggle_browser", &CQuickAssetBrowser::OnToggleAssetBrowser);
	}

	void OnToggleAssetBrowser()
	{
		signalToggleAssetBrowser();
	}

	CCrySignal<void()> signalToggleAssetBrowser;
};

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
	FileUtils::RemoveFile(absPath);
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

	FileUtils::Pak::Unpak(cryPakPath, destinationFolder, std::move(progress));
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
		FileUtils::BackupFile(PathUtil::Make(levelFolder, szOldFilename));

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

}

CLevelEditor::CLevelEditor()
	: CEditor(nullptr)
	, m_pLevelExplorer(nullptr)
	, m_pFindDialog(nullptr)
	, m_pQuickAssetBrowser(nullptr)
	, m_pQuickAssetBrowserDialog(nullptr)
	, m_pTagLocations(new CTagLocations())
{
	InitActions();
}

CLevelEditor::~CLevelEditor()
{
	if (m_pQuickAssetBrowserDialog)
	{
		m_pQuickAssetBrowserDialog->deleteLater();
		m_pQuickAssetBrowserDialog = nullptr;
	}

	if (m_pFindDialog)
	{
		m_pFindDialog->deleteLater();
		m_pFindDialog = nullptr;
	}

	delete m_pTagLocations;
}

void CLevelEditor::InitActions()
{
	// Register general command handlers
	RegisterAction("general.new", &CLevelEditor::OnNew);
	RegisterAction("general.open", &CLevelEditor::OnOpen);
	RegisterAction("general.save", &CLevelEditor::OnSave);
	RegisterAction("general.save_as", &CLevelEditor::OnSaveAs);
	RegisterAction("general.delete", &CLevelEditor::OnDelete);
	RegisterAction("general.duplicate", &CLevelEditor::OnDuplicate);
	RegisterAction("general.cut", &CLevelEditor::OnCut);
	RegisterAction("general.copy", &CLevelEditor::OnCopy);
	RegisterAction("general.paste", &CLevelEditor::OnPaste);
	RegisterAction("general.select_all", &CLevelEditor::OnSelectAll);

	// Register level specific command handlers
	RegisterAction("level.snap_to_grid",           &CLevelEditor::EnableGridSnapping);
	RegisterAction("level.snap_to_angle",          &CLevelEditor::EnableAngleSnapping);
	RegisterAction("level.snap_to_scale",          &CLevelEditor::EnableScaleSnapping);
	RegisterAction("level.snap_to_vertex",         &CLevelEditor::EnableVertexSnapping);
	RegisterAction("level.snap_to_pivot",          &CLevelEditor::EnablePivotSnapping);
	RegisterAction("level.snap_to_terrain",        &CLevelEditor::EnableTerrainSnapping);
	RegisterAction("level.snap_to_geometry",       &CLevelEditor::EnableGeometrySnapping);
	RegisterAction("level.snap_to_surface_normal", &CLevelEditor::EnableSurfaceNormalSnapping);
	RegisterAction("level.toggle_snap_to_grid",           [this]() { EnableGridSnapping(!IsGridSnappingEnabled()); });
	RegisterAction("level.toggle_snap_to_angle",          [this]() { EnableAngleSnapping(!IsAngleSnappingEnabled()); });
	RegisterAction("level.toggle_snap_to_scale",          [this]() { EnableScaleSnapping(!IsScaleSnappingEnabled()); });
	RegisterAction("level.toggle_snap_to_vertex",         [this]() { EnableVertexSnapping(!IsVertexSnappingEnabled()); });
	RegisterAction("level.toggle_snap_to_pivot",          [this]() { EnablePivotSnapping(!IsPivotSnappingEnabled()); });
	RegisterAction("level.toggle_snap_to_terrain",        [this]() { EnableTerrainSnapping(!IsTerrainSnappingEnabled()); });
	RegisterAction("level.toggle_snap_to_geometry",       [this]() { EnableGeometrySnapping(!IsGeometrySnappingEnabled()); });
	RegisterAction("level.toggle_snap_to_surface_normal", [this]() { EnableSurfaceNormalSnapping(!IsSurfaceNormalSnappingEnabled()); });
	RegisterAction("level.tag_location",                  [this](int slot) { m_pTagLocations->TagLocation(slot); });
	RegisterAction("level.go_to_tag_location",            [this](int slot) { m_pTagLocations->GotoTagLocation(slot); });
	RegisterAction("asset.show_in_browser", &CLevelEditor::OnShowInAssetBrowser);
	RegisterAction("asset.toggle_browser", &CLevelEditor::OnToggleAssetBrowser);
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

		m_pTagLocations->LoadTagLocations();
	}
	else if (event == eNotify_OnMainFrameInitialized)
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

		// Load specified level from command line argument on Sandbox startup.
		const ICmdLineArg* pLevelArg = gEnv->pSystem->GetICmdLine()->FindArg(eCLAT_Pre, "level");
		if (pLevelArg != nullptr)
		{
			string levelName = pLevelArg->GetValue();
			if (!levelName.empty())
			{
				CCryEditApp::GetInstance()->DiscardLevelChanges();
				CCryEditApp::GetInstance()->LoadLevel(levelName.c_str());
			}
		}
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
	bEnable ? GetIEditorImpl()->GetLevelEditorSharedState()->SetEditTool("EditTool.VertexSnappingMode") : 
		GetIEditorImpl()->GetLevelEditorSharedState()->SetEditTool(NULL);

	VertexSnappingEnabled(bEnable);
}

void CLevelEditor::EnablePivotSnapping(bool bEnable)
{
	bEnable ? GetIEditorImpl()->GetLevelEditorSharedState()->PickObject(new CAlignPickCallback()) : 
		GetIEditorImpl()->GetLevelEditorSharedState()->SetEditTool(NULL);

	gSnappingPreferences.EnablePivotSnapping(bEnable);

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
	gSnappingPreferences.EnableSnapToTerrain(bEnable);

	TerrainSnappingEnabled(bEnable);
}

void CLevelEditor::EnableGeometrySnapping(bool bEnable)
{
	gSnappingPreferences.EnableSnapToGeometry(bEnable);

	GeometrySnappingEnabled(bEnable);
}

void CLevelEditor::EnableSurfaceNormalSnapping(bool bEnable)
{
	gSnappingPreferences.EnableSnapToNormal(bEnable);

	SurfaceNormalSnappingEnabled(bEnable);
}

bool CLevelEditor::IsVertexSnappingEnabled() const
{
	CEditTool* pTool = GetIEditorImpl()->GetLevelEditorSharedState()->GetEditTool();
	return pTool && pTool->IsKindOf(RUNTIME_CLASS(CVertexSnappingModeTool));
}

bool CLevelEditor::IsPivotSnappingEnabled() const
{
	return gSnappingPreferences.IsPivotSnappingEnabled();
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
	return gSnappingPreferences.IsSnapToTerrainEnabled();
}

bool CLevelEditor::IsGeometrySnappingEnabled() const
{
	return gSnappingPreferences.IsSnapToGeometryEnabled();
}

bool CLevelEditor::IsSurfaceNormalSnappingEnabled() const
{
	return gSnappingPreferences.IsSnapToNormalEnabled();
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
	if (FileUtils::MoveFileAllowOverwrite(oldFilename, newFilename))
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

	const static CAssetType* const pLevelType = GetIEditorImpl()->GetAssetManager()->FindAssetType("Level");
	CRY_ASSERT(pLevelType);

	const string selectedAssetPath = CAssetBrowserDialog::CreateSingleAssetForType(pLevelType->GetTypeName(), CAssetBrowserDialog::OverwriteMode::AllowOverwrite);
	if (selectedAssetPath.empty())
	{
		return true;
	}

	CNewLevelDialog settingsDialog;
	if (settingsDialog.exec() != QDialog::Accepted)
	{
		return true;
	}

	const string cryassetPath = string().Format("%s.%s.cryasset", selectedAssetPath.c_str(), pLevelType->GetFileExtension());
	const QString levelDirectory = QtUtil::ToQString(selectedAssetPath);

	// levelSaveDialog has got the confirmation to overwrite the level.
	CAsset* pAsset = CAssetManager::GetInstance()->FindAssetForMetadata(cryassetPath);

	// If new level path does not point to a level
	if (!pAsset)
	{
		const QString absLevelDirectory = QtUtil::ToQString(PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), selectedAssetPath));

		QDir dirToDelete(absLevelDirectory);
		if (dirToDelete.exists())
		{
			const QString question = tr("All existing contents of the folder %1 will be deleted.\nDo you really want to continue?").arg(levelDirectory);
			if (CQuestionDialog::SQuestion(tr("New Level"), question) != QDialogButtonBox::Yes)
			{
				return true;
			}

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
				auto messageText = tr("Failed to remove some files of the existing folder:\n%1").arg(levelDirectory);
				CQuestionDialog::SWarning(tr("New Level failed"), messageText);
				return true;
			}
		}
	}
	else
	{
		const CLevelType::SLevelCreateParams createParams = settingsDialog.GetResult();
		pAsset->signalAfterRemoved.Connect([createParams](const CAsset* pAsset)
		{
			const static CAssetType* const pLevelType = GetIEditorImpl()->GetAssetManager()->FindAssetType("Level");
			pLevelType->Create(pAsset->GetMetadataFile(), &createParams);
		});

		CAssetManager::GetInstance()->DeleteAssetsWithFiles({ pAsset });

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

	CAssetBrowserDialog dialog({ "Level" }, CAssetBrowserDialog::Mode::OpenSingleAsset);
	QString lastLoadedLevelName(GetIEditorImpl()->GetDocument()->GetLastLoadedLevelName());
	if (!lastLoadedLevelName.isEmpty())
	{
		CAssetManager* const pManager = CAssetManager::GetInstance();
		const CAsset* pAsset = pManager->FindAssetForFile(lastLoadedLevelName.toStdString().c_str());
		if (pAsset)
		{
			dialog.SelectAsset(*pAsset);
		}
	}

	if (dialog.Execute())
	{
		if (CAsset* pSelectedAsset = dialog.GetSelectedAsset())
		{
			CCryEditApp::GetInstance()->DiscardLevelChanges();

			const string levelPath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), pSelectedAsset->GetFile(0));
			CCryEditApp::GetInstance()->LoadLevel(levelPath);
		}
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

	CAssetBrowserDialog dialog({ "Level" }, CAssetBrowserDialog::Mode::Save);
	QString lastLoadedLevelName(GetIEditorImpl()->GetDocument()->GetLastLoadedLevelName());
	if (!lastLoadedLevelName.isEmpty())
	{
		CAssetManager* const pManager = CAssetManager::GetInstance();
		const CAsset* const pAsset = pManager->FindAssetForFile(QtUtil::ToString(lastLoadedLevelName).c_str());
		if (pAsset)
		{
			dialog.SelectAsset(*pAsset);
		}
	}

	if (dialog.Execute())
	{
		auto filename = CLevelType::MakeLevelFilename(dialog.GetSelectedAssetPath());
		GetIEditorImpl()->GetDocument()->DoSave(PathUtil::GamePathToCryPakPath(filename, true), true);
		SaveCryassetFile(GetIEditorImpl()->GetDocument()->GetPathName());
	}
	return true;
}

bool CLevelEditor::OnDelete()
{
	// If Edit tool active cannot delete object.
	if (GetIEditorImpl()->GetLevelEditorSharedState()->GetEditTool())
	{
		if (GetIEditorImpl()->GetLevelEditorSharedState()->GetEditTool()->OnKeyDown(GetIEditorImpl()->GetViewManager()->GetView(0), Qt::Key_Delete, 0, 0))
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
	if (!m_pFindDialog)
	{
		m_pFindDialog = new CEditorDialog(tr("Find Objects"));
		QVBoxLayout* pMainLayout = new QVBoxLayout();
		pMainLayout->setMargin(0);
		pMainLayout->setSpacing(0);
		m_pFindDialog->SetHideOnClose();
		m_pLevelExplorer = new CLevelExplorer();
		m_pLevelExplorer->Initialize();
		m_pLevelExplorer->SetSyncSelection(false);
		m_pLevelExplorer->SetModelType(CLevelExplorer::Objects);
		pMainLayout->addWidget(m_pLevelExplorer);
		m_pFindDialog->setLayout(pMainLayout);
		m_pFindDialog->Popup();
		m_pFindDialog->SetPosCascade();
		m_pLevelExplorer->GrabFocusSearchBar();
	}
	else
	{
		m_pFindDialog->Popup();
		m_pLevelExplorer->GrabFocusSearchBar();
	}
	return true;
}

void CLevelEditor::OnToggleAssetBrowser()
{
	if (!m_pQuickAssetBrowserDialog)
	{
		m_pQuickAssetBrowserDialog = new CEditorDialog("Quick Asset Browser");
		QVBoxLayout* pMainLayout = new QVBoxLayout();
		pMainLayout->setMargin(0);
		pMainLayout->setSpacing(0);
		m_pQuickAssetBrowser = new CQuickAssetBrowser();
		m_pQuickAssetBrowser->Initialize();
		m_pQuickAssetBrowser->signalToggleAssetBrowser.Connect(this, &CLevelEditor::OnToggleAssetBrowser);
		pMainLayout->addWidget(m_pQuickAssetBrowser);
		m_pQuickAssetBrowserDialog->setLayout(pMainLayout);
		m_pQuickAssetBrowserDialog->Popup();
		m_pQuickAssetBrowserDialog->SetHideOnClose();
		m_pQuickAssetBrowserDialog->SetPosCascade();
		m_pQuickAssetBrowser->GrabFocusSearchBar();
	}
	else if (m_pQuickAssetBrowserDialog->window() && m_pQuickAssetBrowserDialog->window()->isActiveWindow())
	{
		m_pQuickAssetBrowserDialog->hide();
	}
	else
	{
		m_pQuickAssetBrowserDialog->Popup();
		m_pQuickAssetBrowserDialog->activateWindow();
		m_pQuickAssetBrowser->GrabFocusSearchBar();
	}
}

void CLevelEditor::SaveCryassetFile(const string& levelPath)
{
	const string localLevelPath(PathUtil::ToGamePath(levelPath));
	CAsset* pAsset = GetIEditor()->GetAssetManager()->FindAssetForFile(localLevelPath);
	if (pAsset)
	{
		CEditableAsset editAsset(*pAsset);
		CLevelType::UpdateFilesAndDependencies(editAsset);
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
	CLevelType::UpdateFilesAndDependencies(editAsset);
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

	IObjectManager* pObjectManager = GetIEditorImpl()->GetObjectManager();

	CObjectArchive archive(pObjectManager, copyNode, true);

	CRandomUniqueGuidProvider guidProvider;
	archive.SetGuidProvider(&guidProvider);
	archive.LoadInCurrentLayer(true);
	pObjectManager->CreateAndSelectObjects(archive);

	return true;
}

bool CLevelEditor::OnSelectAll()
{
	GetIEditorImpl()->GetObjectManager()->SelectAll();
	return true;
}

void CLevelEditor::OnShowInAssetBrowser(const char* asset)
{
	CAssetBrowser* pAssetBrowser = static_cast<CAssetBrowser*>(CTabPaneManager::GetInstance()->FindPaneByClass("Asset Browser"));
	if (!pAssetBrowser)
	{
		if (!m_pQuickAssetBrowserDialog || !m_pQuickAssetBrowserDialog->isActiveWindow())
		{
			OnToggleAssetBrowser();
		}
		pAssetBrowser = m_pQuickAssetBrowser;
	}
	pAssetBrowser->SelectAsset(asset);
}
