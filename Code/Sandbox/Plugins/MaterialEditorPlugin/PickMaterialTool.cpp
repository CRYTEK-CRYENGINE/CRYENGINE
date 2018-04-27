// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "PickMaterialTool.h"

#include <CryRenderer/IRenderAuxGeom.h>

#include "Material/MaterialManager.h"
#include "SurfaceInfoPicker.h"
#include "Objects/DisplayContext.h"

#include "MaterialEditor.h"
#include "AssetSystem/AssetManager.h"

#include "Viewport.h"

#define RENDER_MESH_TEST_DISTANCE 0.2f

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CPickMaterialTool, CEditTool)

//////////////////////////////////////////////////////////////////////////
CPickMaterialTool::CPickMaterialTool()
	: m_pMaterialEditor(nullptr)
{
}

//////////////////////////////////////////////////////////////////////////
CPickMaterialTool::~CPickMaterialTool()
{
	OnHover(0);
}

//////////////////////////////////////////////////////////////////////////
bool CPickMaterialTool::MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	if (event == eMouseLDown)
	{
		if (m_pHoveredMaterial)
		{
			OnPicked(m_pHoveredMaterial);
			Abort();
			return true;
		}
	}
	else if (event == eMouseMove)
	{
		return OnMouseMove(view, flags, point);
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CPickMaterialTool::Display(DisplayContext& dc)
{
	CPoint mousePoint;
	::GetCursorPos(&mousePoint);

	dc.view->ScreenToClient(&mousePoint);

	Vec3 wp = dc.view->ViewToWorld(mousePoint);

	if (m_pHoveredMaterial)
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
bool CPickMaterialTool::OnMouseMove(CViewport* view, UINT nFlags, CPoint point)
{
	view->SetCurrentCursor(STD_CURSOR_HIT, "");

	IMaterial* pNearestMaterial(NULL);
	IEntity* pNearnestEntity(NULL);

	CSurfaceInfoPicker surfacePicker;
	int nPickObjectGroupFlag = CSurfaceInfoPicker::ePOG_All;
	if (nFlags & MK_SHIFT)
		nPickObjectGroupFlag &= ~CSurfaceInfoPicker::ePOG_Vegetation;
	if (surfacePicker.Pick(point, pNearestMaterial, m_HitInfo, NULL, nPickObjectGroupFlag))
	{
		OnHover(pNearestMaterial);
		return true;
	}

	OnHover(0);
	return false;
}

//////////////////////////////////////////////////////////////////////////
// Class description.
//////////////////////////////////////////////////////////////////////////
class CMaterialPickTool_ClassDesc : public IClassDesc
{
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_EDITTOOL; }
	virtual const char*    ClassName()       { return "Material.PickTool"; };
	virtual const char*    Category()        { return "Material"; };
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CPickMaterialTool); }
};

REGISTER_CLASS_DESC(CMaterialPickTool_ClassDesc);

//////////////////////////////////////////////////////////////////////////
void CPickMaterialTool::OnHover(IMaterial* pMaterial)
{
	if (pMaterial == m_pHoveredMaterial)
		return;

	m_pHoveredMaterial = pMaterial;
	CMaterial* pCMaterial = GetIEditor()->GetMaterialManager()->FromIMaterial(m_pHoveredMaterial);
	GetIEditor()->GetMaterialManager()->SetHighlightedMaterial(pCMaterial);

	m_displayString = "";
	if (pMaterial)
	{
		string sfTypeName = GetIEditor()->Get3DEngine()->GetMaterialManager()->GetSurfaceType(m_HitInfo.nHitSurfaceID, "Material Pick Tool")->GetName();
		string sfType;
		sfType.Format("%d : %s", pMaterial->GetSurfaceType()->GetId(), pMaterial->GetSurfaceType()->GetName());

		m_displayString = "\n";
		m_displayString += pMaterial->GetName();
		m_displayString += "\n";
		m_displayString += sfType;
	}
}

void CPickMaterialTool::OnPicked(IMaterial* pEditorMaterial)
{
	CMaterial* pMtl = GetIEditor()->GetMaterialManager()->FromIMaterial(m_pHoveredMaterial);
	if (pMtl)
	{
		GetIEditor()->GetMaterialManager()->SetHighlightedMaterial(0);
		CAsset* asset = GetIEditor()->GetAssetManager()->FindAssetForFile(pMtl->GetFilename());
		if (asset)
		{
			//Opens the material for edit in the relevant editor if specified
			asset->Edit(m_pMaterialEditor);

			auto editor = asset->GetCurrentEditor();
			CRY_ASSERT(editor->GetEditorName() == "Material Editor");
			m_pMaterialEditor = static_cast<CMaterialEditor*>(editor);

			//Select the submaterial if possible
			if (m_pMaterialEditor && pMtl->IsPureChild() && pMtl->GetParent() == m_pMaterialEditor->GetLoadedMaterial())
			{
				m_pMaterialEditor->SelectMaterialForEdit(pMtl);
			}
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Could not find asset for material %s", pMtl->GetFilename().c_str());
		}
	}
}

