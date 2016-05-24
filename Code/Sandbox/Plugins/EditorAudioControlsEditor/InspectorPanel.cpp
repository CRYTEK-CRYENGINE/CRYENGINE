// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "InspectorPanel.h"
#include "QtUtil.h"
#include "QAudioControlEditorIcons.h"
#include <ACETypes.h>
#include <IEditor.h>
#include "QConnectionsWidget.h"

#include <QVBoxLayout>
#include <QPropertyTree/QPropertyTree.h>
#include <QLabel>

using namespace QtUtil;

namespace ACE
{
CInspectorPanel::CInspectorPanel(CATLControlsModel* pATLModel)
	: m_pATLModel(pATLModel)
	, m_bSupressUpdates(false)
{
	assert(m_pATLModel);
	resize(300, 490);
	setWindowTitle(tr("Inspector Panel"));
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	QVBoxLayout* pMainLayout = new QVBoxLayout(this);
	pMainLayout->setContentsMargins(0, 0, 0, 0);

	m_pPropertyTree = new QPropertyTree(this);
	m_pPropertyTree->setSizeToContent(true);

	pMainLayout->addWidget(m_pPropertyTree);

	m_pConnectionList = new QConnectionsWidget(this);
	pMainLayout->addWidget(m_pConnectionList);
	pMainLayout->setAlignment(Qt::AlignTop);

	m_pATLModel->AddListener(this);
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
	m_pATLModel->RemoveListener(this);
}

void CInspectorPanel::Reload()
{
	m_pConnectionList->Reload();
}

void CInspectorPanel::SetSelectedControls(const ControlList& selectedControls)
{
	// Update property tree
	m_pPropertyTree->detach();
	Serialization::SStructs serializers;
	for (auto id : selectedControls)
	{
		CATLControl* pControl = m_pATLModel->GetControlByID(id);
		if (pControl)
		{
			serializers.push_back(Serialization::SStruct(*pControl));
		}
	}
	m_pPropertyTree->attach(serializers);

	//Update connections
	if (selectedControls.size() == 1)
	{
		CATLControl* pControl = m_pATLModel->GetControlByID(selectedControls[0]);
		if (pControl && pControl->GetType() != eACEControlType_Switch)
		{
			m_pConnectionList->SetControl(pControl);
			m_pConnectionList->setHidden(false);
		}
		else
		{
			m_pConnectionList->setHidden(true);
		}
	}
	else
	{
		m_pConnectionList->setHidden(true);
		m_pConnectionList->SetControl(nullptr);
	}
}

void CInspectorPanel::OnControlModified(ACE::CATLControl* pControl)
{
	if (!m_bSupressUpdates)
	{
		m_pPropertyTree->revert();
	}
	m_pConnectionList->Reload();
}

}
