// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "VegetationSelectTool.h"
#include "CryMath/Random.h"
#include "VegetationMap.h"
#include "VegetationObject.h"
#include "Viewport.h"
#include <Preferences/ViewportPreferences.h>
#include "Gizmos/IGizmoManager.h"
#include <QtUtil.h>
#include "IUndoManager.h"

#include <QVector>

IMPLEMENT_DYNCREATE(CVegetationSelectTool, CEditTool)

class CVegetationSelectTool_ClassDesc : public IClassDesc
{
	//! This method returns an Editor defined GUID describing the class this plugin class is associated with.
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_EDITTOOL; }

	//! This method returns the human readable name of the class.
	virtual const char* ClassName() { return "EditTool.VegetationSelect"; };

	//! This method returns Category of this class, Category is specifing where this plugin class fits best in
	//! create panel.
	virtual const char*    Category()        { return "Terrain"; };

	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CVegetationSelectTool); }
};

REGISTER_CLASS_DESC(CVegetationSelectTool_ClassDesc);

CVegetationSelectTool::CVegetationSelectTool()
	: m_pointerPos(0, 0, 0)
	, m_opMode(OPMODE_NONE)
	, m_pVegetationMap(GetIEditorImpl()->GetVegetationMap())
	, m_pManipulator(nullptr)
{
	GetIEditorImpl()->ClearSelection();
}

CVegetationSelectTool::~CVegetationSelectTool()
{
	ClearThingSelection();

	if (m_pManipulator)
	{
		m_pManipulator->signalBeginDrag.DisconnectAll();
		m_pManipulator->signalDragging.DisconnectAll();
		m_pManipulator->signalEndDrag.DisconnectAll();
		GetIEditorImpl()->GetGizmoManager()->RemoveManipulator(m_pManipulator);
		m_pManipulator = nullptr;
	}
}

void CVegetationSelectTool::Display(DisplayContext& dc)
{
	const CViewport* const pActiveView = GetIEditorImpl()->GetActiveView();
	if (pActiveView && pActiveView->GetAdvancedSelectModeFlag())
	{
		// TODO: optimize, instead of rendering ALL vegetation objects
		// one should render only the visible ones.
		std::vector<CVegetationInstance*> instances;
		m_pVegetationMap->GetAllInstances(instances);

		for (auto pVegetationInstance : instances)
		{
			if (!pVegetationInstance)
			{
				continue;
			}

			const CVegetationObject* pVegetationObject = pVegetationInstance->object;

			if (pVegetationObject && pVegetationObject->IsHidden())
				continue;

			auto wp = pVegetationInstance->pos;
			dc.SetColor(RGB(255, 255, 255));
			if (pVegetationInstance->m_boIsSelected)
			{
				dc.SetColor(dc.GetSelectedColor());
			}
			uint32 nPrevState = dc.GetState();
			dc.DepthTestOff();
			float r = dc.view->GetScreenScaleFactor(wp) * 0.006f;
			dc.DrawWireBox(wp - Vec3(r, r, r), wp + Vec3(r, r, r));
			dc.SetState(nPrevState);
		}
	}

	if (dc.flags & DISPLAY_2D)
	{
		return;
	}

	// Single object 3D display mode.
	for (const auto& selectedThing : m_selectedThings)
	{
		auto pCurrentSelectedInstance = selectedThing.pVegetationInstance;
		// The object may become invalid if the vegetation object was destroyed.
		if (!pCurrentSelectedInstance->object)
		{
			continue;
		}

		auto pVegetationObject = pCurrentSelectedInstance->object;
		if (pVegetationObject && pVegetationObject->IsHidden())
		{
			continue;
		}

		auto radius = pCurrentSelectedInstance->object->GetObjectSize() * pCurrentSelectedInstance->scale;
		if (selectedThing.transformFailed)
		{
			dc.SetColor(1, 0, 0, 1); // red
		}
		else
		{
			dc.SetColor(0, 1, 0, 1); // green
		}

		dc.DrawTerrainCircle(pCurrentSelectedInstance->pos, radius / 2.0f, 0.1f);
		dc.SetColor(0, 0, 1.0f, 0.7f);
		dc.DrawTerrainCircle(pCurrentSelectedInstance->pos, 10.0f, 0.1f);
		auto aiRadius = pCurrentSelectedInstance->object->GetAIRadius() * pCurrentSelectedInstance->scale;
		if (aiRadius > 0)
		{
			dc.SetColor(1, 0, 1.0f, 0.7f);
			dc.DrawTerrainCircle(pCurrentSelectedInstance->pos, aiRadius, 0.1f);
		}
	}
}

