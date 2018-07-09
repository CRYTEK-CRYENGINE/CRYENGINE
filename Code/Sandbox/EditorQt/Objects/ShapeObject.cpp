// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ShapeObject.h"
#include "Viewport.h"
#include <Preferences/ViewportPreferences.h>
#include "Objects/AIWave.h"
#include "Objects/ObjectLoader.h"
#include "Objects/InspectorWidgetCreator.h"
#include "Util\Triangulate.h"
#include "AI\AIManager.h"

#include <Cry3DEngine/I3DEngine.h>
#include <CryAISystem/IAISystem.h>
#include <CryInput/IHardwareMouse.h>

#include <vector>
#include <CryEntitySystem/IEntitySystem.h>

#include "GameEngine.h"
#include "AI\NavDataGeneration\Navigation.h"
#include "Controls/PropertiesPanel.h"

#include "Util/BoostPythonHelpers.h"

#include <Serialization/Decorators/EditToolButton.h>
#include <Serialization/Decorators/EditorActionButton.h>
#include <Serialization/Decorators/EntityLink.h>

#include "Controls/DynamicPopupMenu.h"

#include "PickObjectTool.h"
#include "Util/MFCUtil.h"
#include "Gizmos/IGizmoManager.h"

//#pragma optimize("", off)
//#pragma inline_depth(0)

REGISTER_CLASS_DESC(CGameShapeLedgeStaticObjectClassDesc);
REGISTER_CLASS_DESC(CGameShapeLedgeObjectClassDesc);
REGISTER_CLASS_DESC(CGameShapeObjectClassDesc);
//REGISTER_CLASS_DESC(CAIPerceptionModifierObjectClassDesc); 
REGISTER_CLASS_DESC(CAIOcclusionPlaneObjectClassDesc);
REGISTER_CLASS_DESC(CAIPathObjectClassDesc);
REGISTER_CLASS_DESC(CAIShapeObjectClassDesc);
REGISTER_CLASS_DESC(CShapeObjectClassDesc);
REGISTER_CLASS_DESC(CNavigationAreaObjectDesc);
REGISTER_CLASS_DESC(CAITerritoryObjectClassDesc);

#define SHAPE_CLOSE_DISTANCE     0.8f
#define SHAPE_POINT_MIN_DISTANCE 0.1f // Set to 10 cm (this number has been found in cooperation with C2 level designers)

//////////////////////////////////////////////////////////////////////////

CNavigation* GetNavigation()
{
	return GetIEditorImpl()->GetGameEngine()->GetNavigation();
}

//  CEditShapeObjectTool implementation ///////////////////////////////

class CEditShapeTool : public CEditTool, public ITransformManipulatorOwner
{
public:
	DECLARE_DYNCREATE(CEditShapeTool)

	CEditShapeTool();

	virtual string GetDisplayName() const override { return "Edit Shape"; }
	// Ovverides from CEditTool
	bool           MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags);

	virtual void   SetUserData(const char* key, void* userData);

	virtual bool   IsNeedMoveTool() override                 { return true; }

	virtual void   Display(DisplayContext& dc)               {};
	virtual bool   OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags);
	bool           IsNeedToSkipPivotBoxForObjects() override { return true; }
	bool           IsDisplayGrid()                           { return false; }

	void           OnManipulatorBeginDrag(IDisplayViewport*, ITransformManipulator*, const Vec2i&, int flags);
	void           OnManipulatorDrag(IDisplayViewport*, ITransformManipulator*, const Vec2i&, const Vec3&, int);
	void           OnManipulatorEndDrag(IDisplayViewport*, ITransformManipulator*);

	virtual bool   GetManipulatorMatrix(RefCoordSys coordSys, Matrix34& tm) override;
	virtual void   GetManipulatorPosition(Vec3& position) override;
	virtual bool   IsManipulatorVisible() override;

	void           OnShapePropertyChange(CBaseObject*, int);

protected:
	virtual ~CEditShapeTool();
	void DeleteThis() { delete this; };

private:
	CShapeObject*          m_shape;
	bool                   m_modifying;
	CPoint                 m_mouseDownPos;
	Vec3                   m_pointPos;

	ITransformManipulator* m_pManipulator;
};

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CEditShapeTool, CEditTool)

//////////////////////////////////////////////////////////////////////////
CEditShapeTool::CEditShapeTool()
{
	m_shape = nullptr;
	m_modifying = false;

	m_pManipulator = GetIEditorImpl()->GetGizmoManager()->AddManipulator(this);

	m_pManipulator->signalBeginDrag.Connect(this, &CEditShapeTool::OnManipulatorBeginDrag);
	m_pManipulator->signalDragging.Connect(this, &CEditShapeTool::OnManipulatorDrag);
	m_pManipulator->signalEndDrag.Connect(this, &CEditShapeTool::OnManipulatorEndDrag);
}

//////////////////////////////////////////////////////////////////////////
void CEditShapeTool::SetUserData(const char* key, void* userData)
{
	m_shape = (CShapeObject*)userData;
	CRY_ASSERT(m_shape != nullptr);

	// Modify shape undo.
	if (!CUndo::IsRecording())
	{
		CUndo("Modify Shape");
		m_shape->StoreUndo("Shape Modify");
	}

	m_shape->SelectPoint(-1);
	m_shape->AddEventListener(functor(*this, &CEditShapeTool::OnShapePropertyChange));

	m_shape->SetInEditMode(true);
}

//////////////////////////////////////////////////////////////////////////
CEditShapeTool::~CEditShapeTool()
{
	if (m_shape)
	{
		m_shape->SelectPoint(-1);
	}
	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
		GetIEditorImpl()->GetIUndoManager()->Cancel();

	m_pManipulator->signalBeginDrag.DisconnectAll();
	m_pManipulator->signalDragging.DisconnectAll();
	m_pManipulator->signalEndDrag.DisconnectAll();

	GetIEditorImpl()->GetGizmoManager()->RemoveManipulator(m_pManipulator);

	m_shape->RemoveEventListener(functor(*this, &CEditShapeTool::OnShapePropertyChange));

	m_shape->SetInEditMode(false);
}

bool CEditShapeTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (nChar == Qt::Key_Escape)
	{
		GetIEditorImpl()->SetEditTool(0);
	}
	else if (nChar == Qt::Key_Delete)
	{
		if (m_shape)
		{
			int sel = m_shape->GetSelectedPoint();
			if (!m_modifying && sel >= 0 && sel < m_shape->GetPointCount())
			{
				GetIEditorImpl()->GetIUndoManager()->Begin();
				if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
					m_shape->StoreUndo("Delete Point");

				m_shape->RemovePoint(sel);
				m_shape->SelectPoint(-1);
				m_shape->CalcBBox();

				if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
					GetIEditorImpl()->GetIUndoManager()->Accept("Shape Modify");
			}
		}
	}
	return true;
}

void CEditShapeTool::OnManipulatorBeginDrag(IDisplayViewport* view, ITransformManipulator*, const Vec2i&, int flags)
{
	if (GetIEditorImpl()->GetEditMode() == eEditModeMove)
	{
		m_pointPos = m_shape->GetPoint(m_shape->GetSelectedPoint());

		Matrix34 tm;
		tm.SetIdentity();
		tm.SetTranslation(m_pointPos);
		view->SetConstructionMatrix(tm);

		m_modifying = true;
	}
}

void CEditShapeTool::OnManipulatorDrag(IDisplayViewport*, ITransformManipulator*, const Vec2i&, const Vec3& offset, int flags)
{
	if (GetIEditorImpl()->GetEditMode() == eEditModeMove)
	{
		Matrix34 m = m_shape->GetWorldTM();
		m.Invert();
		Vec3 transformedOffset = m.TransformVector(offset);

		m_shape->SetPoint(m_shape->GetSelectedPoint(), m_pointPos + transformedOffset);
	}
}

void CEditShapeTool::OnManipulatorEndDrag(IDisplayViewport*, ITransformManipulator*)
{
	if (GetIEditorImpl()->GetEditMode() == eEditModeMove)
	{
		m_modifying = false;

		m_shape->CalcBBox();
	}
}

bool CEditShapeTool::GetManipulatorMatrix(RefCoordSys coordSys, Matrix34& tm)
{
	if (!m_shape)
		return false;

	return m_shape->GetManipulatorMatrix(coordSys, tm);
}

void CEditShapeTool::GetManipulatorPosition(Vec3& position)
{
	if (m_shape->GetSelectedPoint() != -1)
	{
		position = m_shape->GetWorldTM() * m_shape->GetPoint(m_shape->GetSelectedPoint());
	}
}

bool CEditShapeTool::IsManipulatorVisible()
{
	return m_shape->GetSelectedPoint() != -1;
}

void CEditShapeTool::OnShapePropertyChange(CBaseObject*, int event)
{
	if (event == OBJECT_ON_UI_PROPERTY_CHANGED)
	{
		m_pManipulator->Invalidate();
	}
	else if (event == OBJECT_ON_DELETE)
	{
		GetIEditorImpl()->SetEditTool(nullptr);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEditShapeTool::MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	if (!m_shape)
		return false;

	if (event == eMouseLDown)
	{
		m_mouseDownPos = point;
	}

	if (event == eMouseLDown || event == eMouseMove || event == eMouseLDblClick || event == eMouseLUp)
	{
		const Matrix34& shapeTM = m_shape->GetWorldTM();

		/*
		   float fShapeCloseDistance = SHAPE_CLOSE_DISTANCE;
		   Vec3 pos = view->ViewToWorld( point );
		   if (pos.x == 0 && pos.y == 0 && pos.z == 0)
		   {
		   // Find closest point on the shape.
		   fShapeCloseDistance = SHAPE_CLOSE_DISTANCE * view->GetScreenScaleFactor(pos) * 0.01f;
		   }
		   else
		   fShapeCloseDistance = SHAPE_CLOSE_DISTANCE * view->GetScreenScaleFactor(pos) * 0.01f;
		 */

		float dist;

		Vec3 raySrc, rayDir;
		view->ViewToWorldRay(point, raySrc, rayDir);

		// Find closest point on the shape.
		int p1, p2;
		Vec3 intPnt;
		m_shape->GetNearestEdge(raySrc, rayDir, p1, p2, dist, intPnt);

		float fShapeCloseDistance = SHAPE_CLOSE_DISTANCE * view->GetScreenScaleFactor(intPnt) * 0.01f;

		if ((flags & MK_CONTROL) && !m_modifying)
		{
			// If control we are editing edges..
			if (p1 >= 0 && p2 >= 0 && dist < fShapeCloseDistance + view->GetSelectionTolerance())
			{
				// Cursor near one of edited shape edges.
				view->ResetCursor();
				if (event == eMouseLDown)
				{
					m_modifying = true;
					GetIEditorImpl()->GetIUndoManager()->Begin();
					if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
						m_shape->StoreUndo("Make Point");

					// If last edge, insert at end.
					if (p2 == 0)
						p2 = -1;

					// Create new point between nearest edge.
					// Put intPnt into local space of shape.
					intPnt = shapeTM.GetInverted().TransformPoint(intPnt);

					int index = m_shape->InsertPoint(p2, intPnt, true);
					m_shape->SelectPoint(index);
					m_pManipulator->Invalidate();

					// Set construction plane for view.
					m_pointPos = shapeTM.TransformPoint(m_shape->GetPoint(index));
					Matrix34 tm;
					tm.SetIdentity();
					tm.SetTranslation(m_pointPos);
					view->SetConstructionMatrix(tm);
				}
			}
			return true;
		}

		int index = m_shape->GetNearestPoint(raySrc, rayDir, dist);
		if (dist > fShapeCloseDistance + view->GetSelectionTolerance())
			index = -1;
		bool hitPoint(index != -1);

		// Edit the selected point based on the axis gizmo.
		if (index >= 0)
		{
			// Cursor near one of edited shape points.
			if (event == eMouseLDown)
			{
				if (!m_modifying)
				{
					m_shape->SelectPoint(index);
					m_pManipulator->Invalidate();
					m_modifying = true;
					GetIEditorImpl()->GetIUndoManager()->Begin();

					if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
						m_shape->StoreUndo("Move Point");

					// Set construction plance for view.
					m_pointPos = shapeTM.TransformPoint(m_shape->GetPoint(index));
					Matrix34 tm;
					tm.SetIdentity();
					tm.SetTranslation(m_pointPos);
					view->SetConstructionMatrix(tm);
				}
			}

			//GetNearestEdge

			// Delete points with double click.
			// Test if the use hit the point too so that interacting with the
			// axis helper does not allow to delete points.
			if (event == eMouseLDblClick && hitPoint)
			{
				if (index == 0)
				{
					Matrix34 ltm = m_shape->GetLocalTM();
					Vec3 shapePoint1 = m_shape->GetPoint(1);
					Vec3 newPos = m_shape->GetPos() + ltm.TransformVector(shapePoint1);
				}

				CUndo undo("Remove Point");
				m_modifying = false;
				m_shape->RemovePoint(index);
				m_shape->SelectPoint(-1);
			}

			if (hitPoint)
				view->SetCurrentCursor(STD_CURSOR_HIT);
		}
		else
		{
			if (event == eMouseLDown)
			{
				// Missed a point, deselect all.
				m_shape->SelectPoint(-1);
			}
			view->ResetCursor();
		}

		if (m_modifying && event == eMouseLUp)
		{
			// Accept changes.
			m_modifying = false;
			//			m_shape->SelectPoint( -1 );
			m_shape->CalcBBox();

			if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
				GetIEditorImpl()->GetIUndoManager()->Accept("Shape Modify");
		}

		if (m_modifying && event == eMouseMove)
		{
			// Move selected point point.
			Vec3 p1 = view->MapViewToCP(m_mouseDownPos);
			Vec3 p2 = view->MapViewToCP(point);
			Vec3 v = view->GetCPVector(p1, p2);

			if (m_shape->GetSelectedPoint() >= 0)
			{
				Vec3 wp = m_pointPos;
				Vec3 newp = wp + v;
				if (GetIEditorImpl()->IsSnapToTerrainEnabled())
				{
					// Keep height.
					newp = view->MapViewToCP(point);
					//float z = wp.z - GetIEditorImpl()->GetTerrainElevation(wp.x,wp.y);
					//newp.z = GetIEditorImpl()->GetTerrainElevation(newp.x,newp.y) + z;
					//newp.z = GetIEditorImpl()->GetTerrainElevation(newp.x,newp.y) + SHAPE_Z_OFFSET;
					newp.z += m_shape->GetShapeZOffset();
				}

				if (newp.x != 0 && newp.y != 0 && newp.z != 0)
				{
					newp = view->SnapToGrid(newp);

					// Put newp into local space of shape.
					Matrix34 invShapeTM = shapeTM;
					invShapeTM.Invert();
					newp = invShapeTM.TransformPoint(newp);

					m_shape->SetPoint(m_shape->GetSelectedPoint(), newp);
					m_shape->UpdatePrefab();
				}

				view->SetCurrentCursor(STD_CURSOR_MOVE);
			}
		}

		/*
		   Vec3 raySrc,rayDir;
		   view->ViewToWorldRay( point,raySrc,rayDir );
		   CBaseObject *hitObj = GetIEditorImpl()->GetObjectManager()->HitTest( raySrc,rayDir,view->GetSelectionTolerance() );
		 */
		return true;
	}
	return false;
}

class CMergeShapesTool : public CEditTool
{
public:
	DECLARE_DYNCREATE(CMergeShapesTool)

	CMergeShapesTool();

	// Ovverides from CEditTool
	virtual string GetDisplayName() const override { return "Merge Shapes"; }
	bool           MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags);
	virtual void   SetUserData(const char* key, void* userData);
	virtual void   Display(DisplayContext& dc) {};
	virtual bool   OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags);

protected:
	virtual ~CMergeShapesTool();
	void DeleteThis() { delete this; };

	int           m_curPoint;
	CShapeObject* m_shape;

private:
};

IMPLEMENT_DYNCREATE(CMergeShapesTool, CEditTool)

//////////////////////////////////////////////////////////////////////////
CMergeShapesTool::CMergeShapesTool()
{
	m_curPoint = -1;
	m_shape = 0;
}

//////////////////////////////////////////////////////////////////////////
void CMergeShapesTool::SetUserData(const char* key, void* userData)
{
	/*
	   // Modify shape undo.
	   if (!CUndo::IsRecording())
	   {
	   CUndo ("Modify Shape");
	   m_shape->StoreUndo( "Shape Modify" );
	   }
	 */
}

//////////////////////////////////////////////////////////////////////////
CMergeShapesTool::~CMergeShapesTool()
{
	if (m_shape)
		m_shape->SetMergeIndex(-1);
	m_shape = 0;
	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
		GetIEditorImpl()->GetIUndoManager()->Cancel();
}

//////////////////////////////////////////////////////////////////////////
bool CMergeShapesTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (nChar == Qt::Key_Escape)
		GetIEditorImpl()->SetEditTool(0);
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CMergeShapesTool::MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	//return true;
	if (event == eMouseLDown || event == eMouseMove)
	{
		const CSelectionGroup* pSel = GetIEditorImpl()->GetSelection();

		bool foundSel = false;

		for (int i = 0; i < pSel->GetCount(); i++)
		{
			CBaseObject* pObj = pSel->GetObject(i);
			if (!pObj->IsKindOf(RUNTIME_CLASS(CShapeObject)))
				continue;

			CShapeObject* shape = (CShapeObject*)pObj;

			const Matrix34& shapeTM = shape->GetWorldTM();

			float dist;
			Vec3 raySrc, rayDir;
			view->ViewToWorldRay(point, raySrc, rayDir);

			// Find closest point on the shape.
			int p1, p2;
			Vec3 intPnt;
			shape->GetNearestEdge(raySrc, rayDir, p1, p2, dist, intPnt);

			float fShapeCloseDistance = SHAPE_CLOSE_DISTANCE * view->GetScreenScaleFactor(intPnt) * 0.01f;

			// If control we are editing edges..
			if (p1 >= 0 && p2 >= 0 && dist < fShapeCloseDistance + view->GetSelectionTolerance())
			{
				view->SetCurrentCursor(STD_CURSOR_HIT);
				if (event == eMouseLDown)
				{
					if (m_curPoint == 0)
					{
						if (shape != m_shape)
							m_curPoint++;
						shape->SetMergeIndex(p1);
					}

					if (m_curPoint == -1)
					{
						m_shape = shape;
						m_curPoint++;
						shape->SetMergeIndex(p1);
					}

					if (m_curPoint == 1)
					{
						GetIEditorImpl()->GetIUndoManager()->Begin();
						if (m_shape)
							m_shape->Merge(shape);
						if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
							GetIEditorImpl()->GetIUndoManager()->Accept("Merge shapes");
						GetIEditorImpl()->SetEditTool(0);
					}
				}
				foundSel = true;
				break;
			}
		}

		if (!foundSel)
			view->ResetCursor();

		return true;
	}
	return false;
}

