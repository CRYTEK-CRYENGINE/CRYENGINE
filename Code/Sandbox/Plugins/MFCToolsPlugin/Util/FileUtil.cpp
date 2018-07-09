// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FileUtil.h"

#include "Util/MFCUtil.h"
#include <CryMemory/STLPoolAllocator.h>
#include "IndexedFiles.h"
#include "Dialogs/CheckOutDialog.h"
#include "ISourceControl.h"
#include "IObjectManager.h"
#include "UsedResources.h"
#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/IMaterial.h>
#include "Dialogs/GenericOverwriteDialog.h"
#include "Dialogs/UserOptions.h"
#include "Dialogs/SourceControlDescDlg.h"
#include "Util/Clipboard.h"
#include "Controls/DynamicPopupMenu.h"
#include "FileDialogs/EngineFileDialog.h"
#include "FileDialogs/SystemFileDialog.h"
#include "FileDialogs/FilePopupMenu.h"
#include <CrySystem/File/CryFile.h>
#include <io.h>
#include <time.h>
#include <QDir>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include "Controls/QuestionDialog.h"
#include "FilePathUtil.h"
#include "QtUtil.h"
#include <CryCore/Platform/CryLibrary.h>
#include <Preferences/GeneralPreferences.h>
#include "Objects/BaseObject.h"

bool CFileUtil::s_singleFileDlgPref[EFILE_TYPE_LAST] = { true, true, true, true, true };
bool CFileUtil::s_multiFileDlgPref[EFILE_TYPE_LAST] = { true, true, true, true, true };

namespace Private_FileUtil
{

void CopySourceControlPathToClipboard(const char* path)
{
	if (!GetIEditor()->IsSourceControlAvailable())
	{
		return;
	}
	char outPath[MAX_PATH];
	const bool bSuccess = GetIEditor()->GetSourceControl()->GetInternalPath(path, outPath, MAX_PATH);
	if (bSuccess)
	{
		qApp->clipboard()->setText(outPath);
	}
}

bool CheckIn(const char* path)
{
	if (GetIEditor()->IsSourceControlAvailable())
	{
		CSourceControlDescDlg dlg;
		if (dlg.DoModal() == IDOK)
		{
			return GetIEditor()->GetSourceControl()->CheckIn(path, dlg.m_sDesc);
		}
	}
	return true;
}

bool CheckOut(const char* path)
{
	if (GetIEditor()->IsSourceControlAvailable())
	{
		return GetIEditor()->GetSourceControl()->CheckOut(path);
	}
	return true;
}

bool UndoCheckout(const char* path)
{
	if (GetIEditor()->IsSourceControlAvailable())
	{
		return GetIEditor()->GetSourceControl()->UndoCheckOut(path);
	}
	return true;
}

bool GetLatestVersion(const char* path)
{
	if (GetIEditor()->IsSourceControlAvailable())
	{
		return GetIEditor()->GetSourceControl()->GetLatestVersion(path);
	}
	return true;
}

bool ShowHistory(const char* path)
{
	if (GetIEditor()->IsSourceControlAvailable())
	{
		return GetIEditor()->GetSourceControl()->History(path);
	}
	return true;
}

bool AddToSourceControl(const char* path)
{
	if (GetIEditor()->IsSourceControlAvailable())
	{
		CSourceControlDescDlg dlg;
		if (dlg.DoModal() == IDOK)
		{
			return GetIEditor()->GetSourceControl()->Add(path, dlg.m_sDesc);
		}
	}
	return true;
}

void OnFilePopupMenuPopulation(CFilePopupMenu* pMenu)
{
	const auto nativeFilePath = QDir::toNativeSeparators(pMenu->fileInfo().filePath());
	const auto gameFilePath = PathUtil::AbsolutePathToGamePath(nativeFilePath.toStdString().c_str());
	const auto nativeSepGameFilePathStr = QDir::fromNativeSeparators(QString::fromLocal8Bit(gameFilePath)).toStdString();
	const auto filePath = nativeSepGameFilePathStr.c_str();

	const auto fileAttr = GetIEditor()->IsSourceControlAvailable() ? CFileUtil::GetAttributes(filePath, false) : SCC_FILE_ATTRIBUTE_INVALID;
	if (fileAttr == SCC_FILE_ATTRIBUTE_INVALID)
	{
		return;
	}

	const bool bIsInPak = fileAttr & SCC_FILE_ATTRIBUTE_INPAK;
	const bool bIsScEnabled = fileAttr & SCC_FILE_ATTRIBUTE_MANAGED;
	if (bIsInPak && !bIsScEnabled)
	{
		auto action = pMenu->addAction(QObject::tr("File In Pak(Read Only)"));
		action->setEnabled(false);
	}
	else
	{
		const auto showSourceControlErrorMsgBox = []
		{
			CQuestionDialog::SCritical(QObject::tr("Error"), QObject::tr("Source Control Operation Failed.\nCheck if Source Control Provider correctly setup and working directory is correct."));
		};

		typedef CFilePopupMenu::SFilePopupMenuAction SFilePopupMenuAction;
		if (bIsScEnabled)
		{
			pMenu->addAction(new SFilePopupMenuAction(QObject::tr("Copy Source Control Path To Clipboard"), pMenu, [&] { CopySourceControlPathToClipboard(filePath);
			                                          }));

			const auto bIsScEnabledAndNotInPak = bIsScEnabled && !bIsInPak;
			if (bIsScEnabledAndNotInPak)
			{
				if (fileAttr & SCC_FILE_ATTRIBUTE_CHECKEDOUT)
				{
					pMenu->addAction(new SFilePopupMenuAction(QObject::tr("Check In"), pMenu, [&]
						{
							if (!CheckIn(filePath))
							{
							  showSourceControlErrorMsgBox();
							}
					  }));
				}
				if (fileAttr & SCC_FILE_ATTRIBUTE_READONLY)
				{
					pMenu->addAction(new SFilePopupMenuAction(QObject::tr("Check Out"), pMenu, [&]
						{
							if (!CheckOut(filePath))
							{
							  showSourceControlErrorMsgBox();
							}
					  }));
				}
				if (fileAttr & SCC_FILE_ATTRIBUTE_CHECKEDOUT)
				{
					pMenu->addAction(new SFilePopupMenuAction(QObject::tr("Undo Check Out"), pMenu, [&]
						{
							if (!CheckOut(filePath))
							{
							  showSourceControlErrorMsgBox();
							}
					  }));
				}
			}
			pMenu->addAction(new SFilePopupMenuAction(QObject::tr("Get Latest Version"), pMenu, [&]
				{
					if (!GetLatestVersion(filePath))
					{
					  showSourceControlErrorMsgBox();
					}
			  }));
			pMenu->addAction(new SFilePopupMenuAction(QObject::tr("Show History"), pMenu, [&]
				{
					if (!ShowHistory(filePath))
					{
					  showSourceControlErrorMsgBox();
					}
			  }));
		}
		else
		{
			pMenu->addAction(new SFilePopupMenuAction(QObject::tr("Add To Source Control"), pMenu, [&]
				{
					if (!AddToSourceControl(filePath))
					{
					  showSourceControlErrorMsgBox();
					}
			  }));
		}
	}
}

void SelectExtraItemFromPopup(CFileUtil::ExtraMenuItems* pItems, uint32 id)
{
	if (!pItems)
	{
		return;
	}

	pItems->selectedIndexIfAny = id;
}

void SelectFromPopup(bool* pIsSelected)
{
	if (pIsSelected)
	{
		*pIsSelected = true;
	}
}

void OpenFromPopup(const CString& fullPath, uint32 fileAttr)
{
	if (fileAttr & SCC_FILE_ATTRIBUTE_INPAK)
	{
		return;
	}
	const auto hInst = ShellExecute(NULL, "open", fullPath, NULL, NULL, SW_SHOWNORMAL);
	if ((DWORD_PTR)hInst <= 32)
	{
		if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Can't open the file. Do you want to open the file in the Notepad?"), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::No))
		{
			ShellExecute(NULL, "open", "notepad", fullPath, NULL, SW_SHOWNORMAL);
		}
	}
}

void ExploreFromPopup(const CString& fullPath, const CString& filename, uint32 fileAttr)
{
	if (fileAttr & SCC_FILE_ATTRIBUTE_INPAK)
	{
		ShellExecute(0, _T("explore"), PathUtil::GetPathWithoutFilename(fullPath), 0, 0, SW_SHOWNORMAL);
	}
	else
	{
		if (INT_PTR(ShellExecute(0, _T("open"), _T("explorer"), CString("/select, ") + filename, PathUtil::GetPathWithoutFilename(fullPath), SW_SHOWNORMAL)) <= 32)
		{
			ShellExecute(0, _T("explore"), PathUtil::GetPathWithoutFilename(fullPath), 0, 0, SW_SHOWNORMAL);
		}
	}
}

void CopyNameToClipboardFromPopup(CString filename)
{
	CClipboard clipboard;
	clipboard.PutString(filename.GetString());
}

void CopyPathToClipboardFromPopup(CString fullPath)
{
	CClipboard clipboard;
	clipboard.PutString(fullPath.GetString());
}

void CopySourceControlPathToClipboardFromPopup(CString path)
{
	if (!GetIEditor()->IsSourceControlAvailable())
	{
		return;
	}
	char outPath[MAX_PATH];
	const bool bRes = GetIEditor()->GetSourceControl()->GetInternalPath(path, outPath, MAX_PATH);
	if (bRes && *outPath)
	{
		CClipboard clipboard;
		clipboard.PutString(outPath);
	}
}

void CheckInFromPopup(CString path, const std::function<void()>& createMessageBoxFunc)
{
	if (!GetIEditor()->IsSourceControlAvailable())
	{
		return;
	}
	CSourceControlDescDlg dlg;
	if (dlg.DoModal() == IDOK)
	{
		if (!GetIEditor()->GetSourceControl()->CheckIn(path, dlg.m_sDesc))
		{
			createMessageBoxFunc();
		}
	}
}

void CheckOutFromPopup(CString path, const std::function<void()>& createMessageBoxFunc)
{
	if (GetIEditor()->IsSourceControlAvailable())
	{
		if (!GetIEditor()->GetSourceControl()->CheckOut(path))
		{
			createMessageBoxFunc();
		}
	}
}

void UndoCheckoutFromPopup(CString path, const std::function<void()>& createMessageBoxFunc)
{
	if (GetIEditor()->IsSourceControlAvailable())
	{
		if (!GetIEditor()->GetSourceControl()->UndoCheckOut(path))
		{
			createMessageBoxFunc();
		}
	}
}

void GetLatestVersionFromPopup(CString path, const std::function<void()>& createMessageBoxFunc)
{
	if (GetIEditor()->IsSourceControlAvailable())
	{
		if (!GetIEditor()->GetSourceControl()->GetLatestVersion(path))
		{
			createMessageBoxFunc();
		}
	}
}

void ShowHistoryFromPopup(CString path, const std::function<void()>& createMessageBoxFunc)
{
	if (GetIEditor()->IsSourceControlAvailable())
	{
		if (!GetIEditor()->GetSourceControl()->History(path))
		{
			createMessageBoxFunc();
		}
	}
}

void AddToSourceControlFromPopup(CString path, const std::function<void()>& createMessageBoxFunc)
{
	if (!GetIEditor()->IsSourceControlAvailable())
	{
		return;
	}
	CSourceControlDescDlg dlg;
	if (dlg.DoModal() == IDOK)
	{
		if (!GetIEditor()->GetSourceControl()->Add(path, dlg.m_sDesc))
		{
			createMessageBoxFunc();
		}
	}
}

static std::unique_ptr<CDynamicPopupMenu> CreateDynamicPopupMenu(const char* pFilename, const char* pFullGamePath, bool* pIsSelected = nullptr, CFileUtil::ExtraMenuItems* pItems = nullptr, const std::function<void()>& createMessageBoxFunctor = std::function<void()>())
{
	const CString filename(pFilename);
	CString path(PathUtil::MakeGamePath(pFullGamePath));
	path = PathUtil::Make(path, filename);
	const QString currentDir(QDir::toNativeSeparators(QDir::currentPath()));
	const CString fullPath(CString(currentDir.toStdString().c_str()) + CString("\\") + PathUtil::GamePathToCryPakPath(path.GetString()).c_str());
	const uint32 fileAttr(GetIEditor()->IsSourceControlAvailable() ? CFileUtil::GetAttributes(path) : SCC_FILE_ATTRIBUTE_INVALID);

	std::unique_ptr<CDynamicPopupMenu> menu(new CDynamicPopupMenu);
	CPopupMenuItem& root = menu->GetRoot();

	if (pIsSelected)
	{
		root.Add<bool*>(QObject::tr("Select").toStdString().c_str(), functor(&SelectFromPopup), pIsSelected);
		pIsSelected = false;
	}

	bool bHasExtraItems = pItems && (!pItems->names.empty());
	if (bHasExtraItems)
	{
		pItems->selectedIndexIfAny = -1;
		for (size_t i = 0; i < pItems->names.size(); ++i)
		{
			if (pItems->names[i].IsEmpty())
			{
				root.AddSeparator();
			}
			else
			{
				root.Add<CFileUtil::ExtraMenuItems*, uint32>(pItems->names[i], functor(&SelectExtraItemFromPopup), pItems, i);
			}
			root.AddSeparator();
		}
	}

	auto& openMenuItem = root.Add<const CString&, uint32>(QObject::tr("Open").toStdString().c_str(), functor(&OpenFromPopup), fullPath, fileAttr);
	auto& exploreMenuItem = root.Add<const CString&, const CString&, uint32>(QObject::tr("Explore").toStdString().c_str(), functor(&ExploreFromPopup), fullPath, filename, fileAttr);
	if (fileAttr & SCC_FILE_ATTRIBUTE_INPAK)
	{
		openMenuItem.Enable(false);
		exploreMenuItem.Enable(false);
	}

	root.Add<CString>(QObject::tr("Copy Name To Clipboard").toStdString().c_str(), functor(&CopyNameToClipboardFromPopup), filename);
	root.Add<CString>(QObject::tr("Copy Path To Clipboard").toStdString().c_str(), functor(&CopyPathToClipboardFromPopup), fullPath);

	if (fileAttr != SCC_FILE_ATTRIBUTE_INVALID)
	{
		const bool bIsScEnabled = fileAttr & SCC_FILE_ATTRIBUTE_MANAGED;
		const bool bIsInPak = fileAttr & SCC_FILE_ATTRIBUTE_INPAK;
		root.AddSeparator();
		if (bIsInPak && !bIsScEnabled)
		{
			auto& menuItem = root.Add(QObject::tr("File In Pak (Read Only)").toStdString().c_str());
			menuItem.Enable(false);
		}
		else
		{
			if (bIsScEnabled)
			{
				root.Add<CString>(QObject::tr("Copy Source Control Path To Clipboard").toStdString().c_str(), functor(&CopySourceControlPathToClipboardFromPopup), path);
			}

			auto& checkInItem = root.Add<CString, const std::function<void()>&>(QObject::tr("Check In").toStdString().c_str(), functor(&CheckInFromPopup), path, createMessageBoxFunctor);
			auto& checkOutItem = root.Add<CString, const std::function<void()>&>(QObject::tr("Check Out").toStdString().c_str(), functor(&CheckOutFromPopup), path, createMessageBoxFunctor);
			auto& undoCheckoutItem = root.Add<CString, const std::function<void()>&>(QObject::tr("Undo Check Out").toStdString().c_str(), functor(&UndoCheckoutFromPopup), path, createMessageBoxFunctor);
			auto& getLatestVersionItem = root.Add<CString, const std::function<void()>&>(QObject::tr("Get Latest Version").toStdString().c_str(), functor(&GetLatestVersionFromPopup), path, createMessageBoxFunctor);
			auto& showHistoryItem = root.Add<CString, const std::function<void()>&>(QObject::tr("Show History").toStdString().c_str(), functor(&ShowHistoryFromPopup), path, createMessageBoxFunctor);
			auto& addToSourceControlItem = root.Add<CString, const std::function<void()>&>(QObject::tr("Add To Source Control").toStdString().c_str(), functor(&AddToSourceControlFromPopup), path, createMessageBoxFunctor);

			const bool bIsScEnabledAndNotInPak = bIsScEnabled && !bIsInPak;
			checkInItem.Enable(bIsScEnabledAndNotInPak && (fileAttr & SCC_FILE_ATTRIBUTE_CHECKEDOUT));
			checkOutItem.Enable(bIsScEnabledAndNotInPak && (fileAttr & SCC_FILE_ATTRIBUTE_READONLY));
			undoCheckoutItem.Enable(bIsScEnabledAndNotInPak && (fileAttr & SCC_FILE_ATTRIBUTE_CHECKEDOUT));
			getLatestVersionItem.Enable(bIsScEnabled);
			showHistoryItem.Enable(bIsScEnabled);
			addToSourceControlItem.Enable(!bIsScEnabled);
		}
	}

	return menu;
}

void PyShowInExplorer(const char* path)
{
	CFileUtil::ShowInExplorer(path);
}

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyShowInExplorer, general, show_in_explorer,
                                     "Shows a specified file in an OS browser and selects it.",
                                     "general.show_in_explorer('C:/Sandbox/example.txt')");

}

