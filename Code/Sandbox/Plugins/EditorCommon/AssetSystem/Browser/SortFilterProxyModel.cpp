// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SortFilterProxyModel.h"

#include "AssetFoldersModel.h"
#include "AssetModel.h"
#include "DependenciesAttribute.h"
#include "DragDrop.h"
#include "FilteredFolders.h"

#include <QApplication>

class CUsageCountAttribute : public CItemModelAttribute
{
public:
	CUsageCountAttribute()
		: CItemModelAttribute("Usage count", &Attributes::s_stringAttributeType, CItemModelAttribute::StartHidden, false)
	{
		static CAssetModel::CAutoRegisterColumn column(this, [](const CAsset* pAsset, const CItemModelAttribute* pAttribute, int role)
		{
			if (role != Qt::DisplayRole)
			{
				return QVariant();
			}
			const CUsageCountAttribute* const pUsageCountAttribute = static_cast<const CUsageCountAttribute*>(pAttribute);
			return pUsageCountAttribute->GetValue(*pAsset);
		});
	}

	void SetDetailContext(CAttributeFilter* pFilter)
	{
		m_pFilter = pFilter;
	}

	QVariant GetValue(const CAsset& asset) const
	{
		if (m_pFilter && m_pFilter->GetOperator())
		{
			const string filterValue = QtUtil::ToString(m_pFilter->GetFilterValue().toString());
			const auto usageInfo = static_cast<Attributes::CDependenciesOperatorBase*>(m_pFilter->GetOperator())->GetUsageInfo(asset, filterValue);
			if (usageInfo.first && usageInfo.second != 0)
			{
				return usageInfo.second;
			}
		}

		return QVariant("n/a");
	}
private:
	CAttributeFilter* m_pFilter = nullptr;
};

static CUsageCountAttribute s_usageCountAttribute;

class CSortFilterProxyModel::UsageCountAttributeContext
{
public:
	UsageCountAttributeContext(CAttributeFilter* pFilter)
	{
		s_usageCountAttribute.SetDetailContext(pFilter);
	}

	~UsageCountAttributeContext()
	{
		s_usageCountAttribute.SetDetailContext(nullptr);
	}
};

void CSortFilterProxyModel::sort(int column, Qt::SortOrder order)
{
	UsageCountAttributeContext context(m_pDependencyFilter);
	QAttributeFilterProxyModel::sort(column, order);
}

bool CSortFilterProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
	EAssetModelRowType leftType = (EAssetModelRowType)left.data((int)CAssetModel::Roles::TypeCheckRole).toUInt();
	EAssetModelRowType rightType = (EAssetModelRowType)right.data((int)CAssetModel::Roles::TypeCheckRole).toUInt();

	if (leftType == rightType)
	{
		// Comparing two variants will compare the types they contain, so it works as expected
		if (left.data(sortRole()) == right.data(sortRole()))
		{
			return left.data((int)CAssetModel::Roles::InternalPointerRole).value<intptr_t>() < right.data((int)CAssetModel::Roles::InternalPointerRole).value<intptr_t>();
		}
		else
		{
			return QAttributeFilterProxyModel::lessThan(left, right);
		}
	}
	else
	{
		return leftType == eAssetModelRow_Folder;
	}
}

bool CSortFilterProxyModel::rowMatchesFilter(int sourceRow, const QModelIndex& sourceParent) const
{
	//specific handling for folders here so they are only tested for name
	QModelIndex index = sourceModel()->index(sourceRow, eAssetColumns_Name, sourceParent);
	if (!index.isValid())
		return false;

	EAssetModelRowType rowType = (EAssetModelRowType)index.data((int)CAssetModel::Roles::TypeCheckRole).toUInt();
	if (rowType == eAssetModelRow_Folder)
	{
		if (m_pFilteredFolders && !m_pFilteredFolders->IsEmpty())
		{
			const QString path = sourceModel()->index(sourceRow, EAssetColumns::eAssetColumns_Folder, sourceParent).data((int)CAssetFoldersModel::Roles::FolderPathRole).toString();
			if (!m_pFilteredFolders->Contains(path) && !CAssetFoldersModel::GetInstance()->IsEmptyFolder(path))
			{
				return false;
			}
		}

		if (QDeepFilterProxyModel::rowMatchesFilter(sourceRow, sourceParent))
		{
			for (auto filter : m_filters)
			{
				if (filter->IsEnabled() && filter->GetAttribute() == &Attributes::s_nameAttribute)
				{
					QVariant val = sourceModel()->data(index, Qt::DisplayRole);
					if (!filter->Match(val))
					{
						return false;
					}
				}
			}
			return true;
		}
		return false;
	}
	else
	{
		return QAttributeFilterProxyModel::rowMatchesFilter(sourceRow, sourceParent);
	}
}

bool CSortFilterProxyModel::canDropMimeData(const QMimeData* pMimeData, Qt::DropAction action, int row, int column, const QModelIndex& parent) const
{
	if (QAttributeFilterProxyModel::canDropMimeData(pMimeData, action, row, column, parent))
	{
		return true;
	}

	CDragDropData::ClearDragTooltip(qApp->widgetAt(QCursor::pos()));
	return false;
}

QVariant CSortFilterProxyModel::data(const QModelIndex& index, int role) const
{
	UsageCountAttributeContext context(m_pDependencyFilter);
	return QAttributeFilterProxyModel::data(index, role);
}

void CSortFilterProxyModel::InvalidateFilter()
{
	int usageCountFiltersCount = 0;
	m_pDependencyFilter = nullptr;
	for (const auto filter : m_filters)
	{
		if (!filter->IsEnabled())
		{
			continue;
		}

		if (filter->GetAttribute() == &Attributes::s_dependenciesAttribute)
		{
			m_pDependencyFilter = ++usageCountFiltersCount == 1 ? filter.get() : nullptr;
		}
	}

	QAttributeFilterProxyModel::invalidateFilter();
}

CSortFilterProxyModel::CSortFilterProxyModel(QObject* pParent) : QAttributeFilterProxyModel(QAttributeFilterProxyModel::BaseBehavior, pParent)
{
}

CSortFilterProxyModel::~CSortFilterProxyModel()
{
	// unsubscribe
	SetFilteredFolders(nullptr);
}

void CSortFilterProxyModel::SetFilteredFolders(CFilteredFolders* pFilteredFolders)
{

	if (m_pFilteredFolders)
	{
		m_pFilteredFolders->signalInvalidate.DisconnectObject(this);
	}

	m_pFilteredFolders = pFilteredFolders;

	if (m_pFilteredFolders)
	{
		pFilteredFolders->signalInvalidate.Connect(this, &CSortFilterProxyModel::InvalidateFilter);
		InvalidateFilter();
	}
}
