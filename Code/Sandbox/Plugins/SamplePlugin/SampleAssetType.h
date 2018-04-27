// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <AssetSystem/AssetType.h>

//! A sample asset type. This type of asset references a single text file storing a string.
//! \sa CSampleAssetEditor
class CSampleAssetType : public CAssetType
{
public:
	DECLARE_ASSET_TYPE_DESC(CSampleAssetType);

	virtual const char* GetTypeName() const { return "SampleAssetType"; }
	virtual const char* GetUiTypeName() const { return "Sample Asset Type"; }
	virtual const char* GetFileExtension() const { return "smpl"; }
	virtual bool CanBeCreated() const { return true; }
	virtual bool CanBeEdited() const { return true; }
	virtual CAssetEditor* Edit(CAsset* pAsset) const override;

	virtual bool OnCreate(CEditableAsset& editAsset, const void* pCreateParams) const override;
};