bool CFileUtil::CompileLuaFile(const char* luaFilename)
{
	string luaFile = luaFilename;

	if (luaFile.IsEmpty())
		return false;

	// Check if this file is in Archive.
	{
		CCryFile file;
		if (file.Open(luaFilename, "rb"))
		{
			// Check if in pack.
			if (file.IsInPak())
				return true;
			luaFile = file.GetAdjustedFilename();
		}
	}
	// First try compiling script and see if it have any errors.
	string LuaCompiler;
	string CompilerOutput;

	// Create the filepath of the lua compiler
	char szExeFileName[_MAX_PATH];
	// Get the path of the executable
	GetModuleFileName(CryGetCurrentModule(), szExeFileName, sizeof(szExeFileName));
	string exePath = PathUtil::GetPathWithoutFilename(szExeFileName);

	LuaCompiler = PathUtil::AddBackslash(exePath) + "LuaCompiler.exe ";

	luaFile.Replace('/', '\\');
	// Add the name of the Lua file
	string cmdLine = LuaCompiler + luaFile;

	// Execute the compiler and capture the output
	if (!CMFCUtils::ExecuteConsoleApp(cmdLine, CompilerOutput))
	{
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Error while executing 'LuaCompiler.exe', make sure the file is in your Master CD folder !"));
		return false;
	}

	// Check return string
	if (strlen(CompilerOutput) > 0)
	{
		// Errors while compiling file.

		// Show output from Lua compiler
		if (CryMessageBox((CString("Error output from Lua compiler:\r\n") + CompilerOutput.GetString() +
		                      CString("\r\nDo you want to edit the file ?")), "Lua Compiler", eMB_YesCancel) == eQR_Yes)
		{
			int line = 0;
			string cmdLine = luaFile;
			int index = CompilerOutput.Find("at line");
			if (index >= 0)
			{
				const char* str = CompilerOutput;
				sscanf(&str[index], "at line %d", &line);
			}
			// Open the Lua file for editing
			EditTextFile(luaFile, line);
			return false;
		}
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////
bool CFileUtil::ExtractFile(CString& file, bool bMsgBoxAskForExtraction, const char* pDestinationFilename)
{
	CCryFile cryfile;
	if (cryfile.Open(file, "rb"))
	{
		// Check if in pack.
		if (cryfile.IsInPak())
		{
			const char* sPakName = cryfile.GetPakPath();

			if (bMsgBoxAskForExtraction)
			{
				// Cannot edit file in pack, suggest to extract it for editing.
				CString msg;
				msg.Format(_T("File %s is inside a PAK file %s\r\nDo you want it to be extracted for editing ?"), (const char*)file, sPakName);

				if (QDialogButtonBox::StandardButton::No == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(msg), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::No))
				{
					return false;
				}
			}

			if (pDestinationFilename)
			{
				file = pDestinationFilename;
			}

			CFileUtil::CreatePath(PathUtil::GetPathWithoutFilename(file).c_str());

			// Extract it from Pak file.
			CFile diskFile;

			if (diskFile.Open(file, CFile::modeCreate | CFile::modeWrite))
			{
				// Copy data from packed file to disk file.
				char* data = new char[cryfile.GetLength()];
				cryfile.ReadRaw(data, cryfile.GetLength());
				diskFile.Write(data, cryfile.GetLength());
				delete[]data;
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Failed to create file %s on disk", (const char*)file);
			}
		}
		else
		{
			file = cryfile.GetAdjustedFilename();
		}

		return true;
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////
void CFileUtil::EditTextFile(const char* txtFile, int line, ETextFileType fileType, bool bUseGameFolder)
{
	CString file = txtFile;

	if (bUseGameFolder)
	{
		string sGameFolder = PathUtil::GetGameFolder();
		int nLen = sGameFolder.length();
		if (strnicmp(txtFile, sGameFolder.c_str(), nLen) != 0)
		{
			file = sGameFolder + string("\\") + string(txtFile);
		}
	}

	file.Replace('/', '\\');
	ExtractFile(file);
	file.Replace('/', '\\');
	CString cmd;
	if (line != 0)
	{
		cmd.Format("%s/%d/0", (const char*)file, line);
	}
	else
	{
		cmd = file;
	}

	CString TextEditor = gEditorFilePreferences.textEditorScript;
	if (fileType == FILE_TYPE_SHADER)
	{
		TextEditor = gEditorFilePreferences.textEditorShaders;
	}
	else if (fileType == FILE_TYPE_BSPACE)
	{
		TextEditor = gEditorFilePreferences.textEditorBspaces;
	}

	HINSTANCE hInst = ShellExecute(NULL, "open", TextEditor, cmd, NULL, SW_SHOWNORMAL);
	if ((DWORD_PTR)hInst <= 32)
	{
		// Failed.
		file = file.SpanExcluding("/");
		// Try standard open.
		hInst = ShellExecute(NULL, "open", file, NULL, NULL, SW_SHOWNORMAL);
		if ((DWORD_PTR)hInst <= 32)
		{
			if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Can't open the file. You can specify a source editor in Sandbox Preferences or create an association in Windows. Do you want to open the file in the Notepad?"), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::No))
			{
				ShellExecute(NULL, "open", "notepad", cmd, NULL, SW_SHOWNORMAL);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFileUtil::EditTextureFile(const char* txtureFile, bool bUseGameFolder)
{
	CString file = txtureFile;
	file.Replace('/', '\\');
	ExtractFile(file);
	if (bUseGameFolder)
	{
		string sGameFolder = PathUtil::GetGameFolder();
		int nLen = sGameFolder.length();
		if (strnicmp(txtureFile, sGameFolder.c_str(), nLen) != 0)
		{
			file = sGameFolder + string("\\") + string(txtureFile);
		}
	}
	file.Replace('/', '\\');
	string fullTexturePath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath().c_str(), file);
	HINSTANCE hInst = ShellExecute(NULL, "open", gEditorFilePreferences.textureEditor, fullTexturePath.c_str(), NULL, SW_SHOWNORMAL);
	if ((DWORD_PTR)hInst <= 32)
	{
		//failed
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Failed to open texture editor %s with file %s", gEditorFilePreferences.textureEditor.c_str(), fullTexturePath.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
bool CFileUtil::EditMayaFile(const char* filepath, const bool bExtractFromPak, const bool bUseGameFolder)
{
	CString dosFilepath = PathUtil::ToDosPath(filepath);
	if (bExtractFromPak)
	{
		ExtractFile(dosFilepath);
	}

	if (bUseGameFolder)
	{
		const CString sGameFolder = PathUtil::GetGameFolder();
		int nLength = sGameFolder.GetLength();
		if (strnicmp(filepath, sGameFolder, nLength) != 0)
		{
			dosFilepath = sGameFolder + '\\' + filepath;
		}

		dosFilepath = PathUtil::ToDosPath(dosFilepath.GetString());
	}

	const string fullPath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath().c_str(), dosFilepath);

	CString program = gEditorFilePreferences.animEditor.length() > 0 ? gEditorFilePreferences.animEditor.c_str() : "explorer";
	HINSTANCE hInst = ShellExecute(NULL, "open", program, fullPath.c_str(), NULL, SW_SHOWNORMAL);
	if ((DWORD_PTR)hInst <= 32)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Can't open the file. You can specify a source editor in Sandbox Preferences or create an association in Windows.");
	}
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
bool CFileUtil::EditFile(const char* filePath, const bool bExtrackFromPak, const bool bUseGameFolder)
{
	CString extension = filePath;
	extension.Delete(0, extension.ReverseFind('.'));

	if (extension.Compare(".ma") == 0)
	{
		return EditMayaFile(filePath, bExtrackFromPak, bUseGameFolder);
	}
	else if ((extension.Compare(".bspace") == 0) || (extension.Compare(".comb") == 0))
	{
		EditTextFile(filePath, 0, FILE_TYPE_BSPACE, bUseGameFolder);
		return TRUE;
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
bool CFileUtil::CalculateDccFilename(const CString& assetFilename, CString& dccFilename)
{
	if (ExtractDccFilenameFromAssetDatabase(assetFilename, dccFilename))
		return true;

	if (ExtractDccFilenameUsingNamingConventions(assetFilename, dccFilename))
		return true;

	gEnv->pLog->LogError("Failed to find psd file for texture: '%s'", assetFilename);
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CFileUtil::ExtractDccFilenameFromAssetDatabase(const CString& assetFilename, CString& dccFilename)
{
	// implementation of this function required the old Asset Browser (IAssetItemDatabase, IAssetItem),
	// so it was removed, but can be easily reimplemented,
	// you just need to define a map of correlated extensions for each resource type
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CFileUtil::ExtractDccFilenameUsingNamingConventions(const CString& assetFilename, CString& dccFilename)
{
	//else to try find it by naming conventions
	CString tempStr = assetFilename;
	int foundSplit = -1;
	if ((foundSplit = tempStr.ReverseFind('.')) > 0)
	{
		CString first = tempStr.Mid(0, foundSplit);
		tempStr = first + ".psd";
	}
	if (CFileUtil::FileExists(tempStr))
	{
		dccFilename = tempStr;
		return true;
	}

	//else try to find it by replacing post fix _<description> with .psd
	tempStr = assetFilename;
	foundSplit = -1;
	if ((foundSplit = tempStr.ReverseFind('_')) > 0)
	{
		CString first = tempStr.Mid(0, foundSplit);
		tempStr = first + ".psd";
	}
	if (CFileUtil::FileExists(tempStr))
	{
		dccFilename = tempStr;
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
CString CFileUtil::FormatInitialFolderForFileDialog(const CString& folder)
{
	string fixedFolder(folder);
	fixedFolder.replace('/', '\\');

	string rstrDriveLetter, rstrDirectory, rstrFilename, rstrExtension;
	PathUtil::SplitPath(fixedFolder, rstrDriveLetter, rstrDirectory, rstrFilename, rstrExtension);
	if (rstrDriveLetter.empty())
	{
		char rootpath[_MAX_PATH];
		::GetCurrentDirectory(sizeof(rootpath), rootpath);
		fixedFolder = PathUtil::Make(rootpath, fixedFolder);
	}

	fixedFolder = PathUtil::AddBackslash(fixedFolder);

	return fixedFolder.c_str();
}

//////////////////////////////////////////////////////////////////////////
void CFileUtil::FormatFilterString(CString& filter)
{
	const string s = filter;
	const int numPipeChars = std::count(s.begin(), s.end(), '|');
	if (numPipeChars == 1)
	{
		filter.Format("%s||", filter.GetString());
	}
	else if (numPipeChars > 1)
	{
		assert(numPipeChars % 2 != 0);
		if (s.find("||") == string::npos)
		{
			filter.Format("%s||", filter.GetString());
		}
	}
	else if (filter.GetLength() > 0)
	{
		filter.Format("%s|%s||", filter.GetString(), filter.GetString());
	}
}

void CFileUtil::GetFilterFromCustomFileType(CString& filter, ECustomFileType fileType)
{
	if (!filter.IsEmpty())
	{
		return; // filter already set
	}
	switch (fileType)
	{
	case EFILE_TYPE_ANY:
		filter = "All Files (*.*)|*.*||";
		break;
	case EFILE_TYPE_GEOMETRY:
		filter = "Geometry Files (*.cgf,*.chr,*.skin,*.cdf,*.cga)|*.cgf;*.chr;*.skin;*.cdf;*.cga|All Files (*.*)|*.*||";
		break;
	case EFILE_TYPE_TEXTURE:
		filter = "Texture Files|*.dds;*.tif;*.hdr;*.jpg;*.tga;*.pcx;*.bmp;*.gif;*.pgm;*.raw;*.swf;*.gfx;*.sfd|DDS Files|*.dds|TIF Files|*.tif|HDR Files|*.hdr|Flash Files|*.swf;*.gfx|Video Files|*.sfd|All Files (*.*)|*.*||";
		break;
	case EFILE_TYPE_SOUND:
		filter = "Sound Files (*.wav,*.mp2,*.mp3,*.ogg)|*.wav;*.mp2;*.mp3;*.ogg|All Files|*.*||";
		break;
	case EFILE_TYPE_GEOMCACHE:
		filter = "Geometry Cache Files (*.cax,*.abc)|*.cax;*.abc|All Files|*.*||";
		break;
	case EFILE_TYPE_XML:
		filter = "XML Files (*.xml)|*.xml||";
		break;
	case EFILE_TYPE_MATERIAL:
		filter = "Material Files (*.mtl)|*.mtl||";
		break;
	case EFILE_TYPE_LAST:
	default:
		return;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CFileUtil::SelectFile(const CString& fileSpec, const CString& searchFolder, CString& fullFileName)
{
	CSystemFileDialog::OpenParams dialogParams(CSystemFileDialog::OpenFile);
	dialogParams.initialDir = FormatInitialFolderForFileDialog(searchFolder);
	if (!fullFileName.IsEmpty())
	{
		dialogParams.initialFile = fullFileName;
	}
	dialogParams.extensionFilters = CExtensionFilter::Parse(fileSpec);
	CSystemFileDialog fileDialog(dialogParams);
	if (fileDialog.Execute() == QDialog::Accepted)
	{
		auto files = fileDialog.GetSelectedFiles();
		CRY_ASSERT(!files.empty());
		fullFileName = files.front().toStdString().c_str();
		return true;
	}
	return false;
}

bool CFileUtil::SelectFiles(const CString& fileSpec, const CString& searchFolder, std::vector<CString>& files)
{
	CSystemFileDialog::OpenParams dialogParams(CSystemFileDialog::OpenMultipleFiles);
	dialogParams.initialDir = FormatInitialFolderForFileDialog(searchFolder);
	dialogParams.extensionFilters = CExtensionFilter::Parse(fileSpec);
	CSystemFileDialog fileDialog(dialogParams);
	files.clear();
	if (fileDialog.Execute() == QDialog::Accepted)
	{
		auto filePathes = fileDialog.GetSelectedFiles();
		CRY_ASSERT(!filePathes.empty());
		foreach(const QString &path, filePathes)
		{
			files.push_back(path.toLocal8Bit().data());
		}
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CFileUtil::SelectSaveFile(const char* fileFilter, const char* defaulExtension, const char* startFolder, string& fileName)
{
	CString fF = fileFilter;
	CString dE = defaulExtension;
	CString sF = startFolder;
	CString oF = fileName.GetString();

	bool result = SelectSaveFile(fF, dE, sF, oF);
	fileName = oF.GetString();
	return result;
}

bool CFileUtil::SelectSaveFile(const CString& fileFilter, const CString& defaulExtension, const CString& startFolder, CString& fileName)
{
	CSystemFileDialog::OpenParams dialogParams(CSystemFileDialog::SaveFile);
	dialogParams.initialDir = FormatInitialFolderForFileDialog(startFolder);
	if (!fileName.IsEmpty())
	{
		dialogParams.initialFile = dialogParams.initialDir + fileName;
	}
	dialogParams.extensionFilters = CExtensionFilter::Parse(fileFilter);
	CSystemFileDialog fileDialog(dialogParams);
	if (fileDialog.Execute() == QDialog::Accepted)
	{
		auto files = fileDialog.GetSelectedFiles();
		CRY_ASSERT(!files.empty());
		fileName = files.front().toStdString().c_str();
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CFileUtil::SelectSingleFile(QWidget* pParent, ECustomFileType fileType, string& outputFile, const char* filter, const char* initialDir, const char* baseDir, const FileFilterCallback& fileFilterFunc)
{
	CString oF = outputFile.GetString();
	CString f = filter;
	CString iD = initialDir;
	CString bD = baseDir;

	bool result = SelectSingleFile(pParent, fileType, oF, f, iD, bD, fileFilterFunc);
	outputFile = oF.GetString();
	return result;
}

bool CFileUtil::SelectSingleFile(ECustomFileType fileType, string& outputFile, const char* filter, const char* initialDir, const char* baseDir, const FileFilterCallback& fileFilterFunc)
{
	QWidget* pParent = nullptr;
	return SelectSingleFile(pParent, fileType, outputFile, filter, initialDir, baseDir, fileFilterFunc);
}

bool CFileUtil::SelectSingleFile(ECustomFileType fileType, CString& outputFile, const CString& filter, const CString& initialDir, const CString& baseDir, const FileFilterCallback& fileFilterFunc)
{
	QWidget* pParent = nullptr;
	return SelectSingleFile(pParent, fileType, outputFile, filter, initialDir, baseDir, fileFilterFunc);
}

bool CFileUtil::SelectSingleFile(QWidget* pParent, ECustomFileType fileType, CString& outputFile, const CString& filter, const CString& initialDir, const CString& baseDir, const FileFilterCallback& fileFilterFunc)
{
	return CustomSelectSingleFile(pParent, fileType, outputFile, filter, initialDir, baseDir, fileFilterFunc);
}

//! Display OpenFile dialog and allow to select multiple files.
bool CFileUtil::SelectMultipleFiles(ECustomFileType fileType, std::vector<CString>& files, const CString& filter, const CString& initialDir, const CString& baseDir, const FileFilterCallback& fileFilterFunc)
{
	return CustomSelectMultipleFiles(fileType, files, filter, initialDir, baseDir, fileFilterFunc);
}

//////////////////////////////////////////////////////////////////////////
// Get directory contents.
//////////////////////////////////////////////////////////////////////////
inline bool ScanDirectoryFiles(const CString& root, const CString& path, const CString& fileSpec, std::vector<CFileUtil::FileDesc>& files, bool bSkipPaks)
{
	bool anyFound = false;
	CString dir = PathUtil::AddBackslash((root + path).GetString());

	CString findFilter = PathUtil::Make(dir, fileSpec);
	ICryPak* pIPak = GetIEditor()->GetSystem()->GetIPak();

	// Add all directories.
	_finddata_t fd;
	intptr_t fhandle;

	fhandle = pIPak->FindFirst(findFilter, &fd);
	if (fhandle != -1)
	{
		do
		{
			// Skip back folders.
			if (fd.name[0] == '.')
				continue;
			if (fd.attrib & _A_SUBDIR) // skip sub directories.
				continue;

			if (bSkipPaks && (fd.attrib & _A_IN_CRYPAK))
			{
				continue;
			}

			anyFound = true;

			CFileUtil::FileDesc file;
			file.filename = path + fd.name;
			file.attrib = fd.attrib;
			file.size = fd.size;
			file.time_access = fd.time_access;
			file.time_create = fd.time_create;
			file.time_write = fd.time_write;

			files.push_back(file);
		}
		while (pIPak->FindNext(fhandle, &fd) == 0);
		pIPak->FindClose(fhandle);
	}

	/*
	   CFileFind finder;
	   BOOL bWorking = finder.FindFile( Path::Make(dir,fileSpec) );
	   while (bWorking)
	   {
	   bWorking = finder.FindNextFile();

	   if (finder.IsDots())
	    continue;

	   if (!finder.IsDirectory())
	   {
	    anyFound = true;

	    CFileUtil::FileDesc fd;
	    fd.filename = dir + finder.GetFileName();
	    fd.nFileSize = finder.GetLength();

	    finder.GetCreationTime( &fd.ftCreationTime );
	    finder.GetLastAccessTime( &fd.ftLastAccessTime );
	    finder.GetLastWriteTime( &fd.ftLastWriteTime );

	    fd.dwFileAttributes = 0;
	    if (finder.IsArchived())
	      fd.dwFileAttributes |= FILE_ATTRIBUTE_ARCHIVE;
	    if (finder.IsCompressed())
	      fd.dwFileAttributes |= FILE_ATTRIBUTE_COMPRESSED;
	    if (finder.IsNormal())
	      fd.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
	    if (finder.IsHidden())
	      fd.dwFileAttributes = FILE_ATTRIBUTE_HIDDEN;
	    if (finder.IsReadOnly())
	      fd.dwFileAttributes = FILE_ATTRIBUTE_READONLY;
	    if (finder.IsSystem())
	      fd.dwFileAttributes = FILE_ATTRIBUTE_SYSTEM;
	    if (finder.IsTemporary())
	      fd.dwFileAttributes = FILE_ATTRIBUTE_TEMPORARY;

	    files.push_back(fd);
	   }
	   }
	 */

	return anyFound;
}

//////////////////////////////////////////////////////////////////////////
// Get directory contents.
//////////////////////////////////////////////////////////////////////////
inline int ScanDirectoryRecursive(const CString& root, const CString& path, const CString& fileSpec, std::vector<CFileUtil::FileDesc>& files, bool recursive, bool addDirAlso, CFileUtil::ScanDirectoryUpdateCallBack updateCB, bool bSkipPaks)
{
	bool anyFound = false;
	CString dir = PathUtil::AddBackslash((root + path).GetString());

	if (updateCB)
	{
		string msg;
		msg.Format("Scanning %s...", dir);
		if (updateCB(msg) == false)
			return -1;
	}

	if (ScanDirectoryFiles(root, PathUtil::AddBackslash(path.GetString()).c_str(), fileSpec, files, bSkipPaks))
		anyFound = true;

	if (recursive)
	{
		/*
		   CFileFind finder;
		   BOOL bWorking = finder.FindFile( Path::Make(dir,"*.*") );
		   while (bWorking)
		   {
		   bWorking = finder.FindNextFile();

		   if (finder.IsDots())
		    continue;

		   if (finder.IsDirectory())
		   {
		    // Scan directory.
		    if (ScanDirectoryRecursive( root,Path::AddBackslash(path+finder.GetFileName()),fileSpec,files,recursive ))
		      anyFound = true;
		   }
		   }
		 */

		ICryPak* pIPak = GetIEditor()->GetSystem()->GetIPak();

		// Add all directories.
		_finddata_t fd;
		intptr_t fhandle;

		fhandle = pIPak->FindFirst(PathUtil::Make(dir, "*.*"), &fd);
		if (fhandle != -1)
		{
			do
			{
				// Skip back folders.
				if (fd.name[0] == '.')
					continue;

				if (!(fd.attrib & _A_SUBDIR)) // skip not directories.
					continue;

				if (bSkipPaks && (fd.attrib & _A_IN_CRYPAK))
				{
					continue;
				}

				if (addDirAlso)
				{
					CFileUtil::FileDesc Dir;
					Dir.filename = path + fd.name;
					Dir.attrib = fd.attrib;
					Dir.size = fd.size;
					Dir.time_access = fd.time_access;
					Dir.time_create = fd.time_create;
					Dir.time_write = fd.time_write;
					files.push_back(Dir);
				}

				// Scan directory.
				int result = ScanDirectoryRecursive(root, PathUtil::AddBackslash((path + fd.name).GetString()).c_str(), fileSpec, files, recursive, addDirAlso, updateCB, bSkipPaks);
				if (result == -1)
				// Cancel the scan immediately.
				{
					pIPak->FindClose(fhandle);
					return -1;
				}
				else if (result == 1)
					anyFound = true;

			}
			while (pIPak->FindNext(fhandle, &fd) == 0);
			pIPak->FindClose(fhandle);
		}
	}

	return anyFound ? 1 : 0;
}

//////////////////////////////////////////////////////////////////////////
bool CFileUtil::ScanDirectory(const CString& path, const CString& file, std::vector<FileDesc>& files, bool recursive, bool addDirAlso, CFileUtil::ScanDirectoryUpdateCallBack updateCB, bool bSkipPaks)
{
	CString fileSpec = PathUtil::GetFile(file);
	CString localPath = PathUtil::GetPathWithoutFilename(file);
	return ScanDirectoryRecursive(PathUtil::AddBackslash(path.GetString()).c_str(), localPath, fileSpec, files, recursive, addDirAlso, updateCB, bSkipPaks) > 0;
}

void CFileUtil::ShowInExplorer(const CString& path)
{
	QtUtil::OpenInExplorer(path);
}

/*
   bool CFileUtil::ScanDirectory( const CString &startDirectory,const CString &searchPath,const CString &fileSpec,FileArray &files, bool recursive=true )
   {
   return ScanDirectoryRecursive(startDirectory,SearchPath,file,files,recursive );
   }
 */

//////////////////////////////////////////////////////////////////////////
bool CFileUtil::OverwriteFile(const CString& filename)
{
	// check if file exist.
	CString adjFileName = PathUtil::GamePathToCryPakPath(filename.GetString(), true);

	FILE* file = fopen(adjFileName, "rb");
	if (!file)
		return true;
	fclose(file);

	DWORD dwFileAttribs = ::GetFileAttributes(adjFileName);
	if (dwFileAttribs == INVALID_FILE_ATTRIBUTES)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Cannot Save File %s", adjFileName);
		return false;
	}
	else if (dwFileAttribs & FILE_ATTRIBUTE_READONLY)
	{
		if (!CCheckOutDialog::IsForAll())
		{
			CCheckOutDialog dlg(adjFileName, AfxGetMainWnd());
			if (dlg.DoModal() == IDCANCEL)
			{
				return false;
			}
			else if (dlg.GetResult() == CCheckOutDialog::CHECKOUT)
			{
				return CheckoutFile(adjFileName);
			}
		}
		SetFileAttributes(adjFileName, FILE_ATTRIBUTE_NORMAL);
	}
	return true;
}

/*
   static bool CheckOutFile( const char *filename )
   {
   CString ssafeExe = "C:\\Program Files\\Microsoft Visual Studio\\VSS\\win32\\ss.exe";
   SetEnvironmentVariable( "ssuser","timur" );
   SetEnvironmentVariable( "ssdir","\\\\Server2\\XISLE\\ArtworkVss" );
   //CString SSafeArtwork = "\\\\Server2\\XISLE\\ArtworkVss\\win32\\ss.exe";
   //CString SSafeArtworkProject = "$/MASTERCD";

   CString cmd = ssafeExe + " " + " checkout cg.dll";

   char currDirectory[MAX_PATH];
   GetCurrentDirectory( sizeof(currDirectory),currDirectory  );
   char cmdLine[MAX_PATH];
   cry_strcpy( cmdLine,cmd );

   PROCESS_INFORMATION pi;
   STARTUPINFO si;
   memset( &si,0,sizeof(si) );
   si.cb = sizeof(si);
   memset( &pi,0,sizeof(pi) );
   if (CreateProcess( NULL,cmdLine,NULL,NULL,FALSE,CREATE_NEW_CONSOLE,NULL,currDirectory,&si,&pi ))
   {
    // Wait until child process exits.
    WaitForSingleObject( pi.hProcess, INFINITE );

    // Close process and thread handles.
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
   }
   }
 */

//////////////////////////////////////////////////////////////////////////
bool CFileUtil::CheckoutFile(const char* filename)
{
	/*
	   if (gSettings.ssafeParams.user.IsEmpty())
	   {
	   CQuestionDialog::SWarning(QObject::tr(""),QObject::tr("Source Safe login user name must be configured."));

	   // Source safe not configured.
	   CSrcSafeSettingsDialog dlg;
	   if (dlg.DoModal() != IDOK)
	   {
	    CQuestionDialog::SWarning(QObject::tr(""),QObject::tr("Checkout canceled"));
	    return false;
	   }
	   }
	   SetEnvironmentVariable( "ssuser",gSettings.ssafeParams.user );
	   SetEnvironmentVariable( "ssdir",gSettings.ssafeParams.databasePath );

	   CString relFile = Path::GetRelativePath(filename);
	   if (relFile.IsEmpty())
	   relFile = filename;

	   CString cmd = gSettings.ssafeParams.exeFile + " checkout " + relFile;

	   char currDirectory[MAX_PATH];
	   GetCurrentDirectory( sizeof(currDirectory),currDirectory  );
	   char cmdLine[MAX_PATH];
	   cry_strcpy( cmdLine,cmd );

	   PROCESS_INFORMATION pi;
	   STARTUPINFO si;
	   memset( &si,0,sizeof(si) );
	   si.cb = sizeof(si);
	   memset( &pi,0,sizeof(pi) );
	   if (CreateProcess( NULL,cmdLine,NULL,NULL,FALSE,CREATE_NEW_CONSOLE,NULL,currDirectory,&si,&pi ))
	   {
	   // Wait until child process exits.
	   WaitForSingleObject( pi.hProcess, INFINITE );

	   // Close process and thread handles.
	   CloseHandle( pi.hProcess );
	   CloseHandle( pi.hThread );
	   return true;
	   }
	 */

	uint32 attr = CFileUtil::GetAttributes(filename);
	if (GetIEditor()->IsSourceControlAvailable() && (attr & SCC_FILE_ATTRIBUTE_MANAGED))
	{
		if (attr & SCC_FILE_ATTRIBUTE_CHECKEDOUT)
		{
			return true;
		}
		else
		{
			return GetIEditor()->GetSourceControl()->CheckOut(filename);
		}
	}
	else
	{
		// Files from pak should be easily openable.
		if (attr & SCC_FILE_ATTRIBUTE_INPAK)
			return true;

		if (attr & SCC_FILE_ATTRIBUTE_READONLY)
		{
			CString str;
			str.Format("File %s is Read-Only, Overwrite?", (const char*)filename);

			if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(str), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::No))
			{
				SetFileAttributes(filename, FILE_ATTRIBUTE_NORMAL);
				return true;
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CFileUtil::CheckinFile(const char* filename)
{
	/*
	   SetEnvironmentVariable( "ssuser",gSettings.ssafeParams.user );
	   SetEnvironmentVariable( "ssdir",gSettings.ssafeParams.databasePath );

	   CString relFile = Path::GetRelativePath(filename);
	   if (relFile.IsEmpty())
	   relFile = filename;

	   CString cmd = gSettings.ssafeParams.exeFile + " checkout " + relFile;

	   char currDirectory[MAX_PATH];
	   GetCurrentDirectory( sizeof(currDirectory),currDirectory  );
	   char cmdLine[MAX_PATH];
	   cry_strcpy( cmdLine,cmd );

	   PROCESS_INFORMATION pi;
	   STARTUPINFO si;
	   memset( &si,0,sizeof(si) );
	   si.cb = sizeof(si);
	   memset( &pi,0,sizeof(pi) );
	   if (CreateProcess( NULL,cmdLine,NULL,NULL,FALSE,CREATE_NEW_CONSOLE,NULL,currDirectory,&si,&pi ))
	   {
	   // Wait until child process exits.
	   WaitForSingleObject( pi.hProcess, INFINITE );

	   // Close process and thread handles.
	   CloseHandle( pi.hProcess );
	   CloseHandle( pi.hThread );
	   return true;
	   }
	 */

	if (GetIEditor()->IsSourceControlAvailable())
	{
		uint32 attr = CFileUtil::GetAttributes(filename);
		if (attr & SCC_FILE_ATTRIBUTE_MANAGED)
		{
			if (attr & SCC_FILE_ATTRIBUTE_CHECKEDOUT)
			{
				return GetIEditor()->GetSourceControl()->CheckIn(filename);
			}
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CFileUtil::AddFileToSourceControl(const char* filename)
{
	if (GetIEditor()->IsSourceControlAvailable())
	{
		uint32 attr = CFileUtil::GetAttributes(filename);
		if (!(attr & SCC_FILE_ATTRIBUTE_MANAGED))
		{
			return GetIEditor()->GetSourceControl()->Add(filename, 0, ADD_WITHOUT_SUBMIT);
		}
	}

	return false;
}

// Create new directory, check if directory already exist.
static bool CheckAndCreateDirectory(const char* path)
{
	WIN32_FIND_DATA FindFileData;

	HANDLE hFind = FindFirstFile(path, &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		return CFileUtil::CreatePath(path) == TRUE;
	}
	else
	{
		DWORD attr = FindFileData.dwFileAttributes;
		FindClose(hFind);
		if (attr & FILE_ATTRIBUTE_DIRECTORY)
		{
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CFileUtil::CreateDirectory(const char* directory)
{
	string path = directory;
	if (GetIEditor()->GetConsoleVar("ed_lowercasepaths"))
		path = path.MakeLower();

	string dir;
	bool res = CheckAndCreateDirectory(path);
	if (!res)
	{
		int iStart = 0;
		string token = TokenizeString(path, "\\/", iStart);
		dir = token;
		while (token != "")
		{
			CheckAndCreateDirectory(dir);
			token = TokenizeString(path, "\\/", iStart);
			dir += string("\\") + token;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFileUtil::BackupFile(const char* filename)
{
	// Make a backup of previous file.
	bool makeBackup = true;

	CString bakFilename = PathUtil::ReplaceExtension(filename, "bak");

	{
		// Check if backup needed.
		CFile bak;
		if (bak.Open(filename, CFile::modeRead))
		{
			if (bak.GetLength() <= 0)
				makeBackup = false;
		}
		else
			makeBackup = false;
	}
	// Backup the backup..
	if (makeBackup)
	{
		MoveFileEx(bakFilename, PathUtil::ReplaceExtension(bakFilename.GetString(), "bak2"), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
		SetFileAttributes(filename, FILE_ATTRIBUTE_NORMAL);
		MoveFileEx(filename, bakFilename, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFileUtil::BackupFileDated(const char* filename, bool bUseBackupSubDirectory /*=false*/)
{
	bool makeBackup = true;
	{
		// Check if backup needed.
		CFile bak;
		if (bak.Open(filename, CFile::modeRead))
		{
			if (bak.GetLength() <= 0)
				makeBackup = false;
		}
		else
			makeBackup = false;
	}

	if (makeBackup)
	{
		// Generate new filename
		time_t ltime;
		time(&ltime);
		tm* today = localtime(&ltime);

		char sTemp[128];
		strftime(sTemp, sizeof(sTemp), ".%Y%m%d.%H%M%S.", today);
		CString bakFilename = CString(PathUtil::RemoveExtension(filename)) + sTemp + PathUtil::GetExt(filename);

		if (bUseBackupSubDirectory)
		{
			string sBackupPath = PathUtil::Make(PathUtil::ToUnixPath(PathUtil::GetPathWithoutFilename(filename)), "backups");
			CFileUtil::CreateDirectory(sBackupPath.c_str());
			bakFilename = PathUtil::Make(sBackupPath, PathUtil::GetFile(bakFilename));
		}

		// Do the backup
		SetFileAttributes(filename, FILE_ATTRIBUTE_NORMAL);
		MoveFileEx(filename, bakFilename, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CFileUtil::Deltree(const char* szFolder, bool bRecurse)
{
	CString folder = szFolder;
	if (!folder.IsEmpty() && (folder.Right(1) != "/" && folder.Right(1) != "\\"))
		folder += '/';

	__finddata64_t fd;
	intptr_t hfil = _findfirst64(folder + "*.*", &fd);
	if (hfil == -1)
		return false;

	do
	{
		if (fd.attrib & _A_SUBDIR)
		{
			if (bRecurse)
			{
				CString name = fd.name;
				if (name != "." && name != "..")
				{
					Deltree(folder + fd.name + "/", bRecurse);
				}
			}
		}
		else
		{
			CFileUtil::DeleteFile(folder + fd.name);
		}

	}
	while (!_findnext64(hfil, &fd));

	_findclose(hfil);

	bool bRes = CFileUtil::RemoveDirectory(szFolder);
	return bRes;
}

//////////////////////////////////////////////////////////////////////////
bool CFileUtil::Exists(const CString& strPath, bool boDirectory, CFileUtil::FileDesc* pDesc)
{
	ICryPak* pIPak = GetIEditor()->GetSystem()->GetIPak();
	intptr_t nFindHandle(NULL);
	_finddata_t stFoundData;
	bool boIsDirectory(false);

	memset(&stFoundData, 0, sizeof(_finddata_t));
	nFindHandle = pIPak->FindFirst(strPath, &stFoundData);
	// If it found nothing, no matter if it is a file or directory, it was not found.
	if (nFindHandle == -1)
	{
		return false;
	}
	pIPak->FindClose(nFindHandle);

	if (stFoundData.attrib & _A_SUBDIR)
	{
		boIsDirectory = true;
	}
	else if (pDesc)
	{
		pDesc->filename = strPath;
		pDesc->attrib = stFoundData.attrib;
		pDesc->size = stFoundData.size;
		pDesc->time_access = stFoundData.time_access;
		pDesc->time_create = stFoundData.time_create;
		pDesc->time_write = stFoundData.time_write;
	}

	// If we are seeking directories...
	if (boDirectory)
	{
		// The return value will tell us if the found element is a directory.
		return boIsDirectory;
	}
	else
	{
		// If we are not seeking directories...
		// We return true if the found element is not a directory.
		return !boIsDirectory;
	}
}
//////////////////////////////////////////////////////////////////////////
bool CFileUtil::FileExists(const CString& strFilePath, CFileUtil::FileDesc* pDesc)
{
	return Exists(strFilePath, false, pDesc);
}
//////////////////////////////////////////////////////////////////////////
bool CFileUtil::PathExists(const CString& strPath)
{
	return Exists(strPath, true);
}

bool CFileUtil::GetDiskFileSize(const char* pFilePath, uint64& rOutSize)
{
	BOOL bOK;
	WIN32_FILE_ATTRIBUTE_DATA fileInfo;

#ifndef MAKEQWORD
	#define MAKEQWORD(lo, hi) ((uint64)(((uint64) ((DWORD) (hi))) << 32 | ((DWORD) (lo))))
#endif

	bOK = ::GetFileAttributesEx(pFilePath, GetFileExInfoStandard, (void*)&fileInfo);

	if (!bOK)
	{
		return false;
	}

	rOutSize = MAKEQWORD(fileInfo.nFileSizeLow, fileInfo.nFileSizeHigh);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CFileUtil::IsFileExclusivelyAccessable(const CString& strFilePath)
{
	HANDLE hTesteFileHandle(NULL);

	// We should use instead CreateFileTransacted, but this would mean
	// requiring Vista as the Minimum OS to run.
	hTesteFileHandle = CreateFile(
	  strFilePath,           // The filename
	  0,                     //	We don't really want to access the file...
	  0,                     //	This is the hot spot:  we don't want to share this file!
	  NULL,                  // We don't care about the security attributes.
	  OPEN_EXISTING,         // The file must exist, or this will be of no use.
	  FILE_ATTRIBUTE_NORMAL, // We don't care about attributes.
	  NULL                   // We don't care about template files.
	  );

	// We couldn't actually open the file in exclusive mode for whatever
	// reason: for example some other app is using it or the file doesn't
	// exist.
	if (hTesteFileHandle == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	CloseHandle(hTesteFileHandle);

	return true;
}
//////////////////////////////////////////////////////////////////////////
bool CFileUtil::CreatePath(const CString& strPath)
{
	string strDriveLetter;
	string strDirectory;
	string strFilename;
	string strExtension;
	string strCurrentDirectoryPath;
	std::vector<string> strDirectoryQueue;
	size_t nCurrentPathQueue(0);
	size_t nTotalPathQueueElements(0);
	BOOL bnLastDirectoryWasCreated(FALSE);

	if (PathExists(strPath))
	{
		return true;
	}

	PathUtil::SplitPath(strPath.GetString(), strDriveLetter, strDirectory, strFilename, strExtension);
	strDirectoryQueue = PathUtil::GetDirectoryStructure(strDirectory);

	if (!strDriveLetter.empty())
	{
		strCurrentDirectoryPath = strDriveLetter;
		strCurrentDirectoryPath += "\\";
	}

	nTotalPathQueueElements = strDirectoryQueue.size();
	for (nCurrentPathQueue = 0; nCurrentPathQueue < nTotalPathQueueElements; ++nCurrentPathQueue)
	{
		strCurrentDirectoryPath += strDirectoryQueue[nCurrentPathQueue];
		strCurrentDirectoryPath += "\\";
		// The value which will go out of this loop is the result of the attempt to create the
		// last directory, only.

		bnLastDirectoryWasCreated = ::CreateDirectory(strCurrentDirectoryPath, NULL);
	}

	if (!bnLastDirectoryWasCreated)
	{
		if (ERROR_ALREADY_EXISTS != GetLastError())
		{
			return false;
		}
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////
bool CFileUtil::DeleteFile(const CString& strPath)
{
	BOOL bnAttributeChange(FALSE);
	BOOL bnFileDeletion(FALSE);

	bnAttributeChange = SetFileAttributes(strPath, FILE_ATTRIBUTE_NORMAL);
	bnFileDeletion = ::DeleteFile(strPath);

	return (bnFileDeletion != FALSE);
}
//////////////////////////////////////////////////////////////////////////
bool CFileUtil::RemoveDirectory(const CString& strPath)
{
	BOOL bnRemoveResult(FALSE);
	BOOL bnAttributeChange(FALSE);

	bnAttributeChange = SetFileAttributes(strPath, FILE_ATTRIBUTE_NORMAL);
	bnRemoveResult = ::RemoveDirectory(strPath);

	return (bnRemoveResult != FALSE);
}
//////////////////////////////////////////////////////////////////////////
CFileUtil::ECopyTreeResult CFileUtil::CopyTree(const CString& strSourceDirectory, const CString& strTargetDirectory, bool boRecurse, bool boConfirmOverwrite)
{
	static CUserOptions oFileOptions;
	static CUserOptions oDirectoryOptions;

	CUserOptions::CUserOptionsReferenceCountHelper oFileOptionsHelper(oFileOptions);
	CUserOptions::CUserOptionsReferenceCountHelper oDirectoryOptionsHelper(oDirectoryOptions);

	ECopyTreeResult eCopyResult(ETREECOPYOK);

	__finddata64_t fd;
	string filespec(strSourceDirectory);

	intptr_t hfil(0);

	std::vector<CString> cFiles;
	std::vector<CString> cDirectories;

	size_t nCurrent(0);
	size_t nTotal(0);

	// For this function to work properly, it has to first process all files in the directory AND JUST AFTER IT
	// work on the sub-folders...this is NOT OBVIOUS, but imagine the case where you have a hierarchy of folders,
	// all with the same names and all with the same files names inside. If you would make a depth-first search
	// you could end up with the files from the deepest folder in ALL your folders.

	filespec += "*.*";

	if ((hfil = _findfirst64(filespec.c_str(), &fd)) == -1)
	{
		return ETREECOPYOK;
	}

	do
	{
		if (fd.attrib & _A_SUBDIR)
		{
			CString name(fd.name);

			if ((name != ".") && (name != ".."))
			{
				if (boRecurse)
				{
					cDirectories.push_back(name);
				}
			}
		}
		else
		{
			cFiles.push_back(fd.name);
		}
	}
	while (!_findnext64(hfil, &fd));

	_findclose(hfil);

	// First we copy all files (maybe not all, depending on the user options...)
	nTotal = cFiles.size();
	for (nCurrent = 0; nCurrent < nTotal; ++nCurrent)
	{
		BOOL bnLastFileWasCopied(FALSE);
		CString name(strSourceDirectory);
		CString strTargetName(strTargetDirectory);

		if (eCopyResult == ETREECOPYUSERCANCELED)
		{
			return eCopyResult;
		}

		name += cFiles[nCurrent];
		strTargetName += cFiles[nCurrent];

		if (boConfirmOverwrite)
		{
			if (PathFileExists(strTargetName))
			{

				// If the directory already exists...
				// we must warn our user about the possible actions.
				int nUserOption(0);

				if (boConfirmOverwrite)
				{
					// If the option is not valid to all folder, we must ask anyway again the user option.
					if (!oFileOptions.IsOptionToAll())
					{
						CString strRewriteText;

						strRewriteText.Format("There is already a file named \"%s\" in the target folder. Do you want to move this file anyway replacing the old one?", fd.name);
						CGenericOverwriteDialog oOverwriteDialog("Confirm file overwrite?", strRewriteText);
						nUserOption = oOverwriteDialog.DoModal();
						oFileOptions.SetOption(nUserOption, oOverwriteDialog.IsToAllToggled());
					}
					else
					{
						nUserOption = oFileOptions.GetOption();
					}
				}

				switch (nUserOption)
				{
				case IDYES:
					{
						// Actually, we need to do nothing in this case.
					}
					break;

				case IDNO:
					{
						eCopyResult = ETREECOPYUSERDIDNTCOPYSOMEITEMS;
						continue;
					}
					break;

				// This IS ALWAYS for all... so it's easy to deal with.
				case IDCANCEL:
					{
						return ETREECOPYUSERCANCELED;
					}
					break;
				}
			}
		}

		bnLastFileWasCopied = ::CopyFile(name, strTargetName, FALSE);
		if (!bnLastFileWasCopied)
		{
			eCopyResult = ETREECOPYFAIL;
		}
	}

	// Now we can recurse into the directories, if needed.
	nTotal = cDirectories.size();
	for (nCurrent = 0; nCurrent < nTotal; ++nCurrent)
	{
		CString name(strSourceDirectory);
		CString strTargetName(strTargetDirectory);
		BOOL bnLastDirectoryWasCreated(FALSE);

		if (eCopyResult == ETREECOPYUSERCANCELED)
		{
			return eCopyResult;
		}

		name += cDirectories[nCurrent];
		name += "/";

		strTargetName += cDirectories[nCurrent];
		strTargetName += "/";

		bnLastDirectoryWasCreated = ::CreateDirectory(strTargetName, NULL);
		if (!bnLastDirectoryWasCreated)
		{
			if (ERROR_ALREADY_EXISTS != GetLastError())
			{
				return ETREECOPYFAIL;
			}
			else
			{
				// If the directory already exists...
				// we must warn our user about the possible actions.
				int nUserOption(0);

				if (boConfirmOverwrite)
				{
					// If the option is not valid to all folder, we must ask anyway again the user option.
					if (!oDirectoryOptions.IsOptionToAll())
					{
						CString strRewriteText;

						strRewriteText.Format("There is already a folder named \"%s\" in the target folder. Do you want to move this folder anyway?", fd.name);
						CGenericOverwriteDialog oOverwriteDialog("Confirm directory overwrite?", strRewriteText);
						nUserOption = oOverwriteDialog.DoModal();
						oDirectoryOptions.SetOption(nUserOption, oOverwriteDialog.IsToAllToggled());
					}
					else
					{
						nUserOption = oDirectoryOptions.GetOption();
					}
				}

				switch (nUserOption)
				{
				case IDYES:
					{
						// Actually, we need to do nothing in this case.
					}
					break;

				case IDNO:
					{
						// If no, we just need to go to the next item.
						eCopyResult = ETREECOPYUSERDIDNTCOPYSOMEITEMS;
						continue;
					}
					break;

				// This IS ALWAYS for all... so it's easy to deal with.
				case IDCANCEL:
					{
						return ETREECOPYUSERCANCELED;
					}
					break;
				}
			}
		}

		eCopyResult = CopyTree(name, strTargetName, boRecurse, boConfirmOverwrite);
	}

	return eCopyResult;
}
//////////////////////////////////////////////////////////////////////////
CFileUtil::ECopyTreeResult CFileUtil::CopyFile(const CString& strSourceFile, const CString& strTargetFile, bool boConfirmOverwrite, LPPROGRESS_ROUTINE pfnProgress, LPBOOL pbCancel)
{
	CUserOptions oFileOptions;
	ECopyTreeResult eCopyResult(ETREECOPYOK);

	BOOL bnLastFileWasCopied(FALSE);
	CString name(strSourceFile);
	CString strQueryFilename;
	CString strFullStargetName;

	CString strTargetName(strTargetFile);
	if (GetIEditor()->GetConsoleVar("ed_lowercasepaths"))
		strTargetName = strTargetName.MakeLower();

	string strDriveLetter, strDirectory, strFilename, strExtension;
	PathUtil::SplitPath(strTargetName.GetString(), strDriveLetter, strDirectory, strFilename, strExtension);
	strFullStargetName = strDriveLetter;
	strFullStargetName += strDirectory;

	if (strFilename.empty())
	{
		strFullStargetName += PathUtil::GetFileName(strSourceFile);
		strFullStargetName += ".";
		strFullStargetName += PathUtil::GetExt(strSourceFile);
	}
	else
	{
		strFullStargetName += strFilename;
		strFullStargetName += strExtension;
	}

	if (boConfirmOverwrite)
	{
		if (PathFileExists(strFullStargetName))
		{
			strQueryFilename = strFilename;
			if (strFilename.empty())
			{
				strQueryFilename = PathUtil::GetFileName(strSourceFile);
				strQueryFilename += ".";
				strQueryFilename += PathUtil::GetExt(strSourceFile);
			}
			else
			{
				strQueryFilename += strExtension;
			}

			// If the directory already exists...
			// we must warn our user about the possible actions.
			int nUserOption(0);

			if (boConfirmOverwrite)
			{
				// If the option is not valid to all folder, we must ask anyway again the user option.
				if (!oFileOptions.IsOptionToAll())
				{
					CString strRewriteText;

					strRewriteText.Format("There is already a file named \"%s\" in the target folder. Do you want to move this file anyway replacing the old one?", strQueryFilename.GetBuffer());
					CGenericOverwriteDialog oOverwriteDialog("Confirm file overwrite?", strRewriteText, false);
					nUserOption = oOverwriteDialog.DoModal();
					oFileOptions.SetOption(nUserOption, oOverwriteDialog.IsToAllToggled());
				}
				else
				{
					nUserOption = oFileOptions.GetOption();
				}
			}

			switch (nUserOption)
			{
			case IDYES:
				{
					// Actually, we need to do nothing in this case.
				}
				break;

			case IDNO:
				{
					return eCopyResult = ETREECOPYUSERCANCELED;
				}
				break;

			// This IS ALWAYS for all... so it's easy to deal with.
			case IDCANCEL:
				{
					return ETREECOPYUSERCANCELED;
				}
				break;
			}
		}
	}

	bnLastFileWasCopied = ::CopyFileEx(name, strFullStargetName, pfnProgress, NULL, pbCancel, 0);
	if (!bnLastFileWasCopied)
	{
		eCopyResult = ETREECOPYFAIL;
	}

	return eCopyResult;
}
//////////////////////////////////////////////////////////////////////////
CFileUtil::ECopyTreeResult CFileUtil::MoveTree(const CString& strSourceDirectory, const CString& strTargetDirectory, bool boRecurse, bool boConfirmOverwrite)
{
	static CUserOptions oFileOptions;
	static CUserOptions oDirectoryOptions;

	CUserOptions::CUserOptionsReferenceCountHelper oFileOptionsHelper(oFileOptions);
	CUserOptions::CUserOptionsReferenceCountHelper oDirectoryOptionsHelper(oDirectoryOptions);

	ECopyTreeResult eCopyResult(ETREECOPYOK);

	__finddata64_t fd;
	string filespec(strSourceDirectory);

	intptr_t hfil(0);

	std::vector<CString> cFiles;
	std::vector<CString> cDirectories;

	size_t nCurrent(0);
	size_t nTotal(0);

	// For this function to work properly, it has to first process all files in the directory AND JUST AFTER IT
	// work on the sub-folders...this is NOT OBVIOUS, but imagine the case where you have a hierarchy of folders,
	// all with the same names and all with the same files names inside. If you would make a depth-first search
	// you could end up with the files from the deepest folder in ALL your folders.

	filespec += "*.*";

	if ((hfil = _findfirst64(filespec.c_str(), &fd)) == -1)
	{
		return ETREECOPYOK;
	}

	do
	{
		if (fd.attrib & _A_SUBDIR)
		{
			CString name(fd.name);

			if ((name != ".") && (name != ".."))
			{
				if (boRecurse)
				{
					cDirectories.push_back(name);
				}
			}
		}
		else
		{
			cFiles.push_back(fd.name);
		}
	}
	while (!_findnext64(hfil, &fd));

	_findclose(hfil);

	// First we copy all files (maybe not all, depending on the user options...)
	nTotal = cFiles.size();
	for (nCurrent = 0; nCurrent < nTotal; ++nCurrent)
	{
		BOOL bnLastFileWasCopied(FALSE);
		CString name(strSourceDirectory);
		CString strTargetName(strTargetDirectory);

		if (eCopyResult == ETREECOPYUSERCANCELED)
		{
			return eCopyResult;
		}

		name += cFiles[nCurrent];
		strTargetName += cFiles[nCurrent];

		if (boConfirmOverwrite)
		{
			if (PathFileExists(strTargetName))
			{

				// If the directory already exists...
				// we must warn our user about the possible actions.
				int nUserOption(0);

				if (boConfirmOverwrite)
				{
					// If the option is not valid to all folder, we must ask anyway again the user option.
					if (!oFileOptions.IsOptionToAll())
					{
						CString strRewriteText;

						strRewriteText.Format("There is already a file named \"%s\" in the target folder. Do you want to move this file anyway replacing the old one?", fd.name);
						CGenericOverwriteDialog oOverwriteDialog("Confirm file overwrite?", strRewriteText);
						nUserOption = oOverwriteDialog.DoModal();
						oFileOptions.SetOption(nUserOption, oOverwriteDialog.IsToAllToggled());
					}
					else
					{
						nUserOption = oFileOptions.GetOption();
					}
				}

				switch (nUserOption)
				{
				case IDYES:
					{
						// Actually, we need to do nothing in this case.
					}
					break;

				case IDNO:
					{
						eCopyResult = ETREECOPYUSERDIDNTCOPYSOMEITEMS;
						continue;
					}
					break;

				// This IS ALWAYS for all... so it's easy to deal with.
				case IDCANCEL:
					{
						return ETREECOPYUSERCANCELED;
					}
					break;
				}
			}
		}

		bnLastFileWasCopied = ::MoveFileEx(name, strTargetName, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
		if (!bnLastFileWasCopied)
		{
			eCopyResult = ETREECOPYFAIL;
		}
	}

	// Now we can recurse into the directories, if needed.
	nTotal = cDirectories.size();
	for (nCurrent = 0; nCurrent < nTotal; ++nCurrent)
	{
		CString name(strSourceDirectory);
		CString strTargetName(strTargetDirectory);
		BOOL bnLastDirectoryWasCreated(FALSE);

		if (eCopyResult == ETREECOPYUSERCANCELED)
		{
			return eCopyResult;
		}

		name += cDirectories[nCurrent];
		name += "/";

		strTargetName += cDirectories[nCurrent];
		strTargetName += "/";

		bnLastDirectoryWasCreated = ::CreateDirectory(strTargetName, NULL);
		if (!bnLastDirectoryWasCreated)
		{
			if (ERROR_ALREADY_EXISTS != GetLastError())
			{
				return ETREECOPYFAIL;
			}
			else
			{
				// If the directory already exists...
				// we must warn our user about the possible actions.
				int nUserOption(0);

				if (boConfirmOverwrite)
				{
					// If the option is not valid to all folder, we must ask anyway again the user option.
					if (!oDirectoryOptions.IsOptionToAll())
					{
						CString strRewriteText;

						strRewriteText.Format("There is already a folder named \"%s\" in the target folder. Do you want to move this folder anyway?", fd.name);
						CGenericOverwriteDialog oOverwriteDialog("Confirm directory overwrite?", strRewriteText);
						nUserOption = oOverwriteDialog.DoModal();
						oDirectoryOptions.SetOption(nUserOption, oOverwriteDialog.IsToAllToggled());
					}
					else
					{
						nUserOption = oDirectoryOptions.GetOption();
					}
				}

				switch (nUserOption)
				{
				case IDYES:
					{
						// Actually, we need to do nothing in this case.
					}
					break;

				case IDNO:
					{
						// If no, we just need to go to the next item.
						eCopyResult = ETREECOPYUSERDIDNTCOPYSOMEITEMS;
						continue;
					}
					break;

				// This IS ALWAYS for all... so it's easy to deal with.
				case IDCANCEL:
					{
						return ETREECOPYUSERCANCELED;
					}
					break;
				}
			}
		}

		eCopyResult = MoveTree(name, strTargetName, boRecurse, boConfirmOverwrite);
	}

	CFileUtil::RemoveDirectory(strSourceDirectory);

	return eCopyResult;
}
//////////////////////////////////////////////////////////////////////////
CFileUtil::ECopyTreeResult CFileUtil::MoveFile(const CString& strSourceFile, const CString& strTargetFile, bool boConfirmOverwrite)
{
	CUserOptions oFileOptions;

	ECopyTreeResult eCopyResult(ETREECOPYOK);

	intptr_t hfil(0);

	std::vector<CString> cFiles;
	std::vector<CString> cDirectories;

	size_t nCurrent(0);
	size_t nTotal(0);

	// First we copy all files (maybe not all, depending on the user options...)
	BOOL bnLastFileWasCopied(FALSE);
	CString name(strSourceFile);
	CString strTargetName(strTargetFile);
	CString strQueryFilename;
	CString strFullStargetName;

	string strDriveLetter;
	string strDirectory;
	string strFilename;
	string strExtension;

	PathUtil::SplitPath(strTargetFile.GetString(), strDriveLetter, strDirectory, strFilename, strExtension);
	strFullStargetName = strDriveLetter;
	strFullStargetName += strDirectory;

	if (strFilename.empty())
	{
		strFullStargetName += PathUtil::GetFileName(strSourceFile);
		strFullStargetName += ".";
		strFullStargetName += PathUtil::GetExt(strSourceFile);
	}
	else
	{
		strFullStargetName += strFilename;
		strFullStargetName += strExtension;
	}

	if (boConfirmOverwrite)
	{
		if (PathFileExists(strFullStargetName))
		{
			strQueryFilename = strFilename;
			if (strFilename.empty())
			{
				strQueryFilename = PathUtil::GetFileName(strSourceFile);
				strQueryFilename += ".";
				strQueryFilename += PathUtil::GetExt(strSourceFile);
			}
			else
			{
				strQueryFilename += strExtension;
			}

			// If the directory already exists...
			// we must warn our user about the possible actions.
			int nUserOption(0);

			if (boConfirmOverwrite)
			{
				// If the option is not valid to all folder, we must ask anyway again the user option.
				if (!oFileOptions.IsOptionToAll())
				{
					CString strRewriteText;

					strRewriteText.Format("There is already a file named \"%s\" in the target folder. Do you want to move this file anyway replacing the old one?", strQueryFilename.GetBuffer());
					CGenericOverwriteDialog oOverwriteDialog("Confirm file overwrite?", strRewriteText, false);
					nUserOption = oOverwriteDialog.DoModal();
					oFileOptions.SetOption(nUserOption, oOverwriteDialog.IsToAllToggled());
				}
				else
				{
					nUserOption = oFileOptions.GetOption();
				}
			}

			switch (nUserOption)
			{
			case IDYES:
				{
					// Actually, we need to do nothing in this case.
				}
				break;

			case IDNO:
				{
					return eCopyResult = ETREECOPYUSERCANCELED;
				}
				break;

			// This IS ALWAYS for all... so it's easy to deal with.
			case IDCANCEL:
				{
					return ETREECOPYUSERCANCELED;
				}
				break;
			}
		}
	}

	bnLastFileWasCopied = ::MoveFileEx(name, strFullStargetName, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
	if (!bnLastFileWasCopied)
	{
		eCopyResult = ETREECOPYFAIL;
	}

	return eCopyResult;
}

//////////////////////////////////////////////////////////////////////////
bool CFileUtil::CustomSelectMultipleFiles(
  ECustomFileType fileType,
  std::vector<CString>& outputFiles,
  const CString& filter,
  const CString& initialDir,
  const CString& baseDir,
  const FileFilterCallback& fileFilterFunc)
{
	CEngineFileDialog::OpenParams dialogParams(CEngineFileDialog::OpenMultipleFiles);
	dialogParams.initialDir = initialDir.GetString();
	if (!outputFiles.empty())
	{
		auto outputFile = outputFiles.front();
		dialogParams.initialFile = outputFile.GetString();
	}
	CString realFilter = filter;
	GetFilterFromCustomFileType(realFilter, fileType);
	dialogParams.extensionFilters = CExtensionFilter::Parse(realFilter);
	dialogParams.baseDirectory = baseDir;
	dialogParams.fileFilterFunc = fileFilterFunc;
	CEngineFileDialog fileDialog(dialogParams);
	QObject::connect(&fileDialog, &CEngineFileDialog::PopupMenuToBePopulated, &Private_FileUtil::OnFilePopupMenuPopulation);
	if (fileDialog.exec() == QDialog::Accepted)
	{
		auto files = fileDialog.GetSelectedFiles();
		outputFiles.clear();
		for (const auto& file : files)
		{
			outputFiles.push_back(file.toStdString().c_str());
		}
		return true;
	}
	return false;
}

bool CFileUtil::CustomSelectSingleFile(
  QWidget* pParent,
  ECustomFileType fileType,
  CString& outputFile,
  const CString& filter,
  const CString& initialDir,
  const CString& baseDir,
  const FileFilterCallback& fileFilterFunc)
{
	CEngineFileDialog::OpenParams dialogParams(CEngineFileDialog::OpenFile);
	dialogParams.initialDir = initialDir.GetString();
	if (!outputFile.IsEmpty())
	{
		dialogParams.initialFile = outputFile.GetString();
	}
	CString realFilter = filter;
	GetFilterFromCustomFileType(realFilter, fileType);
	dialogParams.extensionFilters = CExtensionFilter::Parse(realFilter);
	dialogParams.baseDirectory = baseDir;
	dialogParams.fileFilterFunc = fileFilterFunc;
	CEngineFileDialog fileDialog(dialogParams, pParent);
	QObject::connect(&fileDialog, &CEngineFileDialog::PopupMenuToBePopulated, &Private_FileUtil::OnFilePopupMenuPopulation);
	if (fileDialog.exec() == QDialog::Accepted)
	{
		auto files = fileDialog.GetSelectedFiles();
		CRY_ASSERT(!files.empty());
		outputFile = files.front().toStdString().c_str();
		return true;
	}
	return false;
}

void CFileUtil::PopupMenu(const char* filename, const char* fullGamePath, CWnd* wnd,
                          bool* pIsSelected, CFileUtil::ExtraMenuItems* pItems)
{
	std::function<void()> func([wnd]
	{
		CryMessageBox("Source Control Operation Failed.\r\nCheck if Source Control Provider correctly setup and working directory is correct.", "Error", eMB_Error);
	});
	auto pMenu = Private_FileUtil::CreateDynamicPopupMenu(filename, fullGamePath, pIsSelected, pItems, func);
	pMenu->SpawnAtCursor();
}

void CFileUtil::QPopupMenu(const QString& filename, const QString& fullGamePath, QWidget* parent, bool* pIsSelected, CFileUtil::ExtraMenuItems* pItems)
{
	std::function<void()> func([parent]
	{
		CQuestionDialog::SCritical(QObject::tr("Error"), QObject::tr("Source Control Operation Failed.\r\nCheck if Source Control Provider correctly setup and working directory is correct."));
	});
	auto pMenu = Private_FileUtil::CreateDynamicPopupMenu(filename.toStdString().c_str(), fullGamePath.toStdString().c_str(), pIsSelected, pItems, func);
	pMenu->SpawnAtCursor();
}

QString CFileUtil::addUniqueSuffix(const QString& fileName)
{
	if (!QFile::exists(fileName))
	{
		return fileName;
	}

	QFileInfo fileInfo(fileName);
	QString newName;
	QString secondPart = fileInfo.completeSuffix();
	QString firstPart;
	if (!secondPart.isEmpty())
	{
		secondPart = "." + secondPart;
		firstPart = fileName.left(fileName.size() - secondPart.size());
	}
	else
	{
		firstPart = fileName;
	}

	int count = 1;
	while (true)
	{
		newName = QString("%1 (%2)%3").arg(firstPart).arg(count).arg(secondPart);
		if (!QFile::exists(newName))
		{
			break;
		}
		++count;
	}
	return newName;
}

void CFileUtil::GatherAssetFilenamesFromLevel(std::set<CString>& rOutFilenames, bool bMakeLowerCase, bool bMakeUnixPath)
{
	rOutFilenames.clear();
	CBaseObjectsArray objArr;
	CUsedResources usedRes;
	IMaterialManager* pMtlMan = GetIEditor()->Get3DEngine()->GetMaterialManager();
	IParticleManager* pPartMan = GetIEditor()->Get3DEngine()->GetParticleManager();

	GetIEditor()->GetObjectManager()->GetObjects(objArr);

	for (size_t i = 0, iCount = objArr.size(); i < iCount; ++i)
	{
		CBaseObject* pObj = objArr[i];

		usedRes.files.clear();
		pObj->GatherUsedResources(usedRes);

		for (CUsedResources::TResourceFiles::iterator iter = usedRes.files.begin(); iter != usedRes.files.end(); ++iter)
		{
			CString tmpStr = (*iter);

			if (bMakeLowerCase)
			{
				tmpStr.MakeLower();
			}

			if (bMakeUnixPath)
			{
				tmpStr = PathUtil::ToUnixPath(tmpStr.GetString()).c_str();
			}

			rOutFilenames.insert(tmpStr);
		}
	}

	uint32 mtlCount = 0;

	pMtlMan->GetLoadedMaterials(NULL, mtlCount);

	if (mtlCount > 0)
	{
		std::vector<IMaterial*> arrMtls;

		arrMtls.resize(mtlCount);
		pMtlMan->GetLoadedMaterials(&arrMtls[0], mtlCount);

		for (size_t i = 0; i < mtlCount; ++i)
		{
			IMaterial* pMtl = arrMtls[i];

			size_t subMtls = pMtl->GetSubMtlCount();

			// for the main material
			IRenderShaderResources* pShaderRes = pMtl->GetShaderItem().m_pShaderResources;

			// add the material filename
			rOutFilenames.insert(pMtl->GetName());

			if (pShaderRes)
			{
				for (size_t j = 0; j < EFTT_MAX; ++j)
				{
					if (SEfResTexture* pTex = pShaderRes->GetTexture(j))
					{
						// add the texture filename
						rOutFilenames.insert(pTex->m_Name.c_str());
					}
				}
			}

			// for the submaterials
			for (size_t s = 0; s < subMtls; ++s)
			{
				IMaterial* pSubMtl = pMtl->GetSubMtl(s);

				// fill up dependencies
				if (pSubMtl)
				{
					IRenderShaderResources* pShaderRes = pSubMtl->GetShaderItem().m_pShaderResources;

					rOutFilenames.insert(pSubMtl->GetName());

					if (pShaderRes)
					{
						for (size_t j = 0; j < EFTT_MAX; ++j)
						{
							if (SEfResTexture* pTex = pShaderRes->GetTexture(j))
							{
								rOutFilenames.insert(pTex->m_Name.c_str());
							}
						}
					}
				}
			}
		}
	}
}

void CFileUtil::GatherAssetFilenamesFromLevel(DynArray<dll_string>& outFilenames)
{
	outFilenames.clear();
	CBaseObjectsArray objArr;
	CUsedResources usedRes;
	IMaterialManager* pMtlMan = GetIEditor()->Get3DEngine()->GetMaterialManager();
	IParticleManager* pPartMan = GetIEditor()->Get3DEngine()->GetParticleManager();
	bool newFile = true;
	GetIEditor()->GetObjectManager()->GetObjects(objArr);

	for (size_t i = 0, iCount = objArr.size(); i < iCount; ++i)
	{
		CBaseObject* pObj = objArr[i];

		usedRes.files.clear();
		pObj->GatherUsedResources(usedRes);

		for (CUsedResources::TResourceFiles::iterator iter = usedRes.files.begin(); iter != usedRes.files.end(); ++iter)
		{
			string tmpStr = (*iter);
			tmpStr.MakeLower();
			tmpStr.replace('\\', '/');
			newFile = true;
			for (auto var : outFilenames)
			{
				if (strcmp(var.c_str(), tmpStr.c_str()) == 0)
				{
					newFile = false;
					break;
				}
			}
			if (newFile) outFilenames.push_back(tmpStr.c_str());
		}
	}
	uint32 mtlCount = 0;

	pMtlMan->GetLoadedMaterials(NULL, mtlCount);

	if (mtlCount > 0)
	{
		std::vector<IMaterial*> arrMtls;

		arrMtls.resize(mtlCount);
		pMtlMan->GetLoadedMaterials(&arrMtls[0], mtlCount);

		for (size_t i = 0; i < mtlCount; ++i)
		{
			IMaterial* pMtl = arrMtls[i];

			size_t subMtls = pMtl->GetSubMtlCount();

			// for the main material
			IRenderShaderResources* pShaderRes = pMtl->GetShaderItem().m_pShaderResources;

			// add the material filename

			string withMtl;
			withMtl.Format("%s.mtl", pMtl->GetName());
			withMtl.MakeLower();
			withMtl.replace('\\', '/');
			newFile = true;
			for (auto var : outFilenames)
			{
				if (strcmp(var.c_str(), withMtl.c_str()) == 0)
				{
					newFile = false;
					break;
				}
			}
			if (newFile) outFilenames.push_back(withMtl.c_str());

			if (pShaderRes)
			{
				for (size_t j = 0; SEfResTexture* pTex = pShaderRes->GetTexture(j); ++j)
				{
					string textName = pTex->m_Name;
					textName.MakeLower();
					textName.replace('\\', '/');
					newFile = true;
					for (auto var : outFilenames)
					{
						if (strcmp(var.c_str(), textName.c_str()) == 0)
						{
							newFile = false;
							break;
						}
					}
					if (newFile) outFilenames.push_back(textName.c_str());
				}
			}
		}
	}
}

uint32 CFileUtil::GetAttributes(const char* filename, bool bUseSourceControl /*= true*/)
{
	if (bUseSourceControl && GetIEditor()->IsSourceControlAvailable())
		return GetIEditor()->GetSourceControl()->GetFileAttributes(filename);

	CCryFile file;
	if (!file.Open(filename, "rb"))
		return SCC_FILE_ATTRIBUTE_INVALID;

	if (file.IsInPak())
		return SCC_FILE_ATTRIBUTE_READONLY | SCC_FILE_ATTRIBUTE_INPAK;

	DWORD dwAttrib = ::GetFileAttributes(file.GetAdjustedFilename());
	if (dwAttrib == INVALID_FILE_ATTRIBUTES)
		return SCC_FILE_ATTRIBUTE_INVALID;

	if (dwAttrib & FILE_ATTRIBUTE_READONLY)
		return SCC_FILE_ATTRIBUTE_NORMAL | SCC_FILE_ATTRIBUTE_READONLY;

	return SCC_FILE_ATTRIBUTE_NORMAL;
}

bool CFileUtil::CompareFiles(const CString& strFilePath1, const CString& strFilePath2)
{
	// Get the size of both files.  If either fails we say they are different (most likely one doesn't exist)
	uint64 size1, size2;
	if (!GetDiskFileSize(strFilePath1, size1) || !GetDiskFileSize(strFilePath2, size2))
		return false;

	// If the files are different sizes return false
	if (size1 != size2)
		return false;

	// Sizes are the same, we need to compare the bytes.  Try to open both files for read.
	CCryFile file1, file2;
	if (!file1.Open(strFilePath1, "rb") || !file2.Open(strFilePath2, "rb"))
		return false;

	const uint64 bufSize = 4096;

	char buf1[bufSize], buf2[bufSize];

	for (uint64 i = 0; i < size1; i += bufSize)
	{
		size_t amtRead1 = file1.ReadRaw(buf1, bufSize);
		size_t amtRead2 = file2.ReadRaw(buf2, bufSize);

		// Not a match if we didn't read the same amount from each file
		if (amtRead1 != amtRead2)
			return false;

		// Not a match if we didn't read the amount of data we expected
		if (amtRead1 != bufSize && i + amtRead1 != size1)
			return false;

		// Not a match if the buffers aren't the same
		if (memcmp(buf1, buf2, amtRead1) != 0)
			return false;
	}

	return true;
}

bool CFileUtil::PredicateFileNameLess(const CFileUtil::FileDesc& desc1, const CFileUtil::FileDesc& desc2)
{
	return desc1.filename.Compare(desc2.filename) == -1 ? true : false;
}

bool CFileUtil::PredicateFileNameGreater(const CFileUtil::FileDesc& desc1, const CFileUtil::FileDesc& desc2)
{
	return desc1.filename.Compare(desc2.filename) == 1 ? true : false;
}

bool CFileUtil::PredicateDateLess(const CFileUtil::FileDesc& desc1, const CFileUtil::FileDesc& desc2)
{
	return desc1.time_write < desc2.time_write;
}

bool CFileUtil::PredicateDateGreater(const CFileUtil::FileDesc& desc1, const CFileUtil::FileDesc& desc2)
{
	return desc1.time_write > desc2.time_write;
}

bool CFileUtil::PredicateSizeLess(const CFileUtil::FileDesc& desc1, const CFileUtil::FileDesc& desc2)
{
	return desc1.size < desc2.size;
}

bool CFileUtil::PredicateSizeGreater(const CFileUtil::FileDesc& desc1, const CFileUtil::FileDesc& desc2)
{
	return desc1.size > desc2.size;
}

std::vector<string> CFileUtil::PickTagsFromPath(const string& path)
{
	std::vector<string> tags;
	string tag;
	int curPos = 0;
	const char* tokens = " ./\\";
	tag = path.Tokenize(tokens, curPos);
	while (tag != "")
	{
		const int MIN_CHARS_AS_A_SEARCH_TERM = 3;
		// For efficiency, don't take a too short word into consideration.
		if (tag.GetLength() >= MIN_CHARS_AS_A_SEARCH_TERM)
			tags.push_back(tag.MakeLower());
		tag = path.Tokenize(tokens, curPos);
	}
	return tags;
}

CTempFileHelper::CTempFileHelper(const char* pFileName)
	: m_fileName(pFileName)
{
	if (GetIEditor()->GetConsoleVar("ed_lowercasepaths"))
		m_fileName = m_fileName.MakeLower();

	m_tempFileName = PathUtil::ReplaceExtension(m_fileName.GetString(), "tmp");

	SetFileAttributes(m_tempFileName, FILE_ATTRIBUTE_NORMAL);
	DeleteFile(m_tempFileName);
}

CTempFileHelper::~CTempFileHelper()
{
	DeleteFile(m_tempFileName);
}

bool CTempFileHelper::UpdateFile(bool bBackup)
{
	// First, check if the files are actually different
	if (!CFileUtil::CompareFiles(m_tempFileName, m_fileName))
	{
		// If the file changed, make sure the destination file is writable
		if (!CFileUtil::OverwriteFile(m_fileName))
		{
			DeleteFile(m_tempFileName);
			return false;
		}

		// Back up the current file if requested
		if (bBackup)
			CFileUtil::BackupFile(m_fileName);

		// Move the temp file over the top of the destination file
		return MoveFileEx(m_tempFileName, m_fileName, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
	}
	// If the files are the same, just delete the temp file and return.
	else
	{
		DeleteFile(m_tempFileName);
		return true;
	}
}

