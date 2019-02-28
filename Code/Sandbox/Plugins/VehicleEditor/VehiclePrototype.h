// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VehiclePrototype_h__
#define __VehiclePrototype_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "Objects/EntityObject.h"
#include "VehicleDialogComponent.h"

struct IVehicleData;
class CVehicleEditorDialog;
class CVehiclePart;
class CVehicleComponent;
class CVehicleHelper;

struct IVehiclePrototype
{
	virtual IVehicleData* GetVehicleData() = 0;
};

/*!
 *	CVehiclePrototype represents an editable Vehicle.
 *
 */
class CVehiclePrototype : public CBaseObject, public CVeedObject, public IVehiclePrototype
{
public:
	DECLARE_DYNCREATE(CVehiclePrototype)

	//////////////////////////////////////////////////////////////////////////
	// Overwrites from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init(CBaseObject* prev, const string& file);
	void InitVariables() {};
	void Display(CObjectRenderHelper& objRenderHelper);
	void Done();

	bool HitTest(HitContext& hc);

	void GetLocalBounds(AABB& box);
	void GetBoundBox(AABB& box);

	void AttachChild(CBaseObject* child, bool bKeepPos = true, bool bInvalidateTM = true);
	void RemoveChild(CBaseObject* node);

	void Serialize(CObjectArchive& ar);
	//////////////////////////////////////////////////////////////////////////

	// IVeedObject
	//////////////////////////////////////////////////////////////////////////
	void        UpdateVarFromObject() {}
	void        UpdateObjectFromVar() {}

	const char* GetElementName()      { return "Vehicle"; }
	IVariable*  GetVariable();

	virtual int GetIconIndex() { return VEED_VEHICLE_ICON; }
	//////////////////////////////////////////////////////////////////////////

	void                  SetVehicleEntity(CEntityObject* pEnt);
	CEntityObject*        GetCEntity() { return m_vehicleEntity; }

	bool                  hasEntity()  { return m_vehicleEntity != 0; }
	bool                  ReloadEntity();

	IVehicleData*         GetVehicleData();
	void                  ApplyClonedData();

	void                  AddPart(CVehiclePart* pPart);
	void                  AddComponent(CVehicleComponent* pComp);

	void                  AddHelper(CVehicleHelper* pHelper, IVariable* pHelperVar = 0);
	const CVehicleHelper* GetHelper(const string& name) const;

	void                  OnObjectEvent(CBaseObject* node, int event);

protected:
	CVehiclePrototype();
	void DeleteThis();

	CEntityObject* m_vehicleEntity;

	// root of vehicle data tree
	IVehicleData* m_pVehicleData;
	IVariablePtr  m_pClone;

	string       m_name;

	friend class CVehicleEditorDialog;

};

/*!
 * Class Description of VehicleObject.
 */
class CVehiclePrototypeClassDesc : public CObjectClassDesc
{
public:
	ObjectType          GetObjectType()                     { return OBJTYPE_OTHER; };
	const char*         ClassName()                         { return "VehiclePrototype"; };
	const char*         Category()                          { return ""; };
	CRuntimeClass*      GetRuntimeClass()                   { return RUNTIME_CLASS(CVehiclePrototype); };
	const char*         GetFileSpec()                       { return "Scripts/Entities/Vehicles/Implementations/Xml/*.xml"; };
	virtual const char* GetDataFilesFilterString() override { return GetFileSpec(); }
};

#endif // __VehicleObject_h__

