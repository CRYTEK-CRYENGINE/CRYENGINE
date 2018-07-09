// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __AIReinforcementSpot_h__
#define __AIReinforcementSpot_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "EntityObject.h"

// forward declaration.
struct IAIObject;

/*!
 *	CAIReinforcementSpot is an special tag point,that is registered with AI, and tell him what to do when AI calling reinforcements.
 *
 */
class CAIReinforcementSpot : public CEntityObject
{
public:
	DECLARE_DYNCREATE(CAIReinforcementSpot)

	//////////////////////////////////////////////////////////////////////////
	// Overrides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	virtual bool  Init(CBaseObject* prev, const string& file);
	virtual void  InitVariables() {};
	virtual void  Done();
	virtual void  Display(CObjectRenderHelper& objRenderHelper);
	virtual bool  HitTest(HitContext& hc);
	virtual void  GetLocalBounds(AABB& box);
	bool          IsScalable() const override { return false; }
	virtual void  SetHelperScale(float scale) { m_helperScale = scale; };
	virtual float GetHelperScale()            { return m_helperScale; };
	//////////////////////////////////////////////////////////////////////////

protected:
	//! Dtor must be protected.
	CAIReinforcementSpot();

	float GetRadius();

	void  DeleteThis() { delete this; };

	static float m_helperScale;
};

/*!
 * Class Description of ReinforcementSpot.
 */
class CAIReinforcementSpotClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_AIPOINT; };
	const char*    ClassName()         { return "AIReinforcementSpot"; };
	const char*    Category()          { return "AI"; };
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CAIReinforcementSpot); };
	virtual bool   IsCreatable() const override { return gEnv->pEntitySystem->GetClassRegistry()->FindClass("AIReinforcementSpot"); }
};

#endif // __AIReinforcementSpot_h__

