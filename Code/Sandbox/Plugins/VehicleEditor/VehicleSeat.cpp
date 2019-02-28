// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehicleSeat.h"

#include "VehiclePrototype.h"
#include "VehicleData.h"
#include "VehiclePart.h"
#include "VehicleEditorDialog.h"
#include "VehiclePartsPanel.h"
#include "VehicleWeapon.h"
#include "Objects/DisplayContext.h"

REGISTER_CLASS_DESC(CVehicleSeatClassDesc);

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CVehicleSeat, CBaseObject)

namespace
{
IVariable* CreateWeaponVar()
{
	IVariable* pWeapon = new CVariableArray;
	pWeapon->SetName("Weapon");

	IVariable* pClass = new CVariable<string>;
	pClass->SetName("class");
	pWeapon->AddVariable(pClass);

	IVariable* pHelpers = new CVariableArray;
	pHelpers->SetName("Helpers");
	pWeapon->AddVariable(pHelpers);

	return pWeapon;
}
}

//////////////////////////////////////////////////////////////////////////
CVehicleSeat::CVehicleSeat()
{
	m_pVehicle = 0;
	m_pPart = 0;
	m_pVar = 0;
}

//////////////////////////////////////////////////////////////////////////
bool CVehicleSeat::Init(CBaseObject* prev, const string& file)
{
	bool res = CBaseObject::Init(prev, file);
	return res;
}

//////////////////////////////////////////////////////////////////////////
void CVehicleSeat::Display(CObjectRenderHelper& objRenderHelper)
{
	return;
	
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	COLORREF color = GetColor();

	if (IsSelected())
	{
		dc.SetSelectedColor(0.6f);
	}

	//if (dc.flags & DISPLAY_2D)
	{
		AABB box;
		GetLocalBounds(box);
		dc.SetColor(color, 0.8f);
		dc.PushMatrix(GetWorldTM());
		dc.SetLineWidth(2);
		dc.DrawWireBox(box.min, box.max);
		dc.SetLineWidth(0);
		dc.PopMatrix();
	}

	DrawDefault(dc);
}

//////////////////////////////////////////////////////////////////////////
bool CVehicleSeat::HitTest(HitContext& hc)
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CVehicleSeat::GetBoundBox(AABB& box)
{
	box.Reset();
	// Transform local bounding box into world space.
	GetLocalBounds(box);
	box.SetTransformedAABB(GetWorldTM(), box);
}

//////////////////////////////////////////////////////////////////////////
void CVehicleSeat::GetLocalBounds(AABB& box)
{
	// return local bounds
	// todo: take box around sit and eye helpers?
}

//////////////////////////////////////////////////////////////////////////
void CVehicleSeat::UpdateObjectFromVar()
{
	if (!m_pVar)
	{
		// create default seat here
		m_pVar = CreateDefaultChildOf("Seats");
		IVariable* pSeats = GetChildVar(m_pVehicle->GetVariable(), "Seats");
		if (pSeats && m_pVar)
			pSeats->AddVariable(m_pVar);

		if (IVariable* pPart = GetChildVar(m_pVar, "part"))
		{
			pPart->AddOnSetCallback(functor(*this, &CVehicleSeat::OnSetPart));
		}
	}

	if (IVariable* pName = GetChildVar(m_pVar, "name"))
	{
		// update obj name
		string name;
		pName->Get(name);
		SetName(name);
	}

	if (IVariable* pPart = GetChildVar(m_pVar, "part"))
	{
		string val;
		pPart->Get(val);

		DetachThis();
		CBaseObject* pPartObj = GetIEditor()->GetObjectManager()->FindObject(val);

		if (val == "" || !pPartObj || !pPartObj->IsKindOf(RUNTIME_CLASS(CVehiclePart)))
		{
			m_pVehicle->AttachChild(this);
		}
		else
		{
			pPartObj->AttachChild(this);
		}
	}

}

