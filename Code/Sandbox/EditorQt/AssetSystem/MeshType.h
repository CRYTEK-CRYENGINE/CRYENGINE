// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <AssetSystem/AssetType.h>

// Mesh
// Contain geometry data(grouped triangles, vertex attributes like tangent space or vertex color, optional physics data).
class CMeshType : public CAssetType
{
public:
	DECLARE_ASSET_TYPE_DESC(CMeshType);

	virtual const char*                       GetTypeName() const override { return "Mesh"; }
	virtual const char*                       GetUiTypeName() const override { return QT_TR_NOOP("Mesh"); }
	virtual const char* GetFileExtension() const override { return "cgf"; }
	virtual bool IsImported() const override { return true; }
	virtual bool CanBeEdited() const override { return true; }
	virtual CAssetEditor* Edit(CAsset* asset) const override;
	virtual bool HasThumbnail() const override { return true; }
	virtual void GenerateThumbnail(const CAsset* pAsset) const override;
	virtual const char* GetObjectClassName() const { return "Brush"; }
	virtual std::vector<CItemModelAttribute*> GetDetails() const override;
	virtual QVariant                          GetDetailValue(const CAsset* pAsset, const CItemModelAttribute* pDetail) const override;
	virtual bool MoveAsset(CAsset* pAsset, const char* szNewPath, bool bMoveSourcefile) const override;

private:
	virtual CryIcon GetIconInternal() const override;

public:
	//! May be referenced by other asset types, too.
	static CItemModelAttribute s_vertexCountAttribute;
	static CItemModelAttribute s_triangleCountAttribute;
	static CItemModelAttribute s_materialCountAttribute;
};

