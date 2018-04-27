// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "AssetEditor.h"

#include "AssetManager.h"
#include "AssetType.h"
#include "EditableAsset.h"
#include "Loader/AssetLoaderHelpers.h"
#include "Browser/AssetBrowserDialog.h"
#include "Controls/SingleSelectionDialog.h"
#include "Controls/QuestionDialog.h"
#include "QtUtil.h"
#include "FilePathUtil.h"
#include "CryExtension/CryGUID.h"
#include "DragDrop.h"

#include <QCloseEvent>

namespace Private_AssetEditor
{

//! Makes a temporary copy of files in construct time
//!	and moves them back in the destructor.
class CAutoFileRecovery
{
public:
	CAutoFileRecovery(const std::vector<string>& files)
	{
		if (files.empty())
		{
			return;
		}

		static const string tempPrefix = GetTemporaryDirectoryPath();

		m_files.reserve(files.size());

		ICryPak* const pCryPak = GetISystem()->GetIPak();
		pCryPak->MakeDir(tempPrefix.c_str());
		for (const string& file : files)
		{
			const string tempFilemane = PathUtil::Make(tempPrefix, CryGUID::Create().ToString(), "tmp");
			if (!pCryPak->CopyFileOnDisk(file.c_str(), tempFilemane.c_str(), false))
			{
				break;
			}

			m_files.emplace_back(file, tempFilemane);
		}

		if (files.size() != m_files.size())
		{
			Discard();
		}
	}

	bool IsValid() const { return !m_files.empty() || !m_files.capacity(); }

	void Discard()
	{
		ICryPak* const pCryPak = GetISystem()->GetIPak();
		for (const auto& file : m_files)
		{
			pCryPak->RemoveFile(file.second.c_str());
		}
		m_files.clear();
	}

	virtual ~CAutoFileRecovery()
	{
		ICryPak* const pCryPak = GetISystem()->GetIPak();
		for (const auto& file : m_files)
		{
			pCryPak->CopyFileOnDisk(file.second.c_str(), file.first.c_str(), false);
			pCryPak->RemoveFile(file.second.c_str());
		}
	}
private:
	static string GetTemporaryDirectoryPath()
	{
		char path[ICryPak::g_nMaxPath] = {};
		return GetISystem()->GetIPak()->AdjustFileName("%USER%/temp", path, ICryPak::FLAGS_PATH_REAL | ICryPak::FLAGS_FOR_WRITING | ICryPak::FLAGS_ADD_TRAILING_SLASH);
	}

private:
	std::vector<std::pair<string, string>> m_files;
};

class CAutoAssetRecovery : public CAutoFileRecovery
{
public:
	CAutoAssetRecovery(const CAsset& asset)
		: CAutoFileRecovery(GetAssetFiles(asset, false))
	{
	}

	static std::vector<string> GetAssetFiles(const CAsset& asset, bool bIncludeSourceFile)
	{
		std::vector<string> files = asset.GetType()->GetAssetFiles(asset, bIncludeSourceFile, true);

		files.erase(std::remove_if(files.begin(), files.end(), [](const string& filename)
		{
			return !GetISystem()->GetIPak()->IsFileExist(filename.c_str(), ICryPak::eFileLocation_OnDisk);
		}), files.end());

		return files;
	}
};

};

CAssetEditor* CAssetEditor::OpenAssetForEdit(const char* editorClassName, CAsset* asset)
{
	CRY_ASSERT(asset);
	IPane* pane = GetIEditor()->CreateDockable(editorClassName);
	if (pane)
	{
		CAssetEditor* assetEditor = static_cast<CAssetEditor*>(pane);
		if (assetEditor->OpenAsset(asset))
		{
			return assetEditor;
		}
	}
	return nullptr;
}

CAssetEditor::CAssetEditor(const char* assetType, QWidget* pParent /*= nullptr*/)
	: CDockableEditor(pParent)
	, m_assetBeingEdited(nullptr)
{
	auto type = CAssetManager::GetInstance()->FindAssetType(assetType);
	CRY_ASSERT(type);//type must exist
	m_supportedAssetTypes.push_back(type);

	Init();
}

