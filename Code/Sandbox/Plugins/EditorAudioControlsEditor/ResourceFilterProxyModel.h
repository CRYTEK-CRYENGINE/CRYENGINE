// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"
#include <ProxyModels/DeepFilterProxyModel.h>

namespace ACE
{
class CResourceFilterProxyModel final : public QDeepFilterProxyModel
{
public:

	CResourceFilterProxyModel() = delete;
	CResourceFilterProxyModel(CResourceFilterProxyModel const&) = delete;
	CResourceFilterProxyModel(CResourceFilterProxyModel&&) = delete;
	CResourceFilterProxyModel& operator=(CResourceFilterProxyModel const&) = delete;
	CResourceFilterProxyModel& operator=(CResourceFilterProxyModel&&) = delete;

	explicit CResourceFilterProxyModel(EAssetType const type, QObject* const pParent)
		: QDeepFilterProxyModel(QDeepFilterProxyModel::Behavior::AcceptIfChildMatches, pParent)
		, m_type(type)
	{}

	virtual ~CResourceFilterProxyModel() override = default;

private:

	// QDeepFilterProxyModel
	virtual bool rowMatchesFilter(int sourceRow, QModelIndex const& sourcePparent) const override;
	// ~QDeepFilterProxyModel

	// QSortFilterProxyModel
	virtual bool lessThan(QModelIndex const& left, QModelIndex const& right) const override;
	// ~QSortFilterProxyModel

	EAssetType const m_type;
};
} // namespace ACE
