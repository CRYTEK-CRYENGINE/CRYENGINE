// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include "AutoRegister.h"
#include "IEditorClassFactory.h"
#include "CryIcon.h"
#include "ProxyModels/ItemModelAttribute.h"
#include "Asset.h"
#include "CryIcon.h"

#include <CryString/CryString.h>

#include <QColor>
#include <QVariant>

#define ASSET_THUMBNAIL_SIZE 256

class CAbstractMenu;
class CAssetEditor;

//! IAssetMetadata allows to edit asset.
//! \sa CAssetType::OnEdit
struct EDITOR_COMMON_API IEditableAsset
{
	virtual const char* GetMetadataFile() const = 0;
	virtual const char* GetFolder() const = 0;
	virtual const char* GetName() const = 0;

	virtual void        SetSourceFile(const char* szFilepath) = 0;
	virtual void        AddFile(const char* szFilepath) = 0;
	virtual void        SetFiles(const std::vector<string>& filenames) = 0;
	virtual void        AddWorkFile(const char* szFilepath) = 0;
	virtual void        SetWorkFiles(const std::vector<string>& filenames) = 0;
	virtual void        SetDetails(const std::vector<std::pair<string, string>>& details) = 0;
	virtual void        SetDependencies(const std::vector<SAssetDependencyInfo>& dependencies) = 0;

	virtual ~IEditableAsset() {}
};

//! IAssetMetadata allows to assign initial values of a new asset.
//! \sa CAssetType::OnCreate
struct EDITOR_COMMON_API INewAsset : public IEditableAsset
{
	virtual void SetGUID(const CryGUID& guid) = 0;
};

class EDITOR_COMMON_API CAssetType : public IClassDesc
{
public:
	// A base struct for providing type dependent parameters for CAssetType::Create method.
	//! \sa CAssetType::Create
	//! \sa CAssetType::OnCreate
	struct SCreateParams
	{
		//! if not nullprt a copy of the provided asset should be created. The value is sgnored if CanBeCopied() returns false.
		//! \sa CAssetType::CanBeCopied
		CAsset* pSourceAsset = nullptr;
	};

	virtual ESystemClassID SystemClassID() override { return ESYSTEM_CLASS_ASSET_TYPE; }

	void                   Init();

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
	//! \sa CAssetType::SCreateParams().
	virtual bool CanBeCreated() const { return false; }

	//! Returns true if the asset type supports creating a copy of an existing asset.
	//! \sa CAssetType::OnCopy()
	//! \sa CAssetType::SCreateParams().
	//! \sa CAssetType::Create().
	virtual bool CanBeCopied() const { return false; }

	//! Default-initializes an asset. Creates all the necessary asset files and registers the new asset in the asset manager.
	//! This will be used for creating new assets, e.g., in the asset browser or an asset editor.
	//! \param szFilepath Path to the cryasset file that has to be created.
	//! \param pTypeSpecificParameter Pointer to an extra parameter. The default value is nullptr. The actual type of the parameter is specific to the asset type. 
	//! All the asset types should be able to create a default asset instance if the value of pCreateParams is nullptr.
	//! Returns true if the asset was successfully created, false otherwise.
	//! \sa CAssetType::OnCreate()
	//! \sa CAssetManager
	bool Create(const char* szFilepath, const SCreateParams* pCreateParams = nullptr) const;

	//! Can the asset be opened for edit
	virtual bool CanBeEdited() const { return false; }
	//! Opens the asset in the appropriate editor
	//! Usually implemented using CAssetEditor::OpenAssetForEdit
	virtual CAssetEditor* Edit(CAsset* pAsset) const { CRY_ASSERT(0); /*not implemented*/ return nullptr; }

	//! Checks if the given asset is valid
	//! The minimum check that performed is if assets' files are present on the file system.
	virtual bool IsAssetValid(CAsset* pAsset, string& errorMsg) const;

	//! Renames an existing asset.
	//! \param pAsset The asset to be renamed.
	//! \param szNewName The new name for the asset. The asset must not already exist.
	virtual bool RenameAsset(CAsset* pAsset, const char* szNewName) const;

	//! Moves an existing asset to a new folder.
	//! \param pAsset The asset to be moved.
	//! \param szDestinationFolder The new destination folder the asset. Asset with such name must not already exist in the destination folder.
	//! \param bMoveSourcefile If true the asset source file will be moved, otherwise it will be copied.
	virtual bool MoveAsset(CAsset* pAsset, const char* szDestinationFolder, bool bMoveSourcefile) const;

	//! This method is called just before asset's files get removed.
	virtual void PreDeleteAssetFiles(const CAsset& asset) const {}

	//! Returns true if any asset file exists only in paks.
	bool IsInPakOnly(const CAsset& asset) const;

	virtual CryIcon                           GetIcon() const;
	virtual QWidget*                          CreatePreviewWidget(CAsset* pAsset, QWidget* pParent = nullptr) const;

	virtual std::vector<CItemModelAttribute*> GetDetails() const                                                             { return std::vector<CItemModelAttribute*>(); }
	virtual QVariant                          GetDetailValue(const CAsset* pAsset, const CItemModelAttribute* pDetail) const { return QVariant(); }