bool CVegetationSelectTool::MouseCallback(CViewport* pView, EMouseEvent event, CPoint& point, int flags)
{
	auto selectedObjects = m_pVegetationMap->GetSelectedObjects();
	bool isAffectedByBrushes = !selectedObjects.empty() && selectedObjects[0]->IsAffectedByBrushes();
	m_pointerPos = pView->ViewToWorld(point, 0, !isAffectedByBrushes, isAffectedByBrushes);

	bool processed = false;
	switch (event)
	{
	case eMouseLDown:
		processed = OnLButtonDown(pView, flags, point);
		break;
	case eMouseLUp:
		processed = OnLButtonUp(pView, flags, point);
		break;
	case eMouseMove:
		processed = OnMouseMove(pView, flags, point);
		break;
	default:
		break;
	}

	return processed;
}

void CVegetationSelectTool::OnManipulatorDrag(IDisplayViewport* pView, ITransformManipulator* pManipulator, const Vec2i& point0, const Vec3& value, int flags)
{
	// get world/local coordinate system setting.
	auto coordSys = GetIEditorImpl()->GetReferenceCoordSys();
	auto editMode = GetIEditorImpl()->GetEditMode();

	// get current axis constrains.
	switch (editMode)
	{
	case eEditModeMove:
		GetIEditorImpl()->GetIUndoManager()->Restore();
		MoveSelected(pView, value);
		break;
	case eEditModeRotate:
		GetIEditorImpl()->GetIUndoManager()->Restore();
		RotateSelected(value);
		break;
	case eEditModeScale:
		{
			GetIEditorImpl()->GetIUndoManager()->Restore();

			float scale = max(value.x, value.y);
			scale = max(scale, value.z);
			ScaleSelected(scale);
		}
		break;
	default:
		break;
	}
}

bool CVegetationSelectTool::OnKeyDown(CViewport* view, uint32 key, uint32 repCnt, uint32 nFlags)
{
	bool processed = false;
	switch (key)
	{
	case Qt::Key_Delete:
		{
			CUndo undo("Vegetation Delete");
			if (!m_selectedThings.empty())
			{
				// Undoable, so no dialog
				AABB box;
				box.Reset();

				m_pVegetationMap->StoreBaseUndo();
				for (const auto& selectedThing : m_selectedThings)
				{
					box.Add(selectedThing.pVegetationInstance->pos);
					m_pVegetationMap->DeleteObjInstance(selectedThing.pVegetationInstance);
				}

				ClearThingSelection();

				if (!m_selectedThings.empty())
				{
					SetModified(true, box);
				}
				m_pVegetationMap->StoreBaseUndo();
			}
			processed = true;
		}
		break;

	case Qt::Key_Escape:
		{
			if (!m_selectedThings.empty())
			{
				int numSelected = m_selectedThings.size();
				// de-select all the things that were successfully transformed (ie. moved) ... but
				ClearThingSelection(true);
				if (numSelected == m_selectedThings.size())
				{
					// ... if all were unsuccessful, de-select them all
					ClearThingSelection(false);
				}
				processed = true;
			}
		}
		break;
	default:
		break;
	}
	return processed;
}