CAssetEditor::CAssetEditor(const QStringList& assetTypes, QWidget* pParent /*= nullptr*/)
	: CDockableEditor(pParent)
	, m_assetBeingEdited(nullptr)
{
	m_supportedAssetTypes.reserve(assetTypes.size());
	for (const QString& typeName : assetTypes)
	{
		auto type = CAssetManager::GetInstance()->FindAssetType(typeName.toStdString().c_str());
		CRY_ASSERT(type);//type must exist
		m_supportedAssetTypes.push_back(type);
	}

	Init();
}

void CAssetEditor::Init()
{
	InitGenericMenu();

	setAcceptDrops(true);
}

bool CAssetEditor::OpenAsset(CAsset* pAsset)
{
	//An asset can only be opened once in one editor
	if (pAsset->IsBeingEdited())
		return false;

	if (pAsset == m_assetBeingEdited)
		return true;

	if (!Close())
	{
		// User cancelled closing of currently opened asset.
		return false;
	}

	if (!OnOpenAsset(pAsset))
	{
		return false;
	}

	AddRecentFile(QString(pAsset->GetMetadataFile()));
	SetAssetBeingEdited(pAsset);

	CEditableAsset editableAsset(*pAsset);
	editableAsset.SetOpenedInAssetEditor(this);

	return true;
}

bool CAssetEditor::CanOpenAsset(CAsset* pAsset)
{
	if (!pAsset)
		return false;

	return std::find(m_supportedAssetTypes.begin(), m_supportedAssetTypes.end(), pAsset->GetType()) != m_supportedAssetTypes.end();
}

void CAssetEditor::InitGenericMenu()
{
	AddToMenu(CEditor::MenuItems::FileMenu);

	AddToMenu(CEditor::MenuItems::Open);
	AddToMenu(CEditor::MenuItems::Close);
	AddToMenu(CEditor::MenuItems::Save);
	AddToMenu(CEditor::MenuItems::SaveAs);
	AddToMenu(CEditor::MenuItems::RecentFiles);

	//TODO: help menu doesn't always occupy last position
	AddToMenu(CEditor::MenuItems::HelpMenu);
	AddToMenu(CEditor::MenuItems::Help);

	InitNewMenu();
}

int CAssetEditor::GetNewableAssetCount() const
{
	int newableAssets = 0;
	for (const auto& type : m_supportedAssetTypes)
	{
		newableAssets += type->CanBeCreated();
	}
	return newableAssets;
}

void CAssetEditor::UpdateWindowTitle()
{
	if (m_assetBeingEdited)
	{
		if (m_assetBeingEdited->IsModified())
			setWindowTitle(QString(m_assetBeingEdited->GetName()) + " *");
		else
			setWindowTitle(m_assetBeingEdited->GetName());

		setWindowIcon(m_assetBeingEdited->GetType()->GetIcon());
	}
	else
	{
		setWindowTitle(GetPaneTitle());
		setWindowIcon(QIcon());//TODO : this should be the pane's default icon, panes already have an icon from the Tools menu
	}
}

void CAssetEditor::SetAssetBeingEdited(CAsset* pAsset)
{
	if (m_assetBeingEdited == pAsset)
		return;

	bool bWasReadOnly = false;

	if (m_assetBeingEdited)
	{
		m_assetBeingEdited->signalChanged.DisconnectObject(this);
		bWasReadOnly = m_assetBeingEdited->IsReadOnly();
	}

	m_assetBeingEdited = pAsset;

	UpdateWindowTitle();

	if (pAsset)
	{
		CAssetManager::GetInstance()->signalBeforeAssetsRemoved.Connect([this](const std::vector<CAsset*>& assets)
		{
			if (std::find(assets.begin(), assets.end(), GetAssetBeingEdited()) != assets.end())
			{
			  OnCloseAsset();
			  CRY_ASSERT(GetAssetBeingEdited() != nullptr);
			  signalAssetClosed(GetAssetBeingEdited());
			  SetAssetBeingEdited(nullptr);
			}
		}, (uintptr_t)this);

		pAsset->signalChanged.Connect(this, &CAssetEditor::OnAssetChanged);

		if (bWasReadOnly != pAsset->IsReadOnly())
			OnReadOnlyChanged();
	}
	else
	{
		CAssetManager::GetInstance()->signalBeforeAssetsRemoved.DisconnectById((uintptr_t)this);

		//No longer considered read only after closing the asset
		if (bWasReadOnly)
			OnReadOnlyChanged();
	}
}

