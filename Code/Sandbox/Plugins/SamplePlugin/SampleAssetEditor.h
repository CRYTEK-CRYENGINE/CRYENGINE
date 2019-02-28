// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <AssetSystem/AssetEditor.h>

#include <memory>

class QLineEdit;

//! A sample editor. It handles editing of type "Sample asset type".
//! It shows a single line edit that lets a user edit the data of a sample asset directly.
//! \sa CSampleAssetType
class CSampleAssetEditor : public CAssetEditor
{
public:
	CSampleAssetEditor();

	virtual const char* GetEditorName() const override { return "Sample Asset Editor"; }

	virtual bool OnOpenAsset(CAsset* pAsset) override;
	virtual bool OnSaveAsset(CEditableAsset& editAsset) override;
	virtual void OnCloseAsset() override;

private:
	QLineEdit* m_pLineEdit;
};
