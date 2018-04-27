// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "AssetTooltip.h"

#include "AssetSystem/AssetType.h"
#include "AssetSystem/AssetManager.h"
#include "AssetThumbnailsLoader.h"

#include <QBoxLayout>
#include <QEvent.h>

void CAssetTooltip::ShowTrackingTooltip(CAsset* asset, QWidget* parent /*= nullptr*/)
{
	CAssetTooltip* tooltip = qobject_cast<CAssetTooltip*>(QTrackingTooltip::m_instance.data());
	if (tooltip)
	{
		tooltip->SetAsset(asset);
	}
	else
	{
		auto tt = QSharedPointer<QTrackingTooltip>(new CAssetTooltip(asset, parent), &QObject::deleteLater);
		QTrackingTooltip::ShowTrackingTooltip(tt);
	}
}

CAssetTooltip::CAssetTooltip(CAsset* asset, QWidget* parent /*= nullptr*/)
	: m_asset(nullptr)
	, m_loadingThumbnailsCount(0)
	, m_bigWidget(nullptr)
{
	CRY_ASSERT(asset);

	m_label = new QLabel();
	m_label->setTextFormat(Qt::RichText);
	m_label->setObjectName("CAssetTooltipLabel");

	m_thumbnail = new QLabel();
	m_thumbnail->setObjectName("CAssetTooltipThumbnail");
	m_thumbnail->hide();


	auto layout = new QHBoxLayout();
	layout->addWidget(m_label);
	layout->addWidget(m_thumbnail);
	layout->setMargin(0);

	m_mainWidget = new QWidget();
	m_mainWidget->setLayout(layout);

	//For some reason QStackedLayout simply does not work in the tooltip
	//Let's just use visibilty flags in a regular VBowLayout
	auto mainLayout = new QVBoxLayout();
	mainLayout->setMargin(0);
	mainLayout->addWidget(m_mainWidget);

	setLayout(mainLayout);

	SetAsset(asset);
}

CAssetTooltip::~CAssetTooltip()
{
	if (m_loadingThumbnailsCount)
	{
		CAssetThumbnailsLoader::GetInstance().signalAssetThumbnailLoaded.DisconnectObject(this);
	}
}

void CAssetTooltip::SetAsset(CAsset* asset)
{
	if (m_asset == asset)
		return;

	m_asset = asset;

	//Clean big widget, asset has changed
	if (m_bigWidget)
	{
		bool wasInBigMode = m_bigWidget->isVisible();
		SetBigMode(false);
		layout()->removeWidget(m_bigWidget);
		delete m_bigWidget;
		m_bigWidget = nullptr;
		if (wasInBigMode)
			SetBigMode(true);
	}
	else if (QApplication::keyboardModifiers() & Qt::ControlModifier)
	{
		SetBigMode(true);
	}

	//TODO: for some reason when in "small mode" and changing assets, the size is not always adjusted
	UpdateInfos();
	UpdateThumbnail();
	update();
	adjustSize();
}

void CAssetTooltip::UpdateInfos()
{
	QString str;

	str += QString(tr("<b>Name:</b> %1")).arg(m_asset->GetName());
	str += QString(tr("<br/><b>Type:</b> %1")).arg(m_asset->GetType()->GetUiTypeName());
	str += QString(tr("<br/><b>Asset File:</b> %1")).arg(m_asset->GetMetadataFile());

	const int dataFilesCount = m_asset->GetFilesCount();

	if (dataFilesCount > 0)
		str += tr("<br/><b>Data File(s):</b>");

	for (int i = 0; i < dataFilesCount; i++)
	{
		str += "<br/>";
		str += m_asset->GetFile(i);
	}

	if (m_asset->HasSourceFile())
		str += QString(tr("<br/><b>Source File:</b> %1")).arg(m_asset->GetSourceFile());

	const std::vector<CItemModelAttribute*>& details = m_asset->GetType()->GetDetails();
	QString detailsText;
	for (CItemModelAttribute* pDetail : details)
	{
		QVariant value = m_asset->GetType()->GetDetailValue(m_asset, pDetail);
		if (value.isValid())
		{
			detailsText += QStringLiteral("<br/>%1: %2").arg(pDetail->GetName()).arg(value.toString());
		}
	}
	if (!detailsText.isEmpty())
	{
		str += QStringLiteral("<br/><b>Details:</b>") + detailsText;
	}

	m_label->setText(str);
}

void CAssetTooltip::UpdateThumbnail()
{
	if (!m_asset || !m_asset->GetType()->HasThumbnail())
	{
		m_thumbnail->hide();
	}
	else
	{
		if (!m_asset->IsThumbnailLoaded())
		{
			CAssetThumbnailsLoader::GetInstance().PostAsset(m_asset);

			if (!m_loadingThumbnailsCount)
			{
				CAssetThumbnailsLoader::GetInstance().signalAssetThumbnailLoaded.Connect(this, &CAssetTooltip::OnThumbnailLoaded);
			}
			m_loadingThumbnailsCount++;
		}

		const int size = 128;
		m_thumbnail->setPixmap(m_asset->GetThumbnail().pixmap(size, size));
		m_thumbnail->show();
	}
}

void CAssetTooltip::SetBigMode(bool bBigMode)
{
	if (bBigMode && m_asset->GetType()->HasThumbnail())
	{
 		if (m_bigWidget)
		{
			m_bigWidget->setVisible(true);
			m_mainWidget->setVisible(false);
		}
		else
		{
			m_bigWidget = m_asset->GetType()->CreateBigInfoWidget(m_asset);
			if(m_bigWidget)
			{
				layout()->addWidget(m_bigWidget);
				m_mainWidget->setVisible(false);
			}
		}
	}
	else if(m_bigWidget)
	{
		m_bigWidget->setVisible(false);
		m_mainWidget->setVisible(true);
	}
	adjustSize();
}

void CAssetTooltip::OnThumbnailLoaded(CAsset& asset)
{
	if (m_asset == &asset)
	{
		UpdateThumbnail();
	}

	CRY_ASSERT(m_loadingThumbnailsCount > 0);
	m_loadingThumbnailsCount--;
	if (!m_loadingThumbnailsCount)
	{
		CAssetThumbnailsLoader::GetInstance().signalAssetThumbnailLoaded.DisconnectObject(this);
	}
}

bool CAssetTooltip::eventFilter(QObject* object, QEvent* event)
{
	switch (event->type())
	{
	case QEvent::KeyPress:
	{
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
		if (keyEvent->key() == Qt::Key_Control && !keyEvent->isAutoRepeat())
		{
			//Using space as a key prevents the tooltip event from being emitted, using CTRL instead
			SetBigMode(true);
		}
	}
	break;
	case QEvent::KeyRelease:
	{
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
		if (keyEvent->key() == Qt::Key_Control && !keyEvent->isAutoRepeat())
		{
			SetBigMode(false);
		}
	}
	default:
		break;
	}

	//Never intercept the event
	return QTrackingTooltip::eventFilter(object, event);
}

void CAssetTooltip::hideEvent(QHideEvent* event)
{
	SetBigMode(false);
}

