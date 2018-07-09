// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetThumbnailsGenerator.h"
#include "AssetSystem/AssetManager.h"
#include "Notifications/NotificationCenter.h"
#include "FilePathUtil.h"
#include "ThreadingUtils.h"
#include "QtUtil.h"

namespace AsseThumbnailsGenerator
{

void GenerateThumbnailsAsync(const string& folder, const std::function<void()>& finalize)
{
	std::vector<CAssetPtr> assetsCopy = GetIEditor()->GetAssetManager()->GetAssetsFromDirectory(folder);
	if (assetsCopy.empty())
	{
		return;
	}

	// Put requests in a queue, but immediately notify of the progress.
	std::shared_ptr<CProgressNotification> pNotification = std::make_shared<CProgressNotification>(QObject::tr("Generating thumbnails"), QtUtil::ToQString(folder), true);
	ThreadingUtils::AsyncQueue([assetsCopy = std::move(assetsCopy), pNotification, finalize]()
	{
		CAssetManager* const pManager = GetIEditor()->GetAssetManager();
		CRY_ASSERT(pManager);

		const float deltaProgress = 1.0f / assetsCopy.size();
		float progress = 0.0f;

		for (CAsset* pAsset : assetsCopy)
		{
			if (GetIEditor()->IsMainFrameClosing())
			{
				return; // Preempt thumbnail generation when user closed the editor.
			}

			pNotification->SetMessage(QObject::tr("for asset '%1'").arg(pAsset->GetMetadataFile()));

			pAsset->GenerateThumbnail();

			progress += deltaProgress;
			pNotification->SetProgress(progress);
		}

		ThreadingUtils::PostOnMainThread([finalize, assetsCopy]()
		{
			if (GetIEditor()->IsMainFrameClosing())
			{
				return;
			}

			for (CAsset* pAsset : assetsCopy)
			{
				if (pAsset->GetType()->HasThumbnail())
				{
					pAsset->InvalidateThumbnail();
				}
			};

			if (finalize)
			{
				finalize();
			}
		});
	});
}

};
