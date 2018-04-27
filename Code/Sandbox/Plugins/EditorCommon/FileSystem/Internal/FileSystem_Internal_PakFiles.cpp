// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileSystem_Internal_PakFiles.h"

#include <CrySystem/File/ICryPak.h>
#include <IEditor.h>

namespace FileSystem
{
namespace Internal
{

SArchiveContentInternal CPakFiles::GetContents(const QString& keyEnginePath) const
{
	SArchiveContentInternal result;
	//result.pakFilePath = enginePath;
#ifndef EDITOR_COMMON_EXPORTS
	Q_UNUSED(keyEnginePath); // its always the same fake
	SDirectoryInfoInternal fakeFolder;
	fakeFolder.SetAbsolutePath(SAbsolutePath("Fakefolder"));
	result.directories << fakeFolder;

	SDirectoryInfoInternal subFolder;
	subFolder.SetAbsolutePath(SAbsolutePath("Fakefolder/Subfolder"));
	result.directories << subFolder;

	QDateTime fakeTime(QDate(2016, 1, 1), QTime(12, 11, 10, 9));

	SFileInfoInternal fileInfo1;
	fileInfo1.SetAbsolutePath(SAbsolutePath("Fakefolder/Fakefile.txt"));
	fileInfo1.size = 42;
	fileInfo1.lastModified = fakeTime;
	result.files << fileInfo1;

	SFileInfoInternal fileInfo2;
	fileInfo2.SetAbsolutePath(SAbsolutePath("Fakefolder/Morefile.png"));
	fileInfo2.size = 2048;
	fileInfo2.lastModified = fakeTime;
	result.files << fileInfo2;
#else
	// real implementation
	auto keyEnginePathStr = keyEnginePath.mid(1).toStdString();
	QStringList directoryList;
	GetContentsInternal(keyEnginePathStr, QString(), result, directoryList);
	for (int i = 0; i < directoryList.size(); ++i)
	{
		GetContentsInternal(keyEnginePathStr, directoryList[i] + '/', result, directoryList);
	}
#endif
	return result;
}

#ifdef EDITOR_COMMON_EXPORTS

void CPakFiles::GetContentsInternal(const std::string& archiveEnginePathStr, const QString& fullLocalPath, SArchiveContentInternal& result, QStringList& directoryList) const
{
	static QDateTime fileTimeOrigin(QDate(1601, 1, 1), QTime(0, 0, 0, 0), Qt::UTC);

	std::string levelPakPath(GetIEditor()->GetLevelName());
	levelPakPath += "/level.pak";
	
	if (archiveEnginePathStr.find(levelPakPath) != std::string::npos) // don't index the level.pak
		return;

	auto pIPak = GetIEditor()->GetSystem()->GetIPak();
	assert(pIPak);
	auto searchStr = (fullLocalPath + "*").toStdString();
	pIPak->ForEachArchiveFolderEntry(
	  archiveEnginePathStr.c_str(),
	  searchStr.c_str(),
	  [&](const ICryPak::ArchiveEntryInfo& entry)
			{
				auto entryFullName = QString::fromLocal8Bit(entry.szName);
				auto entryFullLocalPath = (fullLocalPath + entryFullName);
				if (entry.bIsFolder)
				{
				  SDirectoryInfoInternal directoryInfo;
				  directoryInfo.absolutePath = SAbsolutePath(entryFullLocalPath);
				  directoryInfo.fullName = entryFullName;
				  directoryInfo.keyName = entryFullName.toLower();
				  result.directories << directoryInfo;
				  directoryList << entryFullLocalPath;
				}
				else
				{
				  auto lastModified = fileTimeOrigin.addMSecs(entry.aModifiedTime / 10000); // 100 nanoseconds
				  SFileInfoInternal fileInfo;
				  fileInfo.absolutePath = SAbsolutePath(entryFullLocalPath);
				  fileInfo.fullName = entryFullName;
				  fileInfo.keyName = entryFullName.toLower();
				  fileInfo.extension = GetFileExtensionFromFilename(fileInfo.keyName);
				  fileInfo.size = entry.size;
				  fileInfo.lastModified = lastModified;
				  result.files << fileInfo;
				}
	    });
}
#endif

} // namespace Internal
} // namespace FileSystem

