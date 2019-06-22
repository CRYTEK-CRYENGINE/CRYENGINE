// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TemplatesModel.h"

#include "ProjectManagement/Data/TemplateManager.h"

#include <FileUtils.h>
#include <QThumbnailView.h>

#include <CrySystem/IProjectManager.h>

namespace Private_TemplatesModel
{

CItemModelAttribute s_attributes[] =
{
	Attributes::s_nameAttribute,
	CItemModelAttribute("Language",&Attributes::s_stringAttributeType,  CItemModelAttribute::Visible, false, "") };

const int s_attributeCount = sizeof(s_attributes) / sizeof(CItemModelAttribute);

} // namespace Private_TemplatesModel

CTemplatesModel::CTemplatesModel(QObject* pParent, const CTemplateManager& templateManager)
	: QAbstractItemModel(pParent)
	, m_templateManager(templateManager)
{
}

const STemplateDescription* CTemplatesModel::GetTemplateDescr(const QModelIndex& index) const
{
	if ((index.row() < 0) || (index.column() < 0))
	{
		return nullptr;
	}
	return static_cast<STemplateDescription*>(index.internalPointer());
}

int CTemplatesModel::columnCount(const QModelIndex& parent) const
{
	if (!parent.isValid())
		return Private_TemplatesModel::s_attributeCount;

	return 0;
}

QVariant CTemplatesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal)
	{
		return QVariant();
	}

	if (Private_TemplatesModel::s_attributeCount > section)
	{
		switch (role)
		{
		case Qt::DisplayRole:
			return Private_TemplatesModel::s_attributes[section].GetName();
		}
	}

	return QVariant();
}

int CTemplatesModel::rowCount(const QModelIndex& parent) const
{
	if (parent.isValid())
	{
		// Flat structure: no children for an element
		return 0;
	}

	return static_cast<int>(m_templateManager.GetTemplates().size());
}

QVariant CTemplatesModel::data(const QModelIndex& index, int role) const
{
	const STemplateDescription* pDescr = GetTemplateDescr(index);
	if (pDescr == nullptr)
	{
		return QVariant();
	}

	const size_t row = static_cast<size_t>(index.row());
	const Column col = static_cast<Column>(index.column());

	switch (role)
	{
	case Qt::DisplayRole:
		switch (col)
		{
		case eColumn_Name:
			return QString(pDescr->templateName);
		case eColumn_Language:
			return QString(pDescr->language);
		}
		break;

	case Qt::DecorationRole:
	case QThumbnailsView::s_ThumbnailRole:
		if (col == eColumn_Name)
		{
			return pDescr->icon;
		}
		break;

	case QThumbnailsView::s_ThumbnailIconsRole:
		return m_subicons.GetIcons(pDescr->language, false);
	}

	return QVariant();
}

QModelIndex CTemplatesModel::index(int row, int column, const QModelIndex& parent) const
{
	const auto& templates = m_templateManager.GetTemplates();
	if (row < 0 || row >= static_cast<int>(templates.size()))
	{
		return QModelIndex();
	}

	return createIndex(row, column, reinterpret_cast<quintptr>(&templates[row]));
}

QModelIndex CTemplatesModel::parent(const QModelIndex& child) const
{
	// Data is flat: no parent
	return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////
CTemplatesSortProxyModel::CTemplatesSortProxyModel(QObject* pParent)
	: QSortFilterProxyModel(pParent)
{
}

bool CTemplatesSortProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
	const auto* pModel = sourceModel();

	const QModelIndex index0 = pModel->index(sourceRow, CTemplatesModel::eColumn_Name, sourceParent);
	const QModelIndex index1 = pModel->index(sourceRow, CTemplatesModel::eColumn_Language, sourceParent);

	return pModel->data(index0).toString().contains(filterRegExp().pattern(), Qt::CaseInsensitive) ||
	       pModel->data(index1).toString().contains(filterRegExp().pattern(), Qt::CaseInsensitive);
}
