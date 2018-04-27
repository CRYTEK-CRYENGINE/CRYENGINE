// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"
#include "QTrackingTooltip.h"
#include "AssetSystem/Asset.h"

//! Descriptive asset tooltip, mostly used by the asset browser but can in theory be used from anywhere
class EDITOR_COMMON_API CAssetTooltip : private QTrackingTooltip
{
	Q_OBJECT
public:
	static void ShowTrackingTooltip(CAsset* asset, QWidget* parent = nullptr);
	static void HideTooltip() { QTrackingTooltip::HideTooltip(); }

private:
	CAssetTooltip(CAsset* asset, QWidget* parent = nullptr);
	~CAssetTooltip();

	void SetAsset(CAsset* asset);
	void UpdateInfos();
	void UpdateThumbnail();

	void SetBigMode(bool bBigMode);

	void OnThumbnailLoaded(CAsset& asset);

	bool eventFilter(QObject* object, QEvent* event) override;
	void hideEvent(QHideEvent* event) override;

	CAsset* m_asset;
	QLabel* m_label;
	QLabel* m_thumbnail;
	QWidget* m_mainWidget;
	QWidget* m_bigWidget;

	size_t m_loadingThumbnailsCount;
};
