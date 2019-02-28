// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ProxyModels/DeepFilterProxyModel.h>

namespace ACE
{
class CFolderSelectorFilterModel final : public QDeepFilterProxyModel
{
public:

	explicit CFolderSelectorFilterModel(QString const& assetpath, QObject* const pParent);

	CFolderSelectorFilterModel() = delete;

protected:

	// QDeepFilterProxyModel
	virtual bool rowMatchesFilter(int sourceRow, QModelIndex const& sourcePparent) const override;
	// ~QDeepFilterProxyModel

private:

	QString const m_assetPath;
};
} // namespace ACE
