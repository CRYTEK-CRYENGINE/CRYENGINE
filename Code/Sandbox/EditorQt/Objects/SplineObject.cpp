// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Viewport.h"

#include "SplineObject.h"
#include "Objects/ObjectLoader.h"
#include "Objects/InspectorWidgetCreator.h"
#include "Gizmos/IGizmoManager.h"

#include "Serialization/Decorators/EditToolButton.h"
#include "Controls/DynamicPopupMenu.h"

//////////////////////////////////////////////////////////////////////////
// CEditSplineObjectTool

class CEditSplineObjectTool : public CEditTool, public ITransformManipulatorOwner
{
public:
	DECLARE_DYNCREATE(CEditSplineObjectTool)

	CEditSplineObjectTool() :
		m_pSpline(0),
		m_currPoint(-1),
		m_modifying(false),
		m_curCursor(STD_CURSOR_DEFAULT),
		m_pManipulator(nullptr)
	{}

	// Ovverides from CEditTool
	virtual string GetDisplayName() const override { return "Edit Spline"; }
	bool           MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags);
	void           OnManipulatorDrag(IDisplayViewport* pView, ITransformManipulator* pManipulator, const Vec2i& point0, const Vec3& value, int flags);
	void           OnManipulatorBegin(IDisplayViewport* view, ITransformManipulator* pManipulator, const Vec2i& point, int flags);
	void           OnManipulatorEnd(IDisplayViewport* view, ITransformManipulator* pManipulator);

	virtual void   SetUserData(const char* key, void* userData);

	virtual void   Display(DisplayContext& dc) {}
	virtual bool   OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags);

	bool           IsNeedMoveTool() override                 { return true; }

	void           OnSplineEvent(CBaseObject* pObj, int evt);

	// ITransformManipulatorOwner
	virtual void GetManipulatorPosition(Vec3& position) override;
	virtual bool IsManipulatorVisible() override;

protected:
	virtual ~CEditSplineObjectTool();
	void DeleteThis() { delete this; }

	void SelectPoint(int index);
	void SetCursor(EStdCursor cursor, bool bForce = false);

private:
	CSplineObject*         m_pSpline;
	int                    m_currPoint;
	bool                   m_modifying;
	CPoint                 m_mouseDownPos;
	Vec3                   m_pointPos;
	EStdCursor             m_curCursor;
	ITransformManipulator* m_pManipulator;
};

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CEditSplineObjectTool, CEditTool)

//////////////////////////////////////////////////////////////////////////
void CEditSplineObjectTool::SetUserData(const char* key, void* userData)
{
	m_pSpline = (CSplineObject*)userData;
	assert(m_pSpline != 0);

	m_pSpline->SetEditMode(true);

	// Modify Spline undo.
	if (!CUndo::IsRecording())
	{
		CUndo("Modify Spline");
		m_pSpline->StoreUndo("Spline Modify");
	}

	m_pSpline->AddEventListener(functor(*this, &CEditSplineObjectTool::OnSplineEvent));

	if (GetIEditorImpl()->GetEditMode() == eEditModeSelect)
	{
		GetIEditorImpl()->SetEditMode(eEditModeMove);
	}

	SelectPoint(-1);

	m_pManipulator = GetIEditorImpl()->GetGizmoManager()->AddManipulator(this);
	m_pManipulator->signalBeginDrag.Connect(this, &CEditSplineObjectTool::OnManipulatorBegin);
	m_pManipulator->signalDragging.Connect(this, &CEditSplineObjectTool::OnManipulatorDrag);
	m_pManipulator->signalEndDrag.Connect(this, &CEditSplineObjectTool::OnManipulatorEnd);
}

void CEditSplineObjectTool::GetManipulatorPosition(Vec3& position)
{
	const Matrix34& wtm = m_pSpline->GetWorldTM();
	int index = m_pSpline->GetSelectedPoint();
	position = wtm * m_pSpline->GetPoint(index);
}

