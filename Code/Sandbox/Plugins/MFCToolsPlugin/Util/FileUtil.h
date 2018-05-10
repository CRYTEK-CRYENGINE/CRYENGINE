// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
//////////////////////////////////////////////////////////////////////////
//  CryENGINE Source File
//  Copyright (C) 2000-2012, Crytek GmbH, All rights reserved
//////////////////////////////////////////////////////////////////////////

#include <CryString/StringUtils.h>
#include "dll_string.h"
#include <CryCore/functor.h>
#include "FileSystem/FileSystem_FileFilter.h"
#include "FileDialogs/ExtensionFilter.h"

class CWnd;
class CDynamicPopupMenu;
class QWidget;

//! File types used for File Open dialogs
enum ECustomFileType
{
	EFILE_TYPE_ANY,
	EFILE_TYPE_GEOMETRY,
	EFILE_TYPE_TEXTURE,
	EFILE_TYPE_SOUND,
	EFILE_TYPE_GEOMCACHE,
	EFILE_TYPE_XML,
	EFILE_TYPE_MATERIAL,
	EFILE_TYPE_LAST,
};

class PLUGIN_API CFileUtil
{
public:
	struct FileDesc
	{
		CString      filename;
		unsigned int attrib;
		time_t       time_create; //! -1 for FAT file systems
		time_t       time_access; //! -1 for FAT file systems
		time_t       time_write;
		int64        size;
	};
	enum ETextFileType
	{
		FILE_TYPE_SCRIPT,
		FILE_TYPE_SHADER,
		FILE_TYPE_BSPACE,
	};

	enum ECopyTreeResult
	{
		ETREECOPYOK,
		ETREECOPYFAIL,
		ETREECOPYUSERCANCELED,
		ETREECOPYUSERDIDNTCOPYSOMEITEMS,
	};

	enum EColumnType
	{
		COLUMN_TYPE_FILELIST = 0,
		COLUMN_TYPE_DATE,
		COLUMN_TYPE_SIZE
	};

	typedef std::vector<FileDesc>                       FileArray;
	typedef bool (*                                     ScanDirectoryUpdateCallBack)(const string& msg);
	typedef Functor1<const char*>                       SelectSingleFileChangeCallback;
	typedef FileSystem::SFileFilter::FileFilterCallback FileFilterCallback;

	static bool ScanDirectory(const CString& path, const CString& fileSpec, FileArray& files, bool recursive = true, bool addDirAlso = false, ScanDirectoryUpdateCallBack updateCB = NULL, bool bSkipPaks = false);
	static bool ScanDirectory(const char* path, const char* fileSpec, FileArray& files, bool recursive = true, bool addDirAlso = false, ScanDirectoryUpdateCallBack updateCB = NULL, bool bSkipPaks = false) // for CString conversion
	{
		return ScanDirectory(CString(path), CString(fileSpec), files, recursive, addDirAlso, updateCB, bSkipPaks);
	}

	static void ShowInExplorer(const CString& path);

	static bool CompileLuaFile(const char* luaFilename);
	static bool ExtractFile(CString& file, bool bMsgBoxAskForExtraction = true, const char* pDestinationFilename = NULL);
	static void EditTextFile(const char* txtFile, int line = 0, ETextFileType fileType = FILE_TYPE_SCRIPT, bool bUseGameFolder = true);
	static void EditTextureFile(const char* txtureFile, bool bUseGameFolder);
	static bool EditMayaFile(const char* mayaFile, const bool bExtractFromPak, const bool bUseGameFolder);
	static bool EditFile(const char* filePath, const bool bExtrackFromPak, const bool bUseGameFolder);

	//! dcc filename calculation and extraction sub-routines
	static bool CalculateDccFilename(const CString& assetFilename, CString& dccFilename);

	//! Reformat filter string for (MFC) CFileDialog style file filtering
	static void FormatFilterString(CString& filter);

	static void GetFilterFromCustomFileType(CString& filter, ECustomFileType fileType);

