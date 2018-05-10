// Copyright 2001-2016 Crytek GmbH. All rights reserved.
#pragma once

#include "AssetSystem/AssetType.h"

class CGeometryCacheType : public CAssetType
{
public:
	DECLARE_ASSET_TYPE_DESC(CGeometryCacheType);

	virtual const char* GetTypeName() const override       { return "GeometryCache"; }
	virtual const char* GetUiTypeName() const override     { return QT_TR_NOOP("Geometry Cache"); }
	virtual const char* GetFileExtension() const override  { return "cax"; }
	virtual bool        IsImported() const override        { return false; }
	virtual bool        CanBeEdited() const override       { return false; }
	virtual QColor      GetThumbnailColor() const override { return QColor(210, 75, 64); }

private:
	virtual CryIcon GetIconInternal() const override;
};

