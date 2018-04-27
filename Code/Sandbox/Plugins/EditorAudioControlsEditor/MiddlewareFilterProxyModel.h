// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ProxyModels/AttributeFilterProxyModel.h>

namespace ACE
{
class CMiddlewareFilterProxyModel final : public QAttributeFilterProxyModel
{
public:

	explicit CMiddlewareFilterProxyModel(QObject* const pParent);

	CMiddlewareFilterProxyModel() = delete;

private:

	// QAttributeFilterProxyModel
	virtual bool rowMatchesFilter(int sourceRow, QModelIndex const& sourcePparent) const override;
	// ~QAttributeFilterProxyModel

	// QSortFilterProxyModel
	virtual bool lessThan(QModelIndex const& left, QModelIndex const& right) const override;
	// ~QSortFilterProxyModel
};
} // namespace ACE