	//! Open file selection dialog.
	static bool SelectFile(const CString& fileSpec, const CString& searchFolder, CString& fullFileName);
	//! Open file selection dialog.
	static bool SelectFiles(const CString& fileSpec, const CString& searchFolder, std::vector<CString>& files);
	//! Open file selection dialog.
	static bool SelectFile(const string& fileSpec, const string& searchFolder, string& fullFileName) // for CString conversion
	{
		CString cFullFileName = fullFileName.GetString();
		bool result = SelectFile(fileSpec.GetString(), searchFolder.GetString(), cFullFileName);
		fullFileName = cFullFileName.GetString();
		return result;
	}

	//! Display OpenFile dialog and allow to select multiple files.
	//! @return true if selected, false if canceled.
	//! @outputFile Inputs and Outputs filename.
	static bool SelectSingleFile(ECustomFileType fileType, CString& outputFile, const CString& filter = "", const CString& initialDir = "", const CString& baseDir = "", const FileFilterCallback& fileFilterFunc = FileFilterCallback());
	static bool SelectSingleFile(ECustomFileType fileType, string& outputFile, const char* filter = "", const char* initialDir = "", const char* baseDir = "", const FileFilterCallback& fileFilterFunc = FileFilterCallback());
	static bool SelectSingleFile(QWidget* pParent, ECustomFileType fileType, CString& outputFile, const CString& filter = "", const CString& initialDir = "", const CString& baseDir = "", const FileFilterCallback& fileFilterFunc = FileFilterCallback());
	static bool SelectSingleFile(QWidget* pParent, ECustomFileType fileType, string& outputFile, const char* filter = "", const char* initialDir = "", const char* baseDir = "", const FileFilterCallback& fileFilterFunc = FileFilterCallback());

	//! Display OpenFile dialog and allow to select multiple files.
	//! @return true if selected, false if canceled.
	static bool SelectMultipleFiles(ECustomFileType fileType, std::vector<CString>& files, const CString& filter = "", const CString& initialDir = "", const CString& baseDir = "", const FileFilterCallback& fileFilterFunc = FileFilterCallback());

	//! Display OpenFile dialog and allow to select multiple files.
	//! @return true if selected, false if canceled.
	static bool SelectMultipleFiles(ECustomFileType fileType, std::vector<string>& files, const char* filter = "", const char* initialDir = "", const char* baseDir = "", const FileFilterCallback& fileFilterFunc = FileFilterCallback()) // for CString conversion
	{
		std::vector<CString> cFiles;
		for (const string& str : files)
			cFiles.push_back(str.GetString());

		bool result = SelectMultipleFiles(fileType, cFiles, CString(filter), CString(initialDir), CString(baseDir), fileFilterFunc);

		files.clear();
		for (const CString& str : cFiles)
			files.push_back(str.GetString());

		return result;
	}

	static bool SelectSaveFile(const char* fileFilter, const char* defaulExtension, const char* startFolder, string& fileName);
	static bool SelectSaveFile(const CString& fileFilter, const CString& defaulExtension, const CString& startFolder, CString& fileName);

	//! If file is read-only ask user if he wants to overwrite it.
	//! If yes file is deleted.
	//! @return True if file was deleted.
	static bool OverwriteFile(const CString& filename);

	//! If file is read-only ask user if he wants to overwrite it.
	//! If yes file is deleted.
	//! @return True if file was deleted.
	static bool OverwriteFile(const char* filename) { return OverwriteFile(CString(filename)); } // for CString conversion

	//////////////////////////////////////////////////////////////////////////
	// Interface to Source safe.
	//////////////////////////////////////////////////////////////////////////
	//! Checks out the file from source safe.
	static bool CheckoutFile(const char* filename);

	//! Checks in the file to source safe.
	static bool CheckinFile(const char* filename);

	//! Adding the file to Source Control changelist without submitting.
	static bool AddFileToSourceControl(const char* filename);

	//! Creates this directory.
	static void CreateDirectory(const char* dir);

	//! Makes a backup file.
	static void BackupFile(const char* filename);

