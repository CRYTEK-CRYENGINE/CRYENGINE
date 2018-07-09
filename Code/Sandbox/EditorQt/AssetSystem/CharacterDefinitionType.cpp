// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CharacterDefinitionType.h"
#include "QT/Widgets/QPreviewWidget.h"
#include "AssetSystem/Asset.h"
#include "FilePathUtil.h"
#include <ThreadingUtils.h>

REGISTER_ASSET_TYPE(CCharacterDefinitionType)

CryIcon CCharacterDefinitionType::GetIconInternal() const
{
	return CryIcon("icons:common/assets_character.ico");
}

void CCharacterDefinitionType::GenerateThumbnail(const CAsset* pAsset) const
{
	ThreadingUtils::PostOnMainThread([=]()
	{
		CMaterialManager* pManager = GetIEditorImpl()->GetMaterialManager();
	if (!pManager)
		return;

	CRY_ASSERT(0 < pAsset->GetFilesCount());
	if (1 > pAsset->GetFilesCount())
		return;

	const char* characterFileName = pAsset->GetFile(0);
	if (!characterFileName || !*characterFileName)
		return;

	const string thumbnailFileName = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), pAsset->GetThumbnailPath());

	static QPreviewWidget* pPreviewWidget = new QPreviewWidget();
	pPreviewWidget->SetGrid(false);
	pPreviewWidget->SetAxis(false);
	pPreviewWidget->EnableMaterialPrecaching(true);
	pPreviewWidget->LoadFile(characterFileName);
	pPreviewWidget->setGeometry(0, 0, ASSET_THUMBNAIL_SIZE, ASSET_THUMBNAIL_SIZE);
	pPreviewWidget->FitToScreen();

		pPreviewWidget->SavePreview(thumbnailFileName.c_str());
	}).get();
}


