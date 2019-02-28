// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Ruler.h"
#include "Viewport.h"

#include <CryRenderer/IRenderAuxGeom.h>

#include "QtUtil.h"

static SRulerPreferences rulerPreferences;
REGISTER_PREFERENCES_PAGE_PTR(SRulerPreferences, &rulerPreferences);

//////////////////////////////////////////////////////////////////////////
CRuler::CRuler()
	: m_bActive(false)
	, m_MouseOverObject(CryGUID::Null())
	, m_sphereScale(0.5f)
	, m_sphereTrans(0.5f)
{
	// can't create path agents here, AI manager doesn't know how many there are yet
}

//////////////////////////////////////////////////////////////////////////
CRuler::~CRuler()
{
	SetActive(false);
}

//////////////////////////////////////////////////////////////////////////
void CRuler::SetActive(bool bActive)
{
	if (m_bActive != bActive)
	{
		m_bActive = bActive;

		if (m_bActive)
		{
			m_sphereScale = rulerPreferences.rulerSphereScale;
			m_sphereTrans = rulerPreferences.rulerSphereTrans;
		}

		// Reset
		m_startPoint.Reset();
		m_endPoint.Reset();

		CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(m_MouseOverObject);
		if (pObject)
		{
			pObject->SetHighlight(false);
		}
		m_MouseOverObject = CryGUID::Null();
	}
}

