// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RulerPoint.h"
#include "Ruler.h"

#include <CryRenderer/IRenderAuxGeom.h>

//////////////////////////////////////////////////////////////////////////
CRulerPoint::CRulerPoint()
	: m_type(eType_Invalid)
	, m_vPoint(ZERO)
	, m_ObjectGUID(CryGUID::Null())
{
	Reset();
}

//////////////////////////////////////////////////////////////////////////
CRulerPoint& CRulerPoint::operator=(CRulerPoint const& other)
{
	if (this != &other)
	{
		Reset(); // Manage deselect of current object, etc.

		m_type = other.m_type;
		m_vPoint = other.m_vPoint;
		m_ObjectGUID = other.m_ObjectGUID;
		m_sphereScale = other.m_sphereScale;
		m_sphereTrans = other.m_sphereTrans;
	}

	return *this;
}

//////////////////////////////////////////////////////////////////////////
void CRulerPoint::Reset()
{
	// Kill highlight of current object
	CBaseObject* pObject = GetObject();
	if (pObject)
	{
		pObject->SetHighlight(false);
	}

	m_type = eType_Invalid;
	m_vPoint.zero();
	m_ObjectGUID = CryGUID::Null();
}

//////////////////////////////////////////////////////////////////////////
void CRulerPoint::Render(IRenderer* pRenderer)
{
	CRY_ASSERT(pRenderer);
	IRenderAuxGeom* pAuxGeom = pRenderer->GetIRenderAuxGeom();

	switch (m_type)
	{
	case eType_Point:
		{
			Vec3 vOffset(0.1f, 0.1f, 0.1f);
			pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags | e_AlphaBlended);
			pAuxGeom->DrawSphere(m_vPoint, m_sphereScale, ColorF(1, 1, 1, m_sphereTrans));
			pAuxGeom->DrawAABB(AABB(m_vPoint - vOffset * m_sphereScale, m_vPoint + vOffset * m_sphereScale), false, ColorF(0.0f, 1.0f, 0.0f, 1.0f), eBBD_Faceted);
		}
		break;

	case eType_Object:
		{
			CBaseObject* pObject = GetObject();
			if (pObject)
				pObject->SetHighlight(true);
		}
		break;

	default:
		return;   // No extra drawing
	}
}

//////////////////////////////////////////////////////////////////////////
void CRulerPoint::Set(const Vec3& vPos)
{
	Reset();

	m_type = eType_Point;
	m_vPoint = vPos;
}

//////////////////////////////////////////////////////////////////////////
void CRulerPoint::Set(CBaseObject* pObject)
{
	Reset();

	m_type = eType_Object;
	m_ObjectGUID = (pObject ? pObject->GetId() : CryGUID::Null());
}

//////////////////////////////////////////////////////////////////////////
void CRulerPoint::SetHelperSettings(float scale, float trans)
{
	m_sphereScale = scale;
	m_sphereTrans = trans;
}

//////////////////////////////////////////////////////////////////////////
bool CRulerPoint::IsEmpty() const
{
	bool bResult = true;

	switch (m_type)
	{
	case eType_Invalid:
		bResult = true;
		break;

	case eType_Point:
		bResult = m_vPoint.IsZero();
		break;

	case eType_Object:
		bResult = (GetObject() == 0);
		break;
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
Vec3 CRulerPoint::GetPos() const
{
	Vec3 vResult(ZERO);

	switch (m_type)
	{
	case eType_Point:
		vResult = m_vPoint;
		break;

	case eType_Object:
		{
			CBaseObject* pObject = GetObject();
			if (pObject)
				vResult = pObject->GetWorldPos();
		}
		break;
	}

	return vResult;
}

//////////////////////////////////////////////////////////////////////////
Vec3 CRulerPoint::GetMidPoint(const CRulerPoint& otherPoint) const
{
	Vec3 vResult(ZERO);

	if (!IsEmpty() && !otherPoint.IsEmpty())
	{
		vResult = GetPos() + (otherPoint.GetPos() - GetPos()) * 0.5f;
	}
	else if (!IsEmpty())
	{
		vResult = GetPos();
	}
	else
	{
		vResult = otherPoint.GetPos();
	}

	return vResult;
}

//////////////////////////////////////////////////////////////////////////
float CRulerPoint::GetDistance(const CRulerPoint& otherPoint) const
{
	float fResult = 0.0f;

	if (!IsEmpty() && !otherPoint.IsEmpty())
	{
		fResult = GetPos().GetDistance(otherPoint.GetPos());
	}

	return fResult;
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CRulerPoint::GetObject() const
{
	CBaseObject* pResult = NULL;

	if (m_type == eType_Object)
	{
		pResult = GetIEditorImpl()->GetObjectManager()->FindObject(m_ObjectGUID);
	}

	return pResult;
}

