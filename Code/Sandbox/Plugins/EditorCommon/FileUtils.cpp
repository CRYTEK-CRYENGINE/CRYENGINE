// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "FileUtils.h"

#include "PathUtils.h"
#include "QtUtil.h"
#include <CryString/CryPath.h>
#include <CrySystem/File/CryFile.h>
#include <CrySystem/File/ICryPak.h>
#include <CrySystem/IProjectManager.h>
#include <IEditor.h>
#include <QDirIterator>
#include <stack>

namespace Private_FileUtils
{

void AddDirectorysContent(const QString& dirPath, std::vector<string>& result, int currentLevel, int levelLimit, bool includeFolders)
{
	if (currentLevel >= levelLimit)
	{
		return;
	}
	QDir dir(dirPath);
	QFileInfoList infoList = dir.entryInfoList(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
	for (const QFileInfo& fileInfo : infoList)
	{
		const QString absolutePath = fileInfo.absoluteFilePath();
		if (fileInfo.isDir())
		{
			if (includeFolders)
			{
				result.push_back(PathUtil::ToGamePath(QtUtil::ToString(absolutePath)));
			}
			AddDirectorysContent(absolutePath, result, currentLevel + 1, levelLimit, includeFolders);
		}
		else
		{
			result.push_back(PathUtil::ToGamePath(QtUtil::ToString(absolutePath)));
		}
	}
}

// Goes through a directory (and it's sub-directories) and calls a callback function.
// Commonly used for copying/moving files
// @param szSource source file or directory to be moved
// @param szDestination destination directory where it should be moved to
// @param callback function with parameters for source and destination paths for a given file or folder
bool TraverseDirectoryAndExecute(const char* szSource, const char* szDestination, std::function<bool(const char*, const char*)> callback)
{
	QFileInfo sourceFileInfo(szSource);
	if (sourceFileInfo.isDir())
	{
		QDir sourceDir(szSource);
		if (!sourceDir.exists())
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "FileUtils: Source directory does not exist: %s", szSource);
			return false;
		}

		string newDestination;
		newDestination.Format("%s/%s", szDestination, QtUtil::ToString(sourceDir.dirName()));

		if (!callback(szSource, newDestination.c_str()))
			return false;

		QFileInfoList infoList = sourceDir.entryInfoList(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
		for (const QFileInfo& fileInfo : infoList)
		{
			string fileName = QtUtil::ToString(fileInfo.fileName());
			string source;
			source.Format("%s/%s", szSource, fileName.c_str());
			if (!TraverseDirectoryAndExecute(source.c_str(), newDestination.c_str(), callback))
				return false;
		}
	}
	else
	{
		string newDestination;
		string fileName = QtUtil::ToString(sourceFileInfo.fileName());
		newDestination.Format("%s/%s", szDestination, fileName.c_str());

		// Make sure destination directory exists
		QFileInfo destinationFileInfo(newDestination.c_str());
		QDir toDir(destinationFileInfo.absolutePath());
		toDir.mkpath(destinationFileInfo.absolutePath());

		return callback(szSource, newDestination.c_str());
	}

	return true;
}

} // namespace Private_FileUtils