//////////////////////////////////////////////////////////////////////////
void CRuler::Update()
{
	if (!IsActive())
		return;

	if (GetAsyncKeyState(VK_ESCAPE) & (1 << 15))  // MSDN: If the most significant bit is set, the key is down
	{
		SetActive(false);
		return;
	}

	IRenderer* pRenderer = GetIEditorImpl()->GetSystem()->GetIRenderer();
	CRY_ASSERT(pRenderer);
	IRenderAuxGeom* pAuxGeom = pRenderer->GetIRenderAuxGeom();
	CRY_ASSERT(pAuxGeom);

	CViewport* pActiveView = GetIEditorImpl()->GetActiveView();
	if (pActiveView)
	{
		// Draw where cursor currently is
		if (!IsObjectSelectMode())
		{
			CPoint vCursorPoint;
			::GetCursorPos(&vCursorPoint);
			pActiveView->ScreenToClient(&vCursorPoint);
			Vec3 vCursorWorldPos = pActiveView->SnapToGrid(pActiveView->ViewToWorld(vCursorPoint));
			Vec3 vOffset(0.1f, 0.1f, 0.1f);
			pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags | e_AlphaBlended);
			pAuxGeom->DrawSphere(vCursorWorldPos, m_sphereScale, ColorF(0.5, 0.5, 0.5, m_sphereTrans));
			pAuxGeom->DrawAABB(AABB(vCursorWorldPos - vOffset * m_sphereScale, vCursorWorldPos + vOffset * m_sphereScale), false, ColorF(1.0f, 0.0f, 0.0f, 1.0f), eBBD_Faceted);
		}

		if (!m_startPoint.IsEmpty())
		{
			m_startPoint.Render(pRenderer);
		}
		if (!m_endPoint.IsEmpty())
		{
			CRY_ASSERT(!m_startPoint.IsEmpty());

			m_endPoint.Render(pRenderer);

			pAuxGeom->DrawLine(m_startPoint.GetPos(), Col_White, m_endPoint.GetPos(), Col_White, 2.0f);

			// Compute distances and output results
			const Vec3 diff = m_endPoint.GetPos() - m_startPoint.GetPos();
			const float distance = diff.GetLength();
			const float projectedDistance = diff.GetLength2D();
			const float elevationDiff = diff.z;
			
			const float lineHeight = 18.0f;
			Vec2 drawPosition(12.0f, 60.0f);

			IRenderAuxText::Draw2dLabel(drawPosition.x, drawPosition.y, 2.0f, Col_White, false, "Straight-line distance: %.3f", distance);
			drawPosition.y += lineHeight;
			IRenderAuxText::Draw2dLabel(drawPosition.x, drawPosition.y, 2.0f, Col_White, false, "Projected distance: %.3f", projectedDistance);
			drawPosition.y += lineHeight;
			IRenderAuxText::Draw2dLabel(drawPosition.x, drawPosition.y, 2.0f, Col_White, false, "Elevation difference: %.3f", elevationDiff);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CRuler::IsObjectSelectMode() const
{
	return QtUtil::IsModifierKeyDown(Qt::ShiftModifier);
}

//////////////////////////////////////////////////////////////////////////
void CRuler::UpdateRulerPoint(CViewport* pView, const CPoint& point, CRulerPoint& rulerPoint, bool bRequestPath)
{
	CRY_ASSERT(pView);

	const bool bObjectSelect = IsObjectSelectMode();

	rulerPoint.SetHelperSettings(m_sphereScale, m_sphereTrans);

	// Do entity hit check
	if (bObjectSelect)
	{
		HitContext hitInfo;
		pView->HitTest(point, hitInfo);

		CBaseObject* pHitObj = hitInfo.object;
		rulerPoint.Set(pHitObj);
	}
	else
	{
		Vec3 vWorldPoint = pView->SnapToGrid(pView->ViewToWorld(point));
		rulerPoint.Set(vWorldPoint);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CRuler::MouseCallback(CViewport* pView, EMouseEvent event, CPoint& point, int flags)
{
	bool bResult = IsActive();

	if (bResult)
	{
		switch (event)
		{
		case eMouseMove:
			OnMouseMove(pView, point, flags);
			break;
		case eMouseLUp:
			OnLButtonUp(pView, point, flags);
			break;
		case eMouseMUp:
			OnMButtonUp(pView, point, flags);
			break;
		case eMouseRUp:
			OnRButtonUp(pView, point, flags);
			break;
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CRuler::OnMouseMove(CViewport* pView, CPoint& point, int flags)
{
	CryGUID hitGUID = CryGUID::Null();

	if (IsObjectSelectMode())
	{
		// Check for hit entity
		HitContext hitInfo;
		pView->HitTest(point, hitInfo);
		CBaseObject* pHitObj = hitInfo.object;
		if (pHitObj)
		{
			hitGUID = pHitObj->GetId();
		}
	}

	if (hitGUID != m_MouseOverObject)
	{
		// Kill highlight on old
		CBaseObject* pOldObj = GetIEditorImpl()->GetObjectManager()->FindObject(m_MouseOverObject);
		if (pOldObj)
			pOldObj->SetHighlight(false);

		CBaseObject* pHitObj = GetIEditorImpl()->GetObjectManager()->FindObject(hitGUID);
		if (pHitObj)
			pHitObj->SetHighlight(true);

		m_MouseOverObject = hitGUID;
	}
}

//////////////////////////////////////////////////////////////////////////
void CRuler::OnLButtonUp(CViewport* pView, CPoint& point, int flags)
{
	if (m_startPoint.IsEmpty())
	{
		UpdateRulerPoint(pView, point, m_startPoint, false);
	}
	else if (m_endPoint.IsEmpty())
	{
		UpdateRulerPoint(pView, point, m_endPoint, true);
	}
	else
	{
		UpdateRulerPoint(pView, point, m_startPoint, false);
		m_endPoint.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////
void CRuler::OnMButtonUp(CViewport* pView, CPoint& point, int flags)
{
	if (m_startPoint.IsEmpty())
	{
		UpdateRulerPoint(pView, point, m_startPoint, false);
	}
	else
	{
		if (!m_endPoint.IsEmpty())
			m_startPoint = m_endPoint;

		UpdateRulerPoint(pView, point, m_endPoint, true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CRuler::OnRButtonUp(CViewport* pView, CPoint& point, int flags)
{
	/*m_startPoint.Reset();
	   m_endPoint.Reset();*/
}

