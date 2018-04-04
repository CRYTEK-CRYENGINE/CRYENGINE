// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "InspectorPanel.h"

#include <QVBoxLayout>
#include <QLabel>
#include <Serialization/QPropertyTree/QPropertyTree.h>
#include <ACETypes.h>
#include <IEditor.h>
#include <QtUtil.h>
#include <QString>

#include "QAudioControlEditorIcons.h"
#include "QConnectionsWidget.h"
#include "AudioAssetsManager.h"

using namespace QtUtil;

namespace ACE
{
CInspectorPanel::CInspectorPanel(CAudioAssetsManager* pAssetsManager)
	: m_pAssetsManager(pAssetsManager)
{
	assert(m_pAssetsManager);
	resize(300, 490);
	setWindowTitle(tr("Inspector Panel"));
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	QVBoxLayout* pMainLayout = new QVBoxLayout(this);
	pMainLayout->setContentsMargins(0, 0, 0, 0);

	m_pPropertyTree = new QPropertyTree(this);
	m_pPropertyTree->setSizeToContent(true);

	pMainLayout->addWidget(m_pPropertyTree);

	m_pUsageHint.reset(new QString(tr("Select an audio control from the left pane to see its properties!")));

	m_pConnectionsLabel = new QLabel(*m_pUsageHint);
	m_pConnectionsLabel->setObjectName("ConnectionsTitle");
	m_pConnectionsLabel->setAlignment(Qt::AlignCenter);
	m_pConnectionsLabel->setWordWrap(true);
	pMainLayout->addWidget(m_pConnectionsLabel);

	m_pConnectionList = new QConnectionsWidget();
	pMainLayout->addWidget(m_pConnectionList);

	auto revertFunction = [&]()
	{
		if (!m_bSupressUpdates)
		{
			m_pPropertyTree->revert();
		}
	};
	pAssetsManager->signalItemAdded.Connect(revertFunction, reinterpret_cast<uintptr_t>(this));
	pAssetsManager->signalItemRemoved.Connect(revertFunction, reinterpret_cast<uintptr_t>(this));
	pAssetsManager->signalControlModified.Connect(revertFunction, reinterpret_cast<uintptr_t>(this));

	m_pConnectionList->Init();

	Reload();

	connect(m_pPropertyTree, &QPropertyTree::signalAboutToSerialize, [&]()
		{
			m_bSupressUpdates = true;
	  });

	connect(m_pPropertyTree, &QPropertyTree::signalSerialized, [&]()
		{
			m_bSupressUpdates = false;
	  });
}

CInspectorPanel::~CInspectorPanel()
{
	m_pAssetsManager->signalItemAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	m_pAssetsManager->signalItemRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
	m_pAssetsManager->signalControlModified.DisconnectById(reinterpret_cast<uintptr_t>(this));
}

void CInspectorPanel::Reload()
{
	m_pConnectionList->Reload();
}

//////////////////////////////////////////////////////////////////////////
void CInspectorPanel::SetSelectedControls(const std::vector<CAudioControl*>& selectedControls)
{
	// Update property tree
	m_pPropertyTree->detach();
	Serialization::SStructs serializers;

	for (auto const pControl : selectedControls)
	{
		CRY_ASSERT(pControl != nullptr);
		serializers.emplace_back(*pControl);
	}

	m_pPropertyTree->attach(serializers);

	// Update connections
	if (selectedControls.size() == 1)
	{
		CAudioControl* pControl = selectedControls[0];
		if (pControl->GetType() != eItemType_Switch)
		{
			m_pConnectionList->SetControl(pControl);
			m_pConnectionList->setHidden(false);
			m_pConnectionsLabel->setAlignment(Qt::AlignLeft);
			m_pConnectionsLabel->setText(tr("Connections"));
		}
		else
		{
			m_pConnectionList->setHidden(true);
			m_pConnectionsLabel->setAlignment(Qt::AlignCenter);
			m_pConnectionsLabel->setText(tr("Select a switch state to see its properties!"));
		}
	}
	else
	{
		m_pConnectionList->setHidden(true);
		m_pConnectionList->SetControl(nullptr);
		m_pConnectionsLabel->setAlignment(Qt::AlignCenter);
		m_pConnectionsLabel->setText(*m_pUsageHint);
	}
}
} // namespace ACE