bool CAssetEditor::OnAboutToCloseAssetInternal(string& reason) const
{
	reason.clear();

	if (!m_assetBeingEdited)
	{
		return true;
	}

	if (m_assetBeingEdited->IsModified())
	{
		reason = QtUtil::ToString(tr("Asset '%1' has unsaved modifications.").arg(m_assetBeingEdited->GetName()));
		return false;
	}

	return OnAboutToCloseAsset(reason);
}

bool CAssetEditor::TryCloseAsset()
{
	if (!m_assetBeingEdited)
	{
		return true;
	}

	string reason;
	bool bClose = true;
	if (!GetIEditor()->IsMainFrameClosing() && !OnAboutToCloseAssetInternal(reason))
	{
		if (reason.empty())
		{
			// Show generic modification message.
			reason = QtUtil::ToString(tr("Asset '%1' has unsaved modifications.").arg(m_assetBeingEdited->GetName()));
		}

		const QString title = tr("Closing %1").arg(GetEditorName());
		const auto button = CQuestionDialog::SQuestion(title, QtUtil::ToQString(reason), QDialogButtonBox::Save | QDialogButtonBox::Discard | QDialogButtonBox::Cancel, QDialogButtonBox::Cancel);
		switch (button)
		{
		case QDialogButtonBox::Save:
			OnSave();
			bClose = true;
			break;
		case QDialogButtonBox::Discard:
			OnDiscardAssetChanges();
			GetAssetBeingEdited()->SetModified(false);
			bClose = true;
			break;
		case QDialogButtonBox::No:
		// Fall-through.
		// "No" is returned when a user clicked the "x" in the window bar.
		case QDialogButtonBox::Cancel:
			bClose = false;
			break;
		default:
			CRY_ASSERT(0 && "Unknown button");
			bClose = false;
			break;
		}
	}

	if (bClose)
	{
		OnCloseAsset();
		CRY_ASSERT(GetAssetBeingEdited() != nullptr);
		signalAssetClosed(GetAssetBeingEdited());
		SetAssetBeingEdited(nullptr);
		return true;
	}
	else
	{
		return false;
	}
}

void CAssetEditor::OnAssetChanged(CAsset& asset, int changeFlags)
{
	CRY_ASSERT(&asset == m_assetBeingEdited);

	if (changeFlags & eAssetChangeFlags_ReadOnly)
	{
		OnReadOnlyChanged();
	}

	if (changeFlags & eAssetChangeFlags_Modified)
	{
		UpdateWindowTitle();
	}
}

void CAssetEditor::InitNewMenu()
{
	const int newableAssets = GetNewableAssetCount();

	if (!newableAssets)
	{
		return;
	}
	else if (newableAssets == 1)
	{
		AddToMenu(MenuItems::New);
	}
	else
	{
		CAbstractMenu* const pSubMenu = GetMenu(MenuItems::FileMenu)->CreateMenu(tr("New"), 0, 0);
		for (const auto& type : m_supportedAssetTypes)
		{
			if (type->CanBeCreated())
			{
				QAction* const pAction = pSubMenu->CreateAction(tr("%1").arg(type->GetUiTypeName()));
				connect(pAction, &QAction::triggered, [this, type]()
				{
					InternalNewAsset(type);
				});
			}
		}
	}
}

bool CAssetEditor::OnNew()
{
	const int newableAssetCount = GetNewableAssetCount();
	if (newableAssetCount == 1)
	{
		InternalNewAsset(m_supportedAssetTypes[0]);
	}
	else if (newableAssetCount > 1)
	{
		std::vector<string> assetTypeNames;
		std::transform(m_supportedAssetTypes.begin(), m_supportedAssetTypes.end(), std::back_inserter(assetTypeNames), [](const auto& t)
		{
			return t->GetTypeName();
		});
		CSingleSelectionDialog assetTypeSelection;
		assetTypeSelection.setWindowTitle(tr("New asset type"));
		assetTypeSelection.SetOptions(assetTypeNames);
		if (assetTypeSelection.Execute())
		{
			InternalNewAsset(m_supportedAssetTypes[assetTypeSelection.GetSelectedIndex()]);
		}
	}
	return true;
}

