// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AnimationType.h"

#include <AssetSystem/AssetEditor.h>

REGISTER_ASSET_TYPE(CAnimationType)

CAssetEditor* CAnimationType::Edit(CAsset* asset) const
{
	return CAssetEditor::OpenAssetForEdit("Animation", asset);
}

CryIcon CAnimationType::GetIconInternal() const
{
	return CryIcon("icons:common/assets_animation.ico");
}

