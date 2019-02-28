// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileSystem_FileFilter.h"

#include "FileSystem/FileType.h"

namespace FileSystem
{

SFileFilter SFileFilter::ForRootExtension(const QString& extension)
{
	SFileFilter filter;
	CRY_ASSERT(!extension.isEmpty());
	filter.fileExtensions << extension;
	return filter;
}

SFileFilter SFileFilter::ForTreeExtension(const QString& extension)
{
	SFileFilter filter = ForRootExtension(extension);
	filter.recursiveSubdirectories = true;
	return filter;
}

SFileFilter SFileFilter::ForDirectoryAndExtension(const QString& directoryEnginePath, const QString& extension)
{
	SFileFilter filter = ForRootExtension(extension);
	CRY_ASSERT(!directoryEnginePath.isEmpty());
	filter.directories << directoryEnginePath;
	return filter;
}

SFileFilter SFileFilter::ForDirectoryTreeAndExtension(const QString& directoryEnginePath, const QString& extension)
{
	SFileFilter filter = ForDirectoryAndExtension(directoryEnginePath, extension);
	filter.recursiveSubdirectories = true;
	return filter;
}

SFileFilter SFileFilter::ForFileType(const SFileType* fileType)
{
	SFileFilter filter;
	if (!fileType->primaryExtension.isEmpty())
	{
		filter.fileExtensions << fileType->primaryExtension;
	}
	foreach(const QString &extension, fileType->extraExtensions)
	{
		filter.fileExtensions << extension;
	}
	return filter;
}

void SFileFilter::MakeInputValid()
{
	SDirectoryFilter::MakeInputValid();
	for (auto& extension : fileExtensions)
	{
		CRY_ASSERT(extension == extension.toLower());
	}
	for (auto& token : fileTokens)
	{
		CRY_ASSERT(token == token.toLower());
	}
}

} // namespace FileSystem

