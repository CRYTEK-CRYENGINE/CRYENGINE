// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include <Serialization/QPropertyTree/QPropertyTree.h>

#include "EditToolPanel.h"
#include "EditTool.h"
#include <CrySerialization/IArchive.h>

void SEditToolSerializer::YASLI_SERIALIZE_METHOD(Serialization::IArchive& ar)
{
	if (pEditTool)
		pEditTool->Serialize(ar);
}

QEditToolPanel::QEditToolPanel(QWidget* parent)
	: QWidget(parent)
{
	GetIEditorImpl()->RegisterNotifyListener(this);
}

QEditToolPanel::~QEditToolPanel()
{
	GetIEditorImpl()->UnregisterNotifyListener(this);

	if (m_pPropertyTree)
		m_pPropertyTree->detach();

	if (m_toolSerializer.pEditTool)
		m_toolSerializer.pEditTool->signalPropertiesChanged.DisconnectObject(this);
}

void QEditToolPanel::OnEditorNotifyEvent(EEditorNotifyEvent e)
{
	if (eNotify_OnEditToolBeginChange == e)
	{
		SetTool(nullptr);
	}
	else if (eNotify_OnEditToolEndChange == e)
	{
		CEditTool* pTool = GetIEditorImpl()->GetEditTool();
		if (pTool && CanEditTool(pTool))
		{
			SetTool(pTool);
		}
	}
}

void QEditToolPanel::ReloadProperties(CEditTool* pTool)
{
	m_pPropertyTree->revertNoninterrupting();
}

void QEditToolPanel::SetTool(CEditTool* pTool)
{
	if (m_toolSerializer.pEditTool)
	{
		m_pPropertyTree->detach();
		m_toolSerializer.pEditTool->signalPropertiesChanged.DisconnectObject(this);
		m_toolSerializer.pEditTool = nullptr;
	}

	if (pTool)
	{
		pTool->signalPropertiesChanged.Connect(this, &QEditToolPanel::ReloadProperties);
		Serialization::SStructs structs;
		m_toolSerializer.pEditTool = pTool;
		structs.emplace_back(m_toolSerializer);
		m_pPropertyTree->setExpandLevels(2);
		m_pPropertyTree->attach(structs);
	}
}

