// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QString>
#include <QDatetime>

#include <CryCore/Platform/CryWindows.h>

namespace FileSystem
{
namespace Internal
{
namespace Win32
{

/// \brief encapsultes a FindFirstFileW/FindNextFileW/FindClose sequence as a C++ class
///
/// \note really lightweight, most functions are inline
class CUniqueFind
{
	HANDLE           m_handle;
	QString          m_path;
	WIN32_FIND_DATAW m_data;

public:
	inline CUniqueFind(const QString& path)
		: m_handle(INVALID_HANDLE_VALUE)
	{
		FindFirst(path);
	}

	inline ~CUniqueFind()
	{
		Close();
	}

	inline bool IsValid() const
	{
		return m_handle != INVALID_HANDLE_VALUE;
	}

	inline const WIN32_FIND_DATAW& GetData() const
	{
		CRY_ASSERT(IsValid());
		return m_data;
	}

	inline QString GetFileName() const
	{
		return QString::fromWCharArray(GetData().cFileName);
	}

	inline quint64 GetSize() const
	{
		return ((quint64)GetData().nFileSizeHigh << 32) + GetData().nFileSizeLow;
	}

	inline QDateTime GetLastModified() const
	{
		SYSTEMTIME systemTime;
		FileTimeToSystemTime(&GetData().ftLastWriteTime, &systemTime);
		return QDateTime(
		  QDate(systemTime.wYear, systemTime.wMonth, systemTime.wDay),
		  QTime(systemTime.wHour, systemTime.wMinute, systemTime.wSecond, systemTime.wMilliseconds),
		  Qt::UTC);
	}

	inline bool IsDirectory() const
	{
		return (FILE_ATTRIBUTE_DIRECTORY & GetData().dwFileAttributes) != 0;
	}

	inline bool IsSystem() const
	{
		return (FILE_ATTRIBUTE_SYSTEM & GetData().dwFileAttributes) != 0;
	}

	inline bool IsHidden() const
	{
		return (FILE_ATTRIBUTE_HIDDEN & GetData().dwFileAttributes) != 0;
	}

	/// \return true for "." and ".." equal the file name
	/// \note use this to avoid the creation of a QString
	inline bool IsRelativeDirectory() const
	{
		const WCHAR* fileName = GetData().cFileName;
		return fileName[0] == '.' && (fileName[1] == 0 || (fileName[1] == '.' && fileName[2] == 0));
	}

	inline bool IsLink() const
	{
		return (FILE_ATTRIBUTE_REPARSE_POINT & GetData().dwFileAttributes) != 0
		       && (
		  (IO_REPARSE_TAG_SYMLINK == GetData().dwReserved0)
		  || (IO_REPARSE_TAG_MOUNT_POINT == GetData().dwReserved0)
		  );
	}

	QString     GetLinkTargetPath() const;

	bool        FindFirst(const QString& path);

	inline bool Next()
	{
		return FindNextFileW(m_handle, &m_data);
	}

	inline bool Close()
	{
		if (IsValid())
		{
			FindClose(m_handle);
			return true;
		}
		return false;
	}
};

} // namespace Win32
} // namespace Internal
} // namespace FileSystem

