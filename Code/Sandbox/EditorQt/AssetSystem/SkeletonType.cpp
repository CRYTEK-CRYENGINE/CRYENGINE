// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SkeletonType.h"
#include "QT/Widgets/QPreviewWidget.h"
#include "FilePathUtil.h"
#include <ThreadingUtils.h>

#include <AssetSystem/AssetEditor.h>

#include <ProxyModels/ItemModelAttribute.h>

REGISTER_ASSET_TYPE(CSkeletonType)

// Detail attributes.
CItemModelAttribute CSkeletonType::s_bonesCountAttribute("Bones count", eAttributeType_Int, CItemModelAttribute::StartHidden);

CAssetEditor* CSkeletonType::Edit(CAsset* asset) const
{
	return CAssetEditor::OpenAssetForEdit("Skeleton", asset);
}

CryIcon CSkeletonType::GetIconInternal() const
{
	return CryIcon("icons:common/assets_skeleton.ico");
}

std::vector<CItemModelAttribute*> CSkeletonType::GetDetails() const
{
	return
	{
		&s_bonesCountAttribute
	};
}

QVariant CSkeletonType::GetDetailValue(const CAsset* pAsset, const CItemModelAttribute* pDetail) const
{
	if (pDetail == &s_bonesCountAttribute)
	{
		return GetVariantFromDetail(pAsset->GetDetailValue("bonesCount"), pDetail);
	}
	return QVariant();
}

void CSkeletonType::GenerateThumbnail(const CAsset* pAsset) const
{
	ThreadingUtils::PostOnMainThread([=]()
	{
		CMaterialManager* pManager = GetIEditorImpl()->GetMaterialManager();
	if (!pManager)
		return;

	CRY_ASSERT(0 < pAsset->GetFilesCount());
	if (1 > pAsset->GetFilesCount())
		return;

	const char* skeletonFileName = pAsset->GetFile(0);
	if (!skeletonFileName || !*skeletonFileName)
		return;

	const string thumbnailFileName = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), pAsset->GetThumbnailPath());

	static QPreviewWidget* pPreviewWidget = new QPreviewWidget();
	pPreviewWidget->SetGrid(false);
	pPreviewWidget->SetAxis(false);
	pPreviewWidget->EnableMaterialPrecaching(true);
	pPreviewWidget->LoadFile(skeletonFileName);
	pPreviewWidget->setGeometry(0, 0, ASSET_THUMBNAIL_SIZE, ASSET_THUMBNAIL_SIZE);
	pPreviewWidget->FitToScreen();

		pPreviewWidget->SavePreview(thumbnailFileName.c_str());
	}).get();
}


