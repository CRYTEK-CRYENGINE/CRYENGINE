// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __smartobject_h__
#define __smartobject_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "Objects/EntityObject.h"
#include "SmartObjectsEditorDialog.h"

/*!
 *	CSmartObject is an special entity visible in editor but invisible in game, that's registered with AI system as an smart object.
 *
 */
class CSmartObject : public CEntityObject
{
protected:
	IStatObj*                             m_pStatObj;
	AABB                                  m_bbox;
	IMaterial*                            m_pHelperMtl;

	CSOLibrary::CClassTemplateData const* m_pClassTemplate;

public:
	DECLARE_DYNCREATE(CSmartObject)

	//////////////////////////////////////////////////////////////////////////
	// Overrides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	virtual bool  Init(CBaseObject* prev, const string& file);
	virtual void  InitVariables() {}
	virtual void  Done();
	virtual void  Display(CObjectRenderHelper& objRenderHelper);
	virtual bool  HitTest(HitContext& hc);
	virtual void  GetLocalBounds(AABB& box);
	bool          IsScalable() const override { return false; }
	virtual void  OnEvent(ObjectEvent eventID);
	virtual void  GetScriptProperties(XmlNodeRef xmlEntityNode);

	//////////////////////////////////////////////////////////////////////////

	IStatObj* GetIStatObj();

protected:
	//! Dtor must be protected.
	CSmartObject();
	~CSmartObject();

	float      GetRadius();

	void       DeleteThis() { delete this; }
	IMaterial* GetHelperMaterial();

public:
	void OnPropertyChange(IVariable* var);
};

/*!
 * Class Description of SmartObject.
 */
class CSmartObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_AIPOINT; }
	const char*    ClassName()         { return "SmartObject"; }
	const char*    Category()          { return "AI"; }
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CSmartObject); }
	virtual bool   IsCreatable() const override { return gEnv->pEntitySystem->GetClassRegistry()->FindClass("SmartObject") != nullptr; }
};

#endif // __smartobject_h__

