// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <AssetSystem/AssetType.h>

namespace CrySchematycEditor {

// TODO: Move this out of Schematyc because this should be a more generic library.
class CLibraryAssetType : public CAssetType
{
public:
	DECLARE_ASSET_TYPE_DESC(CLibraryAssetType);

	static const char* TypeName() { return "SchematycLibrary"; }

	// CAssetType
	virtual const char*   GetTypeName() const override      { return TypeName(); }
	virtual const char*   GetUiTypeName() const override    { return QT_TR_NOOP("Schematyc Library"); }

	virtual const char*   GetFileExtension() const override { return "schematyc_lib"; }

	virtual bool          CanBeCreated() const override     { return true; }
	virtual bool          IsImported() const override       { return false; }
	virtual bool          CanBeEdited() const override      { return true; }

	virtual CAssetEditor* Edit(CAsset* pAsset) const override;
	virtual bool          RenameAsset(CAsset* pAsset, const char* szNewName) const override;
	virtual bool          DeleteAssetFiles(const CAsset& asset, bool bDeleteSourceFile, size_t& numberOfFilesDeleted) const override;

protected:
	virtual bool OnCreate(CEditableAsset& editAsset, const void* pTypeSpecificParameter) const override;

private:
	virtual CryIcon GetIconInternal() const override;
	// ~CAssetType

	Schematyc::IScript* GetScript(const CAsset& asset) const;
};
// ~TODO

}

