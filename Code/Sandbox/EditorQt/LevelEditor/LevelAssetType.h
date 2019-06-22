// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "AssetSystem/AssetType.h"
#include "CryString/CryString.h"
#include <future>

class CLevelType : public CAssetType
{
	friend class CLevelEditor;
public:
	//! \sa CLevelType::OnCreate
	struct SLevelCreateParams : SCreateParams
	{
		int   resolution;
		float unitSize;   // size in Meters = resolution * unitSize
		bool  bUseTerrain;

		struct STexture
		{
			int   resolution;
			float colorMultiplier;
			bool  bHighQualityCompression;
		} texture;
	};

public:
	DECLARE_ASSET_TYPE_DESC(CLevelType)

	virtual const char* GetTypeName() const override { return "Level"; }
	virtual const char*         GetUiTypeName() const override                                                        { return QT_TR_NOOP("Level"); }
	virtual const char*         GetFileExtension() const override                                                     { return GetFileExtensionStatic(); }
	virtual bool                IsImported() const override                                                           { return false; }
	virtual bool                CanBeCreated() const override                                                         { return true; }
	virtual bool                CanAutoRepairMetadata() const override                                                { return false; } //! Levels is a special case when the cryasset is next to the level folder.
	virtual bool                CanBeEdited() const override                                                          { return true; }
	virtual QColor              GetThumbnailColor() const override                                                    { return QColor(230, 230, 230); }
	virtual void                PreDeleteAssetFiles(const CAsset& asset) const override;
	virtual bool                RenameAsset(CAsset* pAsset, const char* szNewName) const override                     { return false; }
	virtual bool                MoveAsset(CAsset* pAsset, const char* szNewPath, bool bMoveSourcefile) const override { return false; }
	virtual CAssetEditor*       Edit(CAsset* pAsset) const override;
	virtual std::vector<string> GetAssetFiles(const CAsset& asset, bool includeSourceFile, bool makeAbsolute, bool includeThumbnail = true, bool includeDerived = false) const override;
	virtual bool                OnValidateAssetPath(const char* szFilepath, /*out*/ string& reasonToReject) const override;

	static const char*          GetFileExtensionStatic() { return "level"; }

	//! Makes level filename from asset name.
	static string MakeLevelFilename(const char* szAssetName);

private:
	virtual CryIcon GetIconInternal() const override;

	static void     UpdateDependencies(IEditableAsset& editAsset);
	static void     UpdateFiles(IEditableAsset& editAsset);

protected:
	//! \sa CLevelType::SCreateParams
	virtual bool OnCreate(INewAsset& asset, const SCreateParams* pCreateParams) const override;

	static void  UpdateFilesAndDependencies(IEditableAsset& editAsset);

protected:
	mutable std::future<bool> m_asyncAction;
};
