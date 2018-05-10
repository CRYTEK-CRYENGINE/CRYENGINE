// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AI\AIManager.h"
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
	, m_pPathAgents(NULL)
	, m_numPathAgents(0)
	, m_sphereScale(0.5f)
	, m_sphereTrans(0.5f)
{
	// can't create path agents here, AI manager doesn't know how many there are yet
}

//////////////////////////////////////////////////////////////////////////
CRuler::~CRuler()
{
	SetActive(false);

	delete[] m_pPathAgents;
}

//////////////////////////////////////////////////////////////////////////
bool CRuler::HasQueuedPaths() const
{
	for (uint32 i = 0; i < m_numPathAgents; ++i)
	{
		if (m_pPathAgents[i].HasQueuedPaths())
			return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CRuler::SetActive(bool bActive)
{
	if (m_bActive != bActive)
	{
		m_bActive = bActive;

		// create path agents if they don't already exist
		if (m_bActive && m_pPathAgents == NULL)
		{
			CAIManager* pAIManager = GetIEditorImpl()->GetAI();
			m_numPathAgents = pAIManager->GetNumPathTypes();
			m_pPathAgents = new CRulerPathAgent[m_numPathAgents];

			for (uint32 i = 0; i < m_numPathAgents; ++i)
			{
				string name = pAIManager->GetPathTypeName(i);
				const AgentPathfindingProperties* pPfP = pAIManager->GetPFPropertiesOfPathType(name);
				if (pPfP)
				{
					m_pPathAgents[i].SetType(pPfP->id, name);
				}
			}
		}

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

		for (uint32 i = 0; i < m_numPathAgents; ++i)
		{
			m_pPathAgents[i].ClearLastRequest();
		}
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

	static const ColorF colours[] =
	{
		Col_Blue,
		Col_Green,
		Col_Red,
		Col_Yellow,
		Col_Magenta,
		Col_Black,
	};

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

		uint32 x = 12, y = 60;

		if (!m_startPoint.IsEmpty())
		{
			//pAuxGeom->DrawSphere(m_startPoint.GetPos(), 1.0f, ColorB(255,255,255,255));
			m_startPoint.Render(pRenderer);
		}
		if (!m_endPoint.IsEmpty())
		{
			//pAuxGeom->DrawSphere(m_endPoint.GetPos(), 1.0f, ColorB(255,255,255,255));
			m_endPoint.Render(pRenderer);

			pAuxGeom->DrawLine(m_startPoint.GetPos(), ColorB(255, 255, 255, 255), m_endPoint.GetPos(), ColorB(255, 255, 255, 255));

			string sTempText;

			// Compute distance and output results
			// TODO: Consider movement speed outputs here as well?
			const float fDistance = m_startPoint.GetDistance(m_endPoint);
			sTempText.Format("Straight-line distance: %.3f", fDistance);

			// Draw mid text
			float white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
			IRenderAuxText::Draw2dLabel(x, y, 2.0f, white, false, sTempText.c_str());
			y += 18;

			CAIManager* pAIManager = GetIEditorImpl()->GetAI();

			for (uint32 i = 0; i < m_numPathAgents; ++i)
			{
				uint32 colourIndex = i % m_numPathAgents;
				m_pPathAgents[i].Render(pRenderer, colours[colourIndex]);

				// Check path agent status
				string sMidText;
				sMidText.Format("%s (type %d) ", m_pPathAgents[i].GetName(), m_pPathAgents[i].GetType());
				if (m_pPathAgents[i].HasQueuedPaths())
				{
					sMidText += "Path Finder: Pending...";
				}
				else if (m_pPathAgents[i].GetLastPathSuccess())
				{
					string sTempText;
					sTempText.Format("Path Finder: %.3f", m_pPathAgents[i].GetLastPathDist());
					sMidText += sTempText;
				}
				else
				{
					sMidText += "Path Finder: No Path Found";
				}

				float col[4] = { colours[colourIndex].r, colours[colourIndex].g, colours[colourIndex].b, 1.0f };
				IRenderAuxText::Draw2dLabel(x, y, 2.0f, col, false, sMidText.c_str());
				y += 18;
			}
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

	if (bRequestPath)
	{
		RequestPath();
	}
	else
	{
		for (uint32 i = 0; i < m_numPathAgents; ++i)
		{
			m_pPathAgents[i].ClearLastRequest();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CRuler::RequestPath()
{
	for (uint32 i = 0; i < m_numPathAgents; ++i)
	{
		m_pPathAgents[i].RequestPath(m_startPoint, m_endPoint);
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