//////////////////////////////////////////////////////////////////////////
void CVehicleSeat::SetVariable(IVariable* pVar)
{
	m_pVar = pVar;
	if (pVar)
		UpdateObjectFromVar();

	if (IVariable* pPart = GetChildVar(m_pVar, "part"))
	{
		pPart->AddOnSetCallback(functor(*this, &CVehicleSeat::OnSetPart));
	}
}

//////////////////////////////////////////////////////////////////////////
void CVehicleSeat::OnSetPart(IVariable* pVar)
{
	UpdateObjectFromVar();
}

//////////////////////////////////////////////////////////////////////////
void CVehicleSeat::Done()
{
	VeedLog("[CVehicleSeat:Done] <%s>", GetName());

	int nChildCount = GetChildCount();
	for (int i = nChildCount - 1; i >= 0; --i)
	{
		CBaseObject* pChild = GetChild(i);
		VeedLog("[CVehicleSeat]: deleting %s", pChild->GetName());
		GetIEditor()->DeleteObject(GetChild(i));
	}

	CBaseObject::Done();
}

//////////////////////////////////////////////////////////////////////////
void CVehicleSeat::UpdateVarFromObject()
{
	assert(m_pVar);
	if (!m_pVar)
		return;

	IEntity* pEnt = m_pVehicle->GetCEntity()->GetIEntity();
	if (!pEnt)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "[CVehicleSeat::UpdateVariable] pEnt is null, returning");
		return;
	}

	if (IVariable* pVar = GetChildVar(m_pVar, "name"))
		pVar->Set(GetName());

	// update part variable
	CBaseObject* pParent = GetParent();
	IVariable* pVar = GetChildVar(m_pVar, "part");
	if (pParent && pVar)
	{
		if (pParent == m_pVehicle)
			pVar->Set("");
		else
			pVar->Set(pParent->GetName());
	}
}

//////////////////////////////////////////////////////////////////////////
void CVehicleSeat::SetVehicle(CVehiclePrototype* pProt)
{
	m_pVehicle = pProt;
	if (pProt)
		SetWorldTM(pProt->GetWorldTM());

}

//////////////////////////////////////////////////////////////////////////
void CVehicleSeat::AddWeapon(int weaponType, CVehicleWeapon* pWeap, IVariable* pVar /*=0*/)
{
	AttachChild(pWeap, true);
	pWeap->SetVehicle(m_pVehicle);

	IVariable* pWeaponVar = pVar;

	// if no variable, create and add it
	if (!pWeaponVar)
	{
		const char* category = (weaponType == WEAPON_PRIMARY) ? "Primary" : "Secondary";

		IVariable* pWeapons = GetChildVar(m_pVar, category, true);
		if (pWeapons)
		{
			pWeaponVar = CreateWeaponVar();
			pWeapons->AddVariable(pWeaponVar);
		}
	}

	pWeap->SetVariable(pWeaponVar);
	pWeap->UpdateObjectFromVar();
}

//////////////////////////////////////////////////////////////////////////
void CVehicleSeat::OnObjectEvent(CBaseObject* node, int event)
{
	if (event == OBJECT_ON_DELETE)
	{
		VeedLog("[CVehicleSeat]: ON_DELETE for %s", node->GetName());
		// delete variable
		if (IVeedObject* pVO = IVeedObject::GetVeedObject(node))
		{
			if (pVO->DeleteVar())
			{
				if (m_pVar)
					m_pVar->DeleteVariable(pVO->GetVariable(), true);
				pVO->SetVariable(0);
				VeedLog("[CVehiclePart] deleting var for %s", node->GetName());
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVehicleSeat::AttachChild(CBaseObject* child, bool bKeepPos, bool bInvalidateTM)
{
	child->AddEventListener(functor(*this, &CVehicleSeat::OnObjectEvent));

	CBaseObject::AttachChild(child, bKeepPos, bInvalidateTM);
}

