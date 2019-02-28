// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <AssetSystem/AssetType.h>

namespace EditorSubstance
{
	namespace AssetTypes
	{

		class CSubstanceInstanceType : public CAssetType
		{
		public:
			DECLARE_ASSET_TYPE_DESC(CSubstanceInstanceType);

			virtual const char* GetTypeName() const override { return "SubstanceInstance"; }
			virtual const char* GetUiTypeName() const override { return QT_TR_NOOP("Substance Instance"); }
			virtual bool IsImported() const override { return false; }
			virtual bool CanBeEdited() const override { return true; }
			virtual CAssetEditor* Edit(CAsset* asset) const override;

			virtual CryIcon GetIcon() const override;
			virtual bool HasThumbnail() const override { return false; }
			virtual const char* GetFileExtension() const override { return "crysub"; }
			virtual void AppendContextMenuActions(const std::vector<CAsset*>& assets, CAbstractMenu* menu) const override;
			virtual bool OnCreate(CEditableAsset& editAsset, const void* pTypeSpecificParameter) const override;

		};
	}
}
