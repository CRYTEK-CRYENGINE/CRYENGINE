// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "ProxyModels/MergingProxyModel.h"

//! This model can be used in two mutually exclusive ways:
//! 1) Filtering the global asset model to show only the assets within a set of folders (directly or recursively)
//! 2) Show the folders themselves as elements alongside assets
class CAssetFolderFilterModel : public CMergingProxyModel
{
public:
	CAssetFolderFilterModel(bool recursiveFilter, bool showFolders, QObject* parent = nullptr);

	void SetAcceptedFolders(const QStringList& folders);
	void ClearAcceptedFolders();

	void SetRecursive(bool recursive);
	bool IsRecursive() const { return m_isRecursive; }

	void SetShowFolders(bool showFolders);
	bool IsShowingFolders() const { return m_showFolders; }

private:
	
	bool m_isRecursive;
	bool m_showFolders;
	QStringList m_acceptedFolders;

	//Internally the model will be a merge of a list of folders and a filtered list of assets
	class FoldersModel;
	class FilteredAssetsModel;

	std::unique_ptr<FoldersModel> m_foldersModel;
	std::unique_ptr<FilteredAssetsModel> m_assetsModel;
	std::unique_ptr<FilteredAssetsModel> m_newAssetModel;
};
