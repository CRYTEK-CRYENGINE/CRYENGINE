// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DialogCommon.h"
#include "FbxScene.h"
#include "SandboxPlugin.h"
#include "AsyncHelper.h"
#include "MaterialHelpers.h"
#include "TextureManager.h"
#include "Util/FileUtil.h"
#include "TempRcObject.h"
#include "ImporterUtil.h"

// EditorCommon
#include <Controls\QuestionDialog.h>
#include <CryIcon.h>
#include <FileDialogs/ExtensionFilter.h>
#include <FileDialogs/SystemFileDialog.h>
#include <FileDialogs/EngineFileDialog.h>
#include <FilePathUtil.h>
#include <DragDrop.h>
#include <Notifications/NotificationCenter.h>
#include <ThreadingUtils.h>

#include <AssetSystem/Asset.h>
#include <AssetSystem/AssetManager.h>
#include <AssetSystem/Browser/AssetBrowserDialog.h>

// EditorQt

// Qt
#include <QFile>
#include <QFileInfo>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QTemporaryDir>
#include <QToolBar>

static const QString s_initialFilePropertyName = QStringLiteral("initialFile");

void SJointsViewSettings::Serialize(Serialization::IArchive& ar)
{
	ar(m_bShowJointNames, "showJointNames", "Joint names");
}

