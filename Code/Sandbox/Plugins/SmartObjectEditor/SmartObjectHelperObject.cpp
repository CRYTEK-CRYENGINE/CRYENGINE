// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryAISystem/IAISystem.h>
#include "Objects/EntityObject.h"
#include "Objects/DisplayContext.h"

#include "Viewport.h"
#include "SmartObjectHelperObject.h"
#include "Util/MFCUtil.h"

REGISTER_CLASS_DESC(CSmartObjectHelperClassDesc);

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CSmartObjectHelperObject, CBaseObject)

#define RADIUS 0.05f

//////////////////////////////////////////////////////////////////////////
CSmartObjectHelperObject::CSmartObjectHelperObject()
{
	m_pVar = NULL;
	m_pSmartObjectClass = NULL;
	m_pEntity = NULL;
	//	m_fromGeometry = false;
}

CSmartObjectHelperObject::~CSmartObjectHelperObject()
{
}


//////////////////////////////////////////////////////////////////////////
void CSmartObjectHelperObject::UpdateVarFromObject()
{
	assert(m_pVar);
	if (!m_pVar || !m_pSmartObjectClass)
		return;

	if (!m_pEntity)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[CSmartObjectHelperObject::UpdateVariable] m_pEntity is null, returning");
		return;
	}

	if (IVariable* pVar = CMFCUtils::GetChildVar(m_pVar, "name"))
		pVar->Set(GetName());

	if (IVariable* pVar = CMFCUtils::GetChildVar(m_pVar, "position"))
	{
		Vec3 pos = m_pEntity->GetWorldTM().GetInvertedFast().TransformPoint(GetWorldPos());
		pVar->Set(pos);
	}

	if (IVariable* pVar = CMFCUtils::GetChildVar(m_pVar, "direction"))
	{
		Matrix33 relTM = Matrix33(m_pEntity->GetWorldTM().GetInvertedFast()) * Matrix33(GetWorldTM());
		Vec3 dir = relTM.TransformVector(FORWARD_DIRECTION);
		pVar->Set(dir);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectHelperObject::Done()
{
	CBaseObject::Done();
}

//////////////////////////////////////////////////////////////////////////
bool CSmartObjectHelperObject::Init(CBaseObject* prev, const string& file)
{
	SetColor(RGB(255, 255, 0));
	return CBaseObject::Init(prev, file);
}

//////////////////////////////////////////////////////////////////////////
bool CSmartObjectHelperObject::HitTest(HitContext& hc)
{
	Vec3 origin = GetWorldPos();
	float radius = RADIUS;

	Vec3 w = origin - hc.raySrc;
	w = hc.rayDir.Cross(w);
	float d = w.GetLength();

	if (d < radius + hc.distanceTolerance)
	{
		Vec3 i0;
		hc.object = this;
		if (Intersect::Ray_SphereFirst(Ray(hc.raySrc, hc.rayDir), Sphere(origin, radius), i0))
		{
			hc.dist = hc.raySrc.GetDistance(i0);
			return true;
		}
		hc.dist = hc.raySrc.GetDistance(origin);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectHelperObject::Display(CObjectRenderHelper& objRenderHelper)
{
	COLORREF color = GetColor();
	float radius = RADIUS;
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();

	//dc.SetColor( color, 0.5f );
	//dc.DrawBall( GetPos(), radius );

	if (IsSelected())
	{
		dc.SetSelectedColor(0.6f);
	}
	if (!IsHighlighted())
	{
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
	}

	DrawDefault(dc);
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectHelperObject::GetBoundBox(AABB& box)
{
	// Transform local bounding box into world space.
	GetLocalBounds(box);
	box.SetTransformedAABB(GetWorldTM(), box);
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectHelperObject::GetLocalBounds(AABB& box)
{
	// return local bounds
	float r = RADIUS;
	box.min = -Vec3(r, r, r);
	box.max = Vec3(r, r, r);
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectHelperObject::GetBoundSphere(Vec3& pos, float& radius)
{
	pos = GetPos();
	radius = RADIUS;
}

