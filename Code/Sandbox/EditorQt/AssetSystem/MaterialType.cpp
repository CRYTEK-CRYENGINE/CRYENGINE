// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MaterialType.h"

#include <AssetSystem/Asset.h>
#include <FilePathUtil.h>
#include <ThreadingUtils.h>
#include "QT/Widgets/QPreviewWidget.h"
#include "Material/MaterialManager.h"
#include <Cry3DEngine/I3DEngine.h>

REGISTER_ASSET_TYPE(CMaterialType)

// Detail attributes.
CItemModelAttribute CMaterialType::s_subMaterialCountAttribute("Sub Mtl count", eAttributeType_Int, CItemModelAttribute::StartHidden);
CItemModelAttribute CMaterialType::s_textureCountAttribute("Texture count", eAttributeType_Int, CItemModelAttribute::StartHidden);

CryIcon CMaterialType::GetIconInternal() const
{
	return CryIcon("icons:common/assets_material.ico");
}

void CMaterialType::GenerateThumbnail(const CAsset* pAsset) const
{
	ThreadingUtils::PostOnMainThread([=]()
	{
		CMaterialManager* pManager = GetIEditorImpl()->GetMaterialManager();
		if (!pManager)
			return;

		CRY_ASSERT(0 < pAsset->GetFilesCount());
		if (1 > pAsset->GetFilesCount())
			return;

		const char* materialFileName = pAsset->GetFile(0);
		if (!materialFileName || !*materialFileName)
			return;

		CMaterial* pMaterial = pManager->LoadMaterial(materialFileName);
		if (!pMaterial)
			return;

		if (!m_pPreviewWidget)
		{
			m_pPreviewWidget.reset(new QPreviewWidget());
			m_pPreviewWidget->SetGrid(false);
			m_pPreviewWidget->SetAxis(false);
			m_pPreviewWidget->EnableMaterialPrecaching(true);
			m_pPreviewWidget->LoadFile("%EDITOR%/Objects/mtlsphere.cgf");
			m_pPreviewWidget->SetCameraLookAt(1.6f, Vec3(0.1f, -1.0f, -0.1f));
			m_pPreviewWidget->setGeometry(0, 0, ASSET_THUMBNAIL_SIZE, ASSET_THUMBNAIL_SIZE);
		}
		m_pPreviewWidget->SetMaterial(pMaterial);

		const string thumbnailFileName = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), pAsset->GetThumbnailPath());

		m_pPreviewWidget->SavePreview(thumbnailFileName.c_str());
	}).get();
}

std::vector<CItemModelAttribute*> CMaterialType::GetDetails() const
{
	return
	{
		&s_subMaterialCountAttribute,
		&s_textureCountAttribute
	};
}

QVariant CMaterialType::GetDetailValue(const CAsset * pAsset, const CItemModelAttribute * pDetail) const
{
	if (pDetail == &s_subMaterialCountAttribute)
	{
		return GetVariantFromDetail(pAsset->GetDetailValue("subMaterialCount"), pDetail);
	}
	else if (pDetail == &s_textureCountAttribute)
	{
		return GetVariantFromDetail(pAsset->GetDetailValue("textureCount"), pDetail);
	}
	return QVariant();
}

CAssetEditor* CMaterialType::Edit(CAsset* pAsset) const
{
	if (!pAsset->GetFilesCount())
	{
		return nullptr;
	}

	return CAssetEditor::OpenAssetForEdit("Material Editor", pAsset);
}

bool CMaterialType::OnCreate(CEditableAsset& editAsset, const void* pTypeSpecificParameter) const
{
	string materialName;
	materialName.Format("%s%s", editAsset.GetAsset().GetFolder(), editAsset.GetAsset().GetName());
	CMaterial* newMaterial = GetIEditor()->GetMaterialManager()->CreateMaterial(materialName);
	CRY_ASSERT(newMaterial->Save());

	editAsset.AddFile(newMaterial->GetFilename(true));

	return true;
}