void CVegetationSelectTool::ClearThingSelection(bool leaveFailedTransformSelected)
{
	for (int i = 0; i < m_selectedThings.size(); ++i)
	{
		if (!leaveFailedTransformSelected || !m_selectedThings[i].transformFailed)
		{
			CVegetationInstance* thing = m_selectedThings[i].pVegetationInstance;
			if (thing->pRenderNode)
			{
				thing->pRenderNode->SetEditorObjectInfo(false, false);
			}
			thing->m_boIsSelected = false;
		}
	}

	if (leaveFailedTransformSelected)
	{
		auto eraseBeginIt = std::remove_if(m_selectedThings.begin(), m_selectedThings.end(), [](const SelectedThing& vegetation) { return !vegetation.transformFailed; });
		m_selectedThings.erase(eraseBeginIt, m_selectedThings.end());
		UpdateTransformManipulator();
	}
	else
	{
		m_selectedThings.clear();
		if (m_pManipulator)
		{
			m_pManipulator->signalBeginDrag.DisconnectAll();
			m_pManipulator->signalDragging.DisconnectAll();
			m_pManipulator->signalEndDrag.DisconnectAll();
			GetIEditorImpl()->GetGizmoManager()->RemoveManipulator(m_pManipulator);
			m_pManipulator = nullptr;
		}
	}
}

void CVegetationSelectTool::UpdateTransformManipulator()
{
	if (m_selectedThings.empty() || !m_pManipulator)
	{
		return;
	}

	Matrix34A tm;
	tm.SetIdentity();

	if (m_selectedThings.size() == 1)
	{
		auto pThing = m_selectedThings[0].pVegetationInstance;
		auto pObject = pThing->object;
		if (pObject->IsAlignToTerrain())
		{
			tm.SetRotationZ(pThing->angle);
		}
		else
		{
			tm = Matrix34::CreateRotationXYZ(Ang3(pThing->angleX, pThing->angleY, pThing->angle));
		}
		tm.SetTranslation(pThing->pos);
	}
	else
	{
		Vec3 pos(0, 0, 0);
		for (const auto& selectedThing : m_selectedThings)
		{
			pos += selectedThing.pVegetationInstance->pos;
		}
		tm.SetTranslation(pos / m_selectedThings.size());
	}
	
	m_pManipulator->SetCustomTransform(true, tm);
}

bool CVegetationSelectTool::IsManipulatorVisible()
{
	return !m_selectedThings.empty();
}

void CVegetationSelectTool::HideSelectedObjects(bool hide)
{
	auto selectedObjects = m_pVegetationMap->GetSelectedObjects();
	for (auto selectedObject : selectedObjects)
	{
		m_pVegetationMap->HideObject(selectedObject, hide);
	}
}

QVector<CVegetationInstance*> CVegetationSelectTool::GetSelectedInstances() const
{
	QVector<CVegetationInstance*> selectedInstances;
	for (const auto& selectedThing : m_selectedThings)
	{
		selectedInstances.push_back(selectedThing.pVegetationInstance);
	}
	return selectedInstances;
}

void CVegetationSelectTool::SelectThing(CVegetationInstance* pVegetationInstance, bool showManipulator)
{
	// If already selected or hidden
	if (pVegetationInstance->m_boIsSelected || pVegetationInstance->object->IsHidden())
	{
		return;
	}

	if (pVegetationInstance->pRenderNode)
	{
		pVegetationInstance->pRenderNode->SetEditorObjectId(cry_random_uint32());
		pVegetationInstance->pRenderNode->SetEditorObjectInfo(true, false);
	}

	pVegetationInstance->m_boIsSelected = true;
	m_selectedThings.push_back(SelectedThing(pVegetationInstance));

	Vec3 pos(0, 0, 0);
	for (const auto& selectedThing : m_selectedThings)
	{
		pos += selectedThing.pVegetationInstance->pos;
	}

	auto numThings = m_selectedThings.size();
	if (numThings > 0)
	{
		pos /= numThings;
	}

	if (showManipulator)
	{
		if (!m_pManipulator)
		{
			m_pManipulator = GetIEditorImpl()->GetGizmoManager()->AddManipulator(this);
			m_pManipulator->signalDragging.Connect(this, &CVegetationSelectTool::OnManipulatorDrag);
		}
		UpdateTransformManipulator();
	}
}

