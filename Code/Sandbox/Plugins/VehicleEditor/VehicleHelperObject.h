// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VehicleHelper_h__
#define __VehicleHelper_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "Objects/BaseObject.h"
#include "VehicleDialogComponent.h"

class CVehiclePart;
class CVehiclePrototype;

/*!
 *	CVehicleHelper is a simple helper object for specifying a position and orientation
 *  in a vehicle part coordinate frame
 *
 */
class CVehicleHelper : public CBaseObject, public CVeedObject
{
public:
	DECLARE_DYNCREATE(CVehicleHelper)
	~CVehicleHelper();
	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init(CBaseObject* prev, const string& file);
	void Done();

	void Display(CObjectRenderHelper& objRenderHelper) override;

	void GetBoundSphere(Vec3& pos, float& radius);
	void GetBoundBox(AABB& box);
	void GetLocalBounds(AABB& box);
	bool HitTest(HitContext& hc);

	void Serialize(CObjectArchive& ar) {}
	//////////////////////////////////////////////////////////////////////////

	// Ovverides from IVeedObject.
	//////////////////////////////////////////////////////////////////////////
	void               UpdateVarFromObject();
	void               UpdateObjectFromVar();

	const char*        GetElementName() { return "Helper"; }
	virtual int        GetIconIndex()   { return (IsFromGeometry() ? VEED_ASSET_HELPER_ICON : VEED_HELPER_ICON); }

	virtual IVariable* GetVariable()    { return m_pVar; }
	virtual void       SetVariable(IVariable* pVar);
	void               UpdateScale(float scale);
	//////////////////////////////////////////////////////////////////////////

	void SetIsFromGeometry(bool b);
	bool IsFromGeometry()                     { return m_fromGeometry; }

	void SetVehicle(CVehiclePrototype* pProt) { m_pVehicle = pProt; }

protected:
	//! Dtor must be protected.
	CVehicleHelper();
	void         DeleteThis() { delete this; };
	void         PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx);

	virtual void OnTransform();

	float              m_innerRadius;
	float              m_outerRadius;

	IVariable*         m_pVar;

	bool               m_fromGeometry;

	CVehiclePrototype* m_pVehicle;
};

/*!
 * Class Description of VehicleHelper.
 */
class CVehicleHelperClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()   { return OBJTYPE_OTHER; };
	const char*    ClassName()       { return "VehicleHelper"; };
	const char*    Category()        { return ""; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CVehicleHelper); };
};

#endif // __VehicleHelper_h__

