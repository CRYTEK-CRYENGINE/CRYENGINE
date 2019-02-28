// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include "AutoRegister.h"
#include "IEditorClassFactory.h"
#include "CryIcon.h"
#include "ProxyModels/ItemModelAttribute.h"
#include "Asset.h"
#include "AssetEditor.h"
#include "EditableAsset.h"
#include "CryIcon.h"

#include <CryString/CryString.h>

#include <QVariant>

#define ASSET_THUMBNAIL_SIZE 256

class CAbstractMenu;

class EDITOR_COMMON_API CAssetType : public IClassDesc
{
public:
	virtual ESystemClassID SystemClassID() override { return ESYSTEM_CLASS_ASSET_TYPE; }

	void Init();

	//! Returns the asset type name. DO NOT CHANGE THIS as this will be serialized to disk and used to identify asset types
	//! Do not use for UI purposes
	virtual const char* GetTypeName() const = 0;

	//! Ui friendly name. Use QT_TR_NOOP to enable localization
	virtual const char* GetUiTypeName() const = 0;

	//! Returns file extension for this asset type. File extension is lower case and without leading dot.
	//! A file extension must not end with ".cryasset", since this suffix is added to the extension
	//! when writing asset meta-data files to disk.
	//! File extensions must be unique among all asset types.
	//! Asset type file extensions are validated in the asset manager.
	virtual const char* GetFileExtension() const = 0;

	//! Can a new asset be created?
	//! Creating a new asset means default-initializing it.
	//! Is it assumed an asset can either be imported or created.
	//! \sa CAssetType::Create().
	virtual bool CanBeCreated() const { return false; }

	//! Default-initializes an asset. Creates all the necessary asset files.
	//! This will be used for creating new assets, e.g., in the asset browser or an asset editor.
	//! \param szFilepath Path to the cryasset file that has to be created.
	//! \param pTypeSpecificParameter Pointer to an extra parameter. The default value is nullprt. The actual type of the parameter is specific to the asset type. 
	//! All the asset types should be able to create a default asset instance if the value of pTypeSpecificParameter is nullptr.
	//! \sa CAssetType::OnCreate()
	bool Create(const char* szFilepath, const void* pTypeSpecificParameter = nullptr) const;

	//! Can the asset be opened for edit
	virtual bool CanBeEdited() const { return false; }
	//! Opens the asset in the appropriate editor
	//! Usually implemented using CAssetEditor::OpenAssetForEdit
	virtual CAssetEditor* Edit(CAsset* asset) const { CRY_ASSERT(0); /*not implemented*/ return nullptr; }

	//! Renames an existing asset.
	//! \param pAsset The asset to be renamed.
	//! \param szNewName The new name for the asset. The asset must not already exist.
	virtual bool RenameAsset(CAsset* pAsset, const char* szNewName) const;

	//! Moves an existing asset to a new folder.
	//! \param pAsset The asset to be moved.
	//! \param szDestinationFolder The new destination folder the asset. Asset with such name must not already exist in the destination folder.
	//! \param bMoveSourcefile If true the asset source file will be moved, otherwise it will be copied.
	virtual bool MoveAsset(CAsset* pAsset, const char* szDestinationFolder, bool bMoveSourcefile) const;

	//! Copies an existing asset to a new asset.
	//! \param pAsset The asset to copy.
	//! \param szNewName The new path and name for the asset. Asset with such name must not already exist in the destination folder.
	virtual bool CopyAsset(CAsset* pAsset, const char* szNewPath) const;

	//! Removes all the asset files including the .cryasset file.
	//! You must remove the read-only file attributes to delete read-only files.
	//! \param bDeleteSourceFile a boolean value, if true the asset source file will be deleted.
	//! \param numberOfFilesDeleted The number of files that were deleted. The asset is invalid if at least one file was deleted.
	//! \return True true if the asset files were deleted. False on errors.
	virtual bool DeleteAssetFiles(const CAsset& asset, bool bDeleteSourceFile, size_t& numberOfFilesDeleted ) const;

	virtual CryIcon GetIcon() const;
	virtual QWidget* CreatePreviewWidget(CAsset* pAsset, QWidget* pParent = nullptr) const;

