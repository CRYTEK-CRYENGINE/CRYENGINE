// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileType.h"

#include <QCoreApplication>

const char* SFileType::trContext = "FileType";

QString SFileType::name() const
{
	return QCoreApplication::translate(trContext, nameTrKey);
}

const SFileType* SFileType::Unknown()
{
	static const SFileType s_unknown = []()
	{
		SFileType unknown;
		unknown.nameTrKey = "Unknown file";
		unknown.iconPath = QStringLiteral("icons:General/File.ico");
		return unknown;
	} ();
	return &s_unknown;
}

const SFileType* SFileType::DirectoryType()
{
	static const SFileType sDirectoryType = []()
	{
		SFileType directoryType;
		directoryType.nameTrKey = "File folder";
		directoryType.iconPath = QStringLiteral("icons:General/Folder.ico");
		return directoryType;
	} ();
	return &sDirectoryType;
}

const SFileType* SFileType::SymLink()
{
	static const SFileType sSymLink = []()
	{
		SFileType directoryType;
		directoryType.nameTrKey = "SymLink";
		directoryType.iconPath = QStringLiteral("icons:General/SymLink.ico");
		return directoryType;
	} ();
	return &sSymLink;
}

void SFileType::CheckValid() const
{
	for (auto& folder : folders)
	{
		CRY_ASSERT(folder == folder.toLower());
	}
	CRY_ASSERT(primaryExtension == primaryExtension.toLower());
	for (auto& extension : extraExtensions)
	{
		CRY_ASSERT(extension == extension.toLower());
	}
}