	//! Makes a backup file, marked with a datestamp, e.g. myfile.20071014.093320.xml
	//! If bUseBackupSubDirectory is true, moves backup file into a relative subdirectory "backups"
	static void BackupFileDated(const char* filename, bool bUseBackupSubDirectory = false);

	// ! Added deltree as a copy from the function found in Crypak.
	static bool Deltree(const char* szFolder, bool bRecurse);

	// Checks if a file or directory exist.
	// We are using 3 functions here in order to make the names more instructive for the programmers.
	// Those functions only work for OS files and directories.
	static bool Exists(const CString& strPath, bool boDirectory, FileDesc* pDesc = NULL);
	static bool FileExists(const CString& strFilePath, FileDesc* pDesc = NULL);
	static bool FileExists(const char* strFilePath, FileDesc* pDesc = NULL) { return FileExists(CString(strFilePath), pDesc); } // for CString conversion
	static bool PathExists(const CString& strPath);
	static bool PathExists(const char* strPath)                             { return PathExists(CString(strPath)); }
	static bool GetDiskFileSize(const char* pFilePath, uint64& rOutSize);

	// This function should be used only with physical files.
	static bool IsFileExclusivelyAccessable(const CString& strFilePath);

	// Creates the entire path, if needed.
	static bool CreatePath(const CString& strPath);

	// Creates the entire path, if needed.
	static bool CreatePath(const char* strPath) { return CreatePath(CString(strPath)); }

	// Attempts to delete a file (if read only it will set its attributes to normal first).
	static bool DeleteFile(const CString& strPath);

	// Attempts to remove a directory (if read only it will set its attributes to normal first).
	static bool RemoveDirectory(const CString& strPath);

	// Copies all the elements from the source directory to the target directory.
	// It doesn't copy the source folder to the target folder, only it's contents.
	// THIS FUNCTION IS NOT DESIGNED FOR MULTI-THREADED USAGE
	static ECopyTreeResult CopyTree(const CString& strSourceDirectory, const CString& strTargetDirectory, bool boRecurse = true, bool boConfirmOverwrite = false);

	//////////////////////////////////////////////////////////////////////////
	// @param LPPROGRESS_ROUTINE pfnProgress - called by the system to notify of file copy progress
	// @param LPBOOL pbCancel - when the contents of this BOOL are set to TRUE, the system cancels the copy operation
	static ECopyTreeResult CopyFile(const CString& strSourceFile, const CString& strTargetFile, bool boConfirmOverwrite = false, LPPROGRESS_ROUTINE pfnProgress = NULL, LPBOOL pbCancel = NULL);

	//////////////////////////////////////////////////////////////////////////
	// @param LPPROGRESS_ROUTINE pfnProgress - called by the system to notify of file copy progress
	// @param LPBOOL pbCancel - when the contents of this BOOL are set to TRUE, the system cancels the copy operation
	static ECopyTreeResult CopyFile(const char* strSourceFile, const char* strTargetFile, bool boConfirmOverwrite = false, LPPROGRESS_ROUTINE pfnProgress = NULL, LPBOOL pbCancel = NULL)
	{
		return CopyFile(CString(strSourceFile), CString(strTargetFile), boConfirmOverwrite, pfnProgress, pbCancel);
	}

	// As we don't have a FileUtil interface here, we have to duplicate some code :-( in order to keep
	// function calls clean.
	// Moves all the elements from the source directory to the target directory.
	// It doesn't move the source folder to the target folder, only it's contents.
	// THIS FUNCTION IS NOT DESIGNED FOR MULTI-THREADED USAGE
	static ECopyTreeResult MoveTree(const CString& strSourceDirectory, const CString& strTargetDirectory, bool boRecurse = true, bool boConfirmOverwrite = false);

	//
	static ECopyTreeResult MoveFile(const CString& strSourceFile, const CString& strTargetFile, bool boConfirmOverwrite = false);

	struct ExtraMenuItems
	{
		std::vector<CString> names;
		int                  selectedIndexIfAny;

		ExtraMenuItems() : selectedIndexIfAny(-1) {}

		int AddItem(const CString& name)
		{
			names.push_back(name);
			return names.size() - 1;
		}
	};