void CAssetEditor::InternalNewAsset(CAssetType* pAssetType)
{
	const string assetTypeName = pAssetType->GetTypeName();

	const string assetBasePath = CAssetBrowserDialog::CreateSingleAssetForType(assetTypeName, CAssetBrowserDialog::OverwriteMode::NoOverwrite);
	if (assetBasePath.empty())
	{
		return; // Operation cancelled by user.
	}

	if (!Close())
		return;

	const string assetPath = assetBasePath + string().Format(".%s.cryasset", pAssetType->GetFileExtension());
	if (pAssetType->Create(assetPath))
	{
		CAsset* const pAsset = CAssetManager::GetInstance()->FindAssetForMetadata(assetPath);
		if (pAsset)
		{
			OpenAsset(pAsset);
		}
	}
}

bool CAssetEditor::OnOpen()
{
	std::vector<string> supportedAssetTypeNames;
	supportedAssetTypeNames.reserve(m_supportedAssetTypes.size());
	for (auto& assetType : m_supportedAssetTypes)
	{
		supportedAssetTypeNames.push_back(assetType->GetTypeName());
	}

	CAsset* const asset = CAssetBrowserDialog::OpenSingleAssetForTypes(supportedAssetTypeNames);
	if (asset)
	{
		(void)OpenAsset(asset);
	}
	return true;
}

bool CAssetEditor::OnOpenFile(const QString& path)
{
	auto asset = CAssetManager::GetInstance()->FindAssetForFile(path.toStdString().c_str());
	if (asset)
	{
		(void)OpenAsset(asset);
	}
	return true;
}

bool CAssetEditor::Close()
{
	if (!GetAssetBeingEdited())
	{
		return true;
	}

	return TryCloseAsset();
}

bool CAssetEditor::OnClose()
{
	//Note: this is only the callback for menu action, every other place should call close()
	(void)Close();
	return true;//returns true because the menu action is handled
}

bool CAssetEditor::CanQuit(std::vector<string>& unsavedChanges)
{
	string reason;
	if (!OnAboutToCloseAssetInternal(reason))
	{
		unsavedChanges.push_back(reason);
		return false;
	}
	return true;
}

void CAssetEditor::closeEvent(QCloseEvent* pEvent)
{
	if (TryCloseAsset())
	{
		pEvent->accept();
	}
	else
	{
		pEvent->ignore();
	}
}

void CAssetEditor::dragEnterEvent(QDragEnterEvent* pEvent)
{
	auto pDragDropData = CDragDropData::FromMimeData(pEvent->mimeData());
	if (pDragDropData->HasCustomData("Assets"))
	{
		QByteArray byteArray = pDragDropData->GetCustomData("Assets");
		QDataStream stream(byteArray);

		QVector<quintptr> tmp;
		stream >> tmp;

		QVector<CAsset*> assets = *reinterpret_cast<QVector<CAsset*>*>(&tmp);

		if (assets.size() == 1 && CanOpenAsset(assets[0]))
		{
			CDragDropData::ShowDragText(this, tr("Open"));
			pEvent->acceptProposedAction();
			return;
		}
	}

	if (pDragDropData->HasCustomData("EngineFilePaths"))
	{
		QByteArray byteArray = pDragDropData->GetCustomData("EngineFilePaths");
		QDataStream stream(byteArray);

		QStringList engineFilePaths;
		stream >> engineFilePaths;

		const auto meshType = CAssetManager::GetInstance()->FindAssetType("Mesh");

		if (engineFilePaths.size() == 1)
		{
			CAsset* asset = CAssetManager::GetInstance()->FindAssetForFile(engineFilePaths[0].toStdString().c_str());
			if (asset && CanOpenAsset(asset))
			{
				CDragDropData::ShowDragText(this, tr("Open"));
				pEvent->acceptProposedAction();
				return;
			}
		}
	}

	if (pDragDropData->HasFilePaths())
	{
		const auto filePaths = pDragDropData->GetFilePaths();
		const auto meshType = CAssetManager::GetInstance()->FindAssetType("Mesh");

		if (filePaths.size() == 1)
		{
			CAsset* asset = CAssetManager::GetInstance()->FindAssetForFile(filePaths[0].toStdString().c_str());
			if (asset && CanOpenAsset(asset))
			{
				CDragDropData::ShowDragText(this, tr("Open"));
				pEvent->acceptProposedAction();
				return;
			}
		}
	}
}

