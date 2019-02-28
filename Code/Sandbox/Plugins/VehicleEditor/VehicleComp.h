// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VehicleComponent_h__
#define __VehicleComponent_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "Objects/BaseObject.h"
#include "Util/Variable.h"

class CVehicleHelper;
class CVehiclePart;
class CVehiclePrototype;

#include "VehicleDialogComponent.h"

/*!
 *	CVehicleComponent represents an editable vehicle Component
 *
 */
class CVehicleComponent
	: public CBaseObject
	  , public CVeedObject
{
public:
	DECLARE_DYNCREATE(CVehicleComponent)

	//////////////////////////////////////////////////////////////////////////
	// Overwrites from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	virtual void Done();
	virtual void InitVariables() {}
	virtual void Display(CObjectRenderHelper& objRenderHelper);

	virtual bool HitTest(HitContext& hc);

	virtual void GetLocalBounds(AABB& box);
	virtual void GetBoundBox(AABB& box);

	virtual void Serialize(CObjectArchive& ar) {}
	/////////////////////////////////////////////////////////////////////////

	// Overrides from IVeedObject
	/////////////////////////////////////////////////////////////////////////
	virtual void        UpdateVarFromObject();
	virtual void        UpdateObjectFromVar();

	virtual const char* GetElementName() { return "Component"; }
	virtual int         GetIconIndex();

	virtual void        SetVariable(IVariable* pVar);
	virtual void        UpdateScale(float scale);
	/////////////////////////////////////////////////////////////////////////

	void CreateVariable();
	void ResetPosition();

	void SetVehicle(CVehiclePrototype* pProt) { m_pVehicle = pProt; }

protected:
	CVehicleComponent();
	virtual void DeleteThis() { delete this; }
	virtual void PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx);

	virtual void OnTransform();

private:
	void UpdateObjectNameFromVar();
	void UpdateObjectBoundsFromVar();

	void UpdateVarNameFromObject();
	void UpdateVarBoundsFromObject();

	bool GetUseBoundsFromParts() const;

	bool HasZeroSize() const;

private:
	CVehiclePrototype* m_pVehicle;

	IVariablePtr       m_pPosition;
	IVariablePtr       m_pSize;
	IVariablePtr       m_pUseBoundsFromParts;
};

/*!
 * Class Description of VehicleComponent.
 */
class CVehicleComponentClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()   { return OBJTYPE_OTHER; };
	const char*    ClassName()       { return "VehicleComponent"; };
	const char*    Category()        { return ""; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CVehicleComponent); };
};

#endif // __VehicleComponent_h__