CVegetationInstance* CVegetationSelectTool::SelectThingAtPoint(CViewport* pView, CPoint point, bool notSelect, bool showManipulator)
{
	Vec3 raySrc, rayDir;
	pView->ViewToWorldRay(point, raySrc, rayDir);

	if (pView->GetAdvancedSelectModeFlag())
	{
		std::vector<CVegetationInstance*> instances;
		m_pVegetationMap->GetAllInstances(instances);

		int minimumDistanceIndex = -1;
		float currentSquareDistance = FLT_MAX;
		float nextSquareDistance = 0;
		CVegetationInstance* pSelectedInstance(nullptr);
		for (auto pCurrentInstance : instances)
		{
			Vec3 origin = pCurrentInstance->pos;
			Vec3 w = origin - raySrc;
			w = rayDir.Cross(w);
			auto d = w.GetLengthSquared();
			auto radius = pView->GetScreenScaleFactor(origin) * 0.008f;

			if (d < radius * radius)
			{
				nextSquareDistance = raySrc.GetDistance(origin);
				if (currentSquareDistance > nextSquareDistance)
				{
					currentSquareDistance = nextSquareDistance;
					pSelectedInstance = pCurrentInstance;
				}
			}
		}
		if (pSelectedInstance)
		{
			if (!notSelect)
			{
				SelectThing(pSelectedInstance, showManipulator);
			}
			return pSelectedInstance;
		}
		else
		{
			return nullptr;
		}
	}

	bool collideTerrain = false;
	auto pos = pView->ViewToWorld(point, &collideTerrain, true);
	auto terrainHitDistance = raySrc.GetDistance(pos);
	auto pPhysics = GetIEditorImpl()->GetSystem()->GetIPhysicalWorld();
	if (pPhysics)
	{
		auto objTypes = ent_static;
		auto flags = rwi_stop_at_pierceable | rwi_ignore_terrain_holes;
		ray_hit hit;
		auto col = pPhysics->RayWorldIntersection(raySrc, rayDir * 1000.0f, objTypes, flags, &hit, 1);
		if (hit.dist > 0 && !hit.bTerrain && hit.dist < terrainHitDistance)
		{
			pe_status_pos statusPos;
			hit.pCollider->GetStatus(&statusPos);
			if (!statusPos.pos.IsZero(0.1f))
			{
				pos = statusPos.pos;
			}
		}
	}

	// Find closest thing to this point.
	auto pObject = m_pVegetationMap->GetNearestInstance(pos, 1.0f);
	if (pObject)
	{
		if (!notSelect)
		{
			SelectThing(pObject, showManipulator);
		}
		return pObject;
	}

	return nullptr;
}

bool CVegetationSelectTool::IsThingSelected(CVegetationInstance* pInstance)
{
	if (!pInstance)
	{
		return false;
	}

	for (const auto& selectedThing : m_selectedThings)
	{
		if (pInstance == selectedThing.pVegetationInstance)
		{
			return true;
		}
	}
	return false;
}

void CVegetationSelectTool::SelectThingsInRect(CViewport* pView, CRect rect, bool showManipulator)
{
	AABB box;
	std::vector<CVegetationInstance*> things;
	m_pVegetationMap->GetAllInstances(things);
	for (auto thing : things)
	{
		Vec3 pos = thing->pos;
		box.min.Set(pos.x - 0.1f, pos.y - 0.1f, pos.z - 0.1f);
		box.max.Set(pos.x + 0.1f, pos.y + 0.1f, pos.z + 0.1f);
		if (!pView->IsBoundsVisible(box))
		{
			continue;
		}
		auto pnt = pView->WorldToView(thing->pos);
		if (rect.PtInRect(pnt))
		{
			SelectThing(thing, showManipulator);
		}
	}
}