	// Show Popup Menu with file commands include Source Control commands
	// filename: a name of file without path
	// fullGamePath: a game path to folder like "/Game/Objects" without filename
	// wnd: pointer to window class, can be NULL
	// isSelected: output value indicated if Select menu item was chosen, if pointer is 0 - no Select menu item.
	// pItems: you can specify additional menu items and get the result of selection using this parameter.
	// return false if source control operation failed
	static void PopupMenu(const char* filename, const char* fullGamePath, CWnd* wnd = nullptr, bool* pIsSelected = nullptr, ExtraMenuItems* pItems = nullptr);
	static void QPopupMenu(const QString& filename, const QString& fullGamePath, QWidget* parent = nullptr, bool* pIsSelected = nullptr, ExtraMenuItems* pItems = nullptr);

	// creates a unique file name from input fileName by appending a counter number to its name
	static QString addUniqueSuffix(const QString& fileName);

	static void    GatherAssetFilenamesFromLevel(std::set<CString>& rOutFilenames, bool bMakeLowerCase = false, bool bMakeUnixPath = false);
	static void    GatherAssetFilenamesFromLevel(DynArray<dll_string>& rOutFilenames);

	// Get file attributes include source control attributes if available
	static uint32 GetAttributes(const char* filename, bool bUseSourceControl = true);

	// Returns true if the files have the same content, false otherwise
	static bool CompareFiles(const CString& strFilePath1, const CString& strFilePath2);

	// Predicators to sort Columns( Ascending/Descending )
	static bool                PredicateFileNameLess(const CFileUtil::FileDesc& desc1, const CFileUtil::FileDesc& desc2);
	static bool                PredicateFileNameGreater(const CFileUtil::FileDesc& desc1, const CFileUtil::FileDesc& desc2);
	static bool                PredicateDateLess(const CFileUtil::FileDesc& desc1, const CFileUtil::FileDesc& desc2);
	static bool                PredicateDateGreater(const CFileUtil::FileDesc& desc1, const CFileUtil::FileDesc& desc2);
	static bool                PredicateSizeLess(const CFileUtil::FileDesc& desc1, const CFileUtil::FileDesc& desc2);
	static bool                PredicateSizeGreater(const CFileUtil::FileDesc& desc1, const CFileUtil::FileDesc& desc2);

	static CString             FormatInitialFolderForFileDialog(const CString& folder);
	static std::vector<string> PickTagsFromPath(const string& path);

private:
	// True means to use the custom file dialog, false means to use the smart file open dialog.
	static bool s_singleFileDlgPref[EFILE_TYPE_LAST];
	static bool s_multiFileDlgPref[EFILE_TYPE_LAST];

	static bool CustomSelectSingleFile(
	  QWidget* pParent,
	  ECustomFileType fileType,
	  CString& outputFile,
	  const CString& filter,
	  const CString& initialDir,
	  const CString& baseDir,
	  const FileFilterCallback& fileFilterFunc);

	static bool CustomSelectMultipleFiles(
	  ECustomFileType fileType,
	  std::vector<CString>& outputFiles,
	  const CString& filter,
	  const CString& initialDir,
	  const CString& baseDir,
	  const FileFilterCallback& fileFilterFunc);

	static bool ExtractDccFilenameFromAssetDatabase(const CString& assetFilename, CString& dccFilename);
	static bool ExtractDccFilenameUsingNamingConventions(const CString& assetFilename, CString& dccFilename);
};

//
// A helper for creating a temp file to write to, then copying that over the destination
// file only if it changes (to avoid requiring the user to check out source controlled
// file unnecessarily)
//
class PLUGIN_API CTempFileHelper
{
public:
	CTempFileHelper(const char* pFileName);
	~CTempFileHelper();

	// Gets the path to the temp file that should be written to
	const CString& GetTempFilePath() { return m_tempFileName; }

	// After the temp file has been written and closed, this should be called to update
	// the destination file.
	// If bBackup is true CFileUtil::BackupFile will be called if the file has changed.
	bool UpdateFile(bool bBackup);

private:
	CString m_fileName;
	CString m_tempFileName;
};

