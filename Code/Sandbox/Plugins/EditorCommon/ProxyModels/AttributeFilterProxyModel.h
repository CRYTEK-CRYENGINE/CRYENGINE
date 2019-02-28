// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"

#include <QSortFilterProxyModel>

#include <CrySandbox/CrySignal.h>

#include "ProxyModels/DeepFilterProxyModel.h"
#include "ProxyModels/ItemModelAttribute.h"

typedef std::map<CItemModelAttribute*, int> AttributeIndexMap;

//! Proxy model class to filter based on attributes. Used with QFilteringPanel
// Assumes the source model's columns are stable
class EDITOR_COMMON_API QAttributeFilterProxyModel : public QDeepFilterProxyModel
{
public:
	QAttributeFilterProxyModel(BehaviorFlags behavior = AcceptIfChildMatches, QObject* pParent = nullptr, int role = Attributes::s_getAttributeRole);
	virtual ~QAttributeFilterProxyModel();
	virtual void setSourceModel(QAbstractItemModel* pSourceModel) override;
	void         SetAttributeRole(int role);
	void         AddFilter(AttributeFilterSharedPtr pFilter);
	void         RemoveFilter(AttributeFilterSharedPtr pFilter);
	void         ClearFilters();

	void         InvalidateFilter() { QDeepFilterProxyModel::invalidateFilter(); }

	CCrySignal<void()> signalAttributesChanged;

protected:
	virtual bool rowMatchesFilter(int sourceRow, const QModelIndex& sourceParent) const override;

private:
	void ResetAttributes();

	void OnColumnsRemoved(const QModelIndex& parent, int first, int last);
	void OnColumnsInserted(const QModelIndex& parent, int first, int last);
	void OnColumnsMoved(const QModelIndex& parent, int start, int end, const QModelIndex& destination, int column);

	friend class QFilteringPanel;

	int                                   m_role;
	AttributeIndexMap                     m_attributes;

protected:
	std::vector<AttributeFilterSharedPtr> m_filters;
};