namespace MeshImporter
{
namespace Private_DialogCommon
{

static QString PromptForImportFbxFile(QWidget* pParent, const QString& initialFile)
{
	FbxTool::TIndex numExtensions;
	const char* const* const ppExtensions = FbxTool::GetSupportedFileExtensions(numExtensions);

	ExtensionFilterVector filters;
	QStringList extensions;

	for (FbxTool::TIndex i = 0; i < numExtensions; ++i)
	{
		const QString extension = QtUtil::ToQString(ppExtensions[i]);
		extensions << extension;
		filters << CExtensionFilter(QObject::tr("%1 files").arg(extension.toUpper()), extension);
	}
	if (!filters.isEmpty())
	{
		filters.prepend(CExtensionFilter(QObject::tr("All supported file types (%1)").arg(extensions.join(L',')), extensions));
	}

	filters << CExtensionFilter(QObject::tr("All files (*.*)"), "*");

	CSystemFileDialog::RunParams runParams;
	runParams.initialDir = QFileInfo(initialFile).absolutePath();
	runParams.initialFile = initialFile;
	runParams.title = QObject::tr("Select file to import");
	runParams.extensionFilters = filters;

	return CSystemFileDialog::RunImportFile(runParams, pParent);
}

static void FillExtensionFilters(ExtensionFilterVector& extensionFilters, int fileFormatFlags)
{
	if (fileFormatFlags & CBaseDialog::eFileFormat_CGF)
	{
		extensionFilters << CExtensionFilter(QStringLiteral("CGF files"), QStringLiteral("cgf"));
	}

	if (fileFormatFlags & CBaseDialog::eFileFormat_JSON)
	{
		extensionFilters << CExtensionFilter(QStringLiteral("Json files"), QStringLiteral("json"));
	}

	if (fileFormatFlags & CBaseDialog::eFileFormat_CHR)
	{
		extensionFilters << CExtensionFilter(QStringLiteral("Skeleton files"), QStringLiteral("chr"));
	}

	if (fileFormatFlags & CBaseDialog::eFileFormat_I_CAF)
	{
		extensionFilters << CExtensionFilter(QStringLiteral("Animation files"), QStringLiteral("i_caf"));
	}

	if (fileFormatFlags & CBaseDialog::eFileFormat_SKIN)
	{
		extensionFilters << CExtensionFilter(QStringLiteral("Skin files"), QStringLiteral("skin"));
	}
}

static QString RemoveExtension(const QString& str)
{
	// TODO: This is the super lazy solution.
	string tmp = QtUtil::ToString(str);
	PathUtil::RemoveExtension(tmp);
	return QtUtil::ToQString(tmp);
}

bool ShowDiscardChangesDialog()
{
	CQuestionDialog dialog;
	dialog.SetupQuestion(
	  QObject::tr("Unsaved changes"),
	  QObject::tr("Discard unsaved changes?"),
	  QDialogButtonBox::Yes | QDialogButtonBox::No);

	return dialog.Execute() == QDialogButtonBox::Yes;
}

} // namespace Private_DialogCommon

// ==================================================
// CBaseDialog::SSceneData
// ==================================================

struct CBaseDialog::SSceneData
{
	QString targetFilename;  //!< 'Save' file path. Relative to asset directory.
	CAsset* pAsset; // Asset associate with this file. May be nullptr.
};

// ==================================================
// CBaseDialog
// ==================================================

CBaseDialog::CBaseDialog(QWidget* pParent)
	: QWidget(pParent)
	, m_bLoadingSuspended(false)
{
	m_pTaskHost.reset(new CAsyncTaskHostForDialog(this));

	m_sceneManager.AddAssignSceneCallback(std::bind(&CBaseDialog::BaseAssignScene, this, std::placeholders::_1));
	m_sceneManager.AddUnloadSceneCallback(std::bind(&CBaseDialog::UnloadScene, this));

	GetIEditor()->RegisterNotifyListener(this);

	setAcceptDrops(true);
}

bool CBaseDialog::MayUnloadSceneInternal()
{
	return MayUnloadScene() || Private_DialogCommon::ShowDiscardChangesDialog();
}

void CBaseDialog::BaseAssignScene(const SImportScenePayload* pUserData)
{
	m_sceneData.reset(new SSceneData());
	if (pUserData)
	{
		m_sceneData->targetFilename = pUserData->targetFilename;
		m_sceneData->pAsset = pUserData->pAsset;
	}

	AssignScene(pUserData);
}

void CBaseDialog::CreateMenu(IDialogHost* pDialogHost)
{
	if (GetToolButtons() & eToolButtonFlags_Reimport)
	{
		pDialogHost->Host_AddToMenu("File", "meshimporter.reimport");
	}
}

const FbxTool::CScene* CBaseDialog::GetScene() const
{
	return m_sceneManager.GetScene();
}

FbxTool::CScene* CBaseDialog::GetScene()
{
	return m_sceneManager.GetScene();
}

bool CBaseDialog::SaveAs(const QString& targetFilePath)
{
	SSaveContext ctx;
	ctx.pTempDir = CreateTemporaryDirectory();
	ctx.targetFilePath = QtUtil::ToString(targetFilePath);

	if (!ctx.pTempDir)
	{
		return false;
	}

	GetIEditor()->GetSystem()->GetIPak()->MakeDir(PathUtil::GetDirectory(ctx.targetFilePath.c_str()));

	const string absMetaSrcPath = 
		m_sceneData->pAsset
		? PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), m_sceneData->pAsset->GetMetadataFile())
		: QtUtil::ToString(targetFilePath) + ".cryasset";
	const string tail = string().Format("/%s", PathUtil::GetFile(absMetaSrcPath));
	const string absMetaTmpPath = QtUtil::ToString(ctx.pTempDir->path() + tail);

	// Copy asset meta-data (.cryasset) to temporary save directory, if present, so that the RC takes the old data into account.
	if (!QFile::copy(QtUtil::ToQString(absMetaSrcPath), ctx.pTempDir->path() + QtUtil::ToQString(tail)) && m_sceneData->pAsset)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, 
			"Asset meta-data '%s' is missing. A new one will be created.", 
			m_sceneData->pAsset->GetMetadataFile());
	}

	return SaveAs(ctx);
}

CAsyncTaskHostForDialog* CBaseDialog::GetTaskHost()
{
	return m_pTaskHost.get();
}

const CSceneManager& CBaseDialog::GetSceneManager() const
{
	return m_sceneManager;
}

CSceneManager& CBaseDialog::GetSceneManager()
{
	return m_sceneManager;
}

void CBaseDialog::dragEnterEvent(QDragEnterEvent* pEvent)
{
	auto pDragDropData = CDragDropData::FromMimeData(pEvent->mimeData());
	const QStringList filePaths = pDragDropData->GetFilePaths();
	if (!filePaths.empty())
	{
		pEvent->acceptProposedAction();
	}
}

