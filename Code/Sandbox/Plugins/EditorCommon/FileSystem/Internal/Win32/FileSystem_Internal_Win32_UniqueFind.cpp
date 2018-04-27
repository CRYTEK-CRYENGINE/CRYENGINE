// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileSystem_Internal_Win32_UniqueFind.h"

#include "FileSystem_Internal_Win32_Utils.h"

#include <QDir>

namespace FileSystem
{
namespace Internal
{
namespace Win32
{

QString CUniqueFind::GetLinkTargetPath() const
{
	if (!IsLink())
	{
		return QString();
	}
	return FileSystem::Internal::Win32::GetLinkTargetPath(QDir(m_path).filePath(GetFileName()));
}

bool CUniqueFind::FindFirst(const QString& path)
{
	Close();
	auto searchMask = QStringLiteral("\\\\?\\") + QDir::toNativeSeparators(QDir(path).filePath("*"));
	auto searchMaskWStr = searchMask.toStdWString();
	m_path = path;
	m_handle = FindFirstFileExW(
	  searchMaskWStr.data(),       // FileName
	  FindExInfoBasic, &m_data,    // InfoLevel, FindData
	  FindExSearchNameMatch, NULL, // SearchOp, searchFilter
	  FIND_FIRST_EX_LARGE_FETCH    // AdditionalFlags
	  );
	return IsValid();
}

} // namespace Win32
} // namespace Internal
} // namespace FileSystem

