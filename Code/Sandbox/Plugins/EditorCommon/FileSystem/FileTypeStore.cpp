// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "FileTypeStore.h"

void CFileTypeStore::RegisterFileType(const SFileType* fileType)
{
	CRY_ASSERT(fileType);
	fileType->CheckValid();

	AddExtensionFileType(fileType->primaryExtension, fileType);
	for (const auto& extraExtension : fileType->extraExtensions)
	{
		AddExtensionFileType(extraExtension, fileType);
	}
}

void CFileTypeStore::RegisterFileTypes(const QVector<const SFileType*>& fileTypes)
{
	for (auto fileType : fileTypes)
	{
		RegisterFileType(fileType);
	}
}

const SFileType* CFileTypeStore::GetType(const QString& keyEnginePath, const QString& extension) const
{
	// search for best fitting file type
	const auto& sortedFileTypes = m_extensionTypes[extension];

	for (auto pFileType : sortedFileTypes)
	{
		CRY_ASSERT(pFileType);
		if (!pFileType)
		{
			continue;
		}

		// no folder means all folders.
		// because of order it can be stopped.
		// all folder types have been checked already
		// before, if present
		if (pFileType->folders.empty())
		{
			return pFileType;
		}

		const auto checkDirectoryPath = [&keyEnginePath](const QString& typeFolder)
		{
			if (typeFolder.length() == keyEnginePath.length())
			{
				return typeFolder == keyEnginePath;
			}
			if (typeFolder.length() < keyEnginePath.length())
			{
				return keyEnginePath[typeFolder.length()] == '/' && keyEnginePath.startsWith(typeFolder);
			}
			return false;
		};

		if (std::any_of(pFileType->folders.begin(), pFileType->folders.end(), checkDirectoryPath))
		{
			return pFileType;
		}
	}

	return SFileType::Unknown(); // no type found
}

void CFileTypeStore::AddExtensionFileType(const QString& extension, const SFileType* fileType)
{
	auto& fileTypes = m_extensionTypes[extension];
	fileTypes << fileType;

	// primary extension > extra extension
	// folder selection > all folders
	std::sort(fileTypes.begin(), fileTypes.end(), [&extension](const SFileType* left, const SFileType* right)
	{
		CRY_ASSERT(left && right);
		if (!left || !right)
		{
		  return left != nullptr;
		}

		// sorting for file extension is irrelevant. Just
		// check for containing directories.
		if (left->folders.size() != right->folders.size())
		{
		  return left->folders.size() > right->folders.size();
		}
		if (left->primaryExtension != right->primaryExtension)
		{
		  return left->primaryExtension < right->primaryExtension;
		}
		return left < right;
	});
}

