// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProjectsModel.h"

#include "ProjectManagement/Data/ProjectManager.h"

#include <ProxyModels/ItemModelAttribute.h>
#include <QThumbnailView.h>

#include <QDateTime>

namespace Private_ProjectsModel
{

CItemModelAttribute s_runOnStartupAttribute("Startup Project", &Attributes::s_booleanAttributeType, CItemModelAttribute::Visible, true);
CItemModelAttribute s_lastOpenTimeAttribute("Last Open Time", &Attributes::s_stringAttributeType, CItemModelAttribute::Visible, false);
CItemModelAttribute s_pathAttribute("Path", &Attributes::s_floatAttributeType, CItemModelAttribute::Visible, false);

const CItemModelAttribute* s_attributes[] = {
	&s_runOnStartupAttribute,
	&Attributes::s_nameAttribute,
	&s_lastOpenTimeAttribute,
	&s_pathAttribute };

const int s_attributeCount = sizeof(s_attributes) / sizeof(CItemModelAttribute*);

} // namespace Private_ProjectsModel

CProjectsModel::CProjectsModel(QObject* pParent, CProjectManager& mgr)
	: QAbstractItemModel(pParent)
	, m_projectManager(mgr)
{
	m_projectManager.signalBeforeProjectsUpdated.Connect([this]() { beginResetModel(); });
	m_projectManager.signalAfterProjectsUpdated.Connect([this]() { endResetModel(); });
	m_projectManager.signalProjectDataChanged.Connect(this, &CProjectsModel::OnProjectUpdate);
}

int CProjectsModel::columnCount(const QModelIndex& parent) const
{
	if (!parent.isValid())
	{
		return Private_ProjectsModel::s_attributeCount;
	}
	return 0;
}

QVariant CProjectsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal || section >= Private_ProjectsModel::s_attributeCount)
	{
		return QVariant();
	}

	switch (role)
	{
	case Qt::DisplayRole:
		return Private_ProjectsModel::s_attributes[section]->GetName();

	case Qt::DecorationRole:
		if (section == eColumn_RunOnStartup)
			return CryIcon("icons:General/Startup_Project.ico");
	}

	return QVariant();
}

int CProjectsModel::rowCount(const QModelIndex& parent) const
{
	if (parent.isValid())
	{
		// Flat structure: no children for an element
		return 0;
	}

	// This model shows all supported projects
	return static_cast<int>(m_projectManager.GetProjects().size());
}

const SProjectDescription* CProjectsModel::ProjectFromIndex(const QModelIndex& index) const
{
	if ((index.row() < 0) || (index.column() < 0))
	{
		return nullptr;
	}
	return static_cast<SProjectDescription*>(index.internalPointer());
}

QModelIndex CProjectsModel::IndexFromProject(const SProjectDescription* pDescr) const
{
	const int rows = rowCount(QModelIndex{});
	for (int i = 0; i < rows; ++i)
	{
		QModelIndex currIdx = index(i, 0, QModelIndex{});
		const SProjectDescription* pCurr = ProjectFromIndex(currIdx);
		if (pCurr == pDescr)
		{
			return currIdx;
		}
	}

	return QModelIndex{};
}

void CProjectsModel::OnProjectUpdate(const SProjectDescription* pDescr)
{
	const int rows = rowCount(QModelIndex{});
	for (int i = 0; i < rows; ++i)
	{
		QModelIndex currIdx = index(i, 0, QModelIndex{});
		const SProjectDescription* pCurr = ProjectFromIndex(currIdx);
		if (pCurr == pDescr)
		{
			QModelIndex bottmRight = index(i, Private_ProjectsModel::s_attributeCount, QModelIndex{});
			dataChanged(currIdx, bottmRight);
			return;
		}
	}
}

QVariant CProjectsModel::data(const QModelIndex& index, int role) const
{
	const SProjectDescription* pDescr = ProjectFromIndex(index);
	if (pDescr == nullptr)
	{
		return QVariant();
	}

	const size_t row = static_cast<size_t>(index.row());
	const Column col = static_cast<Column>(index.column());

	switch (role)
	{
	case UserRoles::SortRole:
		switch (col)
		{
		case eColumn_RunOnStartup:
			return pDescr->startupProject;
		}
		// Intentional fall through, rest of columns use display role for sorting
	case Qt::DisplayRole:
		switch (col)
		{
		case eColumn_Name:
			return QString(pDescr->name);
		case eColumn_LastAccessTime:
			return QDateTime::fromTime_t(pDescr->lastOpened);
		case eColumn_Path:
			return QString(pDescr->fullPathToCryProject);
		}
		break;

	case Qt::DecorationRole:
	case QThumbnailsView::s_ThumbnailRole:
		switch (col)
		{
		case eColumn_RunOnStartup:
			return pDescr->startupProject ? CryIcon("icons:General/Startup_Project.ico") : QVariant();
		case eColumn_Name:
			return pDescr->icon;
		}
		break;
	case QThumbnailsView::s_ThumbnailIconsRole:
		return m_subicons.GetIcons(pDescr->language, pDescr->startupProject);
	}

	return QVariant();
}

QModelIndex CProjectsModel::index(int row, int column, const QModelIndex& parent) const
{
	const auto& projects = m_projectManager.GetProjects();
	if (row < 0 || row >= static_cast<int>(projects.size()))
	{
		return QModelIndex();
	}

	return createIndex(row, column, reinterpret_cast<quintptr>(&projects[row]));
}

QModelIndex CProjectsModel::parent(const QModelIndex& child) const
{
	// Data is flat: no parent
	return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////
CProjectSortProxyModel::CProjectSortProxyModel(QObject* pParent)
	: QSortFilterProxyModel(pParent)
{
	setSortRole(CProjectsModel::SortRole);
}

bool CProjectSortProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
	const auto* pModel = sourceModel();

	const QModelIndex index0 = pModel->index(sourceRow, CProjectsModel::eColumn_Name, sourceParent);
	const QModelIndex index1 = pModel->index(sourceRow, CProjectsModel::eColumn_LastAccessTime, sourceParent);
	const QModelIndex index2 = pModel->index(sourceRow, CProjectsModel::eColumn_Path, sourceParent);

	return pModel->data(index0).toString().contains(filterRegExp().pattern(), Qt::CaseInsensitive) ||
	       pModel->data(index1).toString().contains(filterRegExp().pattern(), Qt::CaseInsensitive) ||
	       pModel->data(index2).toString().contains(filterRegExp().pattern(), Qt::CaseInsensitive);
}
