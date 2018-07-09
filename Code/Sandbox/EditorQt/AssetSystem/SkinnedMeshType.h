// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <AssetSystem/AssetType.h>

// Skinned Render mesh
// Contains skinned character data. It can be any asset that is animated with bone - weighted vertices like humans, aliens, ropes, lamps, heads, and parachutes.The.skin file includes the mesh, vertex weighting, vertex colors, and morph targets.
class CSkinnedMeshType : public CAssetType
{
public:
	DECLARE_ASSET_TYPE_DESC(CSkinnedMeshType);

	virtual const char*                       GetTypeName() const override { return "SkinnedMesh"; }
	virtual const char*                       GetUiTypeName() const override { return QT_TR_NOOP("Skinned Mesh"); }
	virtual const char* GetFileExtension() const override { return "skin"; }
	virtual bool IsImported() const override { return true; }
	virtual bool CanBeEdited() const override { return true; }
	virtual CAssetEditor* Edit(CAsset* asset) const override;
	virtual bool HasThumbnail() const override { return false; }
	virtual void GenerateThumbnail(const CAsset* pAsset) const override;
	virtual std::vector<CItemModelAttribute*> GetDetails() const override;
	virtual QVariant GetDetailValue(const CAsset* pAsset, const CItemModelAttribute* pDetail) const override;

private:
	virtual CryIcon GetIconInternal() const override;
};

