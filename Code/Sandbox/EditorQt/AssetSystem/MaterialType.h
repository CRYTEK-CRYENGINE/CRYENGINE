// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <AssetSystem/AssetType.h>
#include <QScopedPointer>

class QPreviewWidget;

// Material
// Contains settings for shaders, surface types, and references to textures. The .mtl file is a text file which holds all the information for the in - game material library.The material library is a collection of sub materials which can be assigned to each face of a geometry.You can have for example different surfaces like metal, plastic, humanskin within different IDs of the asset.Each of these sub materials can use different shaders and different textures.
class CMaterialType : public CAssetType
{
public:
	DECLARE_ASSET_TYPE_DESC(CMaterialType);

	virtual const char*                       GetTypeName() const override { return "Material"; }
	virtual const char*                       GetUiTypeName() const override { return QT_TR_NOOP("Material"); }
	virtual const char* GetFileExtension() const override { return "mtl"; }
	virtual bool CanBeEdited() const override { return true; }
	virtual bool CanBeCreated() const override { return true; }
	virtual bool HasThumbnail() const override { return true; }
	virtual void GenerateThumbnail(const CAsset* pAsset) const override;
	virtual std::vector<CItemModelAttribute*> GetDetails() const override;
	virtual QVariant                          GetDetailValue(const CAsset* pAsset, const CItemModelAttribute* pDetail) const override;

	virtual CAssetEditor* Edit(CAsset* pAsset) const override;
	virtual bool OnCreate(CEditableAsset& editAsset, const void* pTypeSpecificParameter) const override;
private:
	virtual CryIcon GetIconInternal() const override;

private:
	mutable QScopedPointer<QPreviewWidget> m_pPreviewWidget;

	//! May be referenced by other asset types, too.
	static CItemModelAttribute s_subMaterialCountAttribute;
	static CItemModelAttribute s_textureCountAttribute;
};