namespace FileUtils
{
bool RemoveFile(const char* szFilePath)
{
	return QFile::remove(szFilePath);
}

bool Remove(const char* szPath)
{
	QFileInfo info(szPath);

	if (info.isDir())
		return RemoveDirectory(szPath);
	else
		return RemoveFile(szPath);
}

bool MoveFileAllowOverwrite(const char* szOldFilePath, const char* szNewFilePath)
{
	if (QFileInfo::exists(szNewFilePath) && QFileInfo(szOldFilePath) == QFileInfo(szNewFilePath))
	{
		return true;
	}

	if (QFile::rename(szOldFilePath, szNewFilePath))
	{
		return true;
	}

	// Try to overwrite existing file.
	return QFile::remove(szNewFilePath) && QFile::rename(szOldFilePath, szNewFilePath);
}

bool CopyFileAllowOverwrite(const char* szSourceFilePath, const char* szDestinationFilePath)
{
	if (QFileInfo::exists(szDestinationFilePath) && QFileInfo(szSourceFilePath) == QFileInfo(szDestinationFilePath))
	{
		return true;
	}

	if (Pak::CompareFiles(szSourceFilePath, szDestinationFilePath))
	{
		return true; // Files are identical and copy operation is considered to be successful.
	}

	GetISystem()->GetIPak()->MakeDir(PathUtil::GetDirectory(szDestinationFilePath));

	if (QFile::copy(szSourceFilePath, szDestinationFilePath))
	{
		return true;
	}

	// Try to overwrite existing file.
	return QFile::remove(szDestinationFilePath) && QFile::copy(szSourceFilePath, szDestinationFilePath);
}

bool CopyDirectory(const char* szSource, const char* szDestination)
{
	// Recurse directory will process source and destination to map to files and folders within the root source to the final destination
	return Private_FileUtils::TraverseDirectoryAndExecute(szSource, szDestination, [](const char* szSource, const char* szDestination)
	{
		QFileInfo sourceFileInfo(szSource);
		if (sourceFileInfo.isDir())
		{
			// Ensure the destination directory exists
			QDir toDir(szDestination);
			toDir.mkpath(toDir.absolutePath());
		}
		else
		{
			// Don't replace any existing files
			if (QFile::exists(szDestination))
				return true;

			return QFile::copy(szSource, szDestination);
		}

		return true;
	});
}

bool CopyDirectoryContents(const char* szSource, const char* szDestination)
{
	QDir fromDir(szSource);
	if (!fromDir.exists())
		return false;

	QFileInfoList infoList = fromDir.entryInfoList(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
	for (const QFileInfo& fileInfo : infoList)
	{
		string fileName = QtUtil::ToString(fileInfo.fileName());

		if (!CopyDirectory(PathUtil::Make(szSource, fileName.c_str()), szDestination))
			return false;
	}

	return true;
}

bool CopyFiles(const char* szSource, const char* szDestination)
{
	QDir fromDir(szSource);
	if (!fromDir.exists())
		return false;

	QFileInfoList infoList = fromDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
	for (const QFileInfo& fileInfo : infoList)
	{
		string fileName = QtUtil::ToString(fileInfo.fileName());

		if (!CopyDirectory(PathUtil::Make(szSource, fileName.c_str()), szDestination))
			return false;
	}

	return true;
}

bool MoveDirectory(const char* szSource, const char* szDestination)
{
	// Recurse directory will process source and destination to map to files and folders within the root source to the final destination
	return Private_FileUtils::TraverseDirectoryAndExecute(szSource, szDestination, [](const char* szSource, const char* szDestination)
	{
		QFileInfo sourceFileInfo(szSource);
		if (sourceFileInfo.isDir())
		{
			// Ensure the destination directory exists
			QDir toDir(szDestination);
			toDir.mkpath(toDir.absolutePath());
		}
		else
		{
			// QFile::rename will fail if destination file exists already
			if (QFile::exists(szDestination))
				return true;

			QFile sourceFile(szSource);
			return sourceFile.rename(szDestination);
		}

		return true;
	});
}

bool MoveDirectoryContents(const char* szSource, const char* szDestination)
{
	QDir fromDir(szSource);
	if (!fromDir.exists())
		return false;

	QFileInfoList infoList = fromDir.entryInfoList(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
	for (const QFileInfo& fileInfo : infoList)
	{
		string fileName = QtUtil::ToString(fileInfo.fileName());
		string source;
		source.Format("%s/%s", szSource, fileName.c_str());
		if (!MoveDirectory(source.c_str(), szDestination))
			return false;
	}

	return true;
}

bool MoveFiles(const char* szSource, const char* szDestination)
{
	QDir fromDir(szSource);
	if (!fromDir.exists())
		return false;

	QFileInfoList infoList = fromDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
	for (const QFileInfo& fileInfo : infoList)
	{
		string fileName = QtUtil::ToString(fileInfo.fileName());
		string source;
		source.Format("%s/%s", szSource, fileName.c_str());
		if (!MoveDirectory(source.c_str(), szDestination))
			return false;
	}

	return true;
}

bool RemoveDirectory(const char* szPath, bool bRecursive /* = true*/)
{
	QDir dir(szPath);

	if (!bRecursive)
	{
		const QString dirName = dir.dirName();
		if (dir.cdUp())
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Unable to remove directory: %s", szPath);
			return false;
		}

		if (dir.remove(dirName))
			return true;
	}

	if (dir.removeRecursively())
		return true;

	CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Unable to remove directory: %s", szPath);
	return false;
}

bool MakeFileWritable(const char* szFilePath)
{
	QFile f(QtUtil::ToQString(szFilePath));
	return f.setPermissions(f.permissions() | QFileDevice::WriteOwner);
}

// The pak should be opened.
void Pak::Unpak(const char* szArchivePath, const char* szDestPath, std::function<void(float)> progress)
{
	const string pakFolder = PathUtil::GetDirectory(szArchivePath);

	float progressValue = 0; //(0, 1);
	std::vector<char> buffer(1 << 24);

	std::stack<string> stack;
	stack.push("");
	while (!stack.empty())
	{
		const CryPathString mask = PathUtil::Make(stack.top(), "*");
		const CryPathString folder = stack.top();
		stack.pop();

		GetISystem()->GetIPak()->ForEachArchiveFolderEntry(szArchivePath, mask, [szDestPath, &pakFolder, &stack, &folder, &buffer, &progressValue, progress](const ICryPak::ArchiveEntryInfo& entry)
			{
				const CryPathString path(PathUtil::Make(folder.c_str(), entry.szName));
				if (entry.bIsFolder)
				{
				  stack.push(path);
				  return;
				}

				ICryPak* const pPak = GetISystem()->GetIPak();
				FILE* file = pPak->FOpen(PathUtil::Make(pakFolder, path), "rb");
				if (!file)
				{
				  return;
				}

				if (!pPak->MakeDir(PathUtil::Make(szDestPath, folder)))
				{
				  return;
				}

				buffer.resize(pPak->FGetSize(file));
				const size_t numberOfBytesRead = pPak->FReadRawAll(buffer.data(), buffer.size(), file);
				pPak->FClose(file);

				CryPathString destPath(PathUtil::Make(szDestPath, path));
				QFile destFile(QtUtil::ToQString(destPath.c_str()));
				destFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
				destFile.write(buffer.data(), numberOfBytesRead);

				if (progress)
				{
				  progressValue = std::min(1.0f, progressValue + 0.01f);
				  progress(progressValue);
				}
			});
	}
}

bool PathExists(const string& path)
{
	return QFileInfo(QtUtil::ToQString(path)).exists();
}

bool FileExists(const string& path)
{
	QFileInfo inf(QtUtil::ToQString(path));
	return inf.exists() && inf.isFile();
}

bool FolderExists(const string& path)
{
	QFileInfo inf(QtUtil::ToQString(path));
	return inf.exists() && inf.isDir();
}

std::vector<string> GetDirectorysContent(const string& dirPath, int depthLevel /*= 0*/, bool includeFolders /*= false*/)
{
	return GetDirectorysContent(QtUtil::ToQString(dirPath), depthLevel, includeFolders);
}

std::vector<string> GetDirectorysContent(const QString& dirPath, int depthLevel /*= 0*/, bool includeFolders /*= false*/)
{
	using namespace Private_FileUtils;
	std::vector<string> result;
	AddDirectorysContent(dirPath, result, 0, depthLevel == 0 ? std::numeric_limits<int>::max() : depthLevel, includeFolders);
	return result;
}

bool Pak::IsFileInPakOnly(const string& path)
{
	return !FileExists(path) && GetISystem()->GetIPak()->IsFileExist(PathUtil::ToGamePath(path), ICryPak::eFileLocation_InPak);;
}

bool Pak::IsFileInPakOrOnDisk(const string& path)
{
	return GetISystem()->GetIPak()->IsFileExist(PathUtil::ToGamePath(path));
}

EDITOR_COMMON_API bool Pak::CompareFiles(const string& filePath1, const string& filePath2)
{
	// Try to open both files for read.  If either fails we say they are different (most likely one doesn't exist).
	CCryFile file1, file2;
	if (!file1.Open(filePath1, "rb") || !file2.Open(filePath2, "rb"))
	{
		return false;
	}

	// If the files are different sizes return false
	uint64 size1 = file1.GetLength();
	uint64 size2 = file2.GetLength();
	if (size1 != size2)
	{
		return false;
	}

	// Sizes are the same, we need to compare the bytes.
	const uint64 bufSize = 4096;
	char buf1[bufSize], buf2[bufSize];
	for (uint64 i = 0; i < size1; i += bufSize)
	{
		size_t amtRead1 = file1.ReadRaw(buf1, bufSize);
		size_t amtRead2 = file2.ReadRaw(buf2, bufSize);

		// Not a match if we didn't read the same amount from each file
		if (amtRead1 != amtRead2)
		{
			return false;
		}

		// Not a match if we didn't read the amount of data we expected
		if (amtRead1 != bufSize && i + amtRead1 != size1)
		{
			return false;
		}

		// Not a match if the buffers aren't the same
		if (memcmp(buf1, buf2, amtRead1) != 0)
		{
			return false;
		}
	}

	return true;
}

EDITOR_COMMON_API void BackupFile(const char* szFilePath)
{
	if (!FileExists(szFilePath))
	{
		return;
	}

	const CryPathString adjusted(PathUtil::MatchAbsolutePathToCaseOnFileSystem(szFilePath));
	const CryPathString bakFilename = PathUtil::ReplaceExtension(adjusted, "bak");
	const CryPathString bakFilename2 = PathUtil::ReplaceExtension(adjusted, "bak2");
	MoveFileAllowOverwrite(bakFilename, bakFilename2);
	MoveFileAllowOverwrite(szFilePath, bakFilename);
}

EDITOR_COMMON_API bool Pak::CopyFileAllowOverwrite(const char* szSourceFilePath, const char* szDestinationFilePath)
{
	ICryPak* const pPak = GetISystem()->GetIPak();
	FILE* pFile = pPak->FOpen(szSourceFilePath, "rb");
	if (!pFile)
	{
		return false;
	}

	std::vector<char> buffer(pPak->FGetSize(pFile));
	const size_t numberOfBytesRead = pPak->FReadRawAll(buffer.data(), buffer.size(), pFile);
	pPak->FClose(pFile);
	if (numberOfBytesRead != buffer.size())
	{
		return false;
	}

	CryPathString szAdjustedPath;
	pPak->AdjustFileName(szDestinationFilePath, szAdjustedPath, ICryPak::FLAGS_FOR_WRITING);

	GetISystem()->GetIPak()->MakeDir(PathUtil::GetDirectory(szAdjustedPath));

	QFile destFile(QtUtil::ToQString(szAdjustedPath.c_str()));
	if (!destFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		return false;
	}

	if (!destFile.write(buffer.data(), numberOfBytesRead) == numberOfBytesRead)
	{
		destFile.remove();
		return false;
	}
	return true;
}

}
