// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "AssetSystem/AssetType.h"

class CTextureType : public CAssetType
{
public:
	DECLARE_ASSET_TYPE_DESC(CTextureType);

	virtual const char*                       GetTypeName() const override { return "Texture"; }
	virtual const char*                       GetUiTypeName() const override { return QT_TR_NOOP("Texture"); }
	virtual const char* GetFileExtension() const override { return "dds"; }
	virtual bool IsImported() const override { return true; }
	virtual bool CanBeEdited() const override { return true; }
	virtual bool HasThumbnail() const override { return true; }
	virtual void GenerateThumbnail(const CAsset* pAsset) const override;
	virtual QWidget* CreateBigInfoWidget(const CAsset* pAsset) const override;
	virtual CAssetEditor* Edit(CAsset* asset) const override;
	virtual std::vector<CItemModelAttribute*> GetDetails() const override;
	virtual QVariant                          GetDetailValue(const CAsset* pAsset, const CItemModelAttribute* pDetail) const override;

private:
	virtual CryIcon GetIconInternal() const override;

public:
	//! May be referenced by other asset types, too.
	static CItemModelAttribute s_widthAttribute;
	static CItemModelAttribute s_heightAttribute;
	static CItemModelAttribute s_mipCountAttribute;
};

