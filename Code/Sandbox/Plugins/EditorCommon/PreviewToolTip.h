// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "QTrackingTooltip.h"

#include <QVBoxLayout>

class CPreviewToolTip : public QTrackingTooltip
{
	Q_OBJECT
public:
	static bool ShowTrackingToolTip(const char* szAssetPath, QWidget* parent = nullptr);

	const QString& GetAssetPath() const { return m_assetPath; }

private:
	CPreviewToolTip(const char* szAssetPath, QWidget* pParent = nullptr);
	void SetPreviewWidget(QWidget* pPreview);

private:
	QString m_assetPath;
	QVBoxLayout* m_pLayout;
};

