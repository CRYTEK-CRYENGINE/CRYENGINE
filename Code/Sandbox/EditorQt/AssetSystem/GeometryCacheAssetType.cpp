// Copyright 2001-2016 Crytek GmbH. All rights reserved.
#include <StdAfx.h>
#include "GeometryCacheAssetType.h"

REGISTER_ASSET_TYPE(CGeometryCacheType)

CryIcon CGeometryCacheType::GetIconInternal() const
{
	return CryIcon("icons:common/assets_geomcache.ico");
}