void CVegetationSelectTool::MoveSelected(IDisplayViewport* pView, const Vec3& offset)
{
	if (m_selectedThings.empty())
	{
		return;
	}

	AABB box;
	box.Reset();
	Vec3 newPos, oldPos;
	for (auto& selectedThing : m_selectedThings)
	{
		oldPos = selectedThing.pVegetationInstance->pos;

		box.Add(oldPos);

		newPos = oldPos + offset;
		newPos = pView->SnapToGrid(newPos);

		auto pObject = selectedThing.pVegetationInstance->object;
		if (pObject && pObject->IsAffectedByBrushes())
		{
			float ofz = 1.0f + (abs(offset.x) + abs(offset.y)) / 2;
			if (ofz > 10.0f)
			{
				ofz = 10.0f;
			}
			Vec3 posUpper = newPos + Vec3(0, 0, ofz);
			bool bHitBrush = false;
			newPos.z = m_pVegetationMap->CalcHeightOnBrushes(newPos, posUpper, bHitBrush);
		}
		else if (pObject->IsAffectedByTerrain())
		{
			// Stick to terrain.
			if (I3DEngine* engine = GetIEditorImpl()->GetSystem()->GetI3DEngine())
			{
				newPos.z = engine->GetTerrainElevation(newPos.x, newPos.y);
			}
		}

		if (!m_pVegetationMap->MoveInstance(selectedThing.pVegetationInstance, newPos))
		{
			selectedThing.transformFailed = true;
		}
		else
		{
			selectedThing.transformFailed = false;
		}
		box.Add(newPos);
	}

	UpdateTransformManipulator();

	box.min = box.min - Vec3(1, 1, 1);
	box.max = box.max + Vec3(1, 1, 1);
	SetModified(true, box);
}

void CVegetationSelectTool::ScaleSelected(float scale)
{
	if (scale <= 0.01f || m_selectedThings.empty())
	{
		return;
	}

	AABB box;
	box.Reset();
	for (const auto& selectedThing : m_selectedThings)
	{
		m_pVegetationMap->RecordUndo(selectedThing.pVegetationInstance);
		selectedThing.pVegetationInstance->scale *= scale;

		// Force this object to be placed on terrain again.
		m_pVegetationMap->MoveInstance(selectedThing.pVegetationInstance, selectedThing.pVegetationInstance->pos);
		box.Add(selectedThing.pVegetationInstance->pos);
	}
	SetModified(true, box);
}

void CVegetationSelectTool::RotateSelected(const Vec3& rot)
{
	if (m_selectedThings.empty())
	{
		return;
	}

	AABB box;
	box.Reset();
	for (const auto& selectedThing : m_selectedThings)
	{
		auto pThing = selectedThing.pVegetationInstance;
		m_pVegetationMap->RecordUndo(pThing);

		Matrix34 tm;
		auto pObject = pThing->object;
		if (pObject->IsAlignToTerrain())
		{
			Matrix34 origTm;
			origTm.SetRotationZ(rot.z);
			tm.SetRotationZ(pThing->angle);
			tm = origTm * tm;
		}
		else
		{
			auto origTm = Matrix34::CreateRotationXYZ(Ang3(pThing->angleX, pThing->angleY, pThing->angle));
			tm = Matrix34::CreateRotationXYZ(Ang3(rot));
			tm = origTm * tm;
		}

		Ang3 ang(tm);
		pThing->angleX = ang.x;
		pThing->angleY = ang.y;
		pThing->angle = ang.z;

		m_pVegetationMap->MoveInstance(pThing, pThing->pos);

		box.Add(pThing->pos);
	}
	SetModified(true, box);

	UpdateTransformManipulator();
}

void CVegetationSelectTool::SetModified(bool withBounds, AABB& bounds, bool notifySW)
{
	if (withBounds)
	{
		GetIEditorImpl()->UpdateViews(eUpdateStatObj, &bounds);
	}
	else
	{
		GetIEditorImpl()->UpdateViews(eUpdateStatObj);
	}
}

bool CVegetationSelectTool::OnLButtonDown(CViewport* pView, UINT flags, CPoint point)
{
	m_mouseDownPos = point;

	Matrix34 tm;
	tm.SetIdentity();
	tm.SetTranslation(pView->ViewToWorld(point));
	pView->SetConstructionMatrix(tm);

	m_opMode = OPMODE_SELECT;

	bool isCtrlPressed = flags & MK_CONTROL;
	bool isAltPressed = QtUtil::IsModifierKeyDown(Qt::AltModifier);
	bool isSelectionModified = isCtrlPressed || isAltPressed;

	auto pSelectedInstance = SelectThingAtPoint(pView, point, true);
	if (pSelectedInstance)
	{
		if (!pSelectedInstance->m_boIsSelected && !isSelectionModified)
		{
			ClearThingSelection();
		}

		SelectThing(pSelectedInstance, true);

		if (isAltPressed && isCtrlPressed)
		{
			m_opMode = OPMODE_ROTATE;
		}
		else if (isAltPressed)
		{
			m_opMode = OPMODE_SCALE;
		}
		else if (GetIEditorImpl()->GetEditMode() == eEditModeMove)
		{
			m_opMode = OPMODE_MOVE;
			signalSelectionChanged();
		}

		GetIEditorImpl()->GetIUndoManager()->Begin();
	}
	else if (!isSelectionModified)
	{
		ClearThingSelection();
	}

	return true;
}

