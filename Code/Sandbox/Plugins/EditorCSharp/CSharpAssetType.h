// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <AssetSystem/AssetType.h>
#include <AssetSystem/EditableAsset.h>

class CSharpSourcefileAssetType : public CAssetType
{
private:
	struct SCreateParams;

public:
	DECLARE_ASSET_TYPE_DESC(CSharpSourcefileAssetType);

	virtual const char*   GetTypeName() const           { return "CSharpScript"; }
	virtual const char*   GetUiTypeName() const         { return QT_TR_NOOP("C# script"); }
	virtual const char* GetFileExtension() const { return "cs"; }
	virtual bool CanBeCreated() const override { return true; }
	virtual bool IsImported() const { return false; }
	virtual bool CanBeEdited() const { return true; }
	virtual CAssetEditor* Edit(CAsset* asset) const override;

protected:
	virtual bool OnCreate(CEditableAsset& editAsset, const void* pTypeSpecificParameter) const override;

private:
	virtual CryIcon GetIconInternal() const override;
	string GetCleanName(const string& name) const;
};

