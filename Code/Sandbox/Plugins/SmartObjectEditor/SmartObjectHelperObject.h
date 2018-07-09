// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SmartObjectHelperObject_h__
#define __SmartObjectHelperObject_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "Objects/BaseObject.h"

class CEntityObject;
class CSmartObjectClass;

/*!
 *  CSmartObjectHelperObject is a simple helper object for specifying a position and orientation
 */
class CSmartObjectHelperObject : public CBaseObject
{
public:
	DECLARE_DYNCREATE(CSmartObjectHelperObject)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init(CBaseObject* prev, const string& file);
	void Done();

	void Display(CObjectRenderHelper& objRenderHelper);

	void GetBoundSphere(Vec3& pos, float& radius);
	void GetBoundBox(AABB& box);
	void GetLocalBounds(AABB& box);
	bool HitTest(HitContext& hc);

	//////////////////////////////////////////////////////////////////////////
	void UpdateVarFromObject();
	void UpdateObjectFromVar()
	{
	}

	//	const char* GetElementName() { return "Helper"; }

	IVariable* GetVariable()                { return m_pVar; }
	void       SetVariable(IVariable* pVar) { m_pVar = pVar; }
	//////////////////////////////////////////////////////////////////////////

	//	void IsFromGeometry( bool b ) { m_fromGeometry = b; }
	//	bool IsFromGeometry() { return m_fromGeometry; }

	CSmartObjectClass* GetSmartObjectClass() const                    { return m_pSmartObjectClass; }
	void               SetSmartObjectClass(CSmartObjectClass* pClass) { m_pSmartObjectClass = pClass; }

	CEntityObject*     GetSmartObjectEntity() const                   { return m_pEntity; }
	void               SetSmartObjectEntity(CEntityObject* pEntity)   { m_pEntity = pEntity; }

protected:
	//! Dtor must be protected.
	CSmartObjectHelperObject();
	~CSmartObjectHelperObject();

	void DeleteThis() { delete this; };

	float      m_innerRadius;
	float      m_outerRadius;

	IVariable* m_pVar;

	//	bool m_fromGeometry;

	CSmartObjectClass* m_pSmartObjectClass;

	CEntityObject*     m_pEntity;
};

/*!
 * Class Description of SmartObjectHelper.
 */
class CSmartObjectHelperClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()   { return OBJTYPE_OTHER; };
	const char*    ClassName()       { return "SmartObjectHelper"; };
	const char*    Category()        { return ""; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CSmartObjectHelperObject); };
};

#endif // __SmartObjectHelperObject_h__