void CBaseDialog::dropEvent(QDropEvent* pEvent)
{
	auto pDragDropData = CDragDropData::FromMimeData(pEvent->mimeData());
	const QStringList filePaths = pDragDropData->GetFilePaths();
	CRY_ASSERT(!filePaths.empty());
	OnDropFile(filePaths.first());
}

CBaseDialog::~CBaseDialog()
{
	GetIEditor()->UnregisterNotifyListener(this);
}

QString CBaseDialog::ShowImportFilePrompt()
{
	using namespace Private_DialogCommon;

	const QString initialFilePath = CFbxToolPlugin::GetInstance()->GetPersonalizationProperty(s_initialFilePropertyName).toString();
	return PromptForImportFbxFile(nullptr, initialFilePath);
}

QString CBaseDialog::ShowOpenFilePrompt(int fileFormatFlags)
{
	using namespace Private_DialogCommon;

	CRY_ASSERT(fileFormatFlags);

	CEngineFileDialog::RunParams runParams;
	runParams.title = tr("Open ...");
	runParams.initialDir = "Objects";
	runParams.initialFile = GetAbsoluteGameFolderPath();

	FillExtensionFilters(runParams.extensionFilters, fileFormatFlags);

	return CEngineFileDialog::RunGameOpen(runParams, this);
}

QString CBaseDialog::ShowSaveAsFilePrompt(int fileFormatFlags)
{
	using namespace Private_DialogCommon;

	CRY_ASSERT(fileFormatFlags);

	CEngineFileDialog::RunParams runParams;
	runParams.title = tr("Save as...");
	runParams.initialDir = "Objects";

	if (GetScene() && GetSceneManager().GetImportFile()->GetFilePath() == GetSceneManager().GetImportFile()->GetOriginalFilePath())
	{
		runParams.initialFile = RemoveExtension(GetSceneManager().GetImportFile()->GetFilePath());
	}
	else
	{
		runParams.initialFile = GetAbsoluteGameFolderPath();
	}

	FillExtensionFilters(runParams.extensionFilters, fileFormatFlags);

	return CEngineFileDialog::RunGameSave(runParams, this);
}

QString CBaseDialog::ShowSaveToDirectoryPrompt()
{
	using namespace Private_DialogCommon;

	CEngineFileDialog::RunParams runParams;
	runParams.title = tr("Save to directory...");
	runParams.initialDir = "Objects";

	if (GetScene() && GetSceneManager().GetImportFile()->GetFilePath() == GetSceneManager().GetImportFile()->GetOriginalFilePath())
	{
		runParams.initialDir = QFileInfo(GetSceneManager().GetImportFile()->GetFilePath()).dir().path();
	}
	else
	{
		runParams.initialDir = GetAbsoluteGameFolderPath();
	}

	return CEngineFileDialog::RunGameSelectDirectory(runParams, this);
}

void CBaseDialog::ImportFile(const QString& filePath, const SImportScenePayload* pUserData)
{
	GetSceneManager().ImportFile(filePath, pUserData, GetTaskHost());
}

bool CBaseDialog::IsLoadingSuspended() const
{
	return m_bLoadingSuspended;
}

void CBaseDialog::OnEditorNotifyEvent(EEditorNotifyEvent evt)
{
	if (evt == eNotify_OnIdleUpdate)
	{
		OnIdleUpdate();
	}
	else if (evt == eNotify_OnBeginLoad || evt == eNotify_OnBeginGameMode)
	{
		OnReleaseResources();

		assert(!m_bLoadingSuspended);
		m_bLoadingSuspended = true;
	}
	else if (evt == eNotify_OnEndLoad || evt == eNotify_OnEndGameMode)
	{
		assert(m_bLoadingSuspended);
		m_bLoadingSuspended = false;

		OnReloadResources();
	}
}

bool CBaseDialog::OnImportFile()
{
	if (!MayUnloadSceneInternal())
	{
		return false;
	}

	const QString filePath = ShowImportFilePrompt();
	if (filePath.isEmpty())
	{
		return false;
	}

	OnCloseAsset();

	ImportFile(filePath);

	return true;
}

