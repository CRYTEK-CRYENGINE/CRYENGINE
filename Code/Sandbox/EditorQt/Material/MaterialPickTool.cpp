// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Viewport.h"
#include "Material/MaterialManager.h"
#include "MaterialPickTool.h"
#include "SurfaceInfoPicker.h"
#include "Objects/DisplayContext.h"

#define RENDER_MESH_TEST_DISTANCE 0.2f

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CMaterialPickTool, CEditTool)

//////////////////////////////////////////////////////////////////////////
CMaterialPickTool::CMaterialPickTool()
{
}

//////////////////////////////////////////////////////////////////////////
CMaterialPickTool::~CMaterialPickTool()
{
	SetMaterial(0);
}

//////////////////////////////////////////////////////////////////////////
bool CMaterialPickTool::MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	if (event == eMouseLDown)
	{
		if (m_pMaterial)
		{
			CMaterial* pMtl = GetIEditorImpl()->GetMaterialManager()->FromIMaterial(m_pMaterial);
			if (pMtl)
			{
				GetIEditorImpl()->GetMaterialManager()->SetHighlightedMaterial(0);
				GetIEditorImpl()->OpenDataBaseLibrary(EDB_TYPE_MATERIAL, pMtl);
				Abort();
				return true;
			}
		}
	}
	else if (event == eMouseMove)
	{
		return OnMouseMove(view, flags, point);
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialPickTool::Display(DisplayContext& dc)
{
	CPoint mousePoint;
	::GetCursorPos(&mousePoint);

	dc.view->ScreenToClient(&mousePoint);

	Vec3 wp = dc.view->ViewToWorld(mousePoint);

	if (m_pMaterial)
	{
		float color[4] = { 1, 1, 1, 1 };
		IRenderAuxText::Draw2dLabel(mousePoint.x + 12, mousePoint.y + 8, 1.2f, color, false, "%s", (const char*)m_displayString);
	}

	float fScreenScale = dc.view->GetScreenScaleFactor(m_HitInfo.vHitPos) * 0.06f;

	dc.DepthTestOff();
	dc.SetColor(ColorB(0, 0, 255, 255));
	if (!m_HitInfo.vHitNormal.IsZero())
	{
		dc.DrawLine(m_HitInfo.vHitPos, m_HitInfo.vHitPos + m_HitInfo.vHitNormal * fScreenScale);

		Vec3 raySrc, rayDir;
		dc.view->ViewToWorldRay(mousePoint, raySrc, rayDir);

		Matrix34 tm;

		Vec3 zAxis = m_HitInfo.vHitNormal;
		Vec3 xAxis = rayDir.Cross(zAxis);
		if (!xAxis.IsZero())
		{
			xAxis.Normalize();
			Vec3 yAxis = xAxis.Cross(zAxis).GetNormalized();
			tm.SetFromVectors(xAxis, yAxis, zAxis, m_HitInfo.vHitPos);

			dc.PushMatrix(tm);
			dc.DrawCircle(Vec3(0, 0, 0), 0.5f * fScreenScale);
			dc.PopMatrix();
		}
	}
	dc.DepthTestOn();
}

//////////////////////////////////////////////////////////////////////////
bool CMaterialPickTool::OnMouseMove(CViewport* view, UINT nFlags, CPoint point)
{
	view->SetCurrentCursor(STD_CURSOR_HIT, "");

	IMaterial* pNearestMaterial(NULL);
	IEntity* pNearnestEntity(NULL);

	m_Mouse2DPosition = point;

	CSurfaceInfoPicker surfacePicker;
	int nPickObjectGroupFlag = CSurfaceInfoPicker::ePOG_All;
	if (nFlags & MK_SHIFT)
		nPickObjectGroupFlag &= ~CSurfaceInfoPicker::ePOG_Vegetation;
	if (surfacePicker.Pick(point, pNearestMaterial, m_HitInfo, NULL, nPickObjectGroupFlag))
	{
		SetMaterial(pNearestMaterial);
		return true;
	}

	SetMaterial(0);
	return false;
}

//////////////////////////////////////////////////////////////////////////
// Class description.
//////////////////////////////////////////////////////////////////////////
class CMaterialPickTool_ClassDesc : public IClassDesc
{
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_EDITTOOL; }
	virtual const char*    ClassName()       { return "EditTool.PickMaterial"; };
	virtual const char*    Category()        { return "Material"; };
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CMaterialPickTool); }
};

REGISTER_CLASS_DESC(CMaterialPickTool_ClassDesc);

//////////////////////////////////////////////////////////////////////////
void CMaterialPickTool::SetMaterial(IMaterial* pMaterial)
{
	if (pMaterial == m_pMaterial)
		return;

	m_pMaterial = pMaterial;
	CMaterial* pCMaterial = GetIEditorImpl()->GetMaterialManager()->FromIMaterial(m_pMaterial);
	GetIEditorImpl()->GetMaterialManager()->SetHighlightedMaterial(pCMaterial);

	m_displayString = "";
	if (pMaterial)
	{
		string sfTypeName = GetIEditorImpl()->Get3DEngine()->GetMaterialManager()->GetSurfaceType(m_HitInfo.nHitSurfaceID, "Material Pick Tool")->GetName();
		string sfType;
		sfType.Format("%d : %s", pMaterial->GetSurfaceType()->GetId(), pMaterial->GetSurfaceType()->GetName());

		m_displayString = "\n";
		m_displayString += pMaterial->GetName();
		m_displayString += "\n";
		m_displayString += sfType;
	}
}