class CSplitShapeTool : public CEditTool
{
public:
	DECLARE_DYNCREATE(CSplitShapeTool)

	CSplitShapeTool();

	// Ovverides from CEditTool
	virtual string GetDisplayName() const override { return "Split Shape"; }
	bool           MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags);
	virtual void   SetUserData(const char* key, void* userData);
	virtual void   Display(DisplayContext& dc);
	virtual bool   OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags);

protected:
	virtual ~CSplitShapeTool();
	void DeleteThis() { delete this; };

private:
	CShapeObject* m_shape;
	// current index of point on the shape. If within {0, numpoints - 1} it is exactly on one shape vertex.
	// if -1 it is invalid
	int  m_curIndex;
	// if true we are close enough to a vertex of the shape to snap on it. If true, we also split precisely on that vertex
	bool m_bSnap;
	// actual coordinates of the split.
	Vec3 m_curPoint;
};

IMPLEMENT_DYNCREATE(CSplitShapeTool, CEditTool)

//////////////////////////////////////////////////////////////////////////
CSplitShapeTool::CSplitShapeTool()
{
	m_shape = 0;
	m_curIndex = -1;
}

//////////////////////////////////////////////////////////////////////////
void CSplitShapeTool::SetUserData(const char* key, void* userData)
{
	m_curIndex = -1;
	m_shape = (CShapeObject*)userData;
	CRY_ASSERT(m_shape != 0);

	// Modify shape undo.
	if (!CUndo::IsRecording())
	{
		CUndo("Modify Shape");
		m_shape->StoreUndo("Shape Modify");
	}
}

//////////////////////////////////////////////////////////////////////////
CSplitShapeTool::~CSplitShapeTool()
{
	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
		GetIEditorImpl()->GetIUndoManager()->Cancel();
}

//////////////////////////////////////////////////////////////////////////
bool CSplitShapeTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (nChar == Qt::Key_Escape)
		GetIEditorImpl()->SetEditTool(0);
	return true;
}

void CSplitShapeTool::Display(DisplayContext& dc)
{
	if (m_curIndex >= 0)
	{
		float fPointSize = 1.0f;
		const Matrix34& wtm = m_shape->GetWorldTM();

		COLORREF col = m_shape->GetColor();
		if (m_bSnap)
		{
			dc.SetColor(RGB(127, 255, 127));
			Vec3 p0 = wtm.TransformPoint(m_shape->GetPoint(m_curIndex));
			float fScale = fPointSize * dc.view->GetScreenScaleFactor(p0) * 0.02f;
			dc.DrawBall(p0, fScale);
		}
		else
		{
			dc.SetColor(RGB(255, 127, 127));
			Vec3 p0 = wtm.TransformPoint(m_curPoint);
			float fScale = fPointSize * dc.view->GetScreenScaleFactor(p0) * 0.01f;
			Vec3 vScale(fScale, fScale, fScale);
			dc.DrawSolidBox(p0 - vScale, p0 + vScale);
		}

		dc.SetColor(col);
	}
};

//////////////////////////////////////////////////////////////////////////
bool CSplitShapeTool::MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	if (!m_shape)
		return false;

	if (event == eMouseLDown || event == eMouseMove)
	{
		const Matrix34& shapeTM = m_shape->GetWorldTM();

		float dist;
		Vec3 raySrc, rayDir;
		view->ViewToWorldRay(point, raySrc, rayDir);

		// Find closest point on the shape.
		int p1, p2;
		Vec3 intersectionPoint;
		m_shape->GetNearestEdge(raySrc, rayDir, p1, p2, dist, intersectionPoint);

		float fShapeCloseDistance = SHAPE_CLOSE_DISTANCE * view->GetScreenScaleFactor(intersectionPoint) * 0.04f + view->GetSelectionTolerance();

		// If control we are editing edges..
		if (p1 >= 0 && p2 >= 0 && dist < fShapeCloseDistance)
		{
			view->SetCurrentCursor(STD_CURSOR_HIT);

			// Put intPnt into local space of shape.
			intersectionPoint = shapeTM.GetInverted().TransformPoint(intersectionPoint);

			if ((intersectionPoint - m_shape->GetPoint(p1)).len() < fShapeCloseDistance)
			{
				m_bSnap = true;
				m_curIndex = p1;
			}
			else if ((intersectionPoint - m_shape->GetPoint(p2)).len() < fShapeCloseDistance)
			{
				m_bSnap = true;
				m_curIndex = p2;
			}
			else
			{
				m_curIndex = p1;
				m_bSnap = false;
				m_curPoint = intersectionPoint;
			}

			if (event == eMouseLDown)
			{
				if (m_curIndex > -1)
				{
					GetIEditorImpl()->GetIUndoManager()->Begin();
					m_shape->SplitAtPoint(m_curIndex, m_bSnap, m_curPoint);
					if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
						GetIEditorImpl()->GetIUndoManager()->Accept("Split shape");
					GetIEditorImpl()->SetEditTool(0);
				}
			}
		}
		else
		{
			view->ResetCursor();
			m_curIndex = -1;
			m_bSnap = false;
		}
		return true;
	}
	return false;
}

class ShapeTargetTool : public CPickObjectTool
{
	DECLARE_DYNCREATE(ShapeTargetTool)

	struct SShapeTargetPicker : IPickObjectCallback
	{
		SShapeTargetPicker()
		{
		}

		void OnPick(CBaseObject* pObj) override
		{
			if (m_owner)
			{
				CUndo undo("Add target link");
				m_owner->AddEntity(pObj);
			}
		}

		void OnCancelPick() override
		{
		}

		CShapeObject* m_owner;
	};

public:
	ShapeTargetTool()
		: CPickObjectTool(&m_picker)
	{
	}

	virtual void SetUserData(const char* key, void* userData) override
	{
		m_picker.m_owner = static_cast<CShapeObject*>(userData);
	}

private:
	SShapeTargetPicker m_picker;
};

IMPLEMENT_DYNCREATE(ShapeTargetTool, CPickObjectTool)

//////////////////////////////////////////////////////////////////////////
//! Undo Shape Target
class CUndoShapeTarget : public IUndoObject
{
public:
	CUndoShapeTarget(const std::vector<CBaseObject*>& objects)
	{
		int nObjectSize = objects.size();
		m_Links.reserve(nObjectSize);
		for (CBaseObject* pObj : objects)
		{
			if (pObj->IsKindOf(RUNTIME_CLASS(CShapeObject)))
			{
				SLink link;
				link.shapeID = pObj->GetId();
				link.linkXmlNode = XmlHelpers::CreateXmlNode("undo");
				((CShapeObject*)pObj)->SaveShapeTargets(link.linkXmlNode);
				m_Links.push_back(link);
			}
		}
	}

protected:
	virtual void        Release()        { delete this; };
	virtual const char* GetDescription() { return "Shape Target"; };
	virtual const char* GetObjectName()  { return ""; };

	virtual void        Undo(bool bUndo)
	{
		for (int i = 0, iLinkSize(m_Links.size()); i < iLinkSize; ++i)
		{
			SLink& link = m_Links[i];
			CBaseObject* pObj = GetIEditorImpl()->GetObjectManager()->FindObject(link.shapeID);
			if (pObj == NULL)
				continue;
			if (!pObj->IsKindOf(RUNTIME_CLASS(CShapeObject)))
				continue;
			CShapeObject* pShape = (CShapeObject*)pObj;
			if (link.linkXmlNode->getChildCount() == 0)
				continue;
			pShape->LoadShapeTargets(link.linkXmlNode->getChild(0));
		}
	}
	virtual void Redo() {}

private:

	struct SLink
	{
		CryGUID    shapeID;
		XmlNodeRef linkXmlNode;
	};

	std::vector<SLink> m_Links;
};

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CShapeObject, CEntityObject)
IMPLEMENT_DYNCREATE(CAIPathObject, CShapeObject)
IMPLEMENT_DYNCREATE(CAIShapeObject, CShapeObject)
IMPLEMENT_DYNCREATE(CAIOcclusionPlaneObject, CShapeObject)
IMPLEMENT_DYNCREATE(CAIPerceptionModifierObject, CShapeObject)
IMPLEMENT_DYNCREATE(CAITerritoryObject, CShapeObject)
IMPLEMENT_DYNCREATE(CGameShapeObject, CShapeObject)
IMPLEMENT_DYNCREATE(CGameShapeLedgeObject, CGameShapeObject)
IMPLEMENT_DYNCREATE(CGameShapeLedgeStaticObject, CGameShapeLedgeObject)
IMPLEMENT_DYNCREATE(CNavigationAreaObject, CShapeObject)

#define RAY_DISTANCE 100000.0f

//////////////////////////////////////////////////////////////////////////
const float CShapeObject::m_defaultZOffset = 0.1f;

//////////////////////////////////////////////////////////////////////////
CShapeObject::CShapeObject()
	: m_innerFadeDistance(0.0f)
{
	m_bskipInversionTools = false;
	m_bForce2D = false;
	m_bNoCulling = false;
	mv_closed = true;

	mv_areaId = 0;

	mv_groupId = 0;
	mv_priority = 0;
	mv_width = 0.0f;
	mv_height = 0.0f;
	mv_displayFilled = false;
	mv_displaySoundInfo = false;

	mv_agentHeight = 0.0f;
	mv_agentHeight = 0.0f;
	mv_VoxelOffsetX = 0.0f;
	mv_VoxelOffsetY = 0.0f;
	mv_renderVoxelGrid = false;

	m_bbox.min = m_bbox.max = Vec3(ZERO);
	m_selectedPoint = -1;
	m_lowestHeight = 0;
	m_zOffset = m_defaultZOffset;
	m_shapePointMinDistance = SHAPE_POINT_MIN_DISTANCE;
	m_bIgnoreGameUpdate = true;
	m_bAreaModified = true;
	m_bDisplayFilledWhenSelected = true;
	m_entityClass = "AreaShape";
	m_bPerVertexHeight = true;

	m_mergeIndex = -1;

	m_updateSucceed = true;

	m_bBeingManuallyCreated = false;

	SetColor(CMFCUtils::Vec2Rgb(Vec3(0.0f, 0.8f, 1.0f)));
	UseMaterialLayersMask(false);

	m_pOwnSoundVarBlock = new CVarBlock;

	m_entities.RegisterEventCallback(functor(*this, &CShapeObject::OnObjectEvent));
}

//////////////////////////////////////////////////////////////////////////
CShapeObject::~CShapeObject()
{
	m_entities.UnregisterEventCallback(functor(*this, &CShapeObject::OnObjectEvent));
}

void CShapeObject::StoreUndoShapeTargets(const std::vector<CBaseObject*>& objects)
{
	if (objects.size() > 0 && CUndo::IsRecording())
		CUndo::Record(new CUndoShapeTarget(objects));
}

void CShapeObject::SaveShapeTargets(XmlNodeRef xmlNode)
{
	if (m_entities.GetCount() == 0)
		return;

	XmlNodeRef linksNode = xmlNode->newChild("ShapeTargets");
	for (int i = 0, num = m_entities.GetCount(); i < num; ++i)
	{
		XmlNodeRef linkNode = linksNode->newChild("Link");
		linkNode->setAttr("TargetId", m_entities.Get(i)->GetId());
	}
}

void CShapeObject::LoadShapeTargets(XmlNodeRef xmlNode)
{
	if (!xmlNode)
	{
		return;
	}

	CryGUID targetId;

	while (m_entities.GetCount() > 0)
	{
		RemoveEntity(0);
	}

	for (int i = 0; i < xmlNode->getChildCount(); i++)
	{
		XmlNodeRef linkNode = xmlNode->getChild(i);

		if (linkNode->getAttr("TargetId", targetId))
		{
			CBaseObject* pObj = GetIEditorImpl()->GetObjectManager()->FindObject(targetId);
			if (pObj)
			{
				AddEntity(pObj);
			}
		}
	}

}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::Done()
{
	m_entities.Clear();
	m_points.clear();

	m_pOwnSoundVarBlock->DeleteAllVariables();

	__super::Done();
}