	//! Returns true if the asset's data files are derived and therefore are located in the cache folder (like *.dds).
	virtual bool HasDerivedFiles() const { return false; }

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
	virtual const char* GetObjectClassName() const                    { return nullptr; }
	virtual string      GetObjectFilePath(const CAsset* pAsset) const { return pAsset->GetFile(0); }

	//! @}

	//! Asset type can add their own menu entries to the AssetBrowser context menu
	//! Only assets of this type will be passed into this callback
	//! If menu entries are added, it is good practice to prefix them by a separator
	//! to indicate which assets will be affected, in the following way:
	//! menu->addAction(new QMenuLabelSeparator("Folders"));
	virtual void         AppendContextMenuActions(const std::vector<CAsset*>& assets, CAbstractMenu* menu) const {}

	std::vector<CAsset*> Import(const string& sourceFilePath, const string& outputDirectory, const string& overrideAssetName = string()) const;

	virtual CryIcon      GetIconInternal() const;

	//! When true, this will register a default asset picker for this type, usable with an archive like this
	//! ar(Serialization::MakeResourceSelector(value, "AssetTypeName"), "value", "Value");
	//! Return false if you want to customize how this type is exposed in the UI it is then up to you to provide a resource picker for this asset type
	virtual bool IsUsingGenericPropertyTreePicker() const { return true; }

	// Returns a collection of paths to all files belonging to the asset.
	// The collection includes the asset metadata, thumbnail and data files. As an option, the collection can also include the asset source file.
	//! \param asset An instance of asset to be examined.
	//! \param includeSourceFile If true, the collection will include the asset's source file, if any.
	//! \param makeAbsolute By default the paths are relative to the assets root directory.
	//! \param includeThumbnail If true, the collection will include the asset's thumbnail file, if any.
	//! \param includeDerived If true, the collection will include the asset's derived files, if any.
	virtual std::vector<string> GetAssetFiles(const CAsset& asset, bool includeSourceFile, bool makeAbsolute = false, bool includeThumbnail = true, bool includeDerived = false) const;

	//! Returns the color code of the thumbnail.
	virtual QColor GetThumbnailColor() const { return QColor(Qt::green); }

	//! Makes cryasset filename from asset name.
	string MakeMetadataFilename(const char* szAssetName) const;

	//! Returns true if the asset type supports automatic generation and repairing missing or broken *.cryasset file. 
	//! The default implementation returns true.
	//! \sa CAssetGenerator::GenerateCryasset
	virtual bool CanAutoRepairMetadata() const { return true; }

	//! Returns true if the path points to a valid location.
	//! \param szFilepath Path to be validated.
	//! \param reasonToReject A return value, which will be filled with a user-friendly description of the reason why the path failed the validation.
	static bool IsValidAssetPath(const char* szFilepath, /*out*/string& reasonToReject);

protected:
	//! Helper function that parses a string and returns a variant of a type corresponding to \p pAttrib->GetType().
	//! If conversion fails, a default-constructed varient of that type is returned. (see QVariant::value).
	static QVariant GetVariantFromDetail(const string& detailValue, const CItemModelAttribute* pAttrib);

private:
	// Override this method if the asset type needs special initialization.
	// \sa CAssetType::Init()
	virtual void OnInit() {}

	//! Fills in the files, details and dependencies fields of the editable asset.
	//! Must be overridden if CanBeCreated() returns true.
	//! \param asset An instance to be filled in.
	//! \param pTypeSpecificParameter Pointer to an extra parameter, can be nullptr.
	//! \sa CAssetType::Create
	virtual bool OnCreate(INewAsset& asset, const SCreateParams* pCreateParams) const { CRY_ASSERT_MESSAGE(false, "not implemented"); return false; }

	//! Fills in the files, details and dependencies fields of the new copy of the asset.
	//! if assetToCopy has an active editing session the default implementation calls IAssetEditingSession::OnCopyAsset, 
	//! in the opposite case the default implementation creates a copy of the asset files with the new asset name.
	//! The default implementation CAssetType::OnCopy() should to be overridden if an asset type needs a special handling. 
	//! For example, if the asset files have a built-in unique identifier that must be re-generated for each copy.
	//! \param asset An instance to be filled in.
	//! \param pAssetToCopy Can not be nullptr.
	//! \sa CAssetType::Create
	//! \sa IAssetEditingSession
	virtual bool OnCopy(INewAsset& asset, CAsset& assetToCopy) const;

	//! Should returns true if the path points to a valid location. The default implementation always returns true.
	//! Should be overridden if the asset type introduces restrictions on the allowable asset locations.
	//! \param szFilepath Path to be validated.
	//! \param reasonToReject The return value, which must be filled with a user-friendly description of the reason why the path did not pass the test. Use QT_TR_NOOP to enable localization.
	virtual bool OnValidateAssetPath(const char* szFilepath, /*out*/string& reasonToReject) const { return true; }

private:
	CryIcon       m_icon;
};

//Helper macro for declaring IClassDesc.
#define DECLARE_ASSET_TYPE_DESC(type)                         \
	virtual const char* ClassName() override { return # type; } \
	virtual void* CreateObject() override { return nullptr; }

#define REGISTER_ASSET_TYPE(type) REGISTER_CLASS_DESC(type)
