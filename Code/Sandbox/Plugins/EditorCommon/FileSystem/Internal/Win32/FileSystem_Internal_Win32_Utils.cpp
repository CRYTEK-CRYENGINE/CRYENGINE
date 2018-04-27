// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileSystem_Internal_Win32_Utils.h"

#include "FileSystem_Internal_Win32_UniqueHandle.h"

#include <CryCore/Platform/CryWindows.h>
#include <winioctl.h>

#include <QDir>
#include <QDebug>

extern "C" {

#define SYMLINK_FLAG_RELATIVE 1

	typedef struct _REPARSE_DATA_BUFFER
	{
		ULONG  ReparseTag;
		USHORT ReparseDataLength;
		USHORT Reserved;
		union
		{
			struct
			{
				USHORT SubstituteNameOffset;
				USHORT SubstituteNameLength;
				USHORT PrintNameOffset;
				USHORT PrintNameLength;
				ULONG  Flags;
				WCHAR  PathBuffer[1];
			} SymbolicLinkReparseBuffer;
			struct
			{
				USHORT SubstituteNameOffset;
				USHORT SubstituteNameLength;
				USHORT PrintNameOffset;
				USHORT PrintNameLength;
				WCHAR  PathBuffer[1];
			} MountPointReparseBuffer;
			struct
			{
				UCHAR DataBuffer[1];
			} GenericReparseBuffer;
		};
	} REPARSE_DATA_BUFFER, * PREPARSE_DATA_BUFFER;

} // extern "C"

namespace FileSystem
{
namespace Internal
{
namespace Win32
{

QString GetLinkTargetPath(const QString& linkPath)
{
	auto unicodeLinkPath = QStringLiteral("\\\\?\\") + QDir::toNativeSeparators(linkPath);
	auto linkPathWStr = unicodeLinkPath.toStdWString();
	CUniqueHandle fileHandle = CreateFileW(
	  linkPathWStr.data(),                                       // FileName
	  GENERIC_READ,                                              // DesiredAccess
	  FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,    // ShareMode
	  NULL,                                                      // SecurityAttributes
	  OPEN_EXISTING,                                             // CreationDisposition
	  FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, // FlagsAndAttributes
	  NULL                                                       // TemplateFile
	  );
	if (!fileHandle.IsValid())
	{
		auto error = ::GetLastError();
		qWarning() << "FileSystem::Internal::Win32::GetLinkTargetPath" << "CreateFile Error" << error;
		return QString();
	}

	BYTE buffer[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
	DWORD bytesReturned;
	auto success = DeviceIoControl(
	  fileHandle.GetHandle(),                    // Device
	  FSCTL_GET_REPARSE_POINT,                   // ControlCode
	  NULL, 0,                                   // InBuffer, Size
	  &buffer, MAXIMUM_REPARSE_DATA_BUFFER_SIZE, // OutBuffer, Size
	  &bytesReturned,                            // BytesReturned
	  0                                          // Overlapped
	  );
	if (!success)
	{
		auto error = ::GetLastError();
		qWarning() << "FileSystem::Internal::Win32::GetLinkTargetPath" << "DeviceIoControl Error" << error;
		return QString();
	}
	const REPARSE_DATA_BUFFER* reparseBuffer = (REPARSE_DATA_BUFFER*)&buffer;
	if (IO_REPARSE_TAG_SYMLINK == reparseBuffer->ReparseTag)
	{
		QString result = QString::fromWCharArray(
		  reparseBuffer->SymbolicLinkReparseBuffer.PathBuffer +
		  reparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameOffset / 2,
		  reparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameLength / 2);
		if (result.startsWith("\\??\\"))
		{
			result = result.mid(4);
		}
		if (0 != (reparseBuffer->SymbolicLinkReparseBuffer.Flags & SYMLINK_FLAG_RELATIVE))
		{
			result = QDir(linkPath).absoluteFilePath(result);
		}
		return QDir::cleanPath(QDir::fromNativeSeparators(result));
	}
	if (IO_REPARSE_TAG_MOUNT_POINT == reparseBuffer->ReparseTag)
	{
		QString result = QString::fromWCharArray(
		  reparseBuffer->MountPointReparseBuffer.PathBuffer +
		  reparseBuffer->MountPointReparseBuffer.SubstituteNameOffset / 2,
		  reparseBuffer->MountPointReparseBuffer.SubstituteNameLength / 2);
		if (result.startsWith("\\??\\") || result.startsWith("\\\\?\\"))
		{
			result = result.mid(4);
		}
		// this is always an absolute path
		return QDir::cleanPath(QDir::fromNativeSeparators(result));
	}
	return QString(); // not a parsable link
}

} // namespace Win32
} // namespace Internal
} // namespace FileSystem

