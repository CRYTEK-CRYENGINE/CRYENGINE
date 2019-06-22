// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "AssetSystem/AssetType.h"

class CTextureType : public CAssetType
{
public:
	DECLARE_ASSET_TYPE_DESC(CTextureType);

	virtual const char*                       GetTypeName() const override       { return "Texture"; }
	virtual const char*                       GetUiTypeName() const override     { return QT_TR_NOOP("Texture"); }
	virtual const char*                       GetFileExtension() const override  { return "dds"; }
	virtual bool                              IsImported() const override        { return true; }
	virtual bool                              CanBeCopied() const                { return true; }
	virtual bool                              CanBeEdited() const override       { return true; }
	virtual bool                              HasDerivedFiles() const            { return true; }
	virtual bool                              HasThumbnail() const override      { return true; }
	virtual QColor                            GetThumbnailColor() const override { return QColor(79, 187, 185); }
	virtual std::vector<CItemModelAttribute*> GetDetails() const override;
	virtual QVariant                          GetDetailValue(const CAsset* pAsset, const CItemModelAttribute* pDetail) const override;

	virtual void                              GenerateThumbnail(const CAsset* pAsset) const override;
	virtual QWidget*                          CreateBigInfoWidget(const CAsset* pAsset) const override;
	virtual CAssetEditor*                     Edit(CAsset* asset) const override;
	virtual void                              AppendContextMenuActions(const std::vector<CAsset*>& assets, CAbstractMenu* pMenu) const;

private:
	virtual CryIcon GetIconInternal() const override;

public:
	//! May be referenced by other asset types, too.
	static CItemModelAttribute s_widthAttribute;
	static CItemModelAttribute s_heightAttribute;
	static CItemModelAttribute s_mipCountAttribute;
	static CItemModelAttribute s_parentAssetAttribute;
};