void CBaseDialog::OnReimportFile()
{
	ReimportFile();
}

void CBaseDialog::OnDropFile(const QString& filePath)
{
	if (!MayUnloadSceneInternal())
	{
		return;
	}

	ImportFile(filePath);
}

//! \p flags Bit-wise combination of CBaseDialog::EFileFormats.
std::vector<string> GetAssetTypesFromFileFormatFlags(int flags)
{
	std::vector<string> assetTypes;
	if (0 != (flags & CBaseDialog::eFileFormat_CGF))
	{
		assetTypes.push_back("Mesh");
	}
	if (0 != (flags & CBaseDialog::eFileFormat_JSON))
	{
		// Not an asset type.
	}
	if (0 != (flags & CBaseDialog::eFileFormat_CHR))
	{
		assetTypes.push_back("Skeleton");
	}
	if (0 != (flags & CBaseDialog::eFileFormat_I_CAF))
	{
		assetTypes.push_back("Animation");
	}
	if (0 != (flags & CBaseDialog::eFileFormat_SKIN))
	{
		assetTypes.push_back("SkinnedMesh");
	}
	return assetTypes;
}

static std::vector<string> GetExtensionsFromFileFormatFlags(int flags)
{
	std::vector<string> exts;
	if (0 != (flags & CBaseDialog::eFileFormat_CGF))
	{
		exts.push_back("cgf");
	}
	if (0 != (flags & CBaseDialog::eFileFormat_JSON))
	{
		exts.push_back("json");
	}
	if (0 != (flags & CBaseDialog::eFileFormat_CHR))
	{
		exts.push_back("chr");
	}
	if (0 != (flags & CBaseDialog::eFileFormat_I_CAF))
	{
		exts.push_back("caf"); // Intended;
	}
	if (0 != (flags & CBaseDialog::eFileFormat_SKIN))
	{
		exts.push_back("skin");
	}
	return exts;
}

void CBaseDialog::OnOpen()
{
	if (!MayUnloadSceneInternal())
	{
		return;
	}

	string assetFilePath;

	const auto pickerState = (EAssetResourcePickerState)gEnv->pConsole->GetCVar("ed_enableAssetPickers")->GetIVal();
	if (pickerState == EAssetResourcePickerState::EnableRecommended || pickerState == EAssetResourcePickerState::EnableAll)
	{
		auto assetTypes = GetAssetTypesFromFileFormatFlags(GetOpenFileFormatFlags());
		const CAsset* const pAsset = CAssetBrowserDialog::OpenSingleAssetForTypes(assetTypes);
		if (pAsset && pAsset->GetFilesCount())
		{
			assetFilePath = pAsset->GetFile(0);
		}
	}
	else
	{
		assetFilePath = QtUtil::ToString(ShowOpenFilePrompt(GetOpenFileFormatFlags()));
	}

	if (assetFilePath.empty())
	{
		return;
	}

	(void)Open(assetFilePath);
}