bool CEditSplineObjectTool::IsManipulatorVisible()
{
	if (m_pSpline)
	{
		return m_pSpline->GetSelectedPoint() != -1;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
CEditSplineObjectTool::~CEditSplineObjectTool()
{
	if (m_pSpline)
	{
		m_pSpline->SetEditMode(false);
		SelectPoint(-1);
		m_pSpline->RemoveEventListener(functor(*this, &CEditSplineObjectTool::OnSplineEvent));

	}
	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
		GetIEditorImpl()->GetIUndoManager()->Cancel();

	if (m_pManipulator)
	{
		m_pManipulator->signalBeginDrag.DisconnectAll();
		m_pManipulator->signalDragging.DisconnectAll();
		m_pManipulator->signalEndDrag.DisconnectAll();
		GetIEditorImpl()->GetGizmoManager()->RemoveManipulator(m_pManipulator);
		m_pManipulator = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEditSplineObjectTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (nChar == Qt::Key_Escape)
	{
		GetIEditorImpl()->SetEditTool(0);
	}
	else if (nChar == Qt::Key_Delete)
	{
		int index = m_pSpline->GetSelectedPoint();
		if (index >= 0)
		{
			CUndo undo("Remove Spline Point");
			m_pSpline->SelectPoint(-1);
			m_pSpline->RemovePoint(index);

			if (m_pManipulator)
			{
				m_pManipulator->Invalidate();
			}
		}
	}
	return true;
}

void CEditSplineObjectTool::OnSplineEvent(CBaseObject* pObj, int evt)
{
	if (evt == OBJECT_ON_UI_PROPERTY_CHANGED && m_pManipulator)
	{
		m_pManipulator->Invalidate();
	}
}

//////////////////////////////////////////////////////////////////////////
void CEditSplineObjectTool::SelectPoint(int index)
{
	if (index < 0)
		SetCursor(STD_CURSOR_DEFAULT, true);

	if (!m_pSpline)
		return;

	if (m_pManipulator)
	{
		m_pManipulator->Invalidate();
	}

	m_pSpline->SelectPoint(index);
}

//////////////////////////////////////////////////////////////////////////
void CEditSplineObjectTool::OnManipulatorDrag(IDisplayViewport* pView, ITransformManipulator* pManipulator, const Vec2i& point0, const Vec3& value, int flags)
{
	// get world/local coordinate system setting.
	RefCoordSys coordSys = GetIEditorImpl()->GetReferenceCoordSys();
	int editMode = GetIEditorImpl()->GetEditMode();

	// get current axis constrains.
	if (editMode == eEditModeMove)
	{
		GetIEditorImpl()->GetIUndoManager()->Restore();
		const Matrix34& splineTM = m_pSpline->GetWorldTM();
		Matrix34 invSplineTM = splineTM;
		invSplineTM.Invert();

		int index = m_pSpline->GetSelectedPoint();
		Vec3 pos = m_pSpline->GetPoint(index);

		Vec3 wp = splineTM.TransformPoint(pos);
		Vec3 newPos = wp + value;

		if (!GetAsyncKeyState(VK_CONTROL) && GetIEditorImpl()->IsSnapToTerrainEnabled())
		{
			float height = wp.z - GetIEditorImpl()->GetTerrainElevation(wp.x, wp.y);
			newPos.z = GetIEditorImpl()->GetTerrainElevation(newPos.x, newPos.y) + height;
		}

		if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
			m_pSpline->StoreUndo("Move Point");

		newPos = invSplineTM.TransformPoint(newPos);
		m_pSpline->SetPoint(m_pSpline->GetSelectedPoint(), newPos);

		if (m_pManipulator)
		{
			m_pManipulator->Invalidate();
		}
	}
}

void CEditSplineObjectTool::OnManipulatorBegin(IDisplayViewport* view, ITransformManipulator* pManipulator, const Vec2i& point, int flags)
{
	int editMode = GetIEditorImpl()->GetEditMode();

	if (editMode == eEditModeMove)
	{
		m_modifying = true;
		// Accept changes.
		m_pSpline->CalcBBox(); // TODO: Avoid and make CalcBBox() private
		m_pSpline->OnUpdate();

		m_modifying = false;
	}
}

void CEditSplineObjectTool::OnManipulatorEnd(IDisplayViewport* view, ITransformManipulator* pManipulator)
{
	int editMode = GetIEditorImpl()->GetEditMode();

	if (editMode == eEditModeMove)
	{
		m_modifying = true;
		// Accept changes.
		m_pSpline->CalcBBox(); // TODO: Avoid and make CalcBBox() private
		m_pSpline->OnUpdate();

		m_modifying = false;
	}
}



//////////////////////////////////////////////////////////////////////////
bool CEditSplineObjectTool::MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	if (!m_pSpline)
		return false;

	if (event == eMouseLDown)
	{
		m_mouseDownPos = point;
	}

	if (event == eMouseLDown || event == eMouseMove || event == eMouseLDblClick || event == eMouseLUp)
	{

		const Matrix34& splineTM = m_pSpline->GetWorldTM();

		Vec3 raySrc, rayDir;
		view->ViewToWorldRay(point, raySrc, rayDir);

		// Find closest point on the spline.
		int p1, p2;
		float dist;
		Vec3 intPnt(0, 0, 0);
		m_pSpline->GetNearestEdge(raySrc, rayDir, p1, p2, dist, intPnt);

		float fSplineCloseDistance = kSplinePointSelectionRadius * view->GetScreenScaleFactor(intPnt) * 0.01f;

		if ((flags & MK_CONTROL) && !m_modifying)
		{
			// If control we are editing edges..
			if (p1 >= 0 && p2 >= 0 && dist < fSplineCloseDistance + view->GetSelectionTolerance())
			{
				// Cursor near one of edited Spline edges.
				view->ResetCursor();
				if (event == eMouseLDown)
				{
					m_modifying = true;
					CUndo undo("Spline Make Point");
					if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
						m_pSpline->StoreUndo("Make Point");

					// If last edge, insert at end.
					if (p2 == 0)
						p2 = -1;

					// Create new point between nearest edge.
					// Put intPnt into local space of Spline.
					intPnt = splineTM.GetInverted().TransformPoint(intPnt);

					int index = m_pSpline->InsertPoint(p2, intPnt);
					SelectPoint(index);

					// Set construction plane for view.
					m_pointPos = splineTM.TransformPoint(m_pSpline->GetPoint(index));
					Matrix34 tm;
					tm.SetIdentity();
					tm.SetTranslation(m_pointPos);
					view->SetConstructionMatrix(tm);
				}
			}
			return true;
		}

		int index = m_pSpline->GetNearestPoint(raySrc, rayDir, dist);
		if (index >= 0 && dist < fSplineCloseDistance + view->GetSelectionTolerance())
		{
			// Cursor near one of edited Spline points.
			SetCursor(GetIEditorImpl()->GetEditMode() ? STD_CURSOR_MOVE : STD_CURSOR_HIT);

			if (event == eMouseLDown)
			{
				if (!m_modifying)
				{
					SelectPoint(index);

					// Set construction plance for view.
					m_pointPos = splineTM.TransformPoint(m_pSpline->GetPoint(index));
					Matrix34 tm;
					tm.SetIdentity();
					tm.SetTranslation(m_pointPos);
					view->SetConstructionMatrix(tm);
				}
			}

			if (event == eMouseLDblClick)
			{
				CUndo undo("Remove Spline Point");
				m_modifying = false;
				m_pSpline->RemovePoint(index);
				SelectPoint(-1);
			}
		}
		else
		{
			SetCursor(STD_CURSOR_DEFAULT);

			if (event == eMouseLDown)
			{
				SelectPoint(-1);
			}
		}

		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CEditSplineObjectTool::SetCursor(EStdCursor cursor, bool bForce)
{
	CViewport* pViewport = GetIEditorImpl()->GetActiveView();
	if ((m_curCursor != cursor || bForce) && pViewport)
	{
		m_curCursor = cursor;
		pViewport->SetCurrentCursor(m_curCursor);
	}
}

//////////////////////////////////////////////////////////////////////////
// CSplitSplineObjectTool

class CSplitSplineObjectTool : public CEditTool
{
public:
	DECLARE_DYNCREATE(CSplitSplineObjectTool)

	CSplitSplineObjectTool();

	// Ovverides from CEditTool
	virtual string GetDisplayName() const override { return "Split Spline"; }
	bool           MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags);
	virtual void   SetUserData(const char* key, void* userData);
	virtual void   Display(DisplayContext& dc) {};
	virtual bool   OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags);

protected:
	virtual ~CSplitSplineObjectTool();
	void DeleteThis() { delete this; };

private:
	CSplineObject* m_pSpline;
	int            m_curPoint;
};

IMPLEMENT_DYNCREATE(CSplitSplineObjectTool, CEditTool)

//////////////////////////////////////////////////////////////////////////
CSplitSplineObjectTool::CSplitSplineObjectTool()
{
	m_pSpline = 0;
	m_curPoint = -1;
}

//////////////////////////////////////////////////////////////////////////
void CSplitSplineObjectTool::SetUserData(const char* key, void* userData)
{
	m_curPoint = -1;
	m_pSpline = (CSplineObject*)userData;
	assert(m_pSpline != 0);

	// Modify Spline undo.
	if (!CUndo::IsRecording())
	{
		CUndo("Modify Spline");
		m_pSpline->StoreUndo("Spline Modify");
	}
}

//////////////////////////////////////////////////////////////////////////
CSplitSplineObjectTool::~CSplitSplineObjectTool()
{
	//if (m_pSpline)
	//m_pSpline->SetSplitPoint( -1,Vec3(0,0,0), -1 );
	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
		GetIEditorImpl()->GetIUndoManager()->Cancel();
}

//////////////////////////////////////////////////////////////////////////
bool CSplitSplineObjectTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (nChar == Qt::Key_Escape)
	{
		GetIEditorImpl()->SetEditTool(0);
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CSplitSplineObjectTool::MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	if (!m_pSpline)
		return false;

	if (event == eMouseLDown || event == eMouseMove)
	{
		const Matrix34& shapeTM = m_pSpline->GetWorldTM();

		float dist;
		Vec3 raySrc, rayDir;
		view->ViewToWorldRay(point, raySrc, rayDir);

		// Find closest point on the shape.
		int p1, p2;
		Vec3 intPnt;
		m_pSpline->GetNearestEdge(raySrc, rayDir, p1, p2, dist, intPnt);

		float fShapeCloseDistance = kSplinePointSelectionRadius * view->GetScreenScaleFactor(intPnt) * 0.01f;

		// If control we are editing edges..
		if (p1 >= 0 && p2 > 0 && dist < fShapeCloseDistance + view->GetSelectionTolerance())
		{
			view->SetCurrentCursor(STD_CURSOR_HIT);
			// Put intPnt into local space of shape.
			intPnt = shapeTM.GetInverted().TransformPoint(intPnt);

			if (event == eMouseLDown)
			{
				GetIEditorImpl()->GetIUndoManager()->Begin();
				m_pSpline->Split(p2, intPnt);
				if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
					GetIEditorImpl()->GetIUndoManager()->Accept("Split Spline");
				GetIEditorImpl()->SetEditTool(0);
			}
		}
		else
		{
			view->ResetCursor();
		}

		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// CMergeSplineObjectsTool

class CMergeSplineObjectsTool : public CEditTool
{
public:
	DECLARE_DYNCREATE(CMergeSplineObjectsTool)

	CMergeSplineObjectsTool();

	// Overrides from CEditTool
	virtual string GetDisplayName() const override { return "Merge Splines"; }
	bool           MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags);
	virtual void   SetUserData(const char* key, void* userData);
	virtual void   Display(DisplayContext& dc) {};
	virtual bool   OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags);

protected:
	virtual ~CMergeSplineObjectsTool();
	void DeleteThis() { delete this; };

	int            m_curPoint;
	CSplineObject* m_pSpline;

private:
};

IMPLEMENT_DYNCREATE(CMergeSplineObjectsTool, CEditTool)

//////////////////////////////////////////////////////////////////////////
CMergeSplineObjectsTool::CMergeSplineObjectsTool()
{
	m_curPoint = -1;
	m_pSpline = 0;
}

//////////////////////////////////////////////////////////////////////////
void CMergeSplineObjectsTool::SetUserData(const char* key, void* userData)
{
	m_pSpline = (CSplineObject*)userData;
	assert(m_pSpline != 0);

	// Modify Spline undo.
	if (!CUndo::IsRecording())
	{
		CUndo("Spline Merging");
		m_pSpline->StoreUndo("Spline Merging");
	}

	m_pSpline->SelectPoint(-1);
}

//////////////////////////////////////////////////////////////////////////
CMergeSplineObjectsTool::~CMergeSplineObjectsTool()
{
	if (m_pSpline)
		m_pSpline->SetMergeIndex(-1);
	m_pSpline = 0;
	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
		GetIEditorImpl()->GetIUndoManager()->Cancel();
}

//////////////////////////////////////////////////////////////////////////
bool CMergeSplineObjectsTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (nChar == Qt::Key_Escape)
	{
		GetIEditorImpl()->SetEditTool(0);
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CMergeSplineObjectsTool::MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	//return true;
	if (event == eMouseLDown || event == eMouseMove)
	{
		HitContext hc;
		hc.view = view;
		hc.point2d = point;
		view->ViewToWorldRay(point, hc.raySrc, hc.rayDir);
		if (!GetIEditorImpl()->GetObjectManager()->HitTest(hc))
			return false;

		CBaseObject* pObj = hc.object;

		if (!pObj->IsKindOf(RUNTIME_CLASS(CSplineObject)))
			return false;

		CSplineObject* pSpline = (CSplineObject*)pObj;

		if (!(m_curPoint == -1 && pSpline == m_pSpline || m_curPoint != -1 && pSpline != m_pSpline))
			return false;

		const Matrix34& splineTM = pSpline->GetWorldTM();

		float dist;
		Vec3 raySrc, rayDir;
		view->ViewToWorldRay(point, raySrc, rayDir);

		// Find closest point on the Spline.
		int p1, p2;
		Vec3 intPnt;
		pSpline->GetNearestEdge(raySrc, rayDir, p1, p2, dist, intPnt);

		if (p1 < 0 || p2 <= 0)
			return false;

		float fShapeCloseDistance = kSplinePointSelectionRadius * view->GetScreenScaleFactor(intPnt) * 0.01f;

		if (dist > fShapeCloseDistance + view->GetSelectionTolerance())
			return false;

		int cnt = pSpline->GetPointCount();
		if (cnt < 2)
			return false;

		int p = pSpline->GetNearestPoint(raySrc, rayDir, dist);
		if (p != 0 && p != cnt - 1)
			return false;

		Vec3 pnt = splineTM.TransformPoint(pSpline->GetPoint(p));

		if (intPnt.GetDistance(pnt) > fShapeCloseDistance + view->GetSelectionTolerance())
			return false;

		view->SetCurrentCursor(STD_CURSOR_HIT);

		if (event == eMouseLDown)
		{
			if (m_curPoint == -1)
			{
				m_curPoint = p;
				m_pSpline->SetMergeIndex(p);
			}
			else
			{
				GetIEditorImpl()->GetIUndoManager()->Begin();
				if (m_pSpline)
				{
					pSpline->SetMergeIndex(p);
					m_pSpline->Merge(pSpline);
				}
				if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
					GetIEditorImpl()->GetIUndoManager()->Accept("Spline Merging");
				GetIEditorImpl()->SetEditTool(0);
			}
		}

		return true;
	}

	return false;
}

IMPLEMENT_DYNAMIC(CSplineObject, CBaseObject);

//////////////////////////////////////////////////////////////////////////
CSplineObject::CSplineObject() :
	m_selectedPoint(-1),
	m_mergeIndex(-1),
	m_isEditMode(false)
{
	m_bbox.min = m_bbox.max = Vec3(0, 0, 0);
}

//////////////////////////////////////////////////////////////////////////
void CSplineObject::SelectPoint(int index)
{
	if (m_selectedPoint == index)
		return;

	m_selectedPoint = index;
	OnUpdateUI();
}

//////////////////////////////////////////////////////////////////////////
void CSplineObject::SetPoint(int index, const Vec3& pos)
{
	if (0 <= index && index < GetPointCount())
	{
		m_points[index].pos = pos;
		BezierCorrection(index);
		CalcBBox();
		OnUpdate();
		OnUpdateUI();
	}
}

//////////////////////////////////////////////////////////////////////////
int CSplineObject::InsertPoint(int index, const Vec3& point)
{
	int newindex = GetPointCount();
	if (newindex >= GetMaxPoints())
		return newindex - 1;

	// Don't allow to create 2 points on the same position
	if (newindex >= 2)
	{
		if (m_points[newindex - 2].pos == m_points[newindex - 1].pos)
			return newindex - 1;
	}

	StoreUndo("Insert Point");

	if (index < 0 || index >= GetPointCount())
	{
		CSplinePoint pt;
		pt.pos = point;
		pt.back = point;
		pt.forw = point;
		m_points.push_back(pt);
		newindex = GetPointCount() - 1;
	}
	else
	{
		CSplinePoint pt;
		pt.pos = point;
		pt.back = point;
		pt.forw = point;
		m_points.insert(m_points.begin() + index, pt);
		newindex = index;
	}
	SetPoint(newindex, point);
	OnUpdateUI();
	return newindex;
}

//////////////////////////////////////////////////////////////////////////
void CSplineObject::RemovePoint(int index)
{
	if ((index >= 0 || index < GetPointCount()) && GetPointCount() > GetMinPoints())
	{
		StoreUndo("Remove Point");
		m_points.erase(m_points.begin() + index);
		CalcBBox();
		OnUpdate();
		OnUpdateUI();
	}
}

//////////////////////////////////////////////////////////////////////////
Vec3 CSplineObject::GetBezierPos(int index, float t) const
{
	float invt = 1.0f - t;
	return m_points[index].pos * (invt * invt * invt) +
	       m_points[index].forw * (3 * t * invt * invt) +
	       m_points[index + 1].back * (3 * t * t * invt) +
	       m_points[index + 1].pos * (t * t * t);
}

//////////////////////////////////////////////////////////////////////////
Vec3 CSplineObject::GetBezierTangent(int index, float t) const
{
	float invt = 1.0f - t;
	Vec3 tan = -m_points[index].pos * (invt * invt)
	           + m_points[index].forw * (invt * (invt - 2 * t))
	           + m_points[index + 1].back * (t * (2 * invt - t))
	           + m_points[index + 1].pos * (t * t);

	if (!tan.IsZero())
		tan.Normalize();

	return tan;
}

//////////////////////////////////////////////////////////////////////////
float CSplineObject::GetBezierSegmentLength(int index, float t) const
{
	const int kSteps = 32;
	float kn = t * kSteps + 1.0f;
	float fRet = 0.0f;

	Vec3 pos = GetBezierPos(index, 0.0f);
	for (float k = 1.0f; k <= kn; k += 1.0f)
	{
		Vec3 nextPos = GetBezierPos(index, t * k / kn);
		fRet += (nextPos - pos).GetLength();
		pos = nextPos;
	}
	return fRet;
}

//////////////////////////////////////////////////////////////////////////
float CSplineObject::GetSplineLength() const
{
	float fRet = 0.f;
	for (int i = 0, numSegments = GetPointCount() - 1; i < numSegments; ++i)
	{
		fRet += GetBezierSegmentLength(i);
	}
	return fRet;
}

//////////////////////////////////////////////////////////////////////////
float CSplineObject::GetPosByDistance(float distance, int& outIndex) const
{
	float lenPos = 0.0f;
	int index = 0;
	float segmentLength = 0.0f;
	for (int numSegments = GetPointCount() - 1; index < numSegments; ++index)
	{
		segmentLength = GetBezierSegmentLength(index);
		if (lenPos + segmentLength > distance)
			break;
		lenPos += segmentLength;
	}

	outIndex = index;
	return segmentLength > 0.0f ? (distance - lenPos) / segmentLength : 0.0f;
}

//////////////////////////////////////////////////////////////////////////
Vec3 CSplineObject::GetBezierNormal(int index, float t) const
{
	float kof = 0.0f;
	int i = index;

	if (index >= GetPointCount() - 1)
	{
		kof = 1.0f;
		i = GetPointCount() - 2;
		if (i < 0)
			return Vec3(0, 0, 0);
	}

	const Matrix34& wtm = GetWorldTM();
	Vec3 p0 = wtm.TransformPoint(GetBezierPos(i, t + 0.0001f + kof));
	Vec3 p1 = wtm.TransformPoint(GetBezierPos(i, t - 0.0001f + kof));

	Vec3 e = p0 - p1;
	Vec3 n = e.Cross(Vec3(0, 0, 1));

	if (n.x > 0.00001f || n.x < -0.00001f || n.y > 0.00001f || n.y < -0.00001f || n.z > 0.00001f || n.z < -0.00001f)
		n.Normalize();
	return n;
}

//////////////////////////////////////////////////////////////////////////
Vec3 CSplineObject::GetLocalBezierNormal(int index, float t) const
{
	float kof = t;
	int i = index;

	if (index >= GetPointCount() - 1)
	{
		kof = 1.0f + t;
		i = GetPointCount() - 2;
		if (i < 0)
			return Vec3(0, 0, 0);
	}

	Vec3 e = GetBezierTangent(i, kof);
	if (e.x < 0.00001f && e.x > -0.00001f && e.y < 0.00001f && e.y > -0.00001f && e.z < 0.00001f && e.z > -0.00001f)
		return Vec3(0, 0, 0);

	Vec3 n;

	float an1 = m_points[i].angle;
	float an2 = m_points[i + 1].angle;

	if (-0.00001f > an1 || an1 > 0.00001f || -0.00001f > an2 || an2 > 0.00001f)
	{
		float af = kof * 2 - 1.0f;
		float ed = 1.0f;
		if (af < 0.0f)
			ed = -1.0f;
		af = ed - af;
		af = af * af * af;
		af = ed - af;
		af = (af + 1.0f) / 2;
		float angle = ((1.0f - af) * an1 + af * an2) * 3.141593f / 180.0f;

		e.Normalize();
		n = Vec3(0, 0, 1).Cross(e);
		n = n.GetRotated(e, angle);
	}
	else
		n = Vec3(0, 0, 1).Cross(e);

	if (n.x > 0.00001f || n.x < -0.00001f || n.y > 0.00001f || n.y < -0.00001f || n.z > 0.00001f || n.z < -0.00001f)
		n.Normalize();
	return n;
}

//////////////////////////////////////////////////////////////////////////
void CSplineObject::BezierCorrection(int index)
{
	BezierAnglesCorrection(index - 1);
	BezierAnglesCorrection(index);
	BezierAnglesCorrection(index + 1);
	BezierAnglesCorrection(index - 2);
	BezierAnglesCorrection(index + 2);
}

//////////////////////////////////////////////////////////////////////////
void CSplineObject::BezierAnglesCorrection(int index)
{
	int maxindex = GetPointCount() - 1;
	if (index < 0 || index > maxindex)
	{
		return;
	}

	Vec3& p2 = m_points[index].pos;
	Vec3& back = m_points[index].back;
	Vec3& forw = m_points[index].forw;

	if (index == 0)
	{
		back = p2;
		if (maxindex == 1)
		{
			Vec3& p3 = m_points[index + 1].pos;
			forw = p2 + (p3 - p2) / 3;
		}
		else if (maxindex > 0)
		{
			Vec3& p3 = m_points[index + 1].pos;
			Vec3& pb3 = m_points[index + 1].back;

			float lenOsn = (pb3 - p2).GetLength();
			float lenb = (p3 - p2).GetLength();

			forw = p2 + (pb3 - p2) / (lenOsn / lenb * 3);
		}
	}

	if (index == maxindex)
	{
		forw = p2;
		if (index > 0)
		{
			Vec3& p1 = m_points[index - 1].pos;
			Vec3& pf1 = m_points[index - 1].forw;

			float lenOsn = (pf1 - p2).GetLength();
			float lenf = (p1 - p2).GetLength();

			if (lenOsn > 0.000001f && lenf > 0.000001f)
				back = p2 + (pf1 - p2) / (lenOsn / lenf * 3);
			else
				back = p2;
		}
	}

	if (1 <= index && index <= maxindex - 1)
	{
		Vec3& p1 = m_points[index - 1].pos;
		Vec3& p3 = m_points[index + 1].pos;

		float lenOsn = (p3 - p1).GetLength();
		float lenb = (p1 - p2).GetLength();
		float lenf = (p3 - p2).GetLength();

		back = p2 + (p1 - p3) * (lenb / lenOsn / 3);
		forw = p2 + (p3 - p1) * (lenf / lenOsn / 3);
	}
}

//////////////////////////////////////////////////////////////////////////
float CSplineObject::GetPointAngle() const
{
	int index = m_selectedPoint;
	if (0 <= index && index < GetPointCount())
	{
		return m_points[index].angle;
	}
	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////
void CSplineObject::SetPointAngle(float angle)
{
	int index = m_selectedPoint;
	if (0 <= index && index < GetPointCount())
	{
		m_points[index].angle = angle;
	}
	OnUpdate();
}

//////////////////////////////////////////////////////////////////////////
float CSplineObject::GetPointWidth() const
{
	int index = m_selectedPoint;
	if (0 <= index && index < GetPointCount())
	{
		return m_points[index].width;
	}
	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////
void CSplineObject::SetPointWidth(float width)
{
	int index = m_selectedPoint;
	if (0 <= index && index < GetPointCount())
	{
		m_points[index].width = width;
	}
	OnUpdate();
}

//////////////////////////////////////////////////////////////////////////
bool CSplineObject::IsPointDefaultWidth() const
{
	int index = m_selectedPoint;
	if (0 <= index && index < GetPointCount())
	{
		return m_points[index].isDefaultWidth;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CSplineObject::PointDafaultWidthIs(bool isDefault)
{
	int index = m_selectedPoint;
	if (0 <= index && index < GetPointCount())
	{
		m_points[index].isDefaultWidth = isDefault;
		m_points[index].width = GetWidth();
	}

	OnUpdateUI();
	OnUpdate();
}

//////////////////////////////////////////////////////////////////////////
void CSplineObject::ReverseShape()
{
	StoreUndo("Reverse Shape");
	std::reverse(m_points.begin(), m_points.end());
	OnUpdate();
}

//////////////////////////////////////////////////////////////////////////
void CSplineObject::Split(int index, const Vec3& point)
{
	// do not change the order of things here
	int in0 = InsertPoint(index, point);

	CSplineObject* pNewShape = (CSplineObject*) GetObjectManager()->CloneObject(this);

	for (int i = in0 + 1, cnt = GetPointCount(); i < cnt; ++i)
	{
		RemovePoint(in0 + 1);
	}
	if (pNewShape)
	{
		for (int i = 0; i < in0; ++i)
			pNewShape->RemovePoint(0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSplineObject::Merge(CSplineObject* pSpline)
{
	if (!pSpline || m_mergeIndex < 0 || pSpline->m_mergeIndex < 0)
		return;

	const Matrix34& tm = GetWorldTM();
	const Matrix34& splineTM = pSpline->GetWorldTM();

	if (!m_mergeIndex)
		ReverseShape();

	if (pSpline->m_mergeIndex)
		pSpline->ReverseShape();

	for (int i = 0; i < pSpline->GetPointCount(); ++i)
	{
		Vec3 pnt = splineTM.TransformPoint(pSpline->GetPoint(i));
		pnt = tm.GetInverted().TransformPoint(pnt);
		InsertPoint(GetPointCount(), pnt);
	}

	m_mergeIndex = -1;
	GetObjectManager()->DeleteObject(pSpline);
}

//////////////////////////////////////////////////////////////////////////
bool CSplineObject::Init(CBaseObject* prev, const string& file)
{
	bool res = __super::Init(prev, file);

	if (prev)
	{
		m_points = ((CSplineObject*)prev)->m_points;
		CalcBBox();
		OnUpdate();
	}

	return res;
}

//////////////////////////////////////////////////////////////////////////
void CSplineObject::Done()
{
	LOADING_TIME_PROFILE_SECTION_ARGS(GetName().c_str());
	m_points.clear();
	__super::Done();
}

void CSplineObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CBaseObject::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CSplineObject>("Spline", &CSplineObject::SerializeProperties);
}

void CSplineObject::SerializeProperties(Serialization::IArchive& ar, bool bMultiEdit)
{
	ar(*GetVarObject(), "Variables", "Variables");

	if (ar.openBlock("spline_operators", "<Spline Operators"))
	{
		size_t num_points = m_points.size();
		ar(num_points, "num_points", "!Number Of Points:");

		if (num_points > 1)
		{
			Serialization::SEditToolButton editButton("");
			editButton.SetToolClass(RUNTIME_CLASS(CEditSplineObjectTool), "object", this);
			ar(editButton, "editspline", "^Edit");
		}

		if (num_points >= 2)
		{
			Serialization::SEditToolButton splitButton("");
			splitButton.SetToolClass(RUNTIME_CLASS(CSplitSplineObjectTool), "object", this);
			ar(splitButton, "splitspline", "^Split");

			Serialization::SEditToolButton mergeButton("");
			mergeButton.SetToolClass(RUNTIME_CLASS(CMergeSplineObjectsTool), "object", this);
			ar(mergeButton, "mergespline", "^Merge");
		}
		ar.closeBlock();
	}

	if (m_selectedPoint < 0)
	{
		if (ar.openBlock("sel_point", "No Selection"))
		{
			ar.closeBlock();
		}
	}
	else
	{
		if (ar.openBlock("sel_point", "Selected Point"))
		{
			int selpoint = m_selectedPoint + 1;
			ar(selpoint, "selpointnum", "!Selected Point:");

			Vec3 pos = GetPoint(m_selectedPoint);
			Vec3 oldPos = pos;

			ar(pos, "position", "Position");

			if (ar.isInput() && pos != oldPos)
			{
				SetPoint(m_selectedPoint, pos);
			}

			float pangle = GetPointAngle();
			float oldpangle = pangle;
			ar(pangle, "pangle", "Angle");

			if (ar.isInput() && oldpangle != pangle)
			{
				SetPointAngle(pangle);
			}

			bool isDefault = IsPointDefaultWidth();
			bool oldIsDefault = isDefault;

			ar(isDefault, "pdefault", "Default Width");
			if (ar.isInput() && isDefault != oldIsDefault)
			{
				PointDafaultWidthIs(isDefault);
			}

			if (!isDefault)
			{
				int pwidth = GetPointWidth();
				int oldWidth = pwidth;

				ar(pwidth, "pwidth", "Width:");

				if (ar.isInput() && pwidth != oldWidth)
				{
					SetPointWidth(pwidth);
				}
			}
			ar.closeBlock();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSplineObject::GetBoundBox(AABB& box)
{
	box.SetTransformedAABB(GetWorldTM(), m_bbox);
	float s = 1.0f;
	box.min -= Vec3(s, s, s);
	box.max += Vec3(s, s, s);
}

//////////////////////////////////////////////////////////////////////////
void CSplineObject::GetLocalBounds(AABB& box)
{
	box = m_bbox;
}

//////////////////////////////////////////////////////////////////////////
void CSplineObject::CalcBBox()
{
	if (m_points.empty())
		return;

	// Reposition spline, so that .
	Vec3 center = m_points[0].pos;
	if (center.x != 0 || center.y != 0 || center.z != 0)
	{
		for (int i = 0, n = GetPointCount(); i < n; ++i)
		{
			m_points[i].pos -= center;
		}

		SetPos(GetPos() + GetLocalTM().TransformVector(center));
	}

	for (int i = 0, n = GetPointCount(); i < n; ++i)
		BezierCorrection(i);

	// First point must always be 0,0,0.
	m_bbox.Reset();
	for (int i = 0, n = GetPointCount(); i < n; ++i)
	{
		m_bbox.Add(m_points[i].pos);
	}

	if (m_bbox.min.x > m_bbox.max.x)
	{
		m_bbox.min = m_bbox.max = Vec3(0, 0, 0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSplineObject::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	bool isSelected = IsSelected();

	float fPointSize = isSelected ? 0.005f : 0.0025f;

	COLORREF colLine = GetColor();
	COLORREF colPoint = GetColor();

	const Matrix34& wtm = GetWorldTM();

	if (IsFrozen())
	{
		colLine = dc.GetFreezeColor();
		colPoint = dc.GetFreezeColor();
	}
	else if (isSelected || IsHighlighted())
	{
		colLine = dc.GetSelectedColor();
		colPoint = m_isEditMode ? RGB(0, 255, 0) : dc.GetSelectedColor();
	}

	dc.SetLineWidth(1);

	for (int i = 0, n = GetPointCount(); i < n; ++i)
	{
		// Draw spline segments
		dc.SetColor(colLine);
		if (i < (n - 1))
		{
			int kn = 8;
			for (int k = 0; k < kn; ++k)
			{
				Vec3 p = wtm.TransformPoint(GetBezierPos(i, float(k) / kn));
				Vec3 p1 = wtm.TransformPoint(GetBezierPos(i, float(k) / kn + 1.0f / kn));
				dc.DrawLine(p, p1);
			}
		}

		// Draw spline control points
		if (m_isEditMode && m_selectedPoint == i)
			dc.SetColor(dc.GetSelectedColor());
		else
			dc.SetColor(colPoint);

		Vec3 p0 = wtm.TransformPoint(m_points[i].pos);
		float fScale = fPointSize * dc.view->GetScreenScaleFactor(p0);
		Vec3 sz(fScale, fScale, fScale);
		if (isSelected)
		{
			if (m_isEditMode && m_selectedPoint != i)
				dc.DepthTestOff();
			dc.DrawSolidBox(p0 - sz, p0 + sz);
			if (m_isEditMode && m_selectedPoint != i)
				dc.DepthTestOn();
		}
		else
		{
			dc.DrawWireBox(p0 - sz, p0 + sz);
		}
	}

	// Keep for debug
	//DrawJoints(dc);

	if (m_mergeIndex != -1 && m_mergeIndex < GetPointCount())
	{
		dc.SetColor(RGB(127, 255, 127));
		Vec3 p0 = wtm.TransformPoint(m_points[m_mergeIndex].pos);
		float fScale = fPointSize * dc.view->GetScreenScaleFactor(p0);
		Vec3 sz(fScale, fScale, fScale);
		if (isSelected)
			dc.DepthTestOff();
		dc.DrawWireBox(p0 - sz, p0 + sz);
		if (isSelected)
			dc.DepthTestOn();
	}

	DrawDefault(dc, GetColor());
}

//////////////////////////////////////////////////////////////////////////
void CSplineObject::DrawJoints(DisplayContext& dc)
{
	const Matrix34& wtm = GetWorldTM();
	float fPointSize = 0.5f;

	for (int i = 0, n = GetPointCount(); i < n; ++i)
	{
		Vec3 p0 = wtm.TransformPoint(m_points[i].pos);

		Vec3 pf0 = wtm.TransformPoint(m_points[i].forw);
		float fScalef = fPointSize * dc.view->GetScreenScaleFactor(pf0) * 0.01f;
		Vec3 szf(fScalef, fScalef, fScalef);
		dc.SetColor(RGB(0, 200, 0));
		dc.DrawLine(p0, pf0);
		dc.SetColor(RGB(0, 255, 0));
		if (IsSelected())
			dc.DepthTestOff();
		dc.DrawWireBox(pf0 - szf, pf0 + szf);
		if (IsSelected())
			dc.DepthTestOn();

		Vec3 pb0 = wtm.TransformPoint(m_points[i].back);
		float fScaleb = fPointSize * dc.view->GetScreenScaleFactor(pb0) * 0.01f;
		Vec3 szb(fScaleb, fScaleb, fScaleb);
		dc.SetColor(RGB(0, 200, 0));
		dc.DrawLine(p0, pb0);
		dc.SetColor(RGB(0, 255, 0));
		if (IsSelected())
			dc.DepthTestOff();
		dc.DrawWireBox(pb0 - szb, pb0 + szb);
		if (IsSelected())
			dc.DepthTestOn();
	}
}

//////////////////////////////////////////////////////////////////////////
int CSplineObject::GetNearestPoint(const Vec3& raySrc, const Vec3& rayDir, float& distance)
{
	int index = -1;
	const float kBigRayLen = 100000.0f;
	float minDist = FLT_MAX;
	Vec3 rayLineP1 = raySrc;
	Vec3 rayLineP2 = raySrc + rayDir * kBigRayLen;
	Vec3 intPnt;
	const Matrix34& wtm = GetWorldTM();

	for (int i = 0, n = GetPointCount(); i < n; ++i)
	{
		float d = PointToLineDistance(rayLineP1, rayLineP2, wtm.TransformPoint(m_points[i].pos), intPnt);
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
bool CSplineObject::HitTest(HitContext& hc)
{
	// First check if ray intersect our bounding box.
	float tr = hc.distanceTolerance / 2 + kSplinePointSelectionRadius;

	// Find intersection of line with zero Z plane.
	float minDist = FLT_MAX;
	Vec3 intPnt;
	//GetNearestEdge( hc.raySrc,hc.rayDir,p1,p2,minDist,intPnt );

	const float kBigRayLen = 100000.0f;
	bool bWasIntersection = false;
	Vec3 ip;
	Vec3 rayLineP1 = hc.raySrc;
	Vec3 rayLineP2 = hc.raySrc + hc.rayDir * kBigRayLen;
	const Matrix34& wtm = GetWorldTM();

	int maxpoint = GetPointCount();
	if (maxpoint-- >= GetMinPoints())
	{
		for (int i = 0; i < maxpoint; ++i)
		{
			int kn = 6;
			for (int k = 0; k < kn; ++k)
			{
				Vec3 pi = wtm.TransformPoint(GetBezierPos(i, float(k) / kn));
				Vec3 pj = wtm.TransformPoint(GetBezierPos(i, float(k) / kn + 1.0f / kn));

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
			}
		}
	}

	if (bWasIntersection)
	{
		float fSplineCloseDistance = kSplinePointSelectionRadius * hc.view->GetScreenScaleFactor(intPnt) * 0.01f;

		if (minDist < fSplineCloseDistance + hc.distanceTolerance)
		{
			hc.dist = hc.raySrc.GetDistance(intPnt);
			hc.object = this;
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CSplineObject::HitTestRect(HitContext& hc)
{
	AABB box;
	// Retrieve world space bound box.
	GetBoundBox(box);

	// Project all edges to viewport.
	const Matrix34& wtm = GetWorldTM();

	int maxpoint = GetPointCount();
	if (maxpoint-- >= GetMinPoints())
	{
		for (int i = 0, j = 1; i < maxpoint; ++i, ++j)
		{
			Vec3 p0 = wtm.TransformPoint(m_points[i].pos);
			Vec3 p1 = wtm.TransformPoint(m_points[j].pos);

			CPoint pnt0 = hc.view->WorldToView(p0);
			CPoint pnt1 = hc.view->WorldToView(p1);

			// See if pnt0 to pnt1 line intersects with rectangle.
			// check see if one point is inside rect and other outside, or both inside.
			bool in0 = hc.rect.PtInRect(pnt0);
			bool in1 = hc.rect.PtInRect(pnt1);
			if ((in0 && in1) || (in0 && !in1) || (!in0 && in1))
			{
				hc.object = this;
				return true;
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CSplineObject::RayToLineDistance(const Vec3& rayLineP1, const Vec3& rayLineP2, const Vec3& pi, const Vec3& pj, float& distance, Vec3& intPnt)
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
void CSplineObject::GetNearestEdge(const Vec3& raySrc, const Vec3& rayDir, int& p1, int& p2, float& distance, Vec3& intersectPoint)
{
	p1 = -1;
	p2 = -1;
	const float kBigRayLen = 100000.0f;
	float minDist = FLT_MAX;
	Vec3 intPnt;
	Vec3 rayLineP1 = raySrc;
	Vec3 rayLineP2 = raySrc + rayDir * kBigRayLen;

	const Matrix34& wtm = GetWorldTM();

	int maxpoint = GetPointCount();
	if (maxpoint-- >= GetMinPoints())
	{
		for (int i = 0, j = 1; i < maxpoint; ++i, ++j)
		{
			int kn = 6;
			for (int k = 0; k < kn; ++k)
			{
				Vec3 pi = wtm.TransformPoint(GetBezierPos(i, float(k) / kn));
				Vec3 pj = wtm.TransformPoint(GetBezierPos(i, float(k) / kn + 1.0f / kn));

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
		}
	}

	distance = minDist;
}

void CSplineObject::OnContextMenu(CPopupMenuItem* menu)
{
	menu->Add("Edit", functor(*this, &CSplineObject::EditSpline));
	CBaseObject::OnContextMenu(menu);
}

//////////////////////////////////////////////////////////////////////////
void CSplineObject::OnUpdateUI()
{
	UpdateUIVars();
}

//////////////////////////////////////////////////////////////////////////
void CSplineObject::Serialize(CObjectArchive& ar)
{
	__super::Serialize(ar);

	if (ar.bLoading)
	{
		m_points.clear();
		XmlNodeRef points = ar.node->findChild("Points");
		if (points)
		{
			for (int i = 0, n = points->getChildCount(); i < n; ++i)
			{
				XmlNodeRef pnt = points->getChild(i);
				CSplinePoint pt;
				pnt->getAttr("Pos", pt.pos);
				pnt->getAttr("Back", pt.back);
				pnt->getAttr("Forw", pt.forw);
				pnt->getAttr("Angle", pt.angle);
				pnt->getAttr("Width", pt.width);
				pnt->getAttr("IsDefaultWidth", pt.isDefaultWidth);
				m_points.push_back(pt);
			}
		}

		CalcBBox();

		OnUpdate();
		OnUpdateUI();
	}
	else // Saving.
	{
		if (!m_points.empty())
		{
			XmlNodeRef points = ar.node->newChild("Points");
			for (int i = 0, n = GetPointCount(); i < n; ++i)
			{
				XmlNodeRef pnt = points->newChild("Point");
				pnt->setAttr("Pos", m_points[i].pos);
				pnt->setAttr("Back", m_points[i].back);
				pnt->setAttr("Forw", m_points[i].forw);
				pnt->setAttr("Angle", m_points[i].angle);
				pnt->setAttr("Width", m_points[i].width);
				pnt->setAttr("IsDefaultWidth", m_points[i].isDefaultWidth);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
int CSplineObject::MouseCreateCallback(IDisplayViewport* pView, EMouseEvent event, CPoint& point, int flags)
{
	if (event == eMouseMove || event == eMouseLDown || event == eMouseLDblClick)
	{
		Vec3 pos = ((CViewport*)pView)->MapViewToCP(point, 0, true);

		if (GetPointCount() < GetMinPoints())
		{
			SetPos(pos);
		}

		pos.z += CreationZOffset();

		if (GetPointCount() == 0)
		{
			InsertPoint(-1, Vec3(0, 0, 0));
		}
		else
		{
			SetPoint(GetPointCount() - 1, pos - GetWorldPos());
		}

		if (event == eMouseLDblClick)
		{
			if (GetPointCount() > GetMinPoints())
			{
				// Remove last unneeded point.
				m_points.pop_back();
				OnUpdate();
				OnUpdateUI();
				return MOUSECREATE_OK;
			}
			else
				return MOUSECREATE_ABORT;
		}

		if (event == eMouseLDown)
		{
			Vec3 vlen = Vec3(pos.x, pos.y, 0) - Vec3(GetWorldPos().x, GetWorldPos().y, 0);
			if (GetPointCount() > GetMinPoints() && vlen.GetLength() < kSplinePointSelectionRadius)
			{
				OnUpdateUI();
				return MOUSECREATE_OK;
			}
			if (GetPointCount() >= GetMaxPoints())
			{
				OnUpdateUI();
				return MOUSECREATE_OK;
			}

			InsertPoint(-1, pos - GetWorldPos());
		}
		return MOUSECREATE_CONTINUE;
	}

	return __super::MouseCreateCallback(pView, event, point, flags);
}

void CSplineObject::EditSpline()
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

	if (!pEditTool->IsKindOf(RUNTIME_CLASS(CEditSplineObjectTool)))
	{
		pEditTool = new CEditSplineObjectTool;
		GetIEditorImpl()->SetEditTool(pEditTool);
		pEditTool->SetUserData("object", this);
	}
}

