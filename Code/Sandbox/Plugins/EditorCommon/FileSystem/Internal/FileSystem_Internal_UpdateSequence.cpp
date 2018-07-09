// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileSystem_Internal_UpdateSequence.h"

namespace FileSystem
{
namespace Internal
{

void CUpdateSequence::Reset()
{
	m_typeOrder.clear();
	m_removedPathes.clear();
	m_renamedPathes.clear();
	m_createdDirectories.clear();
	m_createdFiles.clear();
	m_modifiedDirectories.clear();
	m_modifiedFiles.clear();
}

void CUpdateSequence::AddRemovePath(const QString& keyAbsolutePath)
{
	m_typeOrder << RemovePath;
	m_removedPathes << keyAbsolutePath;
}

void CUpdateSequence::AddRename(const QString& keyAbsolutePath, const SAbsolutePath& toName)
{
	m_typeOrder << RenamePath;
	SPathRename rename;
	rename.keyAbsolutePath = keyAbsolutePath;
	rename.toName = toName;
	m_renamedPathes << rename;
}

void CUpdateSequence::AddCreateDirectory(const SDirectoryInfoInternal& info)
{
	m_typeOrder << CreateDirectory;
	m_createdDirectories << info;
}

void CUpdateSequence::AddCreateFile(const SFileInfoInternal& info)
{
	m_typeOrder << CreateFile;
	m_createdFiles << info;
}

void CUpdateSequence::AddModifyDirectory(const SDirectoryInfoInternal& info)
{
	m_typeOrder << ModifyDirectory;
	m_modifiedDirectories << info;
}

void CUpdateSequence::AddModifyFile(const SFileInfoInternal& info)
{
	m_typeOrder << ModifyFile;
	m_modifiedFiles << info;
}

} // namespace Internal
} // namespace FileSystem