void CAssetEditor::dropEvent(QDropEvent* pEvent)
{
	auto pDragDropData = CDragDropData::FromMimeData(pEvent->mimeData());
	if (pDragDropData->HasCustomData("Assets"))
	{
		QByteArray byteArray = pDragDropData->GetCustomData("Assets");
		QDataStream stream(byteArray);

		QVector<quintptr> tmp;
		stream >> tmp;

		QVector<CAsset*> assets = *reinterpret_cast<QVector<CAsset*>*>(&tmp);

		if (assets.size() == 1 && CanOpenAsset(assets[0]))
		{
			OpenAsset(assets[0]);
			pEvent->acceptProposedAction();
			return;
		}
	}

	if (pDragDropData->HasCustomData("EngineFilePaths"))
	{
		QByteArray byteArray = pDragDropData->GetCustomData("EngineFilePaths");
		QDataStream stream(byteArray);

		QStringList engineFilePaths;
		stream >> engineFilePaths;

		const auto meshType = CAssetManager::GetInstance()->FindAssetType("Mesh");

		if (engineFilePaths.size() == 1)
		{
			CAsset* asset = CAssetManager::GetInstance()->FindAssetForFile(engineFilePaths[0].toStdString().c_str());
			if (asset && CanOpenAsset(asset))
			{
				OpenAsset(asset);
				pEvent->acceptProposedAction();
				return;
			}
		}
	}

	if (pDragDropData->HasFilePaths())
	{
		const auto filePaths = pDragDropData->GetFilePaths();
		const auto meshType = CAssetManager::GetInstance()->FindAssetType("Mesh");

		if (filePaths.size() == 1)
		{
			CAsset* asset = CAssetManager::GetInstance()->FindAssetForFile(filePaths[0].toStdString().c_str());
			if (asset && CanOpenAsset(asset))
			{
				OpenAsset(asset);
				pEvent->acceptProposedAction();
				return;
			}
		}
	}
}

bool CAssetEditor::InternalSaveAsset(CAsset* pAsset)
{
	//TODO: Figure out how to handle writing metadata generically without opening the file twice
	//(one in OnSaveAsset implementation and one here)

	//TODO: here the editable asset retains every metadata of the old asset, which means if it is not overwritten in OnSaveAsset, some metadata could carry over.
	//Perhaps the safest way would be to clear it before passing it. Also means that calling things like AddFile() in there will result in warnings because the file was duplicated etc...

	CEditableAsset editAsset(*pAsset);
	if (!OnSaveAsset(editAsset))
	{
		return false;
	}

	editAsset.InvalidateThumbnail();
	editAsset.WriteToFile();
	pAsset->SetModified(false);

	return true;
}

bool CAssetEditor::Save()
{
	if (!m_assetBeingEdited)
	{
		return true;
	}

	return InternalSaveAsset(m_assetBeingEdited);
}

bool CAssetEditor::OnSave()
{
	Save();
	return true;
}

