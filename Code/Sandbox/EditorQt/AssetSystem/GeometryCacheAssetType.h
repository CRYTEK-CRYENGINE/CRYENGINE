// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "AssetSystem/AssetType.h"

class CGeometryCacheType : public CAssetType
{
public:
	DECLARE_ASSET_TYPE_DESC(CGeometryCacheType)

	virtual const char*   GetTypeName() const override      { return GetTypeNameStatic(); }
	virtual const char*   GetUiTypeName() const override    { return QT_TR_NOOP("Geometry Cache"); }
	virtual const char*   GetFileExtension() const override { return "cax"; }
	virtual bool          IsImported() const override       { return true; }
	virtual bool          CanBeEdited() const override      { return true; }
	virtual CAssetEditor* Edit(CAsset* pAsset) const override;

	static const char*    GetTypeNameStatic() { return "GeometryCache"; }

private:
	virtual CryIcon GetIconInternal() const override;
};

