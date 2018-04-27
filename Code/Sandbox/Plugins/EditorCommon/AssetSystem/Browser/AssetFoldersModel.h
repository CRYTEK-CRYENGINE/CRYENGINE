// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QAbstractItemModel>

#include "AssetModel.h"

//! This model describes all the assets contained in the AssetManager
//! It uses attributes to enable smart filtering of assets in the asset browser
//! Behavior: paths with %XXX% prefix will be considered out of the assets folder
//! All other paths will be relative to the the asset folder
class CAssetFoldersModel : public QAbstractItemModel
{
	//CAssetModel instance is managed by the AssetManager
	friend class CAssetManager;
	CAssetFoldersModel(QObject* parent = nullptr);
	~CAssetFoldersModel();

public:
	static CAssetFoldersModel* GetInstance();

	enum class Roles : int
	{
		FolderPathRole = (int)CAssetModel::Roles::Max, //Qt::DisplayRole only returns the folder name. This will return the full path
		DisplayFolderPathRole // Returns full "pretty" path \see GetPrettyPath::GetPrettyPath
	};

	//! Returns the model index that corresponds to the folder path.
	//! \param folder String path of the folder to be searched for.
	//! \param role Describes content of the folder path. \see CAssetFoldersModel::Roles
	QModelIndex FindIndexForFolder(const QString& folder, Roles role = Roles::FolderPathRole) const;

	//! Returns the model index that corresponds to the root folder of assets.
	QModelIndex GetProjectAssetsFolderIndex() const;

	//! Creates a new folder with default name, returns the full path of the created folder
	QString CreateFolder(const QString& parentPath);
	//! Creates a new folder with specified name
	void CreateFolder(const QString& parentPath, const QString& folderName);
	//! Removes a folder, must be empty
	void DeleteFolder(const QString& folderPath);
	//! Opens the specified folder in the OS shell, e.g. Explorer
	void OpenFolderWithShell(const QString& folderPath);

	//! Only newly created folders will be empty, the model only shows folders that contains assets
	//! unless they were created during this editor session
	bool IsEmptyFolder(const QString& folderPath) const;

	//! Only assets located in the current project are writable, other folders are read-only
	bool IsReadOnlyFolder(const QString& folderPath) const;

	bool HasSubfolder(const QString& folderPath, const QString& subFolderName) const;

	//! Utility to get the displayed name for the root folder of assets
	const QString& GetProjectAssetsFolderName() const;

	//! "Prettify" a path, replacing aliases by pretty name and prepending asset folder if necessary
	QString GetPrettyPath(const QString& path) const;

	//////////////////////////////////////////////////////////
	// QAbstractItemModel implementation
	virtual bool			hasChildren(const QModelIndex& parent = QModelIndex()) const override;
	virtual int             rowCount(const QModelIndex& parent) const override;

	//!Folder name is the only column in this model
	virtual int             columnCount(const QModelIndex& parent) const override { return 1; }

	virtual QVariant        data(const QModelIndex& index, int role) const override;
	virtual bool			setData(const QModelIndex& index, const QVariant& value, int role /*= Qt::EditRole*/) override;
	virtual QVariant        headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual Qt::ItemFlags   flags(const QModelIndex& index) const override;
	virtual QModelIndex     index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	virtual QModelIndex     parent(const QModelIndex& index) const override;
	//////////////////////////////////////////////////////////

private:

	//Note: this could maybe be improved by storing all the folders in a sorted array (by path) 
	//which would make the search time for a folder always log(n)
	//searching the tree breadth first has a worse worst case
	struct Folder
	{
		QString m_name;
		Folder* m_parent;
		bool m_empty;
		std::vector<std::unique_ptr<Folder>> m_subFolders;
	};

	void PreUpdate();
	void PostUpdate();

	void PreInsert(const std::vector<CAsset*>& assets);

	void PreRemove(const std::vector<CAsset*>& assets);
	void PostRemove();

	void Clear();

	// Returns the first folder that it needs to create (even if it creates subfolders inside it) 
	// to facitilitate Qt model notifications
	Folder* AddFolder(const char* path, bool empty);

	// Returns the created subfolder
	Folder* CreateSubFolder(Folder* parent, const char* childName, bool empty);

	QString GetPath(const Folder* folder) const;
	Folder* ToFolder(const QModelIndex& folder) const;
	QModelIndex ToIndex(const Folder* pFolder) const;
	const Folder* GetFolder(const char* path) const;

	Folder m_root;//root of the view, not displayed
	Folder* m_assetFolders;//helper pointer containing all the asset folders, displayed by the view
	QVector<QString> m_addedFolders;
};
