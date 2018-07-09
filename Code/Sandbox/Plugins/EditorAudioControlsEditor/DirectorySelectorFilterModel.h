// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ProxyModels/DeepFilterProxyModel.h>

namespace ACE
{
class CDirectorySelectorFilterModel final : public QDeepFilterProxyModel
{
public:

	explicit CDirectorySelectorFilterModel(QString const& assetpath, QObject* const pParent);

	CDirectorySelectorFilterModel() = delete;

protected:

	// QDeepFilterProxyModel
	virtual bool rowMatchesFilter(int sourceRow, QModelIndex const& sourcePparent) const override;
	// ~QDeepFilterProxyModel

private:

	QString const m_assetPath;
};
} // namespace ACE
