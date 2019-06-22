// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "CrySandbox\CrySignal.h"

#include <QStringList>

class CAsset;
class CAssetFolderFilterModel;
class QAttributeFilterProxyModel;

// Maintains a list of non-empty folders in the context of asset model filtering options.
class CFilteredFolders
{
public:
	CFilteredFolders(CAssetFolderFilterModel* pFolderFilterModel, QAttributeFilterProxyModel* pAttributeFilterModel)
		: m_pFolderFilterModel(pFolderFilterModel)
		, m_pAttributeFilterModel(pAttributeFilterModel)
	{
	}

	bool IsEmpty() const {	return m_nonEmptyFolders.empty(); }

	bool Contains(const QString& folderPath) const
	{
		auto it = std::lower_bound(m_nonEmptyFolders.cbegin(), m_nonEmptyFolders.cend(), folderPath);
		return it != m_nonEmptyFolders.cend() && (*it).startsWith(folderPath);
	}

	const QStringList& GetList() const { return m_nonEmptyFolders; }

	void Update(bool enable);

	void FillNonEmptyFoldersList();

	CCrySignal<void()> signalInvalidate;

private:
	CAssetFolderFilterModel*    m_pFolderFilterModel;
	QAttributeFilterProxyModel* m_pAttributeFilterModel;
	QStringList                 m_nonEmptyFolders;

	// a preallocated container for the performance optimization.
	std::vector<const CAsset*> m_filteredAssets;
};