bool CAssetEditor::OnSaveAs()
{
	if (!m_assetBeingEdited)
	{
		return true;
	}

	CRY_ASSERT(!m_supportedAssetTypes.empty());
	CAssetType* const pAssetType = m_supportedAssetTypes[0];
	const string assetTypeName = pAssetType->GetTypeName();

	// #TODO: Let user save any of the supported asset types.
	const string assetBasePath = CAssetBrowserDialog::SaveSingleAssetForType(assetTypeName);
	if (assetBasePath.empty())
	{
		return true; // Operation cancelled by user.
	}
	const string newAssetPath = assetBasePath + string().Format(".%s.cryasset", pAssetType->GetFileExtension());

	if (newAssetPath.Compare(m_assetBeingEdited->GetMetadataFile()) == 0)
	{
		return OnSave();
	}

	CAssetManager* const pAssetManager = CAssetManager::GetInstance();
	CAsset* pAsset = pAssetManager->FindAssetForMetadata(newAssetPath);
	if (pAsset)
	{
		// Cancel if unable to delete.
		pAssetManager->DeleteAssets({ pAsset }, true);
		pAsset = pAssetManager->FindAssetForMetadata(newAssetPath);
		if (pAsset)
		{
			return true;
		}
	}

	// Create new asset on disk.
	if (!InternalSaveAs(newAssetPath))
	{
		return true;
	}

	pAsset = AssetLoader::CAssetFactory::LoadAssetFromXmlFile(newAssetPath);
	pAssetManager->MergeAssets({ pAsset });

	if (pAsset)
	{
		// Close previous asset and unconditionally discard all changes.
		OnDiscardAssetChanges();
		GetAssetBeingEdited()->SetModified(false);
		OnCloseAsset();
		CRY_ASSERT(GetAssetBeingEdited() != nullptr);
		signalAssetClosed(GetAssetBeingEdited());
		SetAssetBeingEdited(nullptr);

		OpenAsset(pAsset);
	}
	return true;
}

bool CAssetEditor::InternalSaveAs(const string& newAssetPath)
{
	CRY_ASSERT(m_assetBeingEdited);

	using namespace Private_AssetEditor;

	// 1. Make a temp copy of asset files.
	// 2. Save asset changes.
	// 3. Move updated files under the new asset name.
	// 4. Restore old files from the temp copy.
	// TODO: consider a direct implementation of the SaveAs. e.g CAssetType::SaveAs(CAsset& asset, string newAssetPath)

	CAutoAssetRecovery tempCopy(*m_assetBeingEdited);
	if (!tempCopy.IsValid() || !OnSave())
	{
		CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "Unable to copy asset: %s", m_assetBeingEdited->GetName());
		return false;
	}

	const bool result = m_assetBeingEdited->GetType()->CopyAsset(m_assetBeingEdited, newAssetPath);
	size_t numberOfFilesDeleted(0);
	m_assetBeingEdited->GetType()->DeleteAssetFiles(*m_assetBeingEdited, false, numberOfFilesDeleted);
	// tempCopy restores asset files.
	return result;
}

bool CAssetEditor::SaveBackup(const string& backupFolder)
{
	CRY_ASSERT(m_assetBeingEdited);
	using namespace Private_AssetEditor;

	// 1. Make a temporary copy of the current asset files.
	// 2. Save asset changes.
	// 3. Move updated files to the backupFolder.
	// 4. Restore old files from the temp copy.

	CAutoAssetRecovery tempCopy(*m_assetBeingEdited);
	
	if (!tempCopy.IsValid() || !OnSave())
	{
		return false;
	}

	std::vector<string> files = CAutoAssetRecovery::GetAssetFiles(*m_assetBeingEdited, false);

	ICryPak* const pCryPak = GetISystem()->GetIPak();
	pCryPak->MakeDir(backupFolder.c_str());
	const string assetsRoot = PathUtil::GetGameProjectAssetsPath();
	for (const auto& file : files)
	{
		CRY_ASSERT(strncmp(file.c_str(), assetsRoot.c_str(), assetsRoot.size()));
		CryPathString destFile = PathUtil::Make(backupFolder.c_str(), file.c_str() + assetsRoot.size());
		pCryPak->MakeDir(PathUtil::GetDirectory(destFile.c_str()));
		PathUtil::MoveFileAllowOverwrite(file.c_str(), destFile.c_str());
	}
	
	// tempCopy restores asset files.
	return true;
}

bool CAssetEditor::IsReadOnly() const
{
	return GetAssetBeingEdited() != nullptr ? GetAssetBeingEdited()->IsReadOnly() : false;
}

