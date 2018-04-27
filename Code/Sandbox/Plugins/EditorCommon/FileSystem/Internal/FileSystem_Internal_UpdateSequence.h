// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "FileSystem_Internal_Data.h"

#include <QMetaType>
#include <QVector>

namespace FileSystem
{
namespace Internal
{

/// \brief internal sequence of updates from a monitor
class CUpdateSequence
{
public:
	template<
	  typename RemovePathCallback, typename RenamePathCallback,
	  typename CreateDirectoryCallback, typename CreateFileCallback,
	  typename ModifyDirectoryCallback, typename ModifyFileCallback>
	void visitAll(
	  const RemovePathCallback& removePathCallback, const RenamePathCallback& renamePathCallback,
	  const CreateDirectoryCallback& createDirectoryCallback, const CreateFileCallback& createFileCallback,
	  const ModifyDirectoryCallback& modifyDirectoryCallback, const ModifyFileCallback& modifyFileCallback) const
	{
		int removePathIndex = 0;
		int renamePathIndex = 0;
		int createDirectoryIndex = 0;
		int createFileIndex = 0;
		int modifyDirectoryIndex = 0;
		int modifyFileIndex = 0;
		foreach(const auto & type, m_typeOrder)
		{
			switch (type)
			{
			case RemovePath:
				removePathCallback(m_removedPathes[removePathIndex++]);
				break;
			case RenamePath:
				renamePathCallback(m_renamedPathes[renamePathIndex].keyAbsolutePath, m_renamedPathes[renamePathIndex].toName);
				renamePathIndex++;
				break;
			case CreateDirectory:
				createDirectoryCallback(m_createdDirectories[createDirectoryIndex++]);
				break;
			case CreateFile:
				createFileCallback(m_createdFiles[createFileIndex++]);
				break;
			case ModifyDirectory:
				modifyDirectoryCallback(m_modifiedDirectories[modifyDirectoryIndex++]);
				break;
			case ModifyFile:
				modifyFileCallback(m_modifiedFiles[modifyFileIndex++]);
				break;
			default:
				CRY_ASSERT(!"Bad Type");
			}
		}
	}

	void Reset();

	void AddRemovePath(const QString& keyAbsolutePath);
	void AddRename(const QString& keyAbsolutePath, const SAbsolutePath& toName);
	void AddCreateDirectory(const SDirectoryInfoInternal&);
	void AddCreateFile(const SFileInfoInternal&);
	void AddModifyDirectory(const SDirectoryInfoInternal&);
	void AddModifyFile(const SFileInfoInternal&);

private:
	struct SPathRename
	{
		QString       keyAbsolutePath;
		SAbsolutePath toName;
	};
	enum Types
	{
		RemovePath,
		RenamePath,
		CreateDirectory,
		CreateFile,
		ModifyFile,
		ModifyDirectory
	};

	QVector<Types>                  m_typeOrder;
	QVector<QString>                m_removedPathes;
	QVector<SPathRename>            m_renamedPathes;
	QVector<SDirectoryInfoInternal> m_createdDirectories;
	QVector<SFileInfoInternal>      m_createdFiles;
	QVector<SDirectoryInfoInternal> m_modifiedDirectories;
	QVector<SFileInfoInternal>      m_modifiedFiles;
};

} // namespace Internal
} // namespace FileSystem

Q_DECLARE_METATYPE(FileSystem::Internal::CUpdateSequence)