	virtual std::vector<CItemModelAttribute*> GetDetails() const { return std::vector<CItemModelAttribute*>(); }
	virtual QVariant GetDetailValue(const CAsset* pAsset, const CItemModelAttribute* pDetail) const { return QVariant(); }

	//! Returns true if the asset may have a thumbnail
	virtual bool HasThumbnail() const { return false; }

	//! Generates the thumbnail next to the asset metafile
	//! This empty implementation shoud never be called when HasThumbnail() returns true
	virtual void GenerateThumbnail(const CAsset* pAsset) const { CRY_ASSERT(0); }
	//! @name Importing
	//! @{

	//! Creates a big thumbnail for display in the "big" asset tooltip (hold space when the asset tooltip is shown)
	//! Example usage is to display more information than the regular asset tooltip, or to display a full size texture
	virtual QWidget* CreateBigInfoWidget(const CAsset* pAsset) const { return nullptr; }

	//! Is it assumed an asset type has one of the two following behaviors
	//! 1) Imported from a source file and can only be created through import
	//! 2) Created within the Sandbox and never has a source file
	//! If returning true, it will be assumed the other import methods are implemented correctly
	//! If returning false, it will be assumed Create() and Edit() are implemented correctly
	virtual bool IsImported() const { return false; }

	//! @}

	//! @name Create instances.
	//! @{

	//! Returns the name of an object class description that is used to create editor objects for this asset; or nullptr, if no such class exists.
	//! @see CObjectClassDesc
	//! @see CBaseObject
	virtual const char* GetObjectClassName() const { return nullptr; }
	virtual string GetObjectFilePath(const CAsset* pAsset) const { return pAsset->GetFile(0); }

	//! @}

	//! Asset type can add their own menu entries to the AssetBrowser context menu
	//! Only assets of this type will be passed into this callback
	//! If menu entries are added, it is good practice to prefix them by a separator 
	//! to indicate which assets will be affected, in the following way:
	//! menu->addAction(new QMenuLabelSeparator("Folders"));
	virtual void AppendContextMenuActions(const std::vector<CAsset*>& assets, CAbstractMenu* menu) const {};

	std::vector<CAsset*> Import(const string& sourceFilePath, const string& outputDirectory, const string& overrideAssetName = string()) const;

	virtual CryIcon GetIconInternal() const;

	//! When true, this will register a default asset picker for this type, usable with an archive like this
	//! ar(Serialization::MakeResourceSelector(value, "AssetTypeName"), "value", "Value");
	//! Return false if you want to customize how this type is exposed in the UI it is then up to you to provide a resource picker for this asset type
	virtual bool IsUsingGenericPropertyTreePicker() const { return true; }

	// Returns a collection of paths to all files belonging to the asset.
	// The collection includes the asset metadata, thumbnail and data files. As an option, the collection can also include the asset source file.
	//! \param asset An instance of asset to be examined.
	//! \param includeSourceFile If true, the collection will include the asset source file, if any.
	//! \param makeAbsolute By default the paths are relative to the assets root directory.
	virtual std::vector<string> GetAssetFiles(const CAsset& asset, bool includeSourceFile, bool makeAbsolute = false) const;

protected:
	//! Helper function that parses a string and returns a variant of a type corresponding to \p pAttrib->GetType().
	//! If conversion fails, a default-constructed varient of that type is returned. (see QVariant::value).
	static QVariant GetVariantFromDetail(const string& detailValue, const CItemModelAttribute* pAttrib);

private:
	//! Fills in the files, details and dependencies fields of the editable asset.
	//! Must be overridden if CanBeCreated() returns true. 
	//! \param editAsset An instance to be filled in.
	//! \param pTypeSpecificParameter Pointer to an extra parameter, can be nullptr. 
	//! \sa CAssetType::Create
	virtual bool OnCreate(CEditableAsset& editAsset, const void* pTypeSpecificParameter) const { CRY_ASSERT(0); /*not implemented*/ return false; }

private:
	CryIcon m_icon;
};

//Helper macro for declaring IClassDesc.
#define DECLARE_ASSET_TYPE_DESC(type)                           \
  virtual const char* ClassName() override { return # type; } \
  virtual void* CreateObject() override { return nullptr; }

#define REGISTER_ASSET_TYPE(type) REGISTER_CLASS_DESC(type)

