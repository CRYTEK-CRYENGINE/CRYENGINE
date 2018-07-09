// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QSortFilterProxyModel>

class CAdvancedFileSystemModel;

/**
 * \brief specialized filter that filters out all files
 * \note used for the directory column
 */
class CDirectoriesOnlyProxyModel
	: public QSortFilterProxyModel
{
	// as we do not need registered QObject behavior it's not registered as such
public:
	explicit CDirectoriesOnlyProxyModel(QObject* parent = nullptr);

	// QSortFilterProxyModel interface
protected:
	virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const;

public:
	virtual void setSourceModel(CAdvancedFileSystemModel* sourceFileSystem);
};

