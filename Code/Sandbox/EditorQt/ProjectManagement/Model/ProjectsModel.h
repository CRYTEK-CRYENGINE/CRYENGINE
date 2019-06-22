// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ProjectManagement/Model/SubIconContainer.h"
#include "ProjectManagement/Utils.h"
#include <QSortFilterProxyModel>

struct SProjectDescription;

class CProjectManager;

//User loads existing list, and there are 2 possibilities:
// 1. If a new project has been created from template, m_projects is updated
// 2. If an existing project is opened, than it's time updated
class CProjectsModel : public QAbstractItemModel
{
public:
	enum Column
	{
		eColumn_RunOnStartup,
		eColumn_Name,
		eColumn_LastAccessTime,
		eColumn_Path,
	};

	enum UserRoles : int
	{
		SortRole = Qt::UserRole + 1
	};

	CProjectsModel(QObject* pParent, CProjectManager& mgr);

	const SProjectDescription* ProjectFromIndex(const QModelIndex& index) const;
	QModelIndex                IndexFromProject(const SProjectDescription* pDescr) const;

private:
	// QAbstractItemModel
	virtual int         columnCount(const QModelIndex& parent) const override;
	virtual QVariant    headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual int         rowCount(const QModelIndex& parent) const override;
	virtual QVariant    data(const QModelIndex& index, int role) const override;
	virtual QModelIndex index(int row, int column, const QModelIndex& parent) const override;
	virtual QModelIndex parent(const QModelIndex& child) const override;

	void                OnProjectUpdate(const SProjectDescription* pDescr);

	CProjectManager&        m_projectManager;
	const CSubIconContainer m_subicons;
};

// Sort + Search via several columns
class CProjectSortProxyModel : public QSortFilterProxyModel
{
public:
	explicit CProjectSortProxyModel(QObject* pParent);

private:
	virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};
