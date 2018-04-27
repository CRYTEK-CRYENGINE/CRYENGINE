// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Core/SmoothingGroupManager.h"
#include "DesignerSubPanel.h"
#include "Tools/BaseTool.h"
#include "QCollapsibleFrame.h"

#include <QBoxLayout>
#include <QTabWidget>

namespace Designer
{
namespace SubPanel
{
DesignerSubPanel* s_pSubBrushDesignerPanel = NULL;
int s_nSubBrushDesignerPanelId = 0;
}

class DesignerSubPanelConstructor
{
public:
	static IDesignerSubPanel* Create()
	{
		return SubPanel::s_pSubBrushDesignerPanel;
	}
};

void DesignerSubPanel::Done()
{
	gDesignerSettings.Save();

	setEnabled(false);
}

void DesignerSubPanel::Init()
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	Model* pModel = pSession->GetModel();
	if (pModel == nullptr)
		return;

	setEnabled(true);

	//TODO: should not really be here but
	gDesignerSettings.bDisplayBackFaces = pModel->CheckFlag(eModelFlag_DisplayBackFace);
}

void DesignerSubPanel::OnDesignerNotifyHandler(EDesignerNotify notification, void* param)
{
	switch (notification)
	{
	case eDesignerNotify_SettingsChanged:
		Update();
		break;
	case eDesignerNotify_BeginDesignerSession:
		Init();
		break;
	case eDesignerNotify_EndDesignerSession:
		Done();
		break;
	}
}

DesignerSubPanel::DesignerSubPanel(QWidget* parent) : QWidget(parent)
{
	gDesignerSettings.Load();
	DesignerSession* pSession = DesignerSession::GetInstance();
	QWidget* pSettings;

	pSettings = OrganizeSettingLayout(parent);

	QVBoxLayout* pMainLayout = new QVBoxLayout;
	pMainLayout->setSpacing(0);
	// margins are stylable
	pMainLayout->setContentsMargins(0, 0, 0, 0);
	pMainLayout->addWidget(pSettings);
	setLayout(pMainLayout);

	UpdateBackFaceCheckBoxFromContext();

	// Disable, only active after Init has been called.
	setEnabled(false);

	pSession->signalDesignerEvent.Connect(this, &DesignerSubPanel::OnDesignerNotifyHandler);

	// attempt to initialize the panel immediately if a session is active
	if (pSession->GetIsActive())
	{
		Init();
	}
}

DesignerSubPanel::~DesignerSubPanel()
{
	Done();
	DesignerSession::GetInstance()->signalDesignerEvent.DisconnectObject(this);
}

void DesignerSubPanel::Update()
{
	m_pSettingProperties->revert();
}

QWidget* DesignerSubPanel::OrganizeSettingLayout(QWidget* pParent)
{
	QWidget* pPaper = new QWidget(pParent);

	QBoxLayout* pBoxLayout = new QBoxLayout(QBoxLayout::TopToBottom);
	pBoxLayout->setContentsMargins(0, 0, 0, 0);

	m_pSettingProperties = new QPropertyTree(pPaper);

	m_pSettingProperties->setSizeToContent(true);
	m_pSettingProperties->setExpandLevels(2);
	m_pSettingProperties->setValueColumnWidth(0.6f);
	m_pSettingProperties->setAutoRevert(false);
	m_pSettingProperties->setAggregateMouseEvents(false);
	m_pSettingProperties->setFullRowContainers(true);
	m_pSettingProperties->attach(Serialization::SStruct(gDesignerSettings));

	pBoxLayout->addWidget(m_pSettingProperties);
	pPaper->setLayout(pBoxLayout);

	QObject::connect(m_pSettingProperties, &QPropertyTree::signalChanged, pPaper, [ = ] { NotifyDesignerSettingChanges(false);
	                 });
	QObject::connect(m_pSettingProperties, &QPropertyTree::signalContinuousChange, pPaper, [ = ] { NotifyDesignerSettingChanges(true);
	                 });

	return pPaper;
}

void DesignerSubPanel::UpdateSettingsTab()
{
	if (!m_pSettingProperties)
		return;
	m_pSettingProperties->revertNoninterrupting();
}

void DesignerSubPanel::UpdateEngineFlagsTab()
{
	const CSelectionGroup* pSelection = GetIEditor()->GetObjectManager()->GetSelection();
	for (int i = 0, iCount(pSelection->GetCount()); i < iCount; ++i)
	{
		CBaseObject* pObj = pSelection->GetObject(i);
		if (pObj->GetType() == OBJTYPE_SOLID)
			((DesignerObject*)pObj)->UpdateEngineFlags();
	}
}

void DesignerSubPanel::NotifyDesignerSettingChanges(bool continuous)
{
	gDesignerSettings.Update(continuous);
	if (GetDesigner() && GetDesigner()->GetCurrentTool())
		UpdateBackFaceFlag(GetDesigner()->GetCurrentTool()->GetMainContext());
}

void DesignerSubPanel::UpdateBackFaceCheckBoxFromContext()
{
	Model* pModel = DesignerSession::GetInstance()->GetModel();
	if (pModel)
	{
		UpdateBackFaceCheckBox(pModel);
	}
	else
	{
		std::vector<MainContext> selection;
		GetSelectedObjectList(selection);
		if (selection.size() > 0)
			UpdateBackFaceCheckBox(selection[0].pModel);
		else
			UpdateBackFaceCheckBox(NULL);
	}
}

void DesignerSubPanel::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eDesignerNotify_BeginDesignerSession:
		Init();
		break;
	case eDesignerNotify_EndDesignerSession:
		Done();
		break;
	case eNotify_OnSelectionChange:
		UpdateBackFaceCheckBoxFromContext();
		break;
	}
}

void DesignerSubPanel::UpdateBackFaceFlag(MainContext& mc)
{
	int modelFlag = mc.pModel->GetFlag();
	bool bDesignerDisplayBackFace = modelFlag & eModelFlag_DisplayBackFace;
	if (gDesignerSettings.bDisplayBackFaces == bDesignerDisplayBackFace)
		return;

	if (gDesignerSettings.bDisplayBackFaces)
		mc.pModel->SetFlag(modelFlag | eModelFlag_DisplayBackFace);
	else
		mc.pModel->SetFlag(modelFlag & (~eModelFlag_DisplayBackFace));

	mc.pModel->GetSmoothingGroupMgr()->InvalidateAll();
	ApplyPostProcess(mc, ePostProcess_SyncPrefab | ePostProcess_Mesh);
}

void DesignerSubPanel::UpdateBackFaceCheckBox(Model* pModel)
{
	gDesignerSettings.bDisplayBackFaces = false;
	if (pModel && pModel->CheckFlag(eModelFlag_DisplayBackFace))
		gDesignerSettings.bDisplayBackFaces = true;
	m_pSettingProperties->revert();
}
}

REGISTER_GENERALPANEL(SettingPanel, DesignerSubPanel, DesignerSubPanelConstructor);

