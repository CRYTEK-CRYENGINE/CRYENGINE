// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ProxyModels/AttributeFilterProxyModel.h"

class CAttributeFilter;
class CFilteredFolders;

class CSortFilterProxyModel : public QAttributeFilterProxyModel
{
	class UsageCountAttributeContext;

	//ensures folders and assets are always together in the sorting order
	bool             lessThan(const QModelIndex& left, const QModelIndex& right) const override;

	bool             rowMatchesFilter(int sourceRow, const QModelIndex& sourceParent) const;
	virtual void     sort(int column, Qt::SortOrder order) override;
	virtual bool     canDropMimeData(const QMimeData* pMimeData, Qt::DropAction action, int row, int column, const QModelIndex& parent) const override;
	virtual QVariant data(const QModelIndex& index, int role) const override;
	virtual void     InvalidateFilter() override;

public:
	CSortFilterProxyModel(QObject* pParent);
	virtual ~CSortFilterProxyModel() override;

	void SetFilteredFolders(CFilteredFolders* pFilteredFolders);

private:
	CAttributeFilter* m_pDependencyFilter = nullptr;
	CFilteredFolders* m_pFilteredFolders = nullptr;
};
