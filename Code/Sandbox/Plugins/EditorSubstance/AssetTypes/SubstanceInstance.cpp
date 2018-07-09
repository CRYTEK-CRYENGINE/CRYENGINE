// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SubstanceInstance.h"
#include "SandboxPlugin.h"

#include <AssetSystem/AssetEditor.h>
#include "EditorSubstanceManager.h"
#include <AssetSystem/EditableAsset.h>
#include <AssetSystem/AssetImportContext.h>
#include <FilePathUtil.h>
#include <ThreadingUtils.h>


namespace EditorSubstance
{
	namespace AssetTypes
	{
		REGISTER_ASSET_TYPE(CSubstanceInstanceType)
		CAssetEditor* CSubstanceInstanceType::Edit(CAsset* asset) const
		{
			return CAssetEditor::OpenAssetForEdit("Substance Instance Editor", asset);
		}

		CryIcon CSubstanceInstanceType::GetIcon() const
		{
			return CryIcon("icons:substance/instance.ico");
		}

		void CSubstanceInstanceType::AppendContextMenuActions(const std::vector<CAsset*>& assets, CAbstractMenu* menu) const
		{
			// todo support more instances slected
			if (assets.size() == 1)
			{
				EditorSubstance::CManager::Instance()->AddSubstanceInstanceContextMenu(assets[0], menu);
			}
		}

		bool CSubstanceInstanceType::OnCreate(CEditableAsset& editAsset, const void* pTypeSpecificParameter) const
		{
			const string relativeFileName = PathUtil::AbsolutePathToGamePath(editAsset.GetAsset().GetName());
			editAsset.SetFiles("",{ relativeFileName + ".crysub" });

			CRY_ASSERT(pTypeSpecificParameter);
			if (pTypeSpecificParameter)
			{
				string& archiveName = *(string*)(pTypeSpecificParameter);
				editAsset.SetDependencies({ {archiveName, 1} });
			}
			return true;
		}

	}
}
