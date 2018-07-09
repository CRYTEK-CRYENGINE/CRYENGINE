// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __aianchor_h__
#define __aianchor_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "EntityObject.h"

// forward declaration.
struct IAIObject;

/*!
 *	CAIAnchor is an special tag point,that registered with AI, and tell him what to do when AI is idle.
 *
 */
class CAIAnchor : public CEntityObject
{
public:
	DECLARE_DYNCREATE(CAIAnchor)

	//////////////////////////////////////////////////////////////////////////
	// Overrides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	virtual bool  Init(CBaseObject* prev, const string& file);
	virtual void  InitVariables() {}
	virtual void  Display(CObjectRenderHelper& objRenderHelper);
	virtual bool  HitTest(HitContext& hc);
	virtual void  GetLocalBounds(AABB& box);
	virtual void  SetScale(const Vec3& scale) {} // Ignore scale
	virtual void  SetHelperScale(float scale) { m_helperScale = scale; };
	virtual float GetHelperScale()            { return m_helperScale; };
	//////////////////////////////////////////////////////////////////////////

protected:
	//! Ctor must be protected.
	CAIAnchor();

	float GetRadius();

	void  DeleteThis() { delete this; };

	static float m_helperScale;
};

/*!
 * Class Description of TagPoint.
 */
class CAIAnchorClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_AIPOINT; };
	const char*    ClassName()         { return "AIAnchor"; };
	const char*    Category()          { return "AI"; };
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CAIAnchor); };
	virtual bool   IsCreatable() const override { return gEnv->pEntitySystem->GetClassRegistry()->FindClass("AIAnchor"); }
};

#endif // __aianchor_h__

