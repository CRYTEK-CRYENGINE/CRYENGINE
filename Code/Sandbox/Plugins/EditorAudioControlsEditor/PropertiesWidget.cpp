// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PropertiesWidget.h"

#include "AudioControlsEditorPlugin.h"
#include "ImplementationManager.h"
#include "ConnectionsWidget.h"

#include <QtUtil.h>
#include <Serialization/QPropertyTree/QPropertyTree.h>

#include <QLabel>
#include <QString>
#include <QVBoxLayout>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CPropertiesWidget::CPropertiesWidget(QWidget* const pParent)
	: QWidget(pParent)
	, m_pPropertyTree(new QPropertyTree(this))
	, m_pConnectionsWidget(new CConnectionsWidget(this))
	, m_suppressUpdates(false)
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	auto const pMainLayout = new QVBoxLayout(this);
	pMainLayout->setContentsMargins(0, 0, 0, 0);

	m_pPropertyTree->setSizeToContent(true);
	m_pPropertyTree->setExpandLevels(2);
	pMainLayout->addWidget(m_pPropertyTree);

	m_pUsageHint = std::make_unique<QString>(tr("Select an audio control from the left pane to see its properties!"));

	m_pConnectionsLabel = new QLabel(*m_pUsageHint, this);
	m_pConnectionsLabel->setAlignment(Qt::AlignCenter);
	m_pConnectionsLabel->setWordWrap(true);
	pMainLayout->addWidget(m_pConnectionsLabel);

	pMainLayout->addWidget(m_pConnectionsWidget);

	if (g_pIImpl == nullptr)
	{
		setEnabled(false);
	}

	g_assetsManager.SignalAssetAdded.Connect([this]()
		{
			if (!g_assetsManager.IsLoading())
			{
			  RevertPropertyTree();
			}
	  }, reinterpret_cast<uintptr_t>(this));

	g_assetsManager.SignalAssetRemoved.Connect([this]()
		{
			if (!g_assetsManager.IsLoading())
			{
			  RevertPropertyTree();
			}
	  }, reinterpret_cast<uintptr_t>(this));

	g_assetsManager.SignalControlModified.Connect([this]()
		{
			if (!g_assetsManager.IsLoading())
			{
			  RevertPropertyTree();
			}
	  }, reinterpret_cast<uintptr_t>(this));

	g_assetsManager.SignalAssetRenamed.Connect([this]()
		{
			if (!g_assetsManager.IsLoading())
			{
			  RevertPropertyTree();
			}
	  }, reinterpret_cast<uintptr_t>(this));

	g_implementationManager.SignalImplementationChanged.Connect([this]()
		{
			setEnabled(g_pIImpl != nullptr);
	  }, reinterpret_cast<uintptr_t>(this));

	QObject::connect(m_pPropertyTree, &QPropertyTree::signalAboutToSerialize, [&]() { m_suppressUpdates = true; });
	QObject::connect(m_pPropertyTree, &QPropertyTree::signalSerialized, [&]() { m_suppressUpdates = false; });
	QObject::connect(m_pConnectionsWidget, &CConnectionsWidget::SignalSelectConnectedImplItem, this, &CPropertiesWidget::SignalSelectConnectedImplItem);
}

//////////////////////////////////////////////////////////////////////////
CPropertiesWidget::~CPropertiesWidget()
{
	g_assetsManager.SignalAssetAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_assetsManager.SignalAssetRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_assetsManager.SignalControlModified.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_assetsManager.SignalAssetRenamed.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_implementationManager.SignalImplementationChanged.DisconnectById(reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
void CPropertiesWidget::Reset()
{
	m_pConnectionsWidget->Reset();
}

//////////////////////////////////////////////////////////////////////////
void CPropertiesWidget::OnSetSelectedAssets(Assets const& selectedAssets, bool const restoreSelection)
{
	// Update property tree
	m_pPropertyTree->detach();
	Serialization::SStructs serializers;

	for (auto const pAsset : selectedAssets)
	{
		CRY_ASSERT_MESSAGE(pAsset != nullptr, "Selected asset is null pointer.");
		serializers.emplace_back(*pAsset);
	}

	m_pPropertyTree->attach(serializers);

	// Update connections
	if (selectedAssets.size() == 1)
	{
		EAssetType const type = selectedAssets[0]->GetType();

		if ((type != EAssetType::Library) && (type != EAssetType::Folder))
		{
			auto const pControl = static_cast<CControl*>(selectedAssets[0]);

			if (type != EAssetType::Switch)
			{
				m_pConnectionsWidget->SetControl(pControl, restoreSelection);
				m_pConnectionsWidget->setHidden(false);
				m_pConnectionsLabel->setAlignment(Qt::AlignLeft);
				m_pConnectionsLabel->setText(tr("Connections"));
			}
			else
			{
				m_pConnectionsWidget->setHidden(true);
				m_pConnectionsWidget->SetControl(nullptr, restoreSelection);
				m_pConnectionsLabel->setAlignment(Qt::AlignCenter);
				m_pConnectionsLabel->setText(tr("Select a switch state to see its connections!"));
			}
		}
		else
		{
			m_pConnectionsWidget->setHidden(true);
			m_pConnectionsWidget->SetControl(nullptr, restoreSelection);
			m_pConnectionsLabel->setAlignment(Qt::AlignCenter);
			m_pConnectionsLabel->setText(*m_pUsageHint);
		}
	}
	else
	{
		m_pConnectionsWidget->setHidden(true);
		m_pConnectionsWidget->SetControl(nullptr, restoreSelection);
		m_pConnectionsLabel->setAlignment(Qt::AlignCenter);
		m_pConnectionsLabel->setText(*m_pUsageHint);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertiesWidget::OnAboutToReload()
{
	m_pConnectionsWidget->OnAboutToReload();
}

//////////////////////////////////////////////////////////////////////////
void CPropertiesWidget::OnReloaded()
{
	m_pConnectionsWidget->OnReloaded();
}

//////////////////////////////////////////////////////////////////////////
void CPropertiesWidget::RevertPropertyTree()
{
	if (!m_suppressUpdates)
	{
		m_pPropertyTree->revert();
	}
}
} // namespace ACE

