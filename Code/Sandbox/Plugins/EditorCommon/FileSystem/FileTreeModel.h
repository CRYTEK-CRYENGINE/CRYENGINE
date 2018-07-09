// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QAbstractItemModel>

#include "FileSystem/FileSystem_FileFilter.h"
#include "EditorCommonAPI.h"

#include <memory>

namespace FileSystem
{
class CEnumerator;
}

class EDITOR_COMMON_API CFileTreeModel : public QAbstractItemModel
{
	Q_OBJECT

public:
	enum ERole
	{
		eRole_EnginePath = Qt::UserRole + 1,
		eRole_FileName,
		eRole_FileSize,
		eRole_LastModified
	};

	enum EColumn
	{
		eColumn_Name = 0,
		eColumn_LastModified,
		eColumn_Type,
		eColumn_Size,
		eColumn_Archive,
		eColumn_Count
	};

public:
	CFileTreeModel(FileSystem::CEnumerator& enumerator, const FileSystem::SFileFilter& filter = FileSystem::SFileFilter(), QObject* pParent = nullptr);
	virtual ~CFileTreeModel();

	QModelIndex      GetIndexByPath(const QString& enginePath) const;
	QString          GetPathFromIndex(const QModelIndex& index) const;
	QString          GetFileNameFromIndex(const QModelIndex& index) const;

	bool             IsDirectory(const QModelIndex& index) const;
	const SFileType* GetFileType(const QModelIndex& index) const;
	quint64          GetFileSize(const QModelIndex& index) const;
	QDateTime        GetLastModified(const QModelIndex& index) const;

	// choose a filter which disables files to get directory model
	void SetFilter(const FileSystem::SFileFilter& filter);

	// QAbstractItemModel interface
public:
	virtual QModelIndex   index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	virtual QModelIndex   parent(const QModelIndex& child) const override;
	virtual bool          hasChildren(const QModelIndex& parent = QModelIndex()) const override;
	virtual int           rowCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int           columnCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual QVariant      data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	virtual QVariant      headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
	virtual QStringList   mimeTypes() const override;
	virtual QMimeData*    mimeData(const QModelIndexList& indexes) const override;
private:
	struct SImplementation;
	std::unique_ptr<SImplementation> p;
};

