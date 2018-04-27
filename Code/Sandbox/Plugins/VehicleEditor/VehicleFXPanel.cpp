// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "VehicleFXPanel.h"

#include "Controls\PropertyItem.h"

#include "VehicleData.h"
#include "VehicleEditorDialog.h"
#include "VehiclePrototype.h"

IMPLEMENT_DYNAMIC(CVehicleFXPanel, CWnd)

CVehicleFXPanel::CVehicleFXPanel(CVehicleEditorDialog* pDialog)
	: m_pVehicle(0)
{
	assert(pDialog); // dialog is needed
	m_pDialog = pDialog;
}

CVehicleFXPanel::~CVehicleFXPanel()
{
}

BEGIN_MESSAGE_MAP(CVehicleFXPanel, CWnd)
ON_WM_CREATE()
ON_WM_SIZE()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CVehicleFXPanel::ExpandProps(CPropertyItem* pItem, bool expand /*=true*/)
{
	// expand all children and their children
	for (int i = 0; i < pItem->GetChildCount(); ++i)
	{
		CPropertyItem* pChild = pItem->GetChild(i);
		m_propsCtrl.Expand(pChild, true);

		for (int j = 0; j < pChild->GetChildCount(); ++j)
		{
			m_propsCtrl.Expand(pChild->GetChild(j), true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVehicleFXPanel::AddCategory(IVariable* pVar)
{
	if (!pVar)
		return;

	CVarBlock* block = new CVarBlock;
	block->AddVariable(pVar);
	CPropertyItem* pItem = m_propsCtrl.AddVarBlock(block);
	ExpandProps(pItem);
}

//////////////////////////////////////////////////////////////////////////
void CVehicleFXPanel::UpdateVehiclePrototype(CVehiclePrototype* pProt)
{
	assert(pProt);
	m_pVehicle = pProt;
	m_propsCtrl.DeleteAllItems();
	m_propsCtrl.SetPreSelChangeCallback(functor(*m_pDialog, &CVehicleEditorDialog::OnPropsSelChanged));

	// get data from root var
	IVariablePtr pRoot = pProt->GetVariable();

	// add Particles table
	if (IVariable* pVar = GetChildVar(pRoot, "Particles"))
	{
		CVehicleData::FillDefaults(pVar, "Particles");

		for (int i = 0; i < pVar->GetNumVariables(); ++i)
			AddCategory(pVar->GetVariable(i));
	}
}

//////////////////////////////////////////////////////////////////////////
int CVehicleFXPanel::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	int nRes = __super::OnCreate(lpCreateStruct);

	m_propsCtrl.Create(WS_CHILD | WS_VISIBLE, CRect(0, 0, 300, 350), this);
	m_propsCtrl.ExpandAll();

	return nRes;
}

//////////////////////////////////////////////////////////////////////////
void CVehicleFXPanel::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	CRect rc;
	GetClientRect(rc);

	if (m_propsCtrl.m_hWnd)
		m_propsCtrl.MoveWindow(0, 0, rc.right, rc.bottom, true);
}