//////////////////////////////////////////////////////////////////////////
bool CShapeObject::Init(CBaseObject* prev, const string& file)
{
	m_bIgnoreGameUpdate = 1;
	bool const bResult = __super::Init(prev, file);
	m_bIgnoreGameUpdate = 0;

	if (prev != nullptr)
	{
		CShapeObject const* const pOriginalObject = static_cast<CShapeObject const* const>(prev);
		m_points = pOriginalObject->m_points;
		mv_closed = pOriginalObject->mv_closed;
		m_bPerVertexHeight = pOriginalObject->m_bPerVertexHeight;
		mv_height = pOriginalObject->mv_height;
		mv_displayFilled = pOriginalObject->mv_displayFilled;

		m_abObstructSound = pOriginalObject->m_abObstructSound;
		CalcBBox();
		SetAreaProxy();
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::InitVariables()
{
	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(mv_width, "Width", functor(*this, &CShapeObject::OnSizeChange));
	m_pVarObject->AddVariable(mv_height, "Height", functor(*this, &CShapeObject::OnSizeChange));
	m_pVarObject->AddVariable(m_innerFadeDistance, "InnerFadeDistance", functor(*this, &CShapeObject::OnShapeChange));
	m_pVarObject->AddVariable(mv_areaId, "AreaId", functor(*this, &CShapeObject::OnShapeChange));
	m_pVarObject->AddVariable(mv_groupId, "GroupId", functor(*this, &CShapeObject::OnShapeChange));
	m_pVarObject->AddVariable(mv_priority, "Priority", functor(*this, &CShapeObject::OnShapeChange));
	m_pVarObject->AddVariable(mv_closed, "Closed", functor(*this, &CShapeObject::OnFormChange));
	m_pVarObject->AddVariable(mv_displayFilled, "DisplayFilled");
	m_pVarObject->AddVariable(mv_displaySoundInfo, "DisplaySoundInfo", functor(*this, &CShapeObject::OnSoundParamsChange));
	m_pVarObject->AddVariable(mv_agentHeight, "Agent_height");
	m_pVarObject->AddVariable(mv_agentWidth, "Agent_width");
	m_pVarObject->AddVariable(mv_renderVoxelGrid, "Render_voxel_grid");
	m_pVarObject->AddVariable(mv_VoxelOffsetX, "voxel_offset_x");
	m_pVarObject->AddVariable(mv_VoxelOffsetY, "voxel_offset_y");
}
//////////////////////////////////////////////////////////////////////////
void CShapeObject::SetName(const string& name)
{
	__super::SetName(name);
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::InvalidateTM(int nWhyFlags)
{
	__super::InvalidateTM(nWhyFlags);

	// Note - doing update calculations in lazy invalidation function is not really nice,
	// but more object types do this - e.g. Entities.
	// At some point we need to check how to properly lazify all those updates.
	m_bAreaModified = true;
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
bool CShapeObject::ConvertFromObject(CBaseObject* pObject)
{
	__super::ConvertFromObject(pObject);

	if (pObject->IsKindOf(RUNTIME_CLASS(CShapeObject)))
	{
		CShapeObject* pShapeObject = (CShapeObject*)pObject;
		ClearPoints();
		for (int i = 0; i < pShapeObject->GetPointCount(); ++i)
		{
			InsertPoint(-1, pShapeObject->GetPoint(i), false);
		}
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::GetBoundBox(AABB& box)
{
	box.SetTransformedAABB(GetWorldTM(), m_bbox);
	float s = 1.0f;
	box.min -= Vec3(s, s, s);
	box.max += Vec3(s, s, s);
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::GetLocalBounds(AABB& box)
{
	box = m_bbox;
}

//////////////////////////////////////////////////////////////////////////
bool CShapeObject::HitTest(HitContext& hc)
{
	// First check if ray intersect our bounding box.
	float tr = hc.distanceTolerance / 2 + SHAPE_CLOSE_DISTANCE;

	// Find intersection of line with zero Z plane.
	float minDist = FLT_MAX;
	Vec3 intPnt(0, 0, 0);
	//GetNearestEdge( hc.raySrc,hc.rayDir,p1,p2,minDist,intPnt );

	bool bWasIntersection = false;
	Vec3 ip(0, 0, 0);
	Vec3 rayLineP1 = hc.raySrc;
	Vec3 rayLineP2 = hc.raySrc + hc.rayDir * RAY_DISTANCE;
	const Matrix34& wtm = GetWorldTM();

	for (int i = 0; i < m_points.size(); i++)
	{
		int j = (i < m_points.size() - 1) ? i + 1 : 0;

		if (!mv_closed && j == 0 && i != 0)
			continue;

		Vec3 pi = wtm.TransformPoint(m_points[i]);
		Vec3 pj = wtm.TransformPoint(m_points[j]);

		float d = 0;
		if (RayToLineDistance(rayLineP1, rayLineP2, pi, pj, d, ip))
		{
			if (d < minDist)
			{
				bWasIntersection = true;
				minDist = d;
				intPnt = ip;
			}
		}

		if (mv_height > 0)
		{
			if (RayToLineDistance(rayLineP1, rayLineP2, pi + Vec3(0, 0, mv_height), pj + Vec3(0, 0, mv_height), d, ip))
			{
				if (d < minDist)
				{
					bWasIntersection = true;
					minDist = d;
					intPnt = ip;
				}
			}
			if (RayToLineDistance(rayLineP1, rayLineP2, pi, pi + Vec3(0, 0, mv_height), d, ip))
			{
				if (d < minDist)
				{
					bWasIntersection = true;
					minDist = d;
					intPnt = ip;
				}
			}
		}
	}

	float fShapeCloseDistance = SHAPE_CLOSE_DISTANCE * hc.view->GetScreenScaleFactor(intPnt) * 0.01f;

	if (bWasIntersection && minDist < fShapeCloseDistance + hc.distanceTolerance)
	{
		hc.dist = hc.raySrc.GetDistance(intPnt);
		hc.object = this;
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
int CShapeObject::MouseCreateCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	// Retransform the last point if we're rotating
	if (event == eMouseWheel && m_points.size() > 1)
	{
		float mouseX, mouseY;
		gEnv->pHardwareMouse->GetHardwareMouseClientPosition(&mouseX, &mouseY);

		Vec3 pos = view->MapViewToCP(CPoint(mouseX, mouseY), AXIS_XY, false);

		SetPoint(m_points.size() - 1, GetWorldTM().GetInverted().TransformPoint(pos));
	}

	if (event == eMouseMove || event == eMouseLDown || event == eMouseLDblClick)
	{
		// Apply the construction matrix so we can specify an axis below
		view->SetConstructionMatrix(Matrix34::Create(Vec3(1.f), IDENTITY, GetWorldPos()));
		// Cast to terrain if we can move the Z axis, otherwise only allow XY movement (unless creating the first point).
		// The reason for checking points < 2 is that the first point is temporarily added to the vector as we are creating the object.
		Vec3 pos = SupportsZAxis() || m_points.size() < 2 ? view->MapViewToCP(point, 0, true, GetCreationOffsetFromTerrain()) : view->MapViewToCP(point, AXIS_XY, false);

		if (m_points.size() < 2)
		{
			SetPos(pos);
		}

		pos.z += GetShapeZOffset();

		if (m_points.size() == 0)
		{
			InsertPoint(-1, Vec3(0, 0, 0), false);
		}
		else
		{
			SetPoint(m_points.size() - 1, GetWorldTM().GetInverted().TransformPoint(pos));
		}

		if (event == eMouseLDblClick)
		{
			if (m_points.size() > GetMinPoints())
			{
				m_points.pop_back();          // Remove last unneeded point.
				m_abObstructSound.pop_back(); // Same with the "side sound obstruction list"
				EndCreation();
				return MOUSECREATE_OK;
			}
			else
				return MOUSECREATE_ABORT;
		}

		if (event == eMouseLDown)
		{
			m_bBeingManuallyCreated = true;

			Vec3 vlen = Vec3(pos.x, pos.y, 0) - Vec3(GetWorldPos().x, GetWorldPos().y, 0);
			/* Disable that for now.
			   if (m_points.size() > 2 && vlen.GetLength() < SHAPE_CLOSE_DISTANCE)
			   {
			   EndCreation();
			   return MOUSECREATE_OK;
			   }
			 */
			if (GetPointCount() >= GetMaxPoints())
			{
				EndCreation();
				return MOUSECREATE_OK;
			}

			InsertPoint(-1, pos - GetWorldPos(), false);
			UpdateGameArea();
		}
		return MOUSECREATE_CONTINUE;
	}
	return __super::MouseCreateCallback(view, event, point, flags);
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::EndCreation()
{
	m_bBeingManuallyCreated = false;
	SetClosed(mv_closed);
}

void CShapeObject::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();

	if (mv_displaySoundInfo)
	{
		DisplaySoundInfo(dc);
	}
	else
	{
		DisplayNormal(dc);
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::DisplayNormal(DisplayContext& dc)
{
	if (!gViewportDebugPreferences.showShapeObjectHelper)
		return;

	//dc.renderer->EnableDepthTest(false);

	const Matrix34& wtm = GetWorldTM();
	COLORREF col = 0;

	float fPointSize = 0.5f;
	if (!IsSelected())
		fPointSize *= 0.5f;

	bool bPointSelected = false;
	if (m_selectedPoint >= 0 && m_selectedPoint < m_points.size())
	{
		bPointSelected = true;
	}

	bool bLineWidth = 0;

	if (!m_updateSucceed)
	{
		// Draw Error in update
		dc.SetColor(GetColor());
		dc.DrawTextLabel(wtm.GetTranslation(), 2.0f, "Error!", true);
		string msg("\n\n");
		msg += GetName();
		msg += " (see log)";
		dc.DrawTextLabel(wtm.GetTranslation(), 1.2f, msg, true);
	}

	if (m_points.size() > 1)
	{
		if (IsSelected())
		{
			col = dc.GetSelectedColor();
			dc.SetColor(col);
		}
		else if (IsHighlighted() && !bPointSelected)
		{
			dc.SetColor(RGB(255, 120, 0));
			dc.SetLineWidth(3);
			bLineWidth = true;
		}
		else
		{
			if (IsFrozen())
				dc.SetFreezeColor();
			else
				dc.SetColor(GetColor());
			col = GetColor();
		}
		dc.SetAlpha(0.8f);

		int num = m_points.size();
		if (num < GetMinPoints())
			num = 1;
		for (int i = 0; i < num; i++)
		{
			int j = (i < m_points.size() - 1) ? i + 1 : 0;
			if (!mv_closed && j == 0 && i != 0)
				continue;

			Vec3 p0 = wtm.TransformPoint(m_points[i]);
			Vec3 p1 = wtm.TransformPoint(m_points[j]);

			ColorB colMergedTmp = dc.GetColor();
			if (m_mergeIndex == i)
				dc.SetColor(RGB(255, 255, 127));
			else
			{
				if (m_bBeingManuallyCreated)
				{
					if ((num == 1) || ((num >= 2) && (i == num - 2)))
					{
						dc.SetColor(RGB(255, 255, 0));
					}
				}
			}
			dc.DrawLine(p0, p1);
			dc.SetColor(colMergedTmp);
			//DrawTerrainLine( dc,pos+m_points[i],pos+m_points[j] );

			if (mv_height != 0)
			{
				AABB box;
				box.SetTransformedAABB(GetWorldTM(), m_bbox);
				m_lowestHeight = box.min.z;
				// Draw second capping shape from above.
				float z0 = 0;
				float z1 = 0;
				if (m_bPerVertexHeight)
				{
					z0 = p0.z + mv_height;
					z1 = p1.z + mv_height;
				}
				else
				{
					z0 = m_lowestHeight + mv_height;
					z1 = z0;
				}
				dc.DrawLine(p0, Vec3(p0.x, p0.y, z0));
				dc.DrawLine(Vec3(p0.x, p0.y, z0), Vec3(p1.x, p1.y, z1));

				if (mv_displayFilled || (gViewportPreferences.fillSelectedShapes && IsSelected()))
				{
					dc.CullOff();
					ColorB c = dc.GetColor();
					dc.SetAlpha(0.3f);
					dc.DrawQuad(p0, Vec3(p0.x, p0.y, z0), Vec3(p1.x, p1.y, z1), p1);
					dc.CullOn();
					dc.SetColor(c);
					if (!IsHighlighted())
						dc.SetAlpha(0.8f);
				}
			}
		}

		// Draw points without depth test to make them editable even behind other objects.
		if (IsSelected())
			dc.DepthTestOff();

		if (IsFrozen())
			col = dc.GetFreezeColor();
		else
			col = GetColor();

		for (int i = 0; i < num; i++)
		{
			Vec3 p0 = wtm.TransformPoint(m_points[i]);
			float fScale = fPointSize * dc.view->GetScreenScaleFactor(p0) * 0.01f;
			Vec3 sz(fScale, fScale, fScale);

			if (bPointSelected && i == m_selectedPoint)
				dc.SetSelectedColor();
			else
				dc.SetColor(col);

			dc.DrawWireBox(p0 - sz, p0 + sz);
		}

		if (IsSelected())
			dc.DepthTestOn();
	}

	if (!m_entities.IsEmpty())
	{
		Vec3 vcol = CMFCUtils::Rgb2Vec(col);
		size_t const num = m_entities.GetCount();
		for (size_t i = 0; i < num; ++i)
		{
			CBaseObject* obj = m_entities.Get(i);
			if (!obj)
				continue;
			int p1, p2;
			float dist;
			Vec3 intPnt;
			GetNearestEdge(obj->GetWorldPos(), p1, p2, dist, intPnt);
			dc.DrawLine(intPnt, obj->GetWorldPos(), ColorF(vcol.x, vcol.y, vcol.z, 0.7f), ColorF(1, 1, 1, 0.7f));
		}
	}

	if (mv_closed && !IsFrozen())
	{
		if (mv_displayFilled || (gViewportPreferences.fillSelectedShapes && IsSelected()))
		{
			if (IsHighlighted())
				dc.SetColor(GetColor(), 0.1f);
			else
				dc.SetColor(GetColor(), 0.3f);
			static AreaPoints tris;
			tris.resize(0);
			tris.reserve(m_points.size() * 3);
			if (CTriangulate::Process(m_points, tris))
			{
				if (m_bNoCulling)
					dc.CullOff();
				for (int i = 0; i < tris.size(); i += 3)
				{
					dc.DrawTri(wtm.TransformPoint(tris[i]), wtm.TransformPoint(tris[i + 1]), wtm.TransformPoint(tris[i + 2]));
				}
				if (m_bNoCulling)
					dc.CullOn();
			}
		}
	}

	if (bLineWidth)
		dc.SetLineWidth(0);

	if (mv_renderVoxelGrid)
	{
		AABB tempbox;
		tempbox.SetTransformedAABB(GetWorldTM(), m_bbox);

		// if either m_agentHeight or mv_agentWidth is zero, this causes infinite loop. In order to avoid it, we should make sure that both parameters are more than a critical value.
		const float criticalSize(0.1f);
		if (mv_agentHeight > criticalSize && mv_agentWidth > criticalSize)
		{
			tempbox.min -= 5.0f * Vec3(mv_agentWidth, mv_agentHeight, 0.0f);
			tempbox.max += 5.0f * Vec3(mv_agentWidth, mv_agentHeight, 0.0f);

			float starty = tempbox.min.y + mv_VoxelOffsetY;
			ColorB colVoxel(255, 255, 255);

			while (starty < tempbox.max.y)
			{
				float startx = tempbox.min.x + mv_VoxelOffsetX;
				while (startx < tempbox.max.x)
				{
					AABB box(
					  Vec3(startx, starty, tempbox.min.z),
					  Vec3(startx + mv_agentWidth, starty + mv_agentHeight, tempbox.min.z + mv_height));
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawAABB(box, false, colVoxel, eBBD_Faceted);
					startx += mv_agentWidth;
				}
				starty += mv_agentHeight;
			}
		}
	}

	DrawDefault(dc, GetColor());
}

namespace Private_SoundDisplayColors
{
// Filled
const ColorB kObstructionFilled(255, 0, 0, 120);
const ColorB kNoObstructionFilled(0, 255, 0, 120);

// Not filled
const ColorB kObstructionNotFilled(255, 0, 0, 20);
const ColorB kNoObstructionNotFilled(0, 255, 0, 20);
}

void CShapeObject::GetSoundObstructionColors(ColorB& obstructionColor, ColorB& noObstructionColor)
{
	if (mv_displayFilled || (gViewportPreferences.fillSelectedShapes && IsSelected()))
	{
		obstructionColor = Private_SoundDisplayColors::kObstructionFilled;
		noObstructionColor = Private_SoundDisplayColors::kNoObstructionFilled;
	}
	else
	{
		obstructionColor = Private_SoundDisplayColors::kObstructionNotFilled;
		noObstructionColor = Private_SoundDisplayColors::kNoObstructionNotFilled;
	}
}

void CShapeObject::DisplaySoundInfo(DisplayContext& dc)
{
	const Matrix34& wtm = GetWorldTM();
	COLORREF col = 0;

	float fPointSize = 0.5f;
	if (!IsSelected())
		fPointSize *= 0.5f;

	bool bPointSelected = false;
	if (m_selectedPoint >= 0 && m_selectedPoint < m_points.size())
	{
		bPointSelected = true;
	}

	bool bLineWidth = 0;

	if (!m_updateSucceed)
	{
		// Draw Error in update
		dc.SetColor(GetColor());
		dc.DrawTextLabel(wtm.GetTranslation(), 2.0f, "Error!", true);
		string msg("\n\n");
		msg += GetName();
		msg += " (see log)";
		dc.DrawTextLabel(wtm.GetTranslation(), 1.2f, msg, true);
	}

	if (m_points.size() > 1)
	{
		if (IsSelected() && mv_height != 0.0f)
		{
			col = dc.GetSelectedColor();
			dc.SetColor(col);
			dc.SetLineWidth(2);
			bLineWidth = true;
		}
		else if (IsHighlighted() && !bPointSelected)
		{
			dc.SetColor(RGB(255, 120, 0));
			dc.SetLineWidth(3);
			bLineWidth = true;
		}
		else
		{
			if (IsFrozen())
				dc.SetFreezeColor();
			else
				dc.SetColor(GetColor());
			col = GetColor();
		}
		dc.SetAlpha(0.8f);

		ColorB obstructionColor;
		ColorB noObstructionColor;
		GetSoundObstructionColors(obstructionColor, noObstructionColor);

		int num = m_points.size();
		if (num < GetMinPoints())
			num = 1;
		for (int i = 0; i < num; i++)
		{
			int j = (i < m_points.size() - 1) ? i + 1 : 0;
			if (!mv_closed && j == 0 && i != 0)
				continue;

			Vec3 p0 = wtm.TransformPoint(m_points[i]);
			Vec3 p1 = wtm.TransformPoint(m_points[j]);

			ColorB colMergedTmp = dc.GetColor();
			if (m_mergeIndex == i)
				dc.SetColor(RGB(255, 255, 127));
			else if (mv_height == 0.0f && IsSelected())
			{
				m_abObstructSound[i] ? dc.SetColor(Private_SoundDisplayColors::kObstructionFilled) : dc.SetColor(Private_SoundDisplayColors::kNoObstructionFilled);
			}

			dc.DrawLine(p0, p1);
			dc.SetColor(colMergedTmp);

			if (mv_height != 0)
			{
				AABB box;
				box.SetTransformedAABB(GetWorldTM(), m_bbox);
				m_lowestHeight = box.min.z;
				// Draw second capping shape from above.
				float z0 = 0;
				float z1 = 0;
				if (m_bPerVertexHeight)
				{
					z0 = p0.z + mv_height;
					z1 = p1.z + mv_height;
				}
				else
				{
					z0 = m_lowestHeight + mv_height;
					z1 = z0;
				}
				dc.DrawLine(p0, Vec3(p0.x, p0.y, z0));
				dc.DrawLine(Vec3(p0.x, p0.y, z0), Vec3(p1.x, p1.y, z1));

				if (!IsSelected())
					continue;

				dc.CullOff();
				dc.DepthWriteOff();
				ColorB c = dc.GetColor();

				// Manipulate color so it looks a little thicker and redder
				m_abObstructSound[i] ? dc.SetColor(obstructionColor) : dc.SetColor(noObstructionColor);
				dc.DrawQuad(p0, Vec3(p0.x, p0.y, z0), Vec3(p1.x, p1.y, z1), p1);

				dc.DepthWriteOn();
				dc.CullOn();
				dc.SetColor(c);
			}
		}

		// Draw points without depth test to make them editable even behind other objects.
		if (IsSelected())
			dc.DepthTestOff();

		if (IsFrozen())
			col = dc.GetFreezeColor();
		else
			col = GetColor();

		for (int i = 0; i < num; i++)
		{
			Vec3 p0 = wtm.TransformPoint(m_points[i]);
			float fScale = fPointSize * dc.view->GetScreenScaleFactor(p0) * 0.01f;
			Vec3 sz(fScale, fScale, fScale);

			if (bPointSelected && i == m_selectedPoint)
				dc.SetSelectedColor();
			else
				dc.SetColor(col);

			dc.DrawWireBox(p0 - sz, p0 + sz);
		}

		if (IsSelected())
			dc.DepthTestOn();
	}

	if (!m_entities.IsEmpty())
	{
		Vec3 vcol = CMFCUtils::Rgb2Vec(col);
		size_t const num = m_entities.GetCount();
		for (size_t i = 0; i < num; i++)
		{
			CBaseObject* obj = m_entities.Get(i);
			if (!obj)
				continue;
			int p1, p2;
			float dist;
			Vec3 intPnt;
			GetNearestEdge(obj->GetWorldPos(), p1, p2, dist, intPnt);
			dc.DrawLine(intPnt, obj->GetWorldPos(), ColorF(vcol.x, vcol.y, vcol.z, 0.7f), ColorF(1, 1, 1, 0.7f));
		}
	}

	if (mv_closed && !IsFrozen() && mv_height > 0.0f)
	{
		DisplaySoundRoofAndFloor(dc);
	}

	if (bLineWidth)
		dc.SetLineWidth(0);

	DrawDefault(dc, GetColor());
}

void CShapeObject::DisplaySoundRoofAndFloor(DisplayContext& dc)
{
	if (!IsSelected())
		return;

	const Matrix34& wtm = GetWorldTM();

	ColorB obstructionColor;
	ColorB noObstructionColor;
	GetSoundObstructionColors(obstructionColor, noObstructionColor);

	static AreaPoints tris;
	tris.resize(0);
	tris.reserve(m_points.size() * 3);
	if (CTriangulate::Process(m_points, tris))
	{
		// Manipulate color on sound obstruction, draw it a little thicker and redder to give this impression
		bool bRoofObstructs = false;
		bool bFloorObstructs = false;

		if (m_abObstructSound[m_abObstructSound.size() - 2])
			bRoofObstructs = true;
		if (m_abObstructSound[m_abObstructSound.size() - 1])
			bFloorObstructs = true;

		ColorB c = dc.GetColor();
		dc.CullOff();
		dc.DepthWriteOff();

		float const fAreaTopPos = m_lowestHeight + mv_height;
		for (int i = 0; i < tris.size(); i += 3)
		{
			bFloorObstructs ? dc.SetColor(obstructionColor) : dc.SetColor(noObstructionColor);
			// Draw the floor
			dc.DrawTri(wtm.TransformPoint(tris[i]), wtm.TransformPoint(tris[i + 1]), wtm.TransformPoint(tris[i + 2]));

			// Draw a roof only if it obstructs sounds (temporarily disabled because of too many ugly render artifacts)
			if (mv_height != 0)
			{
				bRoofObstructs ? dc.SetColor(obstructionColor) : dc.SetColor(noObstructionColor);

				tris[i] = wtm.TransformPoint(tris[i]);
				tris[i + 1] = wtm.TransformPoint(tris[i + 1]);
				tris[i + 2] = wtm.TransformPoint(tris[i + 2]);

				tris[i].z = fAreaTopPos;
				tris[i + 1].z = fAreaTopPos;
				tris[i + 2].z = fAreaTopPos;

				// Draw the roof
				dc.DrawTri(tris[i], tris[i + 1], tris[i + 2]);
			}
		}

		dc.DepthWriteOn();
		dc.CullOn();
		dc.SetColor(c);
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::DrawTerrainLine(DisplayContext& dc, const Vec3& p1, const Vec3& p2)
{
	float len = (p2 - p1).GetLength();
	int steps = len / 2;
	if (steps <= 1)
	{
		dc.DrawLine(p1, p2);
		return;
	}
	Vec3 pos1 = p1;
	Vec3 pos2 = p1;
	for (int i = 0; i < steps - 1; i++)
	{
		pos2 = p1 + (1.0f / i) * (p2 - p1);
		pos2.z = dc.engine->GetTerrainElevation(pos2.x, pos2.y);
		dc.SetColor(i * 2, 0, 0, 1);
		dc.DrawLine(pos1, pos2);
		pos1 = pos2;
	}
	//dc.DrawLine( pos2,p2 );
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::Serialize(CObjectArchive& ar)
{
	m_bIgnoreGameUpdate = true;
	__super::Serialize(ar);
	m_bIgnoreGameUpdate = false;
	CVarBlockPtr soundVarBlock = new CVarBlock;

	XmlNodeRef xmlNode = ar.node;
	if (ar.bLoading)
	{
		m_bAreaModified = true;
		m_entities.Clear();

		CryGUID entityId;
		if (xmlNode->getAttr("EntityId", entityId))
		{
			// For Backward compatability.
			//m_entities.push_back(entityId);
			ar.SetResolveCallback(this, entityId, functor(m_entities, &CSafeObjectsArray::Add));
		}

		// Load Entities.
		m_points.clear();
		XmlNodeRef points = xmlNode->findChild("Points");
		if (points)
		{
			for (int i = 0; i < points->getChildCount(); i++)
			{
				XmlNodeRef pnt = points->getChild(i);

				// Get position
				Vec3 pos;
				pnt->getAttr("Pos", pos);
				m_points.push_back(pos);
			}
		}
		XmlNodeRef entities = xmlNode->findChild("Entities");
		if (entities)
		{
			for (int i = 0; i < entities->getChildCount(); i++)
			{
				XmlNodeRef ent = entities->getChild(i);
				CryGUID entityId;
				if (ent->getAttr("Id", entityId))
				{
					//m_entities.push_back(id);
					ar.SetResolveCallback(this, entityId, functor(m_entities, &CSafeObjectsArray::Add));
				}
			}
		}

		size_t const nPointsCount = m_points.size();
		for (size_t i = 0; i < nPointsCount; ++i)
		{
			CVariable<bool>* const pvTemp = new CVariable<bool>;

			stack_string cTemp;
			cTemp.Format("Side%d", i + 1);

			// And add each to the block
			soundVarBlock->AddVariable(pvTemp, cTemp);

			// If we're at the last point add the last 2 "Roof" and "Floor"
			if (i == nPointsCount - 1)
			{
				CVariable<bool>* const __restrict pvTempRoof = new CVariable<bool>;
				CVariable<bool>* const __restrict pvTempFloor = new CVariable<bool>;
				soundVarBlock->AddVariable(pvTempRoof, "Roof");
				soundVarBlock->AddVariable(pvTempFloor, "Floor");
			}
		}

		// Now read in the data
		XmlNodeRef pSoundDataNode = xmlNode->findChild("SoundData");
		if (pSoundDataNode)
		{
			// Make sure we stay backwards compatible and also find the old "Side:" formatting
			// This can go out once every level got saved with the new formatting
			char const* const pcTemp = pSoundDataNode->getAttr("Side:1");
			if (pcTemp && pcTemp[0])
			{
				// We have the old formatting
				soundVarBlock->DeleteAllVariables();

				for (size_t i = 0; i < nPointsCount; ++i)
				{
					CVariable<bool>* const pvTempOld = new CVariable<bool>;

					stack_string cTempOld;
					cTempOld.Format("Side:%d", i + 1);

					// And add each to the block
					soundVarBlock->AddVariable(pvTempOld, cTempOld);

					// If we're at the last point add the last 2 "Roof" and "Floor"
					if (i == nPointsCount - 1)
					{
						CVariable<bool>* const __restrict pvTempRoofOld = new CVariable<bool>;
						CVariable<bool>* const __restrict pvTempFloorOld = new CVariable<bool>;
						soundVarBlock->AddVariable(pvTempRoofOld, "Roof");
						soundVarBlock->AddVariable(pvTempFloorOld, "Floor");
					}
				}
			}

			soundVarBlock->Serialize(pSoundDataNode, true);
		}

		// Copy the data to the "bools" list
		// First clear the remains out
		m_abObstructSound.clear();

		size_t const nVarCount = soundVarBlock->GetNumVariables();
		for (size_t i = 0; i < nVarCount; ++i)
		{
			IVariable const* const pTemp = soundVarBlock->GetVariable(i);
			if (pTemp)
			{
				bool bTemp = false;
				pTemp->Get(bTemp);
				m_abObstructSound.push_back(bTemp);

				if (i >= nVarCount - 2)
				{
					// Here check for backwards compatibility reasons if "ObstructRoof" and "ObstructFloor" still exists
					bool bTemp = false;
					if (i == nVarCount - 2)
					{
						if (xmlNode->getAttr("ObstructRoof", bTemp))
							m_abObstructSound[i] = bTemp;
					}
					else
					{
						if (xmlNode->getAttr("ObstructFloor", bTemp))
							m_abObstructSound[i] = bTemp;
					}
				}
			}
		}

		// Since the var block is a static object clear it for the next object to be empty
		soundVarBlock->DeleteAllVariables();

		if (ar.bUndo)
		{
			// Update game area only in undo mode.
			UpdateGameArea();
		}

		CalcBBox();
	}
	else
	{
		// Saving.
		// Save Points.
		if (!m_points.empty())
		{
			XmlNodeRef points = xmlNode->newChild("Points");
			for (int i = 0; i < m_points.size(); i++)
			{
				XmlNodeRef pnt = points->newChild("Point");
				pnt->setAttr("Pos", m_points[i]);
			}
		}
		// Save Entities.
		if (!m_entities.IsEmpty())
		{
			XmlNodeRef nodes = xmlNode->newChild("Entities");
			size_t const num = m_entities.GetCount();

			for (size_t i = 0; i < num; ++i)
			{
				XmlNodeRef entNode = nodes->newChild("Entity");
				if (m_entities.Get(i))
					entNode->setAttr("Id", m_entities.Get(i)->GetId());
			}
		}

		XmlNodeRef pSoundDataNode = xmlNode->newChild("SoundData");

		if (pSoundDataNode)
		{
			// First clear the remains out
			soundVarBlock->DeleteAllVariables();

			// Create the variables
			size_t const nCount = m_abObstructSound.size();
			for (size_t i = 0; i < nCount; ++i)
			{
				CVariable<bool>* const pvTemp = new CVariable<bool>;
				pvTemp->Set(m_abObstructSound[i]);
				stack_string cTemp;

				if (i == nCount - 2)        // Next to last one == "Roof"
					cTemp.Format("Roof");
				else if (i == nCount - 1) // Last one == "Floor"
					cTemp.Format("Floor");
				else
					cTemp.Format("Side%d", i + 1);

				// And add each to the block
				soundVarBlock->AddVariable(pvTemp, cTemp);
			}

			soundVarBlock->Serialize(pSoundDataNode, false);
			soundVarBlock->DeleteAllVariables();
		}
	}

	m_bIgnoreGameUpdate = false;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CShapeObject::Export(const string& levelPath, XmlNodeRef& xmlNode)
{
	XmlNodeRef objNode = __super::Export(levelPath, xmlNode);
	return objNode;
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::CalcBBox()
{
	if (m_points.empty())
		return;

	// Reposition shape, so that shape object position is in the middle of the shape.
	Vec3 center = m_points[0];
	if (center.x != 0 || center.y != 0 || center.z != 0)
	{
		Matrix34 ltm = GetLocalTM();
		Vec3 newPos = GetPos() + ltm.TransformVector(center);
		size_t numPoints = m_points.size();

		// May not work correctly if shape is transformed.
		for (int i = 0; i < numPoints; i++)
		{
			m_points[i] -= center;
		}

		IEntityAreaComponent* const pAreaProxy = GetAreaProxy();

		if (pAreaProxy)
		{
			pAreaProxy->SetPoints(&m_points[0], nullptr, numPoints, mv_closed, mv_height);
		}

		SetPos(newPos);
	}

	// First point must always be 0,0,0.
	m_bbox.Reset();
	for (int i = 0; i < m_points.size(); i++)
	{
		m_bbox.Add(m_points[i]);
	}
	if (m_bbox.min.x > m_bbox.max.x)
	{
		m_bbox.min = m_bbox.max = Vec3(0, 0, 0);
	}
	if (mv_height)
	{
		AABB box;
		box.SetTransformedAABB(GetWorldTM(), m_bbox);
		m_lowestHeight = box.min.z;

		if (m_bPerVertexHeight)
			m_bbox.max.z += mv_height;
		else
		{
			//m_bbox.max.z = max( m_bbox.max.z,(float)(m_lowestHeight+mv_height) );
			box.max.z = max(box.max.z, (float)(m_lowestHeight + mv_height));
			Matrix34 mat = GetWorldTM().GetInverted();
			Vec3 p = mat.TransformPoint(box.max);
			m_bbox.max.z = max(m_bbox.max.z, p.z);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::SetSelected(bool bSelect)
{
	__super::SetSelected(bSelect);
	m_mergeIndex = -1;
}

//////////////////////////////////////////////////////////////////////////
int CShapeObject::InsertPoint(int index, const Vec3& point, bool const bModifying)
{
	if (GetPointCount() >= GetMaxPoints())
		return GetPointCount() - 1;

	// Make sure we don't have points too close to each other
	size_t nCount = m_points.size();

	if (nCount > 1) // At least 1 point should be set already
	{
		// Don't compare to the last entry if index is smaller 0,
		// this indicates that we're creating a new shape and not editing it and just adding points.
		// When creating a new shape the new point is already pushed to the end of the vector.
		nCount -= (bModifying ? 0 : 1);
		for (size_t nIdx = 0; nIdx < nCount; ++nIdx)
		{
			float const fDiffX = fabs_tpl(m_points[nIdx].x - point.x);
			float const fDiffY = fabs_tpl(m_points[nIdx].y - point.y);
			float const fDiffZ = fabs_tpl(m_points[nIdx].z - point.z);

			if (fDiffX < SHAPE_POINT_MIN_DISTANCE && fDiffY < SHAPE_POINT_MIN_DISTANCE && fDiffZ < SHAPE_POINT_MIN_DISTANCE)
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "The point is too close to another point!");
				return nIdx;
			}
		}
	}

	StoreUndo("Insert Point");

	int nNewIndex = -1;
	m_bAreaModified = true;

	if (index < 0 || index >= m_points.size())
	{
		m_points.push_back(point);
		nNewIndex = m_points.size() - 1;

		if (m_abObstructSound.empty())
		{
			// This shape is being created therefore add the point plus roof and floor positions
			m_abObstructSound.resize(3);
		}
		else
		{
			// Update the "bools list", insert just before the "Roof" position
			m_abObstructSound.insert(m_abObstructSound.end() - 2, false);
		}
	}
	else
	{
		m_points.insert(m_points.begin() + index, point);
		nNewIndex = index;

		// Also update the "bools list"
		m_abObstructSound.insert(m_abObstructSound.begin() + index, false);
	}

	// Refresh the properties panel as well to display the new point
	UpdateSoundPanelParams();

	SetPoint(nNewIndex, point);
	CalcBBox();
	UpdatePrefab();

	return nNewIndex;
}
void CShapeObject::SplitAtPoint(int index, bool bSnap, Vec3 point)
{
	// if we are not snapping, add a new point on which we will split the line
	if (!bSnap)
	{
		++index;
		InsertPoint(index, point, false);
	}
	// if we - are snapping, then separating on last or first point won't work. Return immediately
	else if (index == 0 || index == GetPointCount() - 1)
	{
		return;
	}

	CShapeObject* pNewShape = (CShapeObject*)GetObjectManager()->CloneObject(this);
	int i, totpoints = GetPointCount();
	// remove all points past the cut point
	for (i = index + 1; i < totpoints; i++)
	{
		RemovePoint(index + 1);
	}

	if (pNewShape)
	{
		// remove all points before the cut point
		for (i = 0; i < index; i++)
			pNewShape->RemovePoint(0);
	}

	m_bAreaModified = true;
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::SetMergeIndex(int index)
{
	m_mergeIndex = index;
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::Merge(CShapeObject* shape)
{
	if (!shape || m_mergeIndex < 0 || shape->m_mergeIndex < 0)
		return;

	const Matrix34& tm = GetWorldTM();
	const Matrix34& shapeTM = shape->GetWorldTM();

	int const index = m_mergeIndex + 1;

	Vec3 p0 = tm.TransformPoint(m_points[m_mergeIndex]);
	Vec3 p1 = tm.TransformPoint(m_points[(index < GetPointCount()) ? index : 0]);

	Vec3 p2 = shapeTM.TransformPoint(shape->GetPoint(shape->m_mergeIndex));
	Vec3 p3 = shapeTM.TransformPoint(shape->GetPoint(shape->m_mergeIndex + 1 < shape->GetPointCount() ? shape->m_mergeIndex + 1 : 0));

	float sum1 = p0.GetDistance(p2) + p1.GetDistance(p3);
	float sum2 = p0.GetDistance(p3) + p1.GetDistance(p2);

	if (sum2 < sum1)
	{
		shape->ReverseShape();
		shape->m_mergeIndex = shape->GetPointCount() - 1 - shape->m_mergeIndex;
	}

	int i;
	for (i = shape->m_mergeIndex + 1; i < shape->GetPointCount(); i++)
	{
		Vec3 pnt = shapeTM.TransformPoint(shape->GetPoint(i));
		pnt = tm.GetInverted().TransformPoint(pnt);
		InsertPoint(index, pnt, true);
	}
	for (i = 0; i <= shape->m_mergeIndex; i++)
	{
		Vec3 pnt = shapeTM.TransformPoint(shape->GetPoint(i));
		pnt = tm.GetInverted().TransformPoint(pnt);
		InsertPoint(index, pnt, true);
	}

	shape->SetMergeIndex(-1);
	GetObjectManager()->DeleteObject(shape);

	m_bAreaModified = true;
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::RemovePoint(int index)
{
	if ((index >= 0 || index < m_points.size()) && m_points.size() > GetMinPoints())
	{
		m_bAreaModified = true;
		StoreUndo("Remove Point");
		m_points.erase(m_points.begin() + index);
		m_abObstructSound.erase(m_abObstructSound.begin() + index);
		CalcBBox();

		m_bAreaModified = true;
		UpdateGameArea();
		UpdatePrefab();
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::ClearPoints()
{
	m_bAreaModified = true;
	StoreUndo("Clear Points");
	m_points.clear();
	m_abObstructSound.clear();
	CalcBBox();
	m_bAreaModified = true;
	UpdateGameArea();
	UpdatePrefab();
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx)
{
	__super::PostClone(pFromObject, ctx);

	CShapeObject* pFromShape = (CShapeObject*)pFromObject;
	// Clone event targets.
	if (!pFromShape->m_entities.IsEmpty())
	{
		size_t const numEntities = pFromShape->m_entities.GetCount();
		for (size_t i = 0; i < numEntities; ++i)
		{
			CBaseObject* pTarget = pFromShape->m_entities.Get(i);
			CBaseObject* pClonedTarget = ctx.FindClone(pTarget);
			if (!pClonedTarget)
				pClonedTarget = pTarget; // If target not cloned, link to original target.

			// Add cloned event.
			if (pClonedTarget)
				AddEntity(pClonedTarget);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::AddEntity(CBaseObject* const pBaseObject)
{
	CRY_ASSERT(pBaseObject != nullptr);
	m_bAreaModified = true;
	StoreUndo("Add Entity");
	m_entities.Add(pBaseObject);

	if (pBaseObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		OnEntityAdded(static_cast<CEntityObject*>(pBaseObject)->GetIEntity());
	}

	UpdatePrefab();
	UpdateUIVars();
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::RemoveEntity(size_t const index)
{
	CRY_ASSERT(index >= 0 && index < m_entities.GetCount());

	if (index < m_entities.GetCount())
	{
		StoreUndo("Remove Entity");
		m_bAreaModified = true;
		CBaseObject* const pBaseObject = m_entities.Get(index);

		if (pBaseObject != nullptr)
		{
			if (pBaseObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
			{
				OnEntityRemoved(static_cast<CEntityObject*>(pBaseObject)->GetIEntity());
			}

			if (this->IsKindOf(RUNTIME_CLASS(CAITerritoryObject)) && pBaseObject->IsKindOf(RUNTIME_CLASS(CAIWaveObject)))
			{
				GetObjectManager()->FindAndRenameProperty2If("aiwave_Wave", pBaseObject->GetName(), "<None>", "aiterritory_Territory", this->GetName());
			}

			m_entities.Remove(pBaseObject);
			UpdateUIVars();
		}
	}

	UpdatePrefab();
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::OnObjectEvent(CBaseObject* const pBaseObject, int const event)
{
	if (event == OBJECT_ON_PREDELETE)
	{
		if (pBaseObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject const* const pEntityObject = static_cast<CEntityObject*>(pBaseObject);
			const IEntity* pEntity = pEntityObject->GetIEntity();
			if (pEntity)
			{
				OnEntityRemoved(pEntity);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CShapeObject::GetEntity(size_t const index)
{
	CRY_ASSERT(index >= 0 && index < m_entities.GetCount());
	return m_entities.Get(index);
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::OnEntityAdded(IEntity const* const pIEntity)
{
	if (m_bIgnoreGameUpdate == 0)
	{
		IEntityAreaComponent* const pAreaProxy = GetAreaProxy();

		if (pAreaProxy)
		{
			pAreaProxy->AddEntity(pIEntity->GetId());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::OnEntityRemoved(IEntity const* const pIEntity)
{
	if (m_bIgnoreGameUpdate == 0)
	{
		IEntityAreaComponent* const pAreaProxy = GetAreaProxy();

		if (pAreaProxy && pIEntity)
		{
			pAreaProxy->RemoveEntity(pIEntity->GetId());
		}
	}
}

void CShapeObject::OnContextMenu(CPopupMenuItem* menu)
{
	menu->Add("Edit", functor(*this, &CShapeObject::EditShape));
	CEntityObject::OnContextMenu(menu);
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::SetClosed(bool bClosed)
{
	StoreUndo("Set Closed");
	mv_closed = bClosed;

	m_bAreaModified = true;

	if (mv_closed)
	{
		UpdateGameArea();
	}

	UpdatePrefab();
}

//////////////////////////////////////////////////////////////////////////
int CShapeObject::GetAreaId()
{
	return mv_areaId;
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::SelectPoint(int index)
{
	m_selectedPoint = index;
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::SetPoint(int index, const Vec3& pos)
{
	Vec3 p = pos;
	if (m_bForce2D)
	{
		if (!m_points.empty())
			p.z = m_points[0].z; // Keep original Z.
	}

	size_t const numPoints = m_points.size();

	if (index >= 0 && index < numPoints)
	{
		for (size_t nIdx = 0; nIdx < numPoints; ++nIdx)
		{
			if (nIdx != index) // Don't check against the same point
			{
				float const fDiffX = fabs_tpl(m_points[nIdx].x - p.x);
				float const fDiffY = fabs_tpl(m_points[nIdx].y - p.y);
				float const fDiffZ = fabs_tpl(m_points[nIdx].z - p.z);

				if ((m_shapePointMinDistance > FLT_EPSILON) && (fDiffX < m_shapePointMinDistance && fDiffY < m_shapePointMinDistance && fDiffZ < m_shapePointMinDistance))
				{
					// Prevent points from being placed too close to each other.
					return;
				}
			}
		}

		m_points[index] = p;

		m_bAreaModified = true;
		UpdateGameArea();
		UpdateUIVars();
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::ReverseShape()
{
	StoreUndo("Reverse Shape");
	std::reverse(m_points.begin(), m_points.end());
	if (mv_closed)
	{
		// Keep the same starting point for closed paths.
		Vec3 tmp(m_points.back());
		for (size_t i = m_points.size() - 1; i > 0; --i)
			m_points[i] = m_points[i - 1];
		m_points[0] = tmp;
	}
	m_bAreaModified = true;
	UpdateGameArea();
	UpdatePrefab();
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::ResetShape()
{
	StoreUndo("Reset Height Shape");

	for (size_t i = 0, count = m_points.size(); i < count; i++)
		m_points[i].z = 0;

	m_bAreaModified = true;
	UpdateGameArea();
	UpdatePrefab();
}

//////////////////////////////////////////////////////////////////////////
int CShapeObject::GetNearestPoint(const Vec3& raySrc, const Vec3& rayDir, float& distance)
{
	int index = -1;
	float minDist = FLT_MAX;
	Vec3 rayLineP1 = raySrc;
	Vec3 rayLineP2 = raySrc + rayDir * RAY_DISTANCE;
	Vec3 intPnt;
	const Matrix34& wtm = GetWorldTM();
	for (int i = 0; i < m_points.size(); i++)
	{
		float d = PointToLineDistance(rayLineP1, rayLineP2, wtm.TransformPoint(m_points[i]), intPnt);
		if (d < minDist)
		{
			minDist = d;
			index = i;
		}
	}
	distance = minDist;
	return index;
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::GetNearestEdge(const Vec3& pos, int& p1, int& p2, float& distance, Vec3& intersectPoint)
{
	p1 = -1;
	p2 = -1;
	float minDist = FLT_MAX;
	Vec3 intPnt;

	const Matrix34& wtm = GetWorldTM();

	for (int i = 0; i < m_points.size(); i++)
	{
		int j = (i < m_points.size() - 1) ? i + 1 : 0;

		if (!mv_closed && j == 0 && i != 0)
			continue;

		float d = PointToLineDistance(wtm.TransformPoint(m_points[i]), wtm.TransformPoint(m_points[j]), pos, intPnt);
		if (d < minDist)
		{
			minDist = d;
			p1 = i;
			p2 = j;
			intersectPoint = intPnt;
		}
	}
	distance = minDist;
}

//////////////////////////////////////////////////////////////////////////
bool CShapeObject::RayToLineDistance(const Vec3& rayLineP1, const Vec3& rayLineP2, const Vec3& pi, const Vec3& pj, float& distance, Vec3& intPnt)
{
	Vec3 pa, pb;
	float ua, ub;
	if (!LineLineIntersect(pi, pj, rayLineP1, rayLineP2, pa, pb, ua, ub))
		return false;

	// If before ray origin.
	if (ub < 0)
		return false;

	float d = 0;
	if (ua < 0)
		d = PointToLineDistance(rayLineP1, rayLineP2, pi, intPnt);
	else if (ua > 1)
		d = PointToLineDistance(rayLineP1, rayLineP2, pj, intPnt);
	else
	{
		intPnt = rayLineP1 + ub * (rayLineP2 - rayLineP1);
		d = (pb - pa).GetLength();
	}
	distance = d;

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::GetNearestEdge(const Vec3& raySrc, const Vec3& rayDir, int& p1, int& p2, float& distance, Vec3& intersectPoint)
{
	p1 = -1;
	p2 = -1;
	float minDist = FLT_MAX;
	Vec3 intPnt;
	Vec3 rayLineP1 = raySrc;
	Vec3 rayLineP2 = raySrc + rayDir * RAY_DISTANCE;

	const Matrix34& wtm = GetWorldTM();

	int totPoints = m_points.size();

	for (int i = 0; i < totPoints; i++)
	{
		int j = (i + 1) % totPoints;

		if (!mv_closed && j == 0 && i != 0)
			continue;

		Vec3 pi = wtm.TransformPoint(m_points[i]);
		Vec3 pj = wtm.TransformPoint(m_points[j]);

		float d = 0;
		if (!RayToLineDistance(rayLineP1, rayLineP2, pi, pj, d, intPnt))
			continue;

		if (d < minDist)
		{
			minDist = d;
			p1 = i;
			p2 = j;
			intersectPoint = intPnt;
		}
	}
	distance = minDist;
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::UpdateGameArea()
{
	if (m_bIgnoreGameUpdate == 0)
	{
		SetAreaProxy();
	}
}

void CShapeObject::EditShape()
{
	// Ensure that only this one object is selected
	if (!CheckFlags(OBJFLAG_SELECTED) || GetObjectManager()->GetSelection()->GetCount() > 1)
	{
		CUndo undo("Select Object");
		// Select just this object, so we need to clear selection
		GetObjectManager()->ClearSelection();
		GetObjectManager()->SelectObject(this);
	}

	CEditTool* pEditTool = GetIEditorImpl()->GetEditTool();

	if (!pEditTool->IsKindOf(RUNTIME_CLASS(CEditShapeTool)))
	{
		pEditTool = new CEditShapeTool;
		GetIEditorImpl()->SetEditTool(pEditTool);
		pEditTool->SetUserData("object", this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::UpdateAttachedEntities()
{
	if (m_bIgnoreGameUpdate == 0)
	{
		IEntityAreaComponent* const pAreaProxy = GetAreaProxy();

		if (pAreaProxy)
		{
			pAreaProxy->RemoveEntities();
			size_t const numEntities = GetEntityCount();

			for (size_t i = 0; i < numEntities; ++i)
			{
				CEntityObject* const pEntity = static_cast<CEntityObject*>(GetEntity(i));

				if (pEntity != nullptr && pEntity->GetIEntity() != nullptr)
				{
					pAreaProxy->AddEntity(pEntity->GetIEntity()->GetId());
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::SetAreaProxy()
{
	IEntityAreaComponent* const pAreaProxy = GetAreaProxy();

	if (pAreaProxy)
	{
		size_t const numPoints = m_points.size();
		size_t const numAudioPoints = numPoints + 2; // Adding "Roof" and "Floor"
		bool* const pbObstructSound = new bool[numAudioPoints];

		for (size_t i = 0; i < numAudioPoints; ++i)
		{
			// Here we "unpack" the data! (1 bit*numPoints to 1 byte*numPoints)
			pbObstructSound[i] = m_abObstructSound[i];
		}

		UpdateSoundPanelParams();
		pAreaProxy->SetPoints(&m_points[0], &pbObstructSound[0], numPoints, mv_closed, mv_height);
		pAreaProxy->SetProximity(mv_width);
		pAreaProxy->SetID(mv_areaId);
		pAreaProxy->SetGroup(mv_groupId);
		pAreaProxy->SetPriority(mv_priority);
		pAreaProxy->SetInnerFadeDistance(m_innerFadeDistance);
		delete[] pbObstructSound;
		m_bAreaModified = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
IEntityAreaComponent* CShapeObject::GetAreaProxy() const
{
	if (m_pEntity != nullptr && m_points.size() > 1)
	{
		return m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::OnShapeChange(IVariable* pVariable)
{
	if (m_bIgnoreGameUpdate == 0)
	{
		IEntityAreaComponent* const pAreaProxy = GetAreaProxy();

		if (pAreaProxy)
		{
			pAreaProxy->SetID(mv_areaId);
			pAreaProxy->SetGroup(mv_groupId);
			pAreaProxy->SetPriority(mv_priority);
			pAreaProxy->SetInnerFadeDistance(m_innerFadeDistance);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::OnSizeChange(IVariable* pVariable)
{
	if (m_bIgnoreGameUpdate == 0)
	{
		IEntityAreaComponent* const pAreaProxy = GetAreaProxy();

		if (pAreaProxy)
		{
			pAreaProxy->SetProximity(mv_width);
			pAreaProxy->SetHeight(mv_height);
		}
	}

	CalcBBox();
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::OnFormChange(IVariable* pVariable)
{
	SetAreaProxy();
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::OnSoundParamsChange(IVariable* var)
{
	if (!m_bIgnoreGameUpdate)
	{
		// Refresh inspector
		UpdateUIVars();
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::OnPointChange(IVariable* var)
{
	if (m_pOwnSoundVarBlock)
	{
		size_t const numVariables = static_cast<size_t>(m_pOwnSoundVarBlock->GetNumVariables());

		for (size_t i = 0; i < numVariables; ++i)
		{
			if (m_pOwnSoundVarBlock->GetVariable(i) == var)
			{
				IEntityAreaComponent* const pAreaProxy = GetAreaProxy();

				if (pAreaProxy)
				{
					bool bObstructs = false;

					if (_stricmp(var->GetName(), "Roof") == 0)
					{
						IVariable const* const __restrict pTempRoof = m_pOwnSoundVarBlock->FindVariable("Roof");

						if (pTempRoof != nullptr)
						{
							pTempRoof->Get(bObstructs);
							pAreaProxy->SetSoundObstructionOnAreaFace(numVariables - 2, bObstructs);
							m_abObstructSound[numVariables - 2] = bObstructs;
						}
					}
					else if (_stricmp(var->GetName(), "Floor") == 0)
					{
						IVariable const* const __restrict pTempFloor = m_pOwnSoundVarBlock->FindVariable("Floor");

						if (pTempFloor != nullptr)
						{
							pTempFloor->Get(bObstructs);
							pAreaProxy->SetSoundObstructionOnAreaFace(numVariables - 1, bObstructs);
							m_abObstructSound[numVariables - 1] = bObstructs;
						}
					}
					else
					{
						var->Get(bObstructs);
						pAreaProxy->SetSoundObstructionOnAreaFace(i, bObstructs);
						m_abObstructSound[i] = bObstructs;
					}
				}

				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::UpdateSoundPanelParams()
{
	if (!m_bIgnoreGameUpdate && mv_displaySoundInfo)
	{
		UpdateUIVars();
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::SpawnEntity()
{
	if (!m_entityClass.IsEmpty())
	{
		__super::SpawnEntity();
		ReadEntityProperties();
		UpdateGameArea();
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::OnEvent(ObjectEvent event)
{
	switch (event)
	{
	case EVENT_ALIGN_TOGRID:
		{
			AlignToGrid();
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::PostLoad(CObjectArchive& ar)
{
	__super::PostLoad(ar);
	UpdateGameArea();
	UpdateAttachedEntities();
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::AlignToGrid()
{
	CViewport* view = GetIEditorImpl()->GetActiveView();
	if (!view)
		return;

	CUndo undo("Align Shape To Grid");
	StoreUndo("Align Shape To Grid");

	Matrix34 wtm = GetWorldTM();
	Matrix34 invTM = wtm.GetInverted();
	for (int i = 0; i < GetPointCount(); i++)
	{
		Vec3 pnt = wtm.TransformPoint(m_points[i]);
		pnt = view->SnapToGrid(pnt);
		SetPoint(i, invTM.TransformPoint(pnt));
	}
	UpdatePrefab();
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::SerializeShapeTargets(Serialization::IArchive& ar)
{
	if (ar.isEdit())
	{
		if (ar.openBlock("targettools", "Target Tools"))
		{

			Serialization::SEditToolButton pickToolButton("");
			pickToolButton.SetToolClass(RUNTIME_CLASS(ShapeTargetTool), nullptr, this);

			ar(pickToolButton, "picker", "^Pick");
			ar(Serialization::ActionButton([ = ] {
				CUndo undo("Clear targets");
				while (!m_entities.IsEmpty())
				{
				  RemoveEntity(0);
				}
			}), "picker", "^Clear");

			ar.closeBlock();
		}
	}

	std::vector<Serialization::EntityTarget> links;
	for (size_t i = 0; i < m_entities.GetCount(); i++)
	{
		links.emplace_back(m_entities.Get(i)->GetId(), (LPCTSTR)m_entities.Get(i)->GetName());
	}
	ar(links, "targets", "!Targets");
	if (ar.isInput())
	{
		// iterate quickly on both input and existing arrays and check if our objects have changed
		bool changed = false;
		if (links.size() == m_entities.GetCount())
		{
			for (size_t i = 0; i < links.size(); ++i)
			{
				CBaseObject* pObj = m_entities.Get(i);

				if (pObj->GetId() != links[i].guid)
				{
					changed = true;
					break;
				}
			}
		}
		else
		{
			changed = true;
		}

		if (changed)
		{
			CUndo undo("Modify Shape Object Links");

			while (m_entities.GetCount() > 0)
			{
				RemoveEntity(0);
			}

			for (Serialization::EntityTarget& link : links)
			{
				CBaseObject* obj = GetIEditorImpl()->GetObjectManager()->FindObject(link.guid);
				if (obj)
				{
					AddEntity(obj);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapeObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CEntityObject::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CShapeObject>("Shape", &CShapeObject::SerializeProperties);
}

void CShapeObject::SerializeProperties(Serialization::IArchive& ar, bool bMultiEdit)
{
	m_pVarObject->SerializeVariable(&mv_width, ar);
	m_pVarObject->SerializeVariable(&mv_height, ar);
	m_pVarObject->SerializeVariable(&m_innerFadeDistance, ar);
	m_pVarObject->SerializeVariable(&mv_areaId, ar);
	m_pVarObject->SerializeVariable(&mv_groupId, ar);
	m_pVarObject->SerializeVariable(&mv_priority, ar);
	m_pVarObject->SerializeVariable(&mv_closed, ar);
	m_pVarObject->SerializeVariable(&mv_displayFilled, ar);
	m_pVarObject->SerializeVariable(&mv_displaySoundInfo, ar);
	m_pVarObject->SerializeVariable(&mv_agentHeight, ar);
	m_pVarObject->SerializeVariable(&mv_agentWidth, ar);
	m_pVarObject->SerializeVariable(&mv_renderVoxelGrid, ar);
	m_pVarObject->SerializeVariable(&mv_VoxelOffsetX, ar);
	m_pVarObject->SerializeVariable(&mv_VoxelOffsetY, ar);

	if (!m_abObstructSound.empty() && !m_bIgnoreGameUpdate && mv_displaySoundInfo && !bMultiEdit)
	{
		if (!ar.isInput() || m_pOwnSoundVarBlock->IsEmpty())
		{
			if (!m_pOwnSoundVarBlock->IsEmpty())
				m_pOwnSoundVarBlock->DeleteAllVariables();
			// If the shape hasn't got a height subtract 2 for roof and floor
			size_t const nVarCount = m_abObstructSound.size() - ((mv_height > 0.0f) ? 0 : 2);
			for (size_t i = 0; i < nVarCount; ++i)
			{
				CVariable<bool>* const pvTemp = new CVariable<bool>;
				pvTemp->Set(m_abObstructSound[i]);
				pvTemp->AddOnSetCallback(functor(*this, &CShapeObject::OnPointChange));

				string cTemp;
				if (i == nVarCount - 2 && mv_height > 0.0f)
					cTemp.Format("Roof");
				else if (i == nVarCount - 1 && mv_height > 0.0f)
					cTemp.Format("Floor");
				else
					cTemp.Format("Side:%d", i + 1);

				pvTemp->SetName(cTemp);
				m_pOwnSoundVarBlock->AddVariable(pvTemp);
			}
		}

		ar(*m_pOwnSoundVarBlock, "SoundObstruction", "|Sound Obstruction");
	}

	if (ar.openBlock("Operators", "Operators"))
	{
		if (bMultiEdit)
		{
			if (ar.openBlock("shape", "Shape"))
			{
				Serialization::SEditToolButton mergeButton("");
				mergeButton.SetToolClass(RUNTIME_CLASS(CMergeShapesTool), 0);
				ar(mergeButton, "merge", "^Merge");
				ar.closeBlock();
			}
		}
		else
		{
			if (ar.openBlock("shape", "Shape"))
			{
				Serialization::SEditToolButton editShapeButton("");
				editShapeButton.SetToolClass(RUNTIME_CLASS(CEditShapeTool), "object", this);
				ar(editShapeButton, "edit_shape", "^Edit Shape");

				Serialization::SEditToolButton splitShapeButton("");
				splitShapeButton.SetToolClass(RUNTIME_CLASS(CSplitShapeTool), "object", this);
				ar(splitShapeButton, "split_shape", "^Split");

				ar.closeBlock();
			}

			if (!m_bskipInversionTools && ar.openBlock("shape", "Shape"))
			{
				ar(Serialization::ActionButton(std::bind(&CShapeObject::ReverseShape, this)), "reverse_path", "^Reverse Path");
				ar(Serialization::ActionButton(std::bind(&CShapeObject::ResetShape, this)), "reset_height", "^Reset Height");
				ar.closeBlock();
			}

			SerializeShapeTargets(ar);
		}
		ar.closeBlock();
	}
}

//////////////////////////////////////////////////////////////////////////
// CAIPathObject.
//////////////////////////////////////////////////////////////////////////
CAIPathObject::CAIPathObject()
{
	SetColor(RGB(180, 180, 180));
	SetClosed(false);
	m_bRoad = false;
	m_bValidatePath = false;

	m_navType.AddEnumItem("Unset", (int)IAISystem::NAV_UNSET);
	m_navType.AddEnumItem("Triangular", (int)IAISystem::NAV_TRIANGULAR);
	m_navType.AddEnumItem("Waypoint Human", (int)IAISystem::NAV_WAYPOINT_HUMAN);
	m_navType.AddEnumItem("Waypoint 3D Surface", (int)IAISystem::NAV_WAYPOINT_3DSURFACE);
	m_navType.AddEnumItem("Flight", (int)IAISystem::NAV_FLIGHT);
	m_navType.AddEnumItem("Volume", (int)IAISystem::NAV_VOLUME);
	m_navType.AddEnumItem("Road", (int)IAISystem::NAV_ROAD);
	m_navType.AddEnumItem("SmartObject", (int)IAISystem::NAV_SMARTOBJECT);
	m_navType.AddEnumItem("Free 2D", (int)IAISystem::NAV_FREE_2D);

	m_navType = (int)IAISystem::NAV_UNSET;
	m_anchorType = "";

	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(m_bRoad, "Road", functor(*this, &CAIPathObject::OnShapeChange));
	m_pVarObject->AddVariable(m_navType, "PathNavType", functor(*this, &CAIPathObject::OnShapeChange));
	m_pVarObject->AddVariable(m_anchorType, "AnchorType", functor(*this, &CAIPathObject::OnShapeChange), IVariable::DT_AI_ANCHOR);
	m_pVarObject->AddVariable(m_bValidatePath, "ValidatePath");

	m_bDisplayFilledWhenSelected = false;
}

void CAIPathObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CAIShapeObjectBase::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CAIPathObject>("AI Path", [](CAIPathObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->m_pVarObject->SerializeVariable(&pObject->m_bRoad, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->m_navType, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->m_anchorType, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->m_bValidatePath, ar);
	});
}

void CAIPathObject::Done()
{
	IAISystem* const pAI = GetIEditorImpl()->GetSystem()->GetAISystem();

	if (pAI != nullptr)
	{
		if (m_updateSucceed && !m_lastGameArea.IsEmpty())
		{
			GetNavigation()->DeleteNavigationShape(m_lastGameArea);
		}
	}

	__super::Done();
}

void CAIPathObject::UpdateGameArea()
{
	if (m_bIgnoreGameUpdate)
		return;
	if (!IsCreateGameObjects())
		return;

	IAISystem* ai = GetIEditorImpl()->GetSystem()->GetAISystem();
	if (ai)
	{
		if (m_updateSucceed && !m_lastGameArea.IsEmpty())
		{
			GetNavigation()->DeleteNavigationShape(m_lastGameArea);
		}

		if (GetNavigation()->DoesNavigationShapeExists(GetName(), AREATYPE_PATH, m_bRoad))
		{
			gEnv->pSystem->GetILog()->LogError("AI Path", "Path '%s' already exists in AIsystem, please rename the path.", GetName());
			m_updateSucceed = false;
			return;
		}

		AreaPoints worldPts;
		const Matrix34& wtm = GetWorldTM();
		for (int i = 0; i < GetPointCount(); i++)
			worldPts.push_back(wtm.TransformPoint(GetPoint(i)));

		m_lastGameArea = GetName();
		if (!worldPts.empty())
		{
			CAIManager* pAIMgr = GetIEditorImpl()->GetAI();
			string anchorType(m_anchorType);
			int type = pAIMgr->AnchorActionToId(anchorType);
			SNavigationShapeParams params(m_lastGameArea, AREATYPE_PATH, m_bRoad, mv_closed, &worldPts[0], worldPts.size(), 0, m_navType, type);
			m_updateSucceed = GetNavigation()->CreateNavigationShape(params);
		}
	}
	m_bAreaModified = false;
}

void CAIPathObject::DrawSphere(DisplayContext& dc, const Vec3& p0, float r, int n)
{
	// Note: The Aux Render already supports drawing spheres, but they appear
	// shaded when rendered and there is no cylinder rendering available.
	// This function is here just for the purpose to make the rendering of
	// the validatedsegments more consistent.
	Vec3 a0, a1, b0, b1;
	for (int j = 0; j < n / 2; j++)
	{
		float theta1 = j * gf_PI2 / n - gf_PI / 2;
		float theta2 = (j + 1) * gf_PI2 / n - gf_PI / 2;

		for (int i = 0; i <= n; i++)
		{
			float theta3 = i * gf_PI2 / n;

			a0.x = p0.x + r * cosf(theta2) * cosf(theta3);
			a0.y = p0.y + r * sinf(theta2);
			a0.z = p0.z + r * cosf(theta2) * sinf(theta3);

			b0.x = p0.x + r * cosf(theta1) * cosf(theta3);
			b0.y = p0.y + r * sinf(theta1);
			b0.z = p0.z + r * cosf(theta1) * sinf(theta3);

			if (i > 0)
				dc.DrawQuad(a0, b0, b1, a1);

			a1 = a0;
			b1 = b0;
		}
	}
}

void CAIPathObject::DrawCylinder(DisplayContext& dc, const Vec3& p0, const Vec3& p1, float rad, int n)
{
	Vec3 dir(p1 - p0);
	Vec3 a = dir.GetOrthogonal();
	Vec3 b = dir.Cross(a);
	a.NormalizeSafe();
	b.NormalizeSafe();

	for (int i = 0; i < n; i++)
	{
		float a0 = ((float)i / (float)n) * gf_PI2;
		float a1 = ((float)(i + 1) / (float)n) * gf_PI2;
		Vec3 n0 = sinf(a0) * rad * a + cosf(a0) * rad * b;
		Vec3 n1 = sinf(a1) * rad * a + cosf(a1) * rad * b;

		dc.DrawQuad(p0 + n0, p1 + n0, p1 + n1, p0 + n1);
	}
}

bool CAIPathObject::IsSegmentValid(const Vec3& p0, const Vec3& p1, float rad)
{
	AABB box;
	box.Reset();
	box.Add(p0);
	box.Add(p1);
	box.min -= Vec3(rad, rad, rad);
	box.max += Vec3(rad, rad, rad);

	IPhysicalEntity** entities;
	unsigned nEntities = gEnv->pPhysicalWorld->GetEntitiesInBox(box.min, box.max, entities, ent_static | ent_ignore_noncolliding);

	primitives::sphere spherePrim;
	spherePrim.center = p0;
	spherePrim.r = rad;
	Vec3 dir(p1 - p0);

	unsigned hitCount = 0;
	ray_hit hit;
	IPhysicalWorld* pPhysics = gEnv->pPhysicalWorld;

	for (unsigned iEntity = 0; iEntity < nEntities; ++iEntity)
	{
		IPhysicalEntity* pEntity = entities[iEntity];
		if (pPhysics->CollideEntityWithPrimitive(pEntity, spherePrim.type, &spherePrim, dir, &hit))
		{
			return false;
			break;
		}
	}
	return true;
}

void CAIPathObject::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();

	// Display path validation
	if (m_bValidatePath && IsSelected())
	{
		dc.DepthWriteOff();
		int num = m_points.size();
		if (!m_bRoad && m_navType == (int)IAISystem::NAV_VOLUME && num >= 3)
		{
			const float rad = mv_width;
			const Matrix34& wtm = GetWorldTM();

			if (!mv_closed)
			{
				Vec3 p0 = wtm.TransformPoint(m_points.front());
				float viewScale = dc.view->GetScreenScaleFactor(p0);
				if (viewScale < 0.01f) viewScale = 0.01f;
				unsigned n = CLAMP((unsigned)(300.0f / viewScale), 4, 20);
				DrawSphere(dc, p0, rad, n);
			}

			for (int i = 0; i < num; i++)
			{
				int j = (i < m_points.size() - 1) ? i + 1 : 0;
				if (!mv_closed && j == 0 && i != 0)
					continue;

				Vec3 p0 = wtm.TransformPoint(m_points[i]);
				Vec3 p1 = wtm.TransformPoint(m_points[j]);

				float viewScale = max(dc.view->GetScreenScaleFactor(p0), dc.view->GetScreenScaleFactor(p1));
				if (viewScale < 0.01f) viewScale = 0.01f;
				unsigned n = CLAMP((unsigned)(300.0f / viewScale), 4, 20);

				if (IsSegmentValid(p0, p1, rad))
					dc.SetColor(1, 1, 1, 0.25f);
				else
					dc.SetColor(1, 0.5f, 0, 0.25f);

				DrawCylinder(dc, p0, p1, rad, n);
				DrawSphere(dc, p1, rad, n);
			}
		}
		dc.DepthWriteOn();
	}

	// Display the direction of the path
	if (m_points.size() > 1)
	{
		const Matrix34& wtm = GetWorldTM();
		Vec3 p0 = wtm.TransformPoint(m_points[0]);
		Vec3 p1 = wtm.TransformPoint(m_points[1]);
		if (IsSelected())
			dc.SetColor(dc.GetSelectedColor());
		else if (IsHighlighted())
			dc.SetColor(RGB(255, 120, 0));
		else
			dc.SetColor(GetColor());
		Vec3 pt((p1 - p0) / 2);
		if (pt.GetLength() > 1.0f)
			pt.SetLength(1.0f);
		dc.DrawArrow(p0, p0 + pt);

		float l = 0;
		for (int i = 1; i < m_points.size(); ++i)
			l += (m_points[i - 1] - m_points[i]).len();

		string msg("");
		msg.Format(" Length: %.2fm", l);
		dc.DrawTextLabel(wtm.GetTranslation(), 1.2f, msg, true);
	}

	CShapeObject::Display(objRenderHelper);
}

int CAIPathObject::InsertPoint(int index, const Vec3& point, bool const bModifying)
{
	int result = __super::InsertPoint(index, point, bModifying);

	UpdateGameArea();

	return result;
}

void CAIPathObject::SetPoint(int index, const Vec3& pos)
{
	__super::SetPoint(index, pos);

	UpdateGameArea();
}

void CAIPathObject::RemovePoint(int index)
{
	__super::RemovePoint(index);

	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
// CAIShapeObject.
//////////////////////////////////////////////////////////////////////////
CAIShapeObject::CAIShapeObject()
{
	SetColor(RGB(30, 65, 120));
	SetClosed(true);

	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_anchorType = "";
	m_pVarObject->AddVariable(m_anchorType, "AnchorType", functor(*this, &CAIShapeObject::OnShapeChange), IVariable::DT_AI_ANCHOR);

	m_lightLevel.AddEnumItem("Default", AILL_NONE);
	m_lightLevel.AddEnumItem("Light", AILL_LIGHT);
	m_lightLevel.AddEnumItem("Medium", AILL_MEDIUM);
	m_lightLevel.AddEnumItem("Dark", AILL_DARK);
	m_lightLevel.AddEnumItem("SuperDark", AILL_SUPERDARK);
	m_lightLevel = AILL_NONE;
	m_pVarObject->AddVariable(m_lightLevel, "LightLevel", functor(*this, &CAIShapeObject::OnShapeChange));

	m_bDisplayFilledWhenSelected = false;
}

void CAIShapeObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CAIShapeObjectBase::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CAIShapeObject>("AI Shape", [](CAIShapeObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->m_pVarObject->SerializeVariable(&pObject->m_anchorType, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->m_lightLevel, ar);
	});
}

void CAIShapeObject::Done()
{
	IAISystem* const pAI = GetIEditorImpl()->GetSystem()->GetAISystem();

	if (pAI != nullptr)
	{
		if (m_updateSucceed && !m_lastGameArea.IsEmpty())
		{
			GetNavigation()->DeleteNavigationShape(m_lastGameArea);
		}
	}
	__super::Done();
}

void CAIShapeObject::UpdateGameArea()
{
	if (m_bIgnoreGameUpdate)
		return;
	if (!IsCreateGameObjects())
		return;

	IAISystem* ai = GetIEditorImpl()->GetSystem()->GetAISystem();
	if (ai)
	{
		if (m_updateSucceed && !m_lastGameArea.IsEmpty())
			GetNavigation()->DeleteNavigationShape(m_lastGameArea);

		if (GetNavigation()->DoesNavigationShapeExists(GetName(), AREATYPE_GENERIC))
		{
			gEnv->pSystem->GetILog()->LogError("AI Shape", "Shape '%s' already exists in AIsystem, please rename the shape.", GetName());
			m_updateSucceed = false;
			return;
		}

		AreaPoints worldPts;
		const Matrix34& wtm = GetWorldTM();
		for (int i = 0; i < GetPointCount(); i++)
			worldPts.push_back(wtm.TransformPoint(GetPoint(i)));

		m_lastGameArea = GetName();
		if (!worldPts.empty())
		{
			CAIManager* pAIMgr = GetIEditorImpl()->GetAI();
			string anchorType(m_anchorType);
			int type = pAIMgr->AnchorActionToId(anchorType);
			SNavigationShapeParams params(m_lastGameArea, AREATYPE_GENERIC, false, mv_closed, &worldPts[0], worldPts.size(),
			                              mv_height, 0, type, (EAILightLevel)(int)m_lightLevel);

			m_updateSucceed = GetNavigation()->CreateNavigationShape(params);
		}
	}
	m_bAreaModified = false;
}

//////////////////////////////////////////////////////////////////////////
// CAIOclussionPlaneObject
//////////////////////////////////////////////////////////////////////////
CAIOcclusionPlaneObject::CAIOcclusionPlaneObject()
{
	m_bDisplayFilledWhenSelected = true;
	SetColor(RGB(24, 90, 231));
	m_bForce2D = true;
	mv_closed = true;
	mv_displayFilled = true;
}

void CAIOcclusionPlaneObject::Done()
{
	IAISystem* const pAI = GetIEditorImpl()->GetSystem()->GetAISystem();

	if (pAI != nullptr)
	{
		if (m_updateSucceed && !m_lastGameArea.IsEmpty())
		{
			GetNavigation()->DeleteNavigationShape(m_lastGameArea);
		}
	}

	__super::Done();
}

void CAIOcclusionPlaneObject::UpdateGameArea()
{
	if (m_bIgnoreGameUpdate)
		return;
	if (!IsCreateGameObjects())
		return;

	IAISystem* ai = GetIEditorImpl()->GetSystem()->GetAISystem();
	if (ai)
	{
		if (m_updateSucceed && !m_lastGameArea.IsEmpty())
		{
			GetNavigation()->DeleteNavigationShape(m_lastGameArea);
		}

		if (GetNavigation()->DoesNavigationShapeExists(GetName(), AREATYPE_OCCLUSION_PLANE))
		{
			gEnv->pSystem->GetILog()->LogError("OcclusionPlane", "Shape '%s' already exists in AIsystem, please rename the shape.", GetName());
			m_updateSucceed = false;
			return;
		}

		AreaPoints worldPts;
		const Matrix34& wtm = GetWorldTM();
		for (int i = 0; i < GetPointCount(); i++)
			worldPts.push_back(wtm.TransformPoint(GetPoint(i)));

		m_lastGameArea = GetName();
		if (!worldPts.empty())
		{
			SNavigationShapeParams params(m_lastGameArea, AREATYPE_OCCLUSION_PLANE, false, mv_closed, &worldPts[0], worldPts.size(), mv_height);
			m_updateSucceed = GetNavigation()->CreateNavigationShape(params);
		}
	}
	m_bAreaModified = false;
}

//////////////////////////////////////////////////////////////////////////
// CAIPerceptionModifier
//////////////////////////////////////////////////////////////////////////
CAIPerceptionModifierObject::CAIPerceptionModifierObject()
{
	m_bDisplayFilledWhenSelected = true;
	SetColor(RGB(24, 90, 231));
	m_bForce2D = false;
	mv_closed = false;
	mv_displayFilled = true;
	m_bPerVertexHeight = false;
	m_fReductionPerMetre = 0.0f;
	m_fReductionMax = 1.0f;
}

void CAIPerceptionModifierObject::InitVariables()
{
	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(m_fReductionPerMetre, "ReductionPerMetre", functor(*this, &CAIPerceptionModifierObject::OnShapeChange), IVariable::DT_PERCENT);
	m_pVarObject->AddVariable(m_fReductionMax, "ReductionMax", functor(*this, &CAIPerceptionModifierObject::OnShapeChange), IVariable::DT_PERCENT);
	m_pVarObject->AddVariable(mv_height, "Height", functor(*this, &CAIPerceptionModifierObject::OnSizeChange));
	m_pVarObject->AddVariable(mv_closed, "Closed");
	m_pVarObject->AddVariable(mv_displayFilled, "DisplayFilled");
}

void CAIPerceptionModifierObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CAIShapeObjectBase::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CAIPerceptionModifierObject>("AI Perception Modifier", [](CAIPerceptionModifierObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->m_pVarObject->SerializeVariable(&pObject->m_fReductionPerMetre, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->m_fReductionMax, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_height, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_closed, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_displayFilled, ar);
	});
}

void CAIPerceptionModifierObject::Done()
{
	IAISystem* const pAI = GetIEditorImpl()->GetSystem()->GetAISystem();

	if (pAI != nullptr)
	{
		if (!m_lastGameArea.IsEmpty())
		{
			GetNavigation()->DeleteNavigationShape(m_lastGameArea);
		}
	}

	__super::Done();
}

void CAIPerceptionModifierObject::UpdateGameArea()
{
	if (m_bIgnoreGameUpdate)
		return;
	if (!IsCreateGameObjects())
		return;

	IAISystem* pAI = GetIEditorImpl()->GetSystem()->GetAISystem();
	if (pAI)
	{
		if (!m_lastGameArea.IsEmpty())
			GetNavigation()->DeleteNavigationShape(m_lastGameArea);

		AreaPoints worldPts;
		const Matrix34& wtm = GetWorldTM();
		for (size_t i = 0; i < GetPointCount(); ++i)
		{
			worldPts.push_back(wtm.TransformPoint(m_points[i]));
		}

		m_lastGameArea = GetName();
		if (!worldPts.empty())
		{
			SNavigationShapeParams params(m_lastGameArea, AREATYPE_PERCEPTION_MODIFIER, false, mv_closed, &worldPts[0], worldPts.size(), mv_height);
			params.fReductionPerMetre = m_fReductionPerMetre;
			params.fReductionMax = m_fReductionMax;
			m_updateSucceed = GetNavigation()->CreateNavigationShape(params);
		}
	}
	m_bAreaModified = false;
}

void CAIPerceptionModifierObject::OnShapeChange(IVariable* pVariable)
{
	IAISystem* const pAI = GetIEditorImpl()->GetSystem()->GetAISystem();

	if (pAI != nullptr)
	{
		AreaPoints worldPts;
		const Matrix34& wtm = GetWorldTM();
		for (size_t i = 0; i < GetPointCount(); ++i)
		{
			worldPts.push_back(wtm.TransformPoint(m_points[i]));
		}

		if (!worldPts.empty())
		{
			SNavigationShapeParams params(m_lastGameArea, AREATYPE_PERCEPTION_MODIFIER, false, mv_closed, &worldPts[0], worldPts.size(), mv_height);
			params.fReductionPerMetre = m_fReductionPerMetre;
			params.fReductionMax = m_fReductionMax;
			GetNavigation()->CreateNavigationShape(params);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// CTerritory
//////////////////////////////////////////////////////////////////////////
CAITerritoryObject::CAITerritoryObject()
{
	m_entityClass = "AITerritory";

	m_bDisplayFilledWhenSelected = true;
	SetColor(RGB(24, 90, 231));
	m_bForce2D = false;
	mv_closed = true;
	mv_displayFilled = false;
	m_bPerVertexHeight = false;
}

void CAITerritoryObject::SetName(const string& newName)
{
	string oldName = GetName();

	__super::SetName(newName);

	GetIEditorImpl()->GetAI()->GetAISystem()->Reset(IAISystem::RESET_INTERNAL);

	GetObjectManager()->FindAndRenameProperty2("aiterritory_Territory", oldName, newName);
}

void CAITerritoryObject::InitVariables()
{
	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(mv_height, "Height", functor(*this, &CAITerritoryObject::OnSizeChange));
	m_pVarObject->AddVariable(mv_displayFilled, "DisplayFilled");
}

void CAITerritoryObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CAIShapeObjectBase::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CAITerritoryObject>("AI Territory", [](CAITerritoryObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_height, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_displayFilled, ar);
	});
}

void CAITerritoryObject::Done()
{
	IAISystem* const pAI = GetIEditorImpl()->GetSystem()->GetAISystem();

	if (pAI != nullptr)
	{
		if (!m_lastGameArea.IsEmpty())
		{
			GetNavigation()->DeleteNavigationShape(m_lastGameArea);
		}
	}

	__super::Done();
}

void CAITerritoryObject::UpdateGameArea()
{
	if (m_bIgnoreGameUpdate)
		return;
	if (!IsCreateGameObjects())
		return;

	IAISystem* pAI = GetIEditorImpl()->GetSystem()->GetAISystem();
	if (pAI)
	{
		if (!m_lastGameArea.IsEmpty())
			GetNavigation()->DeleteNavigationShape(m_lastGameArea);

		AreaPoints worldPts;
		const Matrix34& wtm = GetWorldTM();
		for (int i = 0; i < GetPointCount(); i++)
			worldPts.push_back(wtm.TransformPoint(GetPoint(i)));

		m_lastGameArea = GetName();
		if (!worldPts.empty())
		{
			SNavigationShapeParams params(m_lastGameArea, AREATYPE_GENERIC, false, mv_closed, &worldPts[0], worldPts.size(), mv_height, 0, AIANCHOR_COMBAT_TERRITORY);
			GetNavigation()->CreateNavigationShape(params);
		}
	}
	m_bAreaModified = false;
}

void CAITerritoryObject::GetLinkedWaves(std::vector<CAIWaveObject*>& result)
{
	result.clear();
	size_t const numEntities = GetEntityCount();

	for (size_t i = 0; i < numEntities; ++i)
	{
		CBaseObject* pBaseObject = GetEntity(i);
		if (pBaseObject->IsKindOf(RUNTIME_CLASS(CAIWaveObject)))
		{
			result.push_back(static_cast<CAIWaveObject*>(pBaseObject));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// GameShapeObject
//////////////////////////////////////////////////////////////////////////
CGameShapeObject::CGameShapeObject()
	: m_syncsPivotWithFirstPoint(false)
	, m_minPoints(3)
	, m_maxPoints(1000)
{
	m_entityClass = "";
}

bool CGameShapeObject::Init(CBaseObject * prev, const string & file)
{
	// Init will be called twice here since the OnClassChanged function deletes the entity and respawns it with the new class type.
	// therefore we have to check if the object was already initializing or not. 
	m_bInitialized = false;

	bool bResult = CShapeObject::Init(prev, file);

	m_bInitialized = true;

	return bResult;
}

void CGameShapeObject::InitVariables()
{
	bool noClassesAvailable = true;
	IGameVolumesEdit* pGameVolumesEdit = GetGameVolumesEdit();
	if (pGameVolumesEdit)
	{
		size_t volumeClassesCount = pGameVolumesEdit->GetVolumeClassesCount();
		for (size_t i = 0; i < volumeClassesCount; ++i)
		{
			const char* className = pGameVolumesEdit->GetVolumeClass(i);
			m_shapeClasses.AddEnumItem(className, (int)i);

			noClassesAvailable = false;
		}
	}

	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	// The VolumeClass variable should always be in the first place because the OnClassChanged function resets it and would override the already serialized variables.
	m_pVarObject->AddVariable(m_shapeClasses, "VolumeClass", functor(*this, &CGameShapeObject::OnClassChanged));

	m_pVarObject->AddVariable(mv_displayFilled, "DisplayFilled");
	m_pVarObject->AddVariable(mv_height, "Height", functor(*this, &CGameShapeObject::OnSizeChange));
	m_pVarObject->AddVariable(mv_closed, "Closed", functor(*this, &CGameShapeObject::OnClosedChanged));

	if (noClassesAvailable)
	{
		m_shapeClasses.SetFlags(m_shapeClasses.GetFlags() | IVariable::UI_INVISIBLE);
	}

	m_entityClass = m_shapeClasses.GetDisplayValue();
}

void CGameShapeObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CShapeObject::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CGameShapeObject>("Game Shape", [](CGameShapeObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->m_pVarObject->SerializeVariable(&pObject->m_shapeClasses, ar);
	});
}

void CGameShapeObject::OnClassChanged(IVariable* var)
{
	string classSelection = m_shapeClasses.GetDisplayValue();

	// Only reload the game object if the engine is not loading or the entity is initialized
	// So the object doesn't loose the serialized properties.
	if (classSelection != m_entityClass && !gEnv->pSystem->IsLoading() && m_bInitialized)
	{
		ReloadGameObject(classSelection);
	}
}

void CGameShapeObject::OnClosedChanged(IVariable* pVariable)
{
	UpdateGameShape();
}

void CGameShapeObject::ReloadGameObject(const string& newClass)
{
	UnloadScript();

	m_entityClass = newClass;

	Reload(true);

	ResetDefaultVariables();

	RefreshVariables();

	GetIEditor()->GetObjectManager()->EmitPopulateInspectorEvent();
}

void CGameShapeObject::ResetDefaultVariables()
{
	// Reset the adjustable properties
	mv_closed.SetFlags(mv_closed.GetFlags() & ~IVariable::UI_INVISIBLE);
	mv_closed.Set(true);

	mv_height.SetFlags(mv_height.GetFlags() & ~IVariable::UI_INVISIBLE);
	mv_height.Set(1.0f);
}

IGameVolumesEdit* CGameShapeObject::GetGameVolumesEdit() const
{
	if (!gEnv->pGameFramework)
		return nullptr;

	IGameVolumes* pGameVolumesMgr = gEnv->pGameFramework->GetIGameVolumesManager();
	return pGameVolumesMgr ? pGameVolumesMgr->GetEditorInterface() : nullptr;
}

bool CGameShapeObject::CreateGameObject()
{
	const bool output = __super::CreateGameObject();

	UpdateGameShape();

	return output;
}

void CGameShapeObject::UpdateGameArea()
{
	__super::UpdateGameArea();

	if (!m_bIgnoreGameUpdate)
	{
		UpdateGameShape();
	}
}

void CGameShapeObject::UpdateGameShape()
{
	IEntity* pEntity = GetIEntity();
	if (pEntity)
	{
		if (IGameVolumesEdit* pGameVolumesEdit = GetGameVolumesEdit())
		{
			IGameVolumes::VolumeInfo volumeInfo;
			volumeInfo.pVertices = m_points.size() > 0 ? &m_points[0] : nullptr;
			volumeInfo.verticesCount = m_points.size();
			volumeInfo.volumeHeight = mv_height;
			volumeInfo.closed = mv_closed;
			pGameVolumesEdit->SetVolume(pEntity->GetId(), volumeInfo);
		}

		NotifyPropertyChange();
	}
}

template<typename T>
bool CGameShapeObject::GetParameterValue(T& outValue, const char* functionName) const
{
	IScriptTable* pScript = GetIEntity() ? GetIEntity()->GetScriptTable() : NULL;
	if (pScript)
	{
		HSCRIPTFUNCTION scriptFunction(NULL);

		if (pScript->GetValue(functionName, scriptFunction) && (scriptFunction != NULL))
		{
			Script::CallReturn(gEnv->pScriptSystem, scriptFunction, outValue);

			gEnv->pScriptSystem->ReleaseFunc(scriptFunction);
			return true;
		}
	}

	return false;
}

bool CGameShapeObject::IsShapeOnly() const
{
	bool isShapeOnly = false;
	GetParameterValue(isShapeOnly, "IsShapeOnly");
	return isShapeOnly;
}

bool CGameShapeObject::GetSyncsPivotWithFirstPoint(bool& syncsPivotWithFirstPoint) const
{
	return GetParameterValue(syncsPivotWithFirstPoint, "GetSyncsPivotWithFirstPoint");
}

bool CGameShapeObject::GetForce2D(bool& bForce2D) const
{
	return GetParameterValue(bForce2D, "GetForce2D");
}

bool CGameShapeObject::GetZOffset(float& zOffset) const
{
	return GetParameterValue(zOffset, "GetZOffset");
}

bool CGameShapeObject::GetMinPointDistance(float& minDistance) const
{
	return GetParameterValue(minDistance, "GetMinPointDistance");
}

bool CGameShapeObject::GetIsClosedShape(bool& isClosed) const
{
	return GetParameterValue(isClosed, "IsClosed");
}

bool CGameShapeObject::GetCanEditClosed(bool& canEditClosed) const
{
	return GetParameterValue(canEditClosed, "CanEditClosed");
}

bool CGameShapeObject::NeedExportToGame() const
{
	bool exportToGame = true;
	GetParameterValue(exportToGame, "ExportToGame");
	return exportToGame;
}

void CGameShapeObject::RefreshVariables()
{
	if (IsShapeOnly())
	{
		mv_height.SetFlags(mv_height.GetFlags() | IVariable::UI_INVISIBLE);
		mv_height.Set(0.0f);
	}
	else
	{
		mv_height.SetFlags(mv_height.GetFlags() & ~IVariable::UI_INVISIBLE);
	}

	bool isClosed;
	if (GetIsClosedShape(isClosed))
	{
		if (isClosed)
		{
			mv_closed.SetFlags(mv_closed.GetFlags() & ~IVariable::UI_INVISIBLE);
			mv_closed.Set(true);
		}
		else
		{
			mv_closed.SetFlags(mv_closed.GetFlags() | IVariable::UI_INVISIBLE);
			mv_closed.Set(false);
		}
	}

	bool canEditClosed;
	if (GetCanEditClosed(canEditClosed))
	{
		if (canEditClosed)
		{
			mv_closed.SetFlags(mv_closed.GetFlags() & ~IVariable::UI_INVISIBLE);
		}
		else
		{
			mv_closed.SetFlags(mv_closed.GetFlags() | IVariable::UI_INVISIBLE);
		}
	}

	m_syncsPivotWithFirstPoint = false;
	GetSyncsPivotWithFirstPoint(m_syncsPivotWithFirstPoint);

	bool bForce2D = false;
	GetForce2D(bForce2D);
	m_bForce2D = bForce2D;

	m_zOffset = m_defaultZOffset;
	GetZOffset(m_zOffset);

	m_shapePointMinDistance = SHAPE_POINT_MIN_DISTANCE;
	GetMinPointDistance(m_shapePointMinDistance);

	if (m_pLuaProperties)
	{
		if (IVariable* pVariable_MinSpec = m_pLuaProperties->FindVariable("MinSpec"))
		{
			pVariable_MinSpec->SetFlags(pVariable_MinSpec->GetFlags() | IVariable::UI_INVISIBLE);
		}
		if (IVariable* pVariable_MaterialLayerMask = m_pLuaProperties->FindVariable("MaterialLayerMask"))
		{
			pVariable_MaterialLayerMask->SetFlags(pVariable_MaterialLayerMask->GetFlags() | IVariable::UI_INVISIBLE);
		}
	}

	SetCustomMinAndMaxPoints();

	// Delete not needed points in the shape if there are too many for the new object class
	while (GetPointCount() > GetMaxPoints())
	{
		RemovePoint(GetPointCount() - 1);
	}
}

void CGameShapeObject::SetMaterial(IEditorMaterial* mtl)
{
	__super::SetMaterial(mtl);

	NotifyPropertyChange();
}

void CGameShapeObject::NotifyPropertyChange()
{
	IEntity* pEntity = GetIEntity();
	if (pEntity != NULL)
	{
		SEntityEvent entityEvent;
		entityEvent.event = ENTITY_EVENT_EDITOR_PROPERTY_CHANGED;

		pEntity->SendEvent(entityEvent);
	}
}

void CGameShapeObject::SetMinSpec(uint32 nSpec, bool bSetChildren)
{
	__super::SetMinSpec(nSpec, bSetChildren);

	if (m_pLuaProperties != NULL)
	{
		if (IVariable* pVariable = m_pLuaProperties->FindVariable("MinSpec"))
		{
			pVariable->Set((int)nSpec);

			NotifyPropertyChange();
		}
	}
}

void CGameShapeObject::SetMaterialLayersMask(uint32 nLayersMask)
{
	__super::SetMaterialLayersMask(nLayersMask);

	if (m_pLuaProperties != NULL)
	{
		if (IVariable* pVariable = m_pLuaProperties->FindVariable("MaterialLayerMask"))
		{
			pVariable->Set((int)nLayersMask);

			NotifyPropertyChange();
		}
	}

}

XmlNodeRef CGameShapeObject::Export(const string& levelPath, XmlNodeRef& xmlNode)
{
	// Allow the game side of the entity to decide if we want to export the data
	// In same cases the game is storing the required data in its own format,
	// and we don't need the entities to be created in pure game mode, they only make sense in the editor
	if (NeedExportToGame())
	{
		return CEntityObject::Export(levelPath, xmlNode);
	}
	else
	{
		return XmlNodeRef(NULL);
	}
}

void CGameShapeObject::SpawnEntity()
{
	__super::SpawnEntity();
}

void CGameShapeObject::ReadEntityProperties()
{
	RefreshVariables();
}

void CGameShapeObject::DeleteEntity()
{

	if (IGameVolumesEdit* pVolumesEdit = GetGameVolumesEdit())
	{
		if (IEntity* pEntity = GetIEntity())
		{
			pVolumesEdit->DestroyVolume(pEntity->GetId());
		}
	}

	__super::DeleteEntity();
}

void CGameShapeObject::CalcBBox()
{
	if (m_points.empty())
		return;

	m_bbox.Reset();

	if (m_syncsPivotWithFirstPoint)
	{
		const Vec3 localPivot = m_points[0];

		for (Vec3& point : m_points)
		{
			point -= localPivot;
		}

		const Vec3 newWorldPivot = GetWorldTM().TransformPoint(localPivot);
		SetWorldPos(newWorldPivot);
	}

	for (int i = 0; i < m_points.size(); i++)
	{
		m_bbox.Add(m_points[i]);
	}

	if (mv_height)
	{
		AABB box;
		box.SetTransformedAABB(GetWorldTM(), m_bbox);
		m_lowestHeight = box.min.z;

		if (m_bPerVertexHeight)
		{
			m_bbox.max.z += mv_height;
		}
		else
		{
			box.max.z = max(box.max.z, (float)(m_lowestHeight + mv_height));
			Matrix34 mat = GetWorldTM().GetInverted();
			Vec3 p = mat.TransformPoint(box.max);
			m_bbox.max.z = max(m_bbox.max.z, p.z);
		}
	}
}
void CGameShapeObject::PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx)
{
	__super::PostClone(pFromObject, ctx);

	//////////////////////////////////////////////////////////////////////////
	// Make sure properties are refreshed after cloning
	if ((m_pEntityScript != NULL) && (m_pEntity != NULL))
	{
		if ((m_pLuaProperties != NULL) && (m_prototype == NULL))
		{
			m_pEntityScript->CopyPropertiesToScriptTable(m_pEntity, m_pLuaProperties, false);
		}

		m_pEntityScript->CallOnPropertyChange(m_pEntity);
	}

	NotifyPropertyChange();
}

int CGameShapeObject::GetMaxPoints() const
{
	return m_maxPoints;
}

int CGameShapeObject::GetMinPoints() const
{
	return m_minPoints;
}

void CGameShapeObject::OnPropertyChangeDone(IVariable* var)
{
	NotifyPropertyChange();
	if (var)
		UpdateGroup();
}

void CGameShapeObject::OnShapeChange(IVariable* pVariable)
{
	__super::OnShapeChange(pVariable);

	UpdateGameArea();
}

void CGameShapeObject::OnSizeChange(IVariable* pVariable)
{
	__super::OnSizeChange(pVariable);

	UpdateGameArea();
}

void CGameShapeObject::SetCustomMinAndMaxPoints()
{
	IScriptTable* pScript = GetIEntity() ? GetIEntity()->GetScriptTable() : NULL;
	if (pScript)
	{
		const char* functionNameMinPoints = "ShapeMinPoints";
		HSCRIPTFUNCTION scriptFunctionMinPoints(NULL);

		int minPoints = __super::GetMinPoints();
		if (pScript->GetValue(functionNameMinPoints, scriptFunctionMinPoints) && (scriptFunctionMinPoints != NULL))
		{
			Script::CallReturn(gEnv->pScriptSystem, scriptFunctionMinPoints, minPoints);

			gEnv->pScriptSystem->ReleaseFunc(scriptFunctionMinPoints);
		}
		m_minPoints = minPoints;

		const char* functionNameMaxPoints = "ShapeMaxPoints";
		HSCRIPTFUNCTION scriptFunctionMaxPoints(NULL);

		int maxPoints = __super::GetMaxPoints();
		if (pScript->GetValue(functionNameMaxPoints, scriptFunctionMaxPoints) && (scriptFunctionMaxPoints != NULL))
		{
			Script::CallReturn(gEnv->pScriptSystem, scriptFunctionMaxPoints, maxPoints);

			gEnv->pScriptSystem->ReleaseFunc(scriptFunctionMaxPoints);
		}
		m_maxPoints = maxPoints;
	}
}

//////////////////////////////////////////////////////////////////////////
/// GameShape Ledge Object
//////////////////////////////////////////////////////////////////////////

CGameShapeLedgeObject::CGameShapeLedgeObject()
{
	m_entityClass = "LedgeObject";
}

void CGameShapeLedgeObject::InitVariables()
{
	//Leave empty, we don't need any of the properties of the base class
}

void CGameShapeLedgeObject::UpdateGameArea()
{
	__super::UpdateGameArea();

	// update the vault shape in realtime
	NotifyPropertyChange();
}

void CGameShapeLedgeObject::SetPoint(int index, const Vec3& pos)
{
	__super::SetPoint(index, pos);

	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
/// GameShape LedgeStatic Object
//////////////////////////////////////////////////////////////////////////
CGameShapeLedgeStaticObject::CGameShapeLedgeStaticObject()
{
	m_entityClass = "LedgeObjectStatic";
}

//////////////////////////////////////////////////////////////////////////
// Navigation Area Object
//////////////////////////////////////////////////////////////////////////
CNavigationAreaObject::CNavigationAreaObject()
{
	m_bDisplayFilledWhenSelected = true;

	m_bForce2D = false;
	mv_closed = true;
	mv_displayFilled = true;
	m_bPerVertexHeight = true;

	m_volume = NavigationVolumeID();
	m_bRegistered = false;
	m_bListenerRegistered = false;
	m_bEditingPoints = false;
}

//////////////////////////////////////////////////////////////////////////
CNavigationAreaObject::~CNavigationAreaObject()
{
	mv_agentTypes.DeleteAllVariables();
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::Serialize(CObjectArchive& ar)
{
	XmlNodeRef xmlNode = ar.node;
	if (ar.bLoading)
	{
		// Patch to avoid that old NavigationAreaObject have a serialized entity class name.
		// The navigationAreaObject currently needs to have an empty entity class name after this point
		xmlNode->setAttr("EntityClass", "");
	}
	__super::Serialize(ar);
}


//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CAIShapeObjectBase::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CNavigationAreaObject>("Navigation Area", [](CNavigationAreaObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_height, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_exclusion, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_displayFilled, ar);

		for (int i = 0, n = pObject->mv_agentTypes.GetNumVariables(); i < n; ++i)
		{
			pObject->m_pVarObject->SerializeVariable(pObject->mv_agentTypes.GetVariable(i), ar);
		}

		if (ar.openBlock("Operators", "<Operators"))
		{
			if (bMultiEdit)
			{
				Serialization::SEditToolButton mergeButton("");
				mergeButton.SetToolClass(RUNTIME_CLASS(CMergeShapesTool), 0);
				ar(mergeButton, "merge", "^Merge");
				ar.closeBlock();
			}
			else
			{
				Serialization::SEditToolButton editShapeButton("");
				editShapeButton.SetToolClass(RUNTIME_CLASS(CEditShapeTool), "object", pObject);
				ar(editShapeButton, "edit_shape", "^Edit");

				Serialization::SEditToolButton splitShapeButton("");
				splitShapeButton.SetToolClass(RUNTIME_CLASS(CSplitShapeTool), "object", pObject);
				ar(splitShapeButton, "split_shape", "^Split");
				ar.closeBlock();
			}
			ar.closeBlock();
		}
	});
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();

	if (IsSelected() || gAINavigationPreferences.navigationShowAreas())
	{
		SetColor(mv_exclusion ? RGB(200, 0, 0) : RGB(0, 126, 255));

		float lineWidth = dc.GetLineWidth();
		dc.SetLineWidth(8.0f);
		__super::Display(objRenderHelper);
		dc.SetLineWidth(lineWidth);
	}
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::PostLoad(CObjectArchive& ar)
{
	if (IAISystem* aiSystem = GetIEditorImpl()->GetSystem()->GetAISystem())
	{
		if (INavigationSystem* aiNavigation = aiSystem->GetNavigationSystem())
		{
			RegisterNavigationArea(*aiNavigation, GetName().GetString());

			if (m_bRegistered)
			{
				RelinkWithMesh(ePostLoad);
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Unable to register Navigation Area with name '%s'", GetName().GetString());
			}
		}
	}

	__super::PostLoad(ar);
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::Done()
{
	__super::Done();

	if (m_bRegistered)
	{
		if (IAISystem* aiSystem = GetIEditorImpl()->GetSystem()->GetAISystem())
		{
			if (INavigationSystem* aiNavigation = aiSystem->GetNavigationSystem())
			{
				for (size_t i = 0; i < m_meshes.size(); ++i)
				{
					if (m_meshes[i])
						aiNavigation->DestroyMesh(m_meshes[i]);
				}

				m_meshes.clear();
				DestroyVolume();
				aiNavigation->UnRegisterArea(GetName().GetString());
			}
		}
		m_bRegistered = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::InitVariables()
{
	mv_height = 16;
	mv_exclusion = false;
	mv_displayFilled = false;

	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(mv_height, "Height", functor(*this, &CNavigationAreaObject::OnSizeChange));
	m_pVarObject->AddVariable(mv_exclusion, "Exclusion", functor(*this, &CNavigationAreaObject::OnShapeTypeChange));
	m_pVarObject->AddVariable(mv_displayFilled, "DisplayFilled");

	CAIManager* manager = GetIEditorImpl()->GetAI();
	size_t agentTypeCount = manager->GetNavigationAgentTypeCount();

	if (agentTypeCount)
	{
		mv_agentTypeVars.resize(agentTypeCount);
		m_pVarObject->AddVariable(mv_agentTypes, "AffectedAgentTypes", "Affected Agent Types");

		for (size_t i = 0; i < agentTypeCount; ++i)
		{

			const char* name = manager->GetNavigationAgentTypeName(i);
			m_pVarObject->AddVariable(mv_agentTypes, mv_agentTypeVars[i], name, name, functor(*this, &CNavigationAreaObject::OnShapeAgentTypeChange));
			mv_agentTypeVars[i].Set(true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::OnShapeTypeChange(IVariable* var)
{
	if (m_bIgnoreGameUpdate)
		return;

	UpdateMeshes();
	ApplyExclusion(mv_exclusion);
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::OnShapeAgentTypeChange(IVariable* var)
{
	if (m_bIgnoreGameUpdate)
		return;

	UpdateMeshes();
	ApplyExclusion(mv_exclusion);
}

void CNavigationAreaObject::SetInEditMode(bool editing)
{
	CAIShapeObjectBase::SetInEditMode(editing);

	m_bEditingPoints = editing;
	if (!editing && m_updateAfterEdit)
	{
		UpdateGameArea();
	}
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::OnNavigationEvent(const INavigationSystem::ENavigationEvent event)
{
	switch (event)
	{
	case INavigationSystem::MeshReloaded:
		ReregisterNavigationAreaAfterReload();
		RelinkWithMesh(eUpdateGameArea);
		break;
	case INavigationSystem::MeshReloadedAfterExporting:
		ReregisterNavigationAreaAfterReload();
		RelinkWithMesh(eRelinkOnly);
		break;
	case INavigationSystem::NavigationCleared:
		ReregisterNavigationAreaAfterReload();
		RelinkWithMesh(eUpdateGameArea);
		break;
	default:
		CRY_ASSERT(0);
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::OnContextMenu(CPopupMenuItem* menu)
{
	if (!mv_exclusion)
	{
		menu->Add("Update Navigation Area", functor(*this, &CNavigationAreaObject::UpdateGameArea));
	}
	CAIShapeObjectBase::OnContextMenu(menu);
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::UpdateMeshes()
{
	CRY_ASSERT(m_bRegistered);
	if (!m_bRegistered)
		return;

	CAIManager* manager = GetIEditorImpl()->GetAI();
	const size_t agentTypeCount = manager->GetNavigationAgentTypeCount();

	IAISystem* aiSystem = GetIEditorImpl()->GetSystem()->GetAISystem();

	if (mv_exclusion)
	{
		for (size_t i = 0; i < m_meshes.size(); ++i)
		{
			if (const NavigationMeshID meshID = m_meshes[i])
			{
				aiSystem->GetNavigationSystem()->DestroyMesh(meshID);
			}
		}
		m_meshes.clear();
	}
	else
	{
		m_meshes.resize(agentTypeCount);

		for (size_t i = 0; i < agentTypeCount; ++i)
		{
			NavigationMeshID meshID = m_meshes[i];

			if (mv_agentTypeVars[i] && !meshID)
			{
				NavigationAgentTypeID agentTypeID = aiSystem->GetNavigationSystem()->GetAgentTypeID(i);

				INavigationSystem::CreateMeshParams params; // TODO: expose at least the tile size
				m_meshes[i] = aiSystem->GetNavigationSystem()->CreateMeshForVolumeAndUpdate(GetName().GetString(), agentTypeID, params, m_volume);
			}
			else if (!mv_agentTypeVars[i] && meshID)
			{
				aiSystem->GetNavigationSystem()->DestroyMesh(meshID);
				m_meshes[i] = NavigationMeshID();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::ApplyExclusion(bool apply)
{
	CRY_ASSERT(m_bRegistered);
	if (!m_bRegistered)
		return;

	CAIManager* manager = GetIEditorImpl()->GetAI();
	size_t agentTypeCount = manager->GetNavigationAgentTypeCount();

	IAISystem* aiSystem = GetIEditorImpl()->GetSystem()->GetAISystem();

	std::vector<NavigationAgentTypeID> affectedAgentTypes;

	if (apply)
	{
		affectedAgentTypes.reserve(agentTypeCount);

		for (size_t i = 0; i < agentTypeCount; ++i)
		{
			if (mv_agentTypeVars[i])
			{
				NavigationAgentTypeID agentTypeID = aiSystem->GetNavigationSystem()->GetAgentTypeID(i);
				affectedAgentTypes.push_back(agentTypeID);
			}
		}
	}

	if (affectedAgentTypes.empty())
		aiSystem->GetNavigationSystem()->SetExclusionVolume(0, 0, m_volume);
	else
		aiSystem->GetNavigationSystem()->SetExclusionVolume(&affectedAgentTypes[0], affectedAgentTypes.size(), m_volume);
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::RelinkWithMesh(const ERelinkWithMeshesMode relinkMode)
{
	if (!m_bRegistered)
		return;

	IAISystem* pAISystem = GetIEditorImpl()->GetSystem()->GetAISystem();

	if (!pAISystem)
		return;

	INavigationSystem* pAINavigation = pAISystem->GetNavigationSystem();
	CRY_ASSERT(pAINavigation);
	if (!pAINavigation)
		return;

	const char* navigationAreaName = GetName().GetString();
	const NavigationVolumeID volumeId = pAINavigation->GetAreaId(navigationAreaName);
	CRY_ASSERT(volumeId == m_volume);
	if (volumeId != m_volume)
	{
		AIWarning("Navigation area '%s' went out of sync with Navigation System: expected volumeId = %u, actual volumeId = %u",
		          (uint32)volumeId, (uint32)m_volume);
	}

	if (!mv_exclusion)
	{
		const size_t agentTypeCount = pAINavigation->GetAgentTypeCount();
		m_meshes.resize(agentTypeCount);
		for (size_t i = 0; i < agentTypeCount; ++i)
		{
			const NavigationAgentTypeID agentTypeID = pAINavigation->GetAgentTypeID(i);
			m_meshes[i] = pAINavigation->GetMeshID(navigationAreaName, agentTypeID);
		}
	}
	else if (relinkMode & ePostLoad)
	{
		// We just loaded an exclusion area - let's make sure, that there are no meshes left for this area.
		const size_t agentTypeCount = pAINavigation->GetAgentTypeCount();
		for (size_t i = 0; i < agentTypeCount; ++i)
		{
			const NavigationAgentTypeID agentTypeID = pAINavigation->GetAgentTypeID(i);
			if (const NavigationMeshID meshId = pAINavigation->GetMeshID(navigationAreaName, agentTypeID))
			{
				pAINavigation->DestroyMesh(meshId);
			}
		}
	}

	/*
	   FR: We need to update the game area even in the case that after having read
	   the data from the binary file the volumewas not recreated.
	   This happens when there was no mesh associated to the actual volume.
	 */
	if ((relinkMode & eUpdateGameArea) || (!pAINavigation->ValidateVolume(m_volume) && !(relinkMode & ePostLoad)))
		UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::CreateVolume(Vec3* points, size_t pointsSize, NavigationVolumeID requestedID /* = NavigationVolumeID */)
{
	CRY_ASSERT(m_bRegistered);
	if (!m_bRegistered)
		return;

	if (IAISystem* aiSystem = GetIEditorImpl()->GetSystem()->GetAISystem())
	{
		if (INavigationSystem* aiNavigation = aiSystem->GetNavigationSystem())
		{
			const char* navigationAreaName = GetName().GetString();
			m_volume = aiNavigation->CreateVolume(points, pointsSize, mv_height, requestedID);
			aiNavigation->RegisterListener(this, navigationAreaName);
			if (requestedID == NavigationVolumeID())
				aiNavigation->SetAreaId(navigationAreaName, m_volume);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::RegisterNavigationArea(INavigationSystem& navigationSystem, const char* szAreaName)
{
	CRY_ASSERT(m_bRegistered == false);
	CRY_ASSERT(m_volume == NavigationVolumeID());
	m_bRegistered = navigationSystem.RegisterArea(szAreaName, m_volume);
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::ReregisterNavigationAreaAfterReload()
{
	if (m_bRegistered)
	{
		CRY_ASSERT(m_volume != NavigationVolumeID());

		m_bRegistered = false;
		m_volume = NavigationVolumeID();

		const char* szName = GetName().GetString();

		if (IAISystem* pAISystem = GetIEditorImpl()->GetSystem()->GetAISystem())
		{
			if (INavigationSystem* pNavigationSystem = pAISystem->GetNavigationSystem())
			{

				// At this moment, navigation system cleared registered area ID's, so it should be empty.
				// TODO pavloi 2016.10.24: areas in prefabs are not yet supported, they'll have same names and assert is triggered.
				//CRY_ASSERT(NavigationVolumeID() == pNavigationSystem->GetAreaId(szName));

				RegisterNavigationArea(*pNavigationSystem, szName);
			}
		}

		if (!m_bRegistered)
		{
			AIWarning("Navigation area '%s' failed to re-register itself after Navigation System reloaded the data.", szName);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::RegisterNavigationListener(INavigationSystem& navigationSystem, const char* szAreaName)
{
	if (!m_bListenerRegistered)
	{
		navigationSystem.RegisterListener(this, szAreaName);
		m_bListenerRegistered = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::UnregisterNavigationListener(INavigationSystem& navigationSystem)
{
	if (m_bListenerRegistered)
	{
		navigationSystem.UnRegisterListener(this);
		m_bListenerRegistered = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::DestroyVolume()
{
	if (!m_volume)
		return;
	if (IAISystem* aiSystem = GetIEditorImpl()->GetSystem()->GetAISystem())
	{
		if (INavigationSystem* aiNavigation = aiSystem->GetNavigationSystem())
		{
			aiNavigation->DestroyVolume(m_volume);
			UnregisterNavigationListener(*aiNavigation);
			m_volume = NavigationVolumeID();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::UpdateGameArea()
{
	if (m_bIgnoreGameUpdate)
		return;

	if (!IsCreateGameObjects())
		return;

	if (!m_bRegistered)
		return;

	if (m_bEditingPoints || m_bBeingManuallyCreated)
	{
		m_updateAfterEdit = true;
		return;
	}
	m_updateAfterEdit = false;

	if (IAISystem* aiSystem = GetIEditorImpl()->GetSystem()->GetAISystem())
	{
		const Matrix34& worldTM = GetWorldTM();
		const size_t pointCount = GetPointCount();

		if (pointCount > 2)
		{
			INavigationSystem* aiNavigation = aiSystem->GetNavigationSystem();
			CRY_ASSERT(aiNavigation);
			AreaPoints points;
			points.reserve(pointCount);

			for (size_t i = 0; i < pointCount; ++i)
				points.push_back(worldTM.TransformPoint(GetPoint(i)));

			// The volume could be set but if the binary data didn't exist the volume
			// was not correctly recreated
			if (!m_volume || !aiNavigation->ValidateVolume(m_volume))
			{
				CreateVolume(&points[0], points.size(), m_volume);
			}
			else
			{
				aiNavigation->SetVolume(m_volume, &points[0], points.size(), mv_height);
			}

			RegisterNavigationListener(*aiNavigation, GetName().GetString());

			UpdateMeshes();
		}
		else if (m_volume)
		{
			DestroyVolume();
			UpdateMeshes();
		}

		ApplyExclusion(mv_exclusion);
	}

	m_lastGameArea = GetName();
	m_bAreaModified = false;
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::SetName(const string& name)
{
	__super::SetName(name);

	if (IAISystem* aiSystem = GetIEditorImpl()->GetSystem()->GetAISystem())
	{
		INavigationSystem* pNavigationSystem = aiSystem->GetNavigationSystem();
		CRY_ASSERT(pNavigationSystem);
		if (pNavigationSystem)
		{
			if (m_bRegistered)
			{
				CRY_ASSERT(m_volume != NavigationVolumeID());
				pNavigationSystem->UpdateAreaNameForId(m_volume, name);
				for (size_t i = 0; i < m_meshes.size(); ++i)
				{
					if (m_meshes[i])
						pNavigationSystem->SetMeshName(m_meshes[i], name.GetString());
				}
			}
			else
			{
				// NOTE pavloi 2016.06.02: this function is called when the new area is created in editor, so let's register it from here
				RegisterNavigationArea(*pNavigationSystem, GetName().GetString());
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::RemovePoint(int index)
{
	__super::RemovePoint(index);

	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
int CNavigationAreaObject::InsertPoint(int index, const Vec3& point, bool const bModifying)
{
	int result = __super::InsertPoint(index, point, bModifying);

	UpdateGameArea();

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::SetPoint(int index, const Vec3& pos)
{
	__super::SetPoint(index, pos);

	UpdateGameArea();
}

void CNavigationAreaObject::OnSizeChange(IVariable* pVariable)
{
	__super::OnSizeChange(pVariable);
	UpdateGameArea();
}

void CNavigationAreaObject::EndCreation()
{
	// initialNavigationAreaHeightOffset is applied after the shape is created, otherwise points of the shape won't be properly visible during the editing of the shape
	// Navigation area should be slightly below the terrain so that agents' positions are found inside the navigation volume.
	const float zOffset = GetShapeZOffset();
	for (size_t i = 1, count = m_points.size(); i < count; ++i)
	{
		// skip the first point, it should be zero every time
		m_points[i].z -= zOffset;
	}

	Vec3 position = GetPos();
	position.z += gAINavigationPreferences.initialNavigationAreaHeightOffset() - zOffset;
	SetPos(position);
	
	__super::EndCreation();
}

//////////////////////////////////////////////////////////////////////////
void CNavigationAreaObject::ChangeColor(COLORREF color)
{
	SetModified(false, false);
}

//////////////////////////////////////////////////////////////////////////
namespace
{
static void PyInsertPoint(const char* objName, int idx, float xPos, float yPos, float zPos)
{
	CBaseObject* pObject;

	if (GetIEditorImpl()->GetObjectManager()->FindObject(objName))
		pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objName);
	else if (GetIEditorImpl()->GetObjectManager()->FindObject(CryGUID::FromString(objName)))
		pObject = GetIEditorImpl()->GetObjectManager()->FindObject(CryGUID::FromString(objName));
	else
		throw std::logic_error(string("\"") + objName + "\" is an invalid object.");

	if (pObject->IsKindOf(RUNTIME_CLASS(CNavigationAreaObject)))
	{
		CNavigationAreaObject* pNavArea = static_cast<CNavigationAreaObject*>(pObject);
		if (pNavArea != NULL)
		{
			Vec3 pos(xPos, yPos, zPos);
			pNavArea->SetPoint(idx, pos);
			pNavArea->InsertPoint(-1, pos, false);
		}
	}
}
}

REGISTER_PYTHON_COMMAND(PyInsertPoint, general, nav_insert_point, "Added a point at the given position to the given nav area");

