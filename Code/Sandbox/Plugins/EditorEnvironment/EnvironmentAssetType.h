// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <AssetSystem/AssetType.h>

// Environment is an asset type that is used to setup most of the aspects of the environment, such as fog, volumetric clouds and the direction and position of the sun in a 24 - hour range.
class CEnvironmentAssetType : public CAssetType
{
public:
	DECLARE_ASSET_TYPE_DESC(CEnvironmentAssetType);

private:
	virtual const char*   GetTypeName() const override       { return "Environment"; }
	virtual const char*   GetUiTypeName() const override     { return QT_TR_NOOP("Environment"); }
	virtual const char*   GetFileExtension() const override  { return "env"; }
	virtual bool          IsImported() const override        { return false; }
	virtual bool          CanBeCreated() const               { return true; }
	virtual bool          CanBeCopied() const                { return true; }
	virtual bool          CanBeEdited() const override       { return true; }
	virtual QColor        GetThumbnailColor() const override { return QColor(255, 201, 14); }
	virtual const char*   GetObjectClassName() const         { return GetTypeName(); }
	virtual void          AppendContextMenuActions(const std::vector<CAsset*>& assets, CAbstractMenu* pMenu) const;

	virtual void          OnInit() override;
	virtual bool          OnCreate(INewAsset& asset, const SCreateParams* pCreateParams) const;
	virtual CAssetEditor* Edit(CAsset* pAsset) const override;
	virtual CryIcon       GetIconInternal() const override;
};
