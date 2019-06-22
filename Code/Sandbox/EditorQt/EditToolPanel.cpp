// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include <Serialization/QPropertyTreeLegacy/QPropertyTreeLegacy.h>

#include "EditToolPanel.h"
#include "LevelEditor/Tools/EditTool.h"
#include <LevelEditor/LevelEditorSharedState.h>
#include <CrySerialization/IArchive.h>

void SEditToolSerializer::YASLI_SERIALIZE_METHOD(Serialization::IArchive& ar)
{
	if (pEditTool)
		pEditTool->Serialize(ar);
}

QEditToolPanel::QEditToolPanel(QWidget* parent)
	: QWidget(parent)
	, m_pPropertyTree(nullptr)
{
	GetIEditorImpl()->GetLevelEditorSharedState()->signalPreEditToolChanged.Connect(this, &QEditToolPanel::OnPreEditToolChanged);
	GetIEditorImpl()->GetLevelEditorSharedState()->signalEditToolChanged.Connect(this, &QEditToolPanel::OnEditToolChanged);
}

QEditToolPanel::~QEditToolPanel()
{
	GetIEditorImpl()->GetLevelEditorSharedState()->signalPreEditToolChanged.DisconnectObject(this);
	GetIEditorImpl()->GetLevelEditorSharedState()->signalEditToolChanged.DisconnectObject(this);

	if (m_pPropertyTree)
		m_pPropertyTree->detach();

	if (m_toolSerializer.pEditTool)
		m_toolSerializer.pEditTool->signalPropertiesChanged.DisconnectObject(this);
}

void QEditToolPanel::OnPreEditToolChanged()
{
	SetTool(nullptr);
}

void QEditToolPanel::OnEditToolChanged()
{
	CEditTool* pTool = GetIEditorImpl()->GetLevelEditorSharedState()->GetEditTool();
	if (pTool && CanEditTool(pTool))
	{
		SetTool(pTool);
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
