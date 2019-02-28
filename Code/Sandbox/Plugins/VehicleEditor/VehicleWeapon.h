// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VehicleWeapon_h__
#define __VehicleWeapon_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "Objects/BaseObject.h"
#include "VehicleDialogComponent.h"

class CVehiclePart;
class CVehicleSeat;
class CVehiclePrototype;

/*!
 *	CVehicleWeapon represents a mounted vehicle weapon and is supposed
 *  to be used as children of a VehicleSeat.
 */
class CVehicleWeapon : public CBaseObject, public CVeedObject
{
public:
	DECLARE_DYNCREATE(CVehicleWeapon)
	~CVehicleWeapon(){}

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	void Done();

	void Display(CObjectRenderHelper& objRenderHelper);

	void GetBoundBox(AABB& box);
	void GetLocalBounds(AABB& box);
	bool HitTest(HitContext& hc);
	void Serialize(CObjectArchive& ar) {}
	//////////////////////////////////////////////////////////////////////////

	// Ovverides from IVeedObject.
	//////////////////////////////////////////////////////////////////////////
	void        UpdateVarFromObject();
	void        UpdateObjectFromVar()        {}

	const char* GetElementName()             { return "Weapon"; }
	virtual int GetIconIndex()               { return VEED_WEAPON_ICON; }

	void        SetVariable(IVariable* pVar) { m_pVar = pVar; }
	//////////////////////////////////////////////////////////////////////////

	void SetVehicle(CVehiclePrototype* pProt) { m_pVehicle = pProt; }

protected:
	CVehicleWeapon();
	void DeleteThis() { delete this; };

	CVehicleSeat*      m_pSeat;
	CVehiclePrototype* m_pVehicle;
};

/*!
 * Class Description of VehicleWeapon.
 */
class CVehicleWeaponClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()   { return OBJTYPE_OTHER; };
	const char*    ClassName()       { return "VehicleWeapon"; };
	const char*    Category()        { return ""; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CVehicleWeapon); };
};

#endif // __VehicleWeapon_h__