bool CBaseDialog::Open(const string& filePath)
{	
	if (!MayUnloadSceneInternal())
	{
		return false;
	}

	string sourceMetaFilePath = filePath;

	CAsset* pAsset = nullptr;
	if (IsAssetMetaDataFile(filePath))
	{
		pAsset = CAssetManager::GetInstance()->FindAssetForMetadata(filePath);
		if (pAsset)
		{
			if (pAsset->IsBeingEdited())
			{
				GetIEditor()->GetNotificationCenter()->ShowInfo(tr("Asset already open"), tr("'%1' already open").arg(pAsset->GetName()));
				return false;  // A particular asset can only edited by a single editor at a time.
			}

			const int fmt = GetOpenFileFormatFlags() & ~eFileFormat_JSON;
			sourceMetaFilePath = GetFileFromAsset(*pAsset, GetExtensionsFromFileFormatFlags(fmt));
		}
	}

	const string absSourceMetaFilePath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), sourceMetaFilePath);

	std::unique_ptr<FbxMetaData::SMetaData> pMetaData(new FbxMetaData::SMetaData);
	if (!ReadMetaDataFromFile(QtUtil::ToQString(absSourceMetaFilePath), *pMetaData))
	{
		return false;
	}

	string absSourceFilePath;
	if (pAsset && pAsset->GetSourceFile() && *pAsset->GetSourceFile() && (!pAsset->GetFilesCount() || stricmp(pAsset->GetSourceFile(), pAsset->GetFile(0)) != 0) )
	{
		absSourceFilePath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), pAsset->GetSourceFile());
	}
	else
	{
		// When opening a file, the source file is expected to be in the same directory.
		absSourceFilePath = PathUtil::Make(PathUtil::GetParentDirectory(absSourceMetaFilePath), pMetaData->sourceFilename);
	}

	std::unique_ptr<MeshImporter::SImportScenePayload> pPayload(new MeshImporter::SImportScenePayload());
	pPayload->pMetaData = std::move(pMetaData);
	pPayload->targetFilename = sourceMetaFilePath;
	pPayload->pAsset = pAsset;

	ImportFile(QtUtil::ToQString(absSourceFilePath), pPayload.release());

	return true;
}

QString GetFilename(const QString& filePath)
{
	QFileInfo fileInfo(filePath);
	return fileInfo.exists() ? fileInfo.fileName() : QString();
}

void RenameAllowOverwrite(QFile& file, const QString& newFilePath)
{
	if (!file.rename(newFilePath))
	{
		if (QFile::remove(newFilePath))
		{
			file.rename(newFilePath);
		}
	}
}

void RenameAllowOverwrite(const string& oldFilePath, const string& newFilePath)
{
	const QString oldFilePathCopy(QtUtil::ToQString(oldFilePath));
	const QString newFilePathCopy(QtUtil::ToQString(newFilePath));
	if (!QFile::rename(oldFilePathCopy, newFilePathCopy))
	{
		if (QFile::remove(newFilePathCopy))
		{
			QFile::rename(oldFilePathCopy, newFilePathCopy);
		}
	}
}

void CBaseDialog::OnSaveAs()
{
	if (!GetScene())
	{
		return;  // Nothing to save.
	}

	const QString assetFilePath = ShowSaveAsDialog();
	if (assetFilePath.isEmpty())
	{
		return;
	}

	const QString absFilePath = GetAbsoluteGameFolderPath() + QDir::separator() + assetFilePath;
	if (!SaveAs(absFilePath))
	{
		CQuestionDialog::SCritical(tr("Save to CGF failed"), tr("Failed to write current data to %1").arg(absFilePath));
	}
	else
	{
		CRY_ASSERT(m_sceneData);
		m_sceneData->targetFilename = assetFilePath;
	}
}

void CBaseDialog::OnSave()
{
	if (!GetScene())
	{
		return;
	}

	CRY_ASSERT(m_sceneData);

	if (m_sceneData->targetFilename.isEmpty())
	{
		OnSaveAs();
		return;
	}

	const QString targetFilePath = QtUtil::ToQString(PathUtil::GetGameProjectAssetsPath()) + "/" + m_sceneData->targetFilename;

	if (!SaveAs(targetFilePath))
	{
		CQuestionDialog::SCritical(tr("Save to CGF failed"), tr("Failed to write current data to %1").arg(targetFilePath));
	}
}

void CBaseDialog::OnCloseAsset()
{
	GetSceneManager().UnloadScene();
}

std::unique_ptr<CTempRcObject> CBaseDialog::CreateTempRcObject()
{
	return std::unique_ptr<CTempRcObject>(new CTempRcObject(GetTaskHost(), &m_sceneManager));
}

string CBaseDialog::GetTargetFilePath() const
{
	return m_sceneData ? QtUtil::ToString(m_sceneData->targetFilename) : string();
}

QString ReplaceExtension(const QString& str, const char* ext)
{
	// TODO: This is the lazy solution.
	return QtUtil::ToQString(PathUtil::ReplaceExtension(QtUtil::ToString(str), ext));
}

} // namespace MeshImporter

