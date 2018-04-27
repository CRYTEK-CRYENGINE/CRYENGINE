// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VehicleMovementPanel_h__
#define __VehicleMovementPanel_h__

#pragma once

#include "PropertyCtrlExt.h"
#include "VehicleDialogComponent.h"

class CVehicleEditorDialog;

/*!
 * CVehicleMovementPanel
 */
class CVehicleMovementPanel : public CWnd, public IVehicleDialogComponent
{
	DECLARE_DYNAMIC(CVehicleMovementPanel)
public:
	CVehicleMovementPanel(CVehicleEditorDialog* pDialog);
	virtual ~CVehicleMovementPanel();

	void UpdateVehiclePrototype(CVehiclePrototype* pProt);
	void OnPaneClose() {}

	void OnMovementTypeChange(IVariable* pVar);

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);

	void         ExpandProps(CPropertyItem* pItem, bool expand = true);

	CVehiclePrototype* m_pVehicle;

	// pointer to main dialog
	CVehicleEditorDialog* m_pDialog;

	// controls
	CPropertyCtrlExt         m_propsCtrl;
	CVarBlockPtr             m_pMovementBlock;
	CVarBlockPtr             m_pPhysicsBlock;
	CVarBlockPtr             m_pTypeBlock;

	IVariable::OnSetCallback m_onSetCallback;

};

#endif // __VehicleMovementPanel_h__

