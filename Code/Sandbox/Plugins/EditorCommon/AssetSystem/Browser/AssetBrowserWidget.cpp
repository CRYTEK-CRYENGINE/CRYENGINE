// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetBrowserWidget.h"

#include "AssetModel.h"
#include "AssetSystem/AssetEditor.h"
#include "AssetSystem/AssetManagerHelpers.h"
#include "Commands/QCommandAction.h"
#include "ProxyModels/AttributeFilterProxyModel.h"

#include <QItemSelectionModel>
#include <QVariant>
#include <QTimer>

CAssetBrowserWidget::CAssetBrowserWidget(CAssetEditor* pOwner)
	: CAssetBrowser(pOwner->GetSupportedAssetTypes(), false)
	, m_pAssetEditor(pOwner)
{
	m_pActionSyncSelection = RegisterAction("general.toggle_sync_selection", &CAssetBrowserWidget::OnSyncSelection);
	m_pActionSyncSelection->setChecked(true);

	SetRecursiveView(true);
	SetFoldersViewVisible(false);
	SetViewMode(CAssetBrowser::Thumbnails);

	pOwner->signalAssetOpened.Connect(this, &CAssetBrowserWidget::OnAssetOpened);
	OnAssetOpened();
}

CAssetBrowserWidget::~CAssetBrowserWidget()
{
	m_pAssetEditor->signalAssetOpened.DisconnectObject(this);
}

bool CAssetBrowserWidget::IsSyncSelection() const
{
	return m_pActionSyncSelection->isChecked();
}

void CAssetBrowserWidget::SetSyncSelection(bool syncSelection)
{
	m_pActionSyncSelection->setChecked(syncSelection);
}

void CAssetBrowserWidget::SetLayout(const QVariantMap& state)
{
	CAssetBrowser::SetLayout(state);
	QVariant syncSelection = state.value("syncSelection");
	if (syncSelection.isValid())
	{
		SetSyncSelection(syncSelection.toBool());
	}
}

QVariantMap CAssetBrowserWidget::GetLayout() const
{
	QVariantMap state = CAssetBrowser::GetLayout();
	state.insert("syncSelection", IsSyncSelection());
	return state;
}

CAsset* CAssetBrowserWidget::GetCurrentAsset() const
{
	const QModelIndex currentIndex = GetItemSelectionModel()->currentIndex();
	return currentIndex.isValid() && CAssetModel::IsAsset(currentIndex) ? CAssetModel::ToAsset(currentIndex) : nullptr;
}

void CAssetBrowserWidget::OnAssetOpened()
{
	if (m_pAssetEditor->GetAssetBeingEdited())
	{
		const CAsset* const pAsset = GetCurrentAsset();
		if (m_pAssetEditor->GetAssetBeingEdited() != pAsset)
		{
			SelectAsset(*m_pAssetEditor->GetAssetBeingEdited());
		}
	}
}

bool CAssetBrowserWidget::OnSyncSelection()
{
	UpdatePreview(GetItemSelectionModel()->currentIndex());
	return true;
}

void CAssetBrowserWidget::UpdatePreview(const QModelIndex& currentIndex)
{
	if (!IsSyncSelection() || !CAssetModel::IsAsset(currentIndex))
	{
		return;
	}

	CAsset* const pAsset = CAssetModel::ToAsset(currentIndex);
	if (!pAsset)
	{
		return;
	}

	if (!m_pSyncSelectionTimer)
	{
		m_pSyncSelectionTimer.reset(new QTimer());
		m_pSyncSelectionTimer->setSingleShot(true);
		m_pSyncSelectionTimer->setInterval(200);

		connect(m_pSyncSelectionTimer.get(), &QTimer::timeout, [this]()
		{
			CAsset* const pAsset = GetCurrentAsset();
			if (pAsset && m_pAssetEditor->CanOpenAsset(pAsset))
			{
				if (!pAsset->IsBeingEdited())
				{
					m_pAssetEditor->OpenAsset(pAsset);
				}
				else // Brings current asset editor on top
				{
					pAsset->Edit();
				}
			}
		});
	}
	m_pSyncSelectionTimer->start();
}

void CAssetBrowserWidget::OnActivated(CAsset* pAsset)
{
	if (m_pSyncSelectionTimer)
	{
		m_pSyncSelectionTimer->stop();
	}

	if (!m_pAssetEditor->CanOpenAsset(pAsset))
	{
		CAssetBrowser::OnActivated(pAsset);
	}
	else if (m_pAssetEditor->Close())
	{
		m_pAssetEditor->OpenAsset(pAsset);
	}
}

void CAssetBrowserWidget::showEvent(QShowEvent* pEvent)
{
	// Defer ScrollToSelected until the ThumbnailView calculates icons layout.
	QTimer::singleShot(0, [this]()
	{
		ScrollToSelected();
	});
}
