// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SkinnedMeshType.h"
#include "QT/Widgets/QPreviewWidget.h"

// For shared detail attributes.
#include "MeshType.h"
#include "SkeletonType.h"

#include <AssetSystem/AssetEditor.h>

#include <FilePathUtil.h>
#include <ThreadingUtils.h>

REGISTER_ASSET_TYPE(CSkinnedMeshType)

CAssetEditor* CSkinnedMeshType::Edit(CAsset* asset) const
{
	return CAssetEditor::OpenAssetForEdit("Mesh", asset);
}

CryIcon CSkinnedMeshType::GetIconInternal() const
{
	return CryIcon("icons:common/assets_skin.ico");
}

std::vector<CItemModelAttribute*> CSkinnedMeshType::GetDetails() const
{
	return
	{
		&CMeshType::s_triangleCountAttribute,
		&CMeshType::s_vertexCountAttribute,
		&CMeshType::s_materialCountAttribute,
		&CSkeletonType::s_bonesCountAttribute
	};
}

QVariant CSkinnedMeshType::GetDetailValue(const CAsset* pAsset, const CItemModelAttribute* pDetail) const
{
	if (pDetail == &CMeshType::s_triangleCountAttribute)
	{
		return GetVariantFromDetail(pAsset->GetDetailValue("triangleCount"), pDetail);
	}
	else if (pDetail == &CMeshType::s_vertexCountAttribute)
	{
		return GetVariantFromDetail(pAsset->GetDetailValue("vertexCount"), pDetail);
	}
	else if (pDetail == &CMeshType::s_materialCountAttribute)
	{
		return GetVariantFromDetail(pAsset->GetDetailValue("materialCount"), pDetail);
	}
	else if (pDetail == &CSkeletonType::s_bonesCountAttribute)
	{
		return GetVariantFromDetail(pAsset->GetDetailValue("bonesCount"), pDetail);
	}
	return QVariant();
}

void CSkinnedMeshType::GenerateThumbnail(const CAsset* pAsset) const
{
	ThreadingUtils::PostOnMainThread([=]()
	{
		CMaterialManager* pManager = GetIEditorImpl()->GetMaterialManager();
	if (!pManager)
		return;

	CRY_ASSERT(0 < pAsset->GetFilesCount());
	if (1 > pAsset->GetFilesCount())
		return;

	const char* meshFileName = pAsset->GetFile(0);
	if (!meshFileName || !*meshFileName)
		return;

	const string thumbnailFileName = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), pAsset->GetThumbnailPath());

	static QPreviewWidget* pPreviewWidget = new QPreviewWidget();
	pPreviewWidget->SetGrid(false);
	pPreviewWidget->SetAxis(false);
	pPreviewWidget->EnableMaterialPrecaching(true);
	pPreviewWidget->LoadFile(meshFileName);
	pPreviewWidget->setGeometry(0, 0, ASSET_THUMBNAIL_SIZE, ASSET_THUMBNAIL_SIZE);
	pPreviewWidget->FitToScreen();

		pPreviewWidget->SavePreview(thumbnailFileName.c_str());
	}).get();
}


