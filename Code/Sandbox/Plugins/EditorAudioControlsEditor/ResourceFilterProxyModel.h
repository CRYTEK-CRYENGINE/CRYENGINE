// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ProxyModels/DeepFilterProxyModel.h>
#include <SharedData.h>

namespace ACE
{
class CResourceFilterProxyModel final : public QDeepFilterProxyModel
{
public:

	explicit CResourceFilterProxyModel(EAssetType const type, Scope const scope, QObject* const pParent);

	CResourceFilterProxyModel() = delete;

private:

	// QDeepFilterProxyModel
	virtual bool rowMatchesFilter(int sourceRow, QModelIndex const& sourcePparent) const override;
	// ~QDeepFilterProxyModel

	// QSortFilterProxyModel
	virtual bool lessThan(QModelIndex const& left, QModelIndex const& right) const override;
	// ~QSortFilterProxyModel

	EAssetType const m_type;
	Scope const      m_scope;
};
} // namespace ACE
