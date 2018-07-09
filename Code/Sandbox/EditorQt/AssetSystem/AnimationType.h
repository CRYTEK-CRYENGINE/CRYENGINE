// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <AssetSystem/AssetType.h>

// Character Animation
// Compressed representation of the i_caf format. This format uses lossy compression and such files are created either by Sandbox or during the asset build build.Such files are loaded by the engine at runtime.
class CAnimationType : public CAssetType
{
public:
	DECLARE_ASSET_TYPE_DESC(CAnimationType);

	virtual const char*   GetTypeName() const override { return "Animation"; }
	virtual const char*   GetUiTypeName() const override { return QT_TR_NOOP("Animation"); }
	virtual const char*   GetFileExtension() const override { return "caf"; }
	virtual bool          IsImported() const override { return true; }
	virtual bool          CanBeEdited() const override { return true; }
	virtual CAssetEditor* Edit(CAsset* asset) const override;
	
private:
	virtual CryIcon GetIconInternal() const override;
};

