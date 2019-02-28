// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Controls/EditorDialog.h>

#include <QWidget>

class CAssetImportContext;
class CAssetType;

//! Dialog that shows "Import" and "Import all" buttons, and the content of the drop importer.
class CAssetImportDialog : public CEditorDialog
{
public:
	CAssetImportDialog(
		CAssetImportContext& context,
		const std::vector<CAssetType*>& assetTypes,
		const std::vector<bool>& assetTypeSelection,
		const std::vector<string>& allFilePaths,
		QWidget* pParent = nullptr);

	std::vector<bool> GetAssetTypeSelection() const;

private:
	QWidget* CreateContentWidget(const std::vector<CAssetType*>& assetTypes);

private:
	CAssetImportContext& m_context;

	std::vector<bool> m_assetTypeSelection;
};

