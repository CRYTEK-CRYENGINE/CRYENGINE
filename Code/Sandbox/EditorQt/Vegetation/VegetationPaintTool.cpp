// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "VegetationPaintTool.h"

#include "Viewport.h"
#include "Terrain/Heightmap.h"
#include "VegetationMap.h"
#include "VegetationObject.h"
#include "Objects/DisplayContext.h"
#include "Objects/BaseObject.h"
#include "Dialogs/ToolbarDialog.h"
#include "Material/Material.h"
#include "Gizmos/ITransformManipulator.h"

#include <Cry3DEngine/I3DEngine.h>
#include <CryPhysics/IPhysics.h>

#include "Util/BoostPythonHelpers.h"

//////////////////////////////////////////////////////////////////////////
// Class description.
//////////////////////////////////////////////////////////////////////////
class CVegetationPaintTool_ClassDesc : public IClassDesc
{
	//! This method returns an Editor defined GUID describing the class this plugin class is associated with.
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_EDITTOOL; }

	//! This method returns the human readable name of the class.
	virtual const char* ClassName() { return "EditTool.VegetationPaint"; };

	//! This method returns Category of this class, Category is specifing where this plugin class fits best in
	//! create panel.
	virtual const char*    Category()        { return "Terrain"; };

	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CVegetationPaintTool); }
};

REGISTER_CLASS_DESC(CVegetationPaintTool_ClassDesc);

//////////////////////////////////////////////////////////////////////////
REGISTER_PYTHON_COMMAND(CVegetationPaintTool::Command_Activate, edit_mode, vegetation, "Activates vegetation editing mode");

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CVegetationPaintTool, CEditTool)

const float CVegetationPaintTool::s_minBrushRadius = 1.0f;
const float CVegetationPaintTool::s_maxBrushRadius = 200.0f;

//////////////////////////////////////////////////////////////////////////
CVegetationPaintTool::CVegetationPaintTool()
	: m_eraseMode(false)
	, m_isEraseTool(false)
	, m_isModeSwitchAllowed(true)
	, m_pointerPos(0, 0, 0)
	, m_vegetationMap(GetIEditorImpl()->GetVegetationMap())
	, m_isAffectedByBrushes(false)
	, m_brushRadius(0.0f)
{
}