bool CVegetationSelectTool::OnLButtonUp(CViewport* pView, UINT flags, CPoint point)
{
	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
	{
		switch (m_opMode)
		{
		case OPMODE_MOVE:
			GetIEditorImpl()->GetIUndoManager()->Accept("Vegetation Move");
			break;
		case OPMODE_SCALE:
			GetIEditorImpl()->GetIUndoManager()->Accept("Vegetation Scale");
			break;
		case OPMODE_ROTATE:
			GetIEditorImpl()->GetIUndoManager()->Accept("Vegetation Rotate");
			break;
		default:
			GetIEditorImpl()->GetIUndoManager()->Cancel();
			break;
		}
	}
	if (m_opMode == OPMODE_SELECT)
	{
		auto rect = pView->GetSelectionRectangle();
		if (!rect.IsRectEmpty())
		{
			SelectThingsInRect(pView, rect, true);
		}
		signalSelectionChanged();
	}
	m_opMode = OPMODE_NONE;

	// Reset selected region.
	pView->ResetSelectionRegion();

	return true;
}

bool CVegetationSelectTool::OnMouseMove(CViewport* pView, UINT flags, CPoint point)
{
	switch (m_opMode)
	{
	case OPMODE_SELECT:
		{
			// Define selection.
			pView->SetSelectionRectangle(m_mouseDownPos, point);
			CRect rect = pView->GetSelectionRectangle();

			if (!rect.IsRectEmpty())
			{
				if (!(flags & MK_CONTROL))
				{
					ClearThingSelection();
				}
				SelectThingsInRect(pView, rect, true);
			}
		}
		break;
	case OPMODE_MOVE:
		{
			GetIEditorImpl()->GetIUndoManager()->Restore();
			// Define selection.
			auto p1 = pView->MapViewToCP(m_mouseDownPos);
			auto p2 = pView->MapViewToCP(point);
			auto v = pView->GetCPVector(p1, p2);
			MoveSelected(pView, v);
		}
		break;
	case OPMODE_SCALE:
		{
			GetIEditorImpl()->GetIUndoManager()->Restore();
			// Define selection.
			auto y = m_mouseDownPos.y - point.y;
			if (y != 0)
			{
				auto scale = 1.0f + (float)y / 100.0f;
				ScaleSelected(scale);
			}
		}
		break;
	case OPMODE_ROTATE:
		{
			GetIEditorImpl()->GetIUndoManager()->Restore();
			// Define selection.
			auto y = m_mouseDownPos.y - point.y;
			if (y != 0)
			{
				auto radianStepSizePerPixel = 1.0f / 255.0f;
				RotateSelected(Vec3(0, 0, radianStepSizePerPixel * y));
			}
		}
		break;
	default:
		break;
	}

	return true;
}

QEditToolButtonPanel::SButtonInfo CVegetationSelectTool::CreateSelectToolButtonInfo()
{
	QEditToolButtonPanel::SButtonInfo selectToolButtonInfo;
	selectToolButtonInfo.name = string(QObject::tr("Select").toStdString().c_str()); // string copies contents of assignment operand
	selectToolButtonInfo.toolTip = string(QObject::tr("Select/Move/Scale/Rotate/Remove selected vegetation object instances.\n"
	                                                  "Ctrl: Add To Selection  Alt: Scale Selected  Alt+Ctrl: Rotate Selected DEL: Delete Selected").toStdString().c_str());
	selectToolButtonInfo.pToolClass = RUNTIME_CLASS(CVegetationSelectTool);
	selectToolButtonInfo.icon = "icons:Vegetation/Vegetation_Select.ico";
	return selectToolButtonInfo;
}

