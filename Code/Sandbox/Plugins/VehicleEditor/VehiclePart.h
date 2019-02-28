// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VehiclePart_h__
#define __VehiclePart_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <list>

#include "Objects/EntityObject.h"
#include "Util/Variable.h"

class CVehicleHelper;
class CVehicleSeat;
class CVehiclePrototype;

#include "VehicleDialogComponent.h"

#define PARTCLASS_WHEEL "SubPartWheel"
#define PARTCLASS_MASS  "MassBox"
#define PARTCLASS_LIGHT "Light"
#define PARTCLASS_TREAD "Tread"

typedef std::list<CVehicleHelper*> THelperList;

/*!
 *	CVehiclePart represents an editable vehicle part.
 *
 */
class CVehiclePart : public CBaseObject, public CVeedObject
{
public:
	DECLARE_DYNCREATE(CVehiclePart)

	//////////////////////////////////////////////////////////////////////////
	// Overwrites from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init(CBaseObject* prev, const string& file);
	void Done();
	void InitVariables() {};
	void Display(CObjectRenderHelper& objRenderHelper);

	bool HitTest(HitContext& hc);

	void GetLocalBounds(AABB& box);
	void GetBoundBox(AABB& box);

	void AttachChild(CBaseObject* child, bool bKeepPos = true, bool bInvalidateTM = true);
	void RemoveChild(CBaseObject* node);

	void Serialize(CObjectArchive& ar) {}
	/////////////////////////////////////////////////////////////////////////

	void    AddPart(CVehiclePart* pPart);
	void    SetMainPart(bool bMain) { m_isMain = bMain; }
	bool    IsMainPart() const      { return m_isMain; }
	string GetPartClass();

	//! returns whether this part is a leaf part (false means it can have children)
	bool       IsLeaf();

	void       SetVariable(IVariable* pVar);
	IVariable* GetVariable() { return m_pVar; }

	// Overrides from IVeedObject
	/////////////////////////////////////////////////////////////////////////
	void        UpdateVarFromObject();
	void        UpdateObjectFromVar();

	const char* GetElementName() { return "Part"; }
	int         GetIconIndex();
	void        UpdateScale(float scale);
	void        OnTreeSelection();
	/////////////////////////////////////////////////////////////////////////

	void SetVehicle(CVehiclePrototype* pProt) { m_pVehicle = pProt; }

	void OnObjectEvent(CBaseObject* node, int event);

protected:
	CVehiclePart();
	void DeleteThis() { delete this; };

	void UpdateFromVar();

	void OnSetClass(IVariable* pVar);
	void OnSetPos(IVariable* pVar);

	void DrawRotationLimits(DisplayContext& dc, IVariable* pSpeed, IVariable* pLimits, IVariable* pHelper, int axis);

	CVehiclePrototype* m_pVehicle;
	CVehiclePart*      m_pParent;

	bool               m_isMain;
	IVariable*         m_pVar;

	// pointer for saving per-frame lookups
	IVariable* m_pYawSpeed;
	IVariable* m_pYawLimits;
	IVariable* m_pPitchSpeed;
	IVariable* m_pPitchLimits;
	IVariable* m_pHelper;
	IVariable* m_pPartClass;
};

/*!
 * Class Description of VehiclePart.
 */
class CVehiclePartClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()   { return OBJTYPE_OTHER; };
	const char*    ClassName()       { return "VehiclePart"; };
	const char*    Category()        { return ""; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CVehiclePart); };
};

#endif // __VehiclePart_h__

