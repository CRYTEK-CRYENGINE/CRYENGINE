// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PropertiesWidget.h"

#include "AssetsManager.h"
#include "ContextManager.h"
#include "ImplManager.h"
#include "ConnectionsWidget.h"
#include "Common/IImpl.h"

#include <QtUtil.h>
#include <Serialization/QPropertyTreeLegacy/QPropertyTreeLegacy.h>

#include <QLabel>
#include <QString>
#include <QVBoxLayout>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CPropertiesWidget::CPropertiesWidget(QWidget* const pParent)
	: QWidget(pParent)
	, m_pPropertyTree(new QPropertyTreeLegacy(this))
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

	g_assetsManager.SignalOnAfterAssetAdded.Connect([this]()
		{
			if (!g_assetsManager.IsLoading())
			{
			  RevertPropertyTree();
			}
		}, reinterpret_cast<uintptr_t>(this));

	g_assetsManager.SignalOnAfterAssetRemoved.Connect([this]()
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

	g_contextManager.SignalOnAfterContextAdded.Connect([this]()
		{
			if (!g_assetsManager.IsLoading())
			{
			  RevertPropertyTree();
			}
		}, reinterpret_cast<uintptr_t>(this));

	g_contextManager.SignalOnAfterContextRemoved.Connect([this]()
		{
			if (!g_assetsManager.IsLoading())
			{
			  RevertPropertyTree();
			}
		}, reinterpret_cast<uintptr_t>(this));

	g_contextManager.SignalContextRenamed.Connect([this]()
		{
			if (!g_assetsManager.IsLoading())
			{
			  RevertPropertyTree();
			}
		}, reinterpret_cast<uintptr_t>(this));

	g_implManager.SignalOnAfterImplChange.Connect([this]()
		{
			setEnabled(g_pIImpl != nullptr);
		}, reinterpret_cast<uintptr_t>(this));

	QObject::connect(m_pPropertyTree, &QPropertyTreeLegacy::signalAboutToSerialize, [&]() { m_suppressUpdates = true; });
	QObject::connect(m_pPropertyTree, &QPropertyTreeLegacy::signalSerialized, [&]() { m_suppressUpdates = false; });
}

//////////////////////////////////////////////////////////////////////////
CPropertiesWidget::~CPropertiesWidget()
{
	g_assetsManager.SignalOnAfterAssetAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_assetsManager.SignalOnAfterAssetRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_assetsManager.SignalControlModified.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_assetsManager.SignalAssetRenamed.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_contextManager.SignalOnAfterContextAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_contextManager.SignalOnAfterContextRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_contextManager.SignalContextRenamed.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_implManager.SignalOnAfterImplChange.DisconnectById(reinterpret_cast<uintptr_t>(this));
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
		CRY_ASSERT_MESSAGE(pAsset != nullptr, "Selected asset is null pointer during %s", __FUNCTION__);
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
void CPropertiesWidget::OnConnectionAdded(ControlId const id)
{
	m_pConnectionsWidget->OnConnectionAdded(id);
}

//////////////////////////////////////////////////////////////////////////
void CPropertiesWidget::OnBeforeReload()
{
	m_pConnectionsWidget->OnBeforeReload();
}

//////////////////////////////////////////////////////////////////////////
void CPropertiesWidget::OnAfterReload()
{
	m_pConnectionsWidget->OnAfterReload();
}

//////////////////////////////////////////////////////////////////////////
void CPropertiesWidget::OnFileImporterOpened()
{
	m_pConnectionsWidget->OnFileImporterOpened();
}

//////////////////////////////////////////////////////////////////////////
void CPropertiesWidget::OnFileImporterClosed()
{
	m_pConnectionsWidget->OnFileImporterClosed();
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