//////////////////////////////////////////////////////////////////////////
// Specific mouse events handlers.
void CVegetationPaintTool::OnLButtonDown(CViewport* pView, int flags, const CPoint& point)
{
	GetIEditorImpl()->GetIUndoManager()->Begin();
	// impossible to place in erase mode
	if (!m_eraseMode && (flags & MK_CONTROL))
	{
		PlaceThing(pView, point);
	}
	// if Alt is pressed the brush radius will be modified
	else if (!(flags & MK_ALT))
	{
		PaintBrush();
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPaintTool::OnLButtonUp()
{
	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
	{
		GetIEditorImpl()->GetIUndoManager()->Accept("Vegetation Paint");
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPaintTool::OnMouseMove(CViewport* pView, int flags, const CPoint& point)
{
	if (flags & MK_LBUTTON)
	{
		if (flags & MK_ALT)
		{
			const auto prevPointerPos = pView->ViewToWorld(m_prevMousePos, nullptr, !m_isAffectedByBrushes, m_isAffectedByBrushes);
			const auto pointerPos = pView->ViewToWorld(point, nullptr, !m_isAffectedByBrushes, m_isAffectedByBrushes);
			const Vec2 offset(prevPointerPos.x - pointerPos.x, prevPointerPos.y - pointerPos.y);

			const auto isNegative = (point.x - m_prevMousePos.x) < 0;
			const auto newRadius = m_brushRadius + (isNegative ? -offset.GetLength() : offset.GetLength());

			SetBrushRadiusWithSignal(newRadius);
		}
		else
		{
			PaintBrush();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CVegetationPaintTool::MouseCallback(CViewport* pView, EMouseEvent event, CPoint& point, int flags)
{
	if (eMouseWheel == event)
		return false;

	m_isAffectedByBrushes = false;
	auto selectedObjects = m_vegetationMap->GetSelectedObjects();
	if (!selectedObjects.empty() && selectedObjects[0]->IsAffectedByBrushes())
	{
		m_isAffectedByBrushes = true;
	}

	// update only if brush radius is not modified
	if (!((flags & MK_ALT) && (flags && MK_LBUTTON)))
	{
		m_pointerPos = pView->ViewToWorld(point, nullptr, !m_isAffectedByBrushes, m_isAffectedByBrushes);
	}

	switch (event)
	{
	case eMouseLDown:
		OnLButtonDown(pView, flags, point);
		break;

	case eMouseLUp:
		OnLButtonUp();
		break;

	case eMouseMove:
		OnMouseMove(pView, flags, point);
		break;

	default:
		break;
	}

	if (flags & MK_CONTROL)
	{
		//swap x/y
		auto unitSize = GetIEditorImpl()->GetHeightmap()->GetUnitSize();
		auto slope = GetIEditorImpl()->GetHeightmap()->GetSlope(m_pointerPos.y / unitSize, m_pointerPos.x / unitSize);
		QString statusText(QString("Slope: %1").arg(slope));
	}

	m_prevMousePos = point;

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPaintTool::Display(DisplayContext& dc)
{
	// Brush paint mode.
	if (dc.flags & DISPLAY_2D)
	{
		CPoint p1 = dc.view->WorldToView(m_pointerPos);
		CPoint p2 = dc.view->WorldToView(m_pointerPos + Vec3(m_brushRadius, 0, 0));
		float radius = sqrtf((p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y));
		dc.SetColor(0, 1, 0, 1);
		dc.DrawWireCircle2d(p1, radius, 0);
		return;
	}

	dc.SetColor(0, 1, 0, 1);

	if (m_isAffectedByBrushes)
	{
		dc.DrawCircle(m_pointerPos, m_brushRadius);
		dc.SetColor(0, 0, 1, 1);
		Vec3 posUpper = m_pointerPos + Vec3(0, 0, 0.1f + m_brushRadius / 2);
		dc.DrawCircle(posUpper, m_brushRadius);
		dc.DrawLine(m_pointerPos, posUpper);
	}
	else
	{
		dc.DrawTerrainCircle(m_pointerPos, m_brushRadius, 0.2f);
	}

	float col[4] = { 1, 1, 1, 0.8f };
	if (m_eraseMode)
	{
		IRenderAuxText::DrawLabelEx(m_pointerPos + Vec3(0, 0, 1), 1.0f, col, true, true, "Remove");
	}
	else
	{
		IRenderAuxText::DrawLabelEx(m_pointerPos + Vec3(0, 0, 1), 1.0f, col, true, true, "Place");
	}
}

//////////////////////////////////////////////////////////////////////////
bool CVegetationPaintTool::OnKeyDown(CViewport* pView, uint32 key, uint32 repCnt, uint32 flags)
{
	bool processed = false;
	switch (key)
	{
	case Qt::Key_Shift:
		m_eraseMode = !m_isEraseTool;
		processed = true;
		break;

	case Qt::Key_Plus:
		SetBrushRadiusWithSignal(m_brushRadius + 1);
		processed = true;
		break;

	case Qt::Key_Minus:
		SetBrushRadiusWithSignal(m_brushRadius - 1);
		processed = true;
		break;

	default:
		break;
	}

	return processed;
}

//////////////////////////////////////////////////////////////////////////
bool CVegetationPaintTool::OnKeyUp(CViewport* pView, uint32 key, uint32 repCnt, uint32 flags)
{
	bool processed = false;
	if (key == Qt::Key_Shift)
	{
		m_eraseMode = m_isEraseTool;
		processed = true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPaintTool::SetBrushRadiusWithSignal(float radius)
{
	SetBrushRadius(radius);
	signalBrushRadiusChanged();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPaintTool::PaintBrush()
{
	auto selectedObjects = m_vegetationMap->GetSelectedObjects();
	CRect rc(m_pointerPos.x - m_brushRadius, m_pointerPos.y - m_brushRadius,
	         m_pointerPos.x + m_brushRadius, m_pointerPos.y + m_brushRadius);

	AABB updateRegion;
	updateRegion.min = m_pointerPos - Vec3(m_brushRadius, m_brushRadius, m_brushRadius);
	updateRegion.max = m_pointerPos + Vec3(m_brushRadius, m_brushRadius, m_brushRadius);

	if (m_eraseMode)
	{
		m_vegetationMap->StoreBaseUndo(CVegetationMap::eStoreUndo_Begin);
		for (auto pSelectedObject : selectedObjects)
		{
			m_vegetationMap->ClearBrush(rc, true, pSelectedObject);
		}
		m_vegetationMap->StoreBaseUndo(CVegetationMap::eStoreUndo_End);
	}
	else
	{
		for (auto pSelectedObject : selectedObjects)
		{
			m_vegetationMap->PaintBrush(rc, true, pSelectedObject, &m_pointerPos);
		}
	}

	SetModified(updateRegion);
}

//////////////////////////////////////////////////////////////////////////
QEditToolButtonPanel::SButtonInfo CVegetationPaintTool::CreatePaintToolButtonInfo()
{
	QEditToolButtonPanel::SButtonInfo paintToolButtonInfo;
	paintToolButtonInfo.name = string(QObject::tr("Paint").toStdString().c_str()); // string copies contents of parameter
	paintToolButtonInfo.toolTip = string(QObject::tr("Paints all vegetation objects selected in the tree. Hold Shift to erase").toStdString().c_str());
	paintToolButtonInfo.pToolClass = RUNTIME_CLASS(CVegetationPaintTool);
	paintToolButtonInfo.icon = "icons:Vegetation/Vegetation_Paint.ico";
	return paintToolButtonInfo;
}

//////////////////////////////////////////////////////////////////////////
float CVegetationPaintTool::GetMinBrushRadius()
{
	return s_minBrushRadius;
}

//////////////////////////////////////////////////////////////////////////
float CVegetationPaintTool::GetMaxBrushRadius()
{
	return s_maxBrushRadius;
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPaintTool::PlaceThing(CViewport* pView, const CPoint& point)
{
	auto selectedObjects = m_vegetationMap->GetSelectedObjects();
	if (selectedObjects.empty())
	{
		return;
	}

	Matrix34 tm;
	tm.SetIdentity();
	tm.SetTranslation(pView->ViewToWorld(point));
	pView->SetConstructionMatrix(tm);

	CUndo undo("Vegetation Place");
	CVegetationInstance* pThing = m_vegetationMap->PlaceObjectInstance(m_pointerPos, selectedObjects[0]);
	// If number of instances changed.
	if (pThing)
	{
		AABB updateRegion;
		updateRegion.min = m_pointerPos - Vec3(1, 1, 1);
		updateRegion.max = m_pointerPos + Vec3(1, 1, 1);
		SetModified(updateRegion);
	}
}

/////////////////////////////////////////////////////////////////////////
void CVegetationPaintTool::SetBrushRadius(float r)
{
	m_brushRadius = clamp_tpl(r, s_minBrushRadius, s_maxBrushRadius);
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPaintTool::Command_Activate(bool activateEraseMode)
{
	CEditTool* pTool = GetIEditorImpl()->GetEditTool();
	if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CVegetationPaintTool)))
	{
		// Already active.
		return;
	}
	auto pPaintTool = new CVegetationPaintTool();
	pPaintTool->m_eraseMode = activateEraseMode;
	GetIEditorImpl()->SetEditTool(pPaintTool);
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPaintTool::SetModified(AABB& bounds, bool notifySW)
{
	GetIEditorImpl()->UpdateViews(eUpdateStatObj, &bounds);
}

