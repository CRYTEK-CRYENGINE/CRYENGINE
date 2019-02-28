// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "PreviewToolTip.h"

bool CPreviewToolTip::ShowTrackingToolTip(const char* szAssetPath, QWidget* parent/* = nullptr*/)
{
	CPreviewToolTip* pToolTip = qobject_cast<CPreviewToolTip*>(QTrackingTooltip::m_instance.data());
	if (pToolTip)
	{
		if (pToolTip->GetAssetPath() == szAssetPath)
		{
			return true;
		}
	}

	if (QWidget* pPreviewWidget = GetIEditor()->CreatePreviewWidget(szAssetPath))
	{
		CPreviewToolTip* pPreviewToolTip = new CPreviewToolTip(szAssetPath);
		pPreviewToolTip->SetPreviewWidget(pPreviewWidget);

		QTrackingTooltip::ShowTrackingTooltip(QSharedPointer<QTrackingTooltip>(pPreviewToolTip, &QObject::deleteLater));
		return true;
	}

	return false;
}

CPreviewToolTip::CPreviewToolTip(const char* szAssetPath, QWidget* pParent/* = nullptr*/)
	: QTrackingTooltip(pParent)
	, m_assetPath(szAssetPath)
{
	m_pLayout = new QVBoxLayout();

	m_pLayout->setMargin(0);
	m_pLayout->setSpacing(0);

	setLayout(m_pLayout);
}

void CPreviewToolTip::SetPreviewWidget(QWidget* pPreview)
{
	m_pLayout->addWidget(pPreview);
}
