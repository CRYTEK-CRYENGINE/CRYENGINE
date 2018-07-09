// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GravityVolumeObject.h"
#include "Viewport.h"
#include "Util\Triangulate.h"
#include "Material\Material.h"
#include "EditTool.h"
#include "Objects/DisplayContext.h"
#include "Objects/ObjectLoader.h"
#include "Objects/InspectorWidgetCreator.h"
#include "Util/MFCUtil.h"
#include <Cry3DEngine/I3DEngine.h>
#include "Serialization/Decorators/EditToolButton.h"
#include "Gizmos/ITransformManipulator.h"
#include "Gizmos/IGizmoManager.h"

class CEditGravityVolumeTool : public CEditTool, public ITransformManipulatorOwner
{
public:
	DECLARE_DYNCREATE(CEditGravityVolumeTool)

	CEditGravityVolumeTool();

	virtual string GetDisplayName() const override { return "Edit Gravity Volume"; }

	bool           MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags);

	virtual void   SetUserData(const char* key, void* userData);

	virtual void   Display(DisplayContext& dc)                                           {};
	virtual bool   OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags);
	virtual bool   OnKeyUp(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags) { return false; };

	void           OnManipulatorBeginDrag(IDisplayViewport*, ITransformManipulator*, const Vec2i&, int flags);
	void           OnManipulatorDrag(IDisplayViewport*, ITransformManipulator*, const Vec2i&, const Vec3&, int);
	void           OnManipulatorEndDrag(IDisplayViewport*, ITransformManipulator*);

	virtual bool   GetManipulatorMatrix(RefCoordSys coordSys, Matrix34& tm) override;
	virtual void   GetManipulatorPosition(Vec3& position) override;
	virtual bool   IsManipulatorVisible() override;

	virtual bool   IsNeedMoveTool() override { return true; }

protected:
	virtual ~CEditGravityVolumeTool();
	// Delete itself.
	void DeleteThis() { delete this; };

private:
	CGravityVolumeObject* m_GravityVolume;
	int                   m_currPoint;
	bool                  m_modifying;
	CPoint                m_mouseDownPos;
	Vec3                  m_pointPos;
	ITransformManipulator* m_pManipulator;
};

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CEditGravityVolumeTool, CEditTool)

//////////////////////////////////////////////////////////////////////////
CEditGravityVolumeTool::CEditGravityVolumeTool()
{
	m_GravityVolume = 0;
	m_currPoint = -1;
	m_modifying = false;

	m_pManipulator = GetIEditorImpl()->GetGizmoManager()->AddManipulator(this);

	m_pManipulator->signalBeginDrag.Connect(this, &CEditGravityVolumeTool::OnManipulatorBeginDrag);
	m_pManipulator->signalDragging.Connect(this, &CEditGravityVolumeTool::OnManipulatorDrag);
	m_pManipulator->signalEndDrag.Connect(this, &CEditGravityVolumeTool::OnManipulatorEndDrag);
}

//////////////////////////////////////////////////////////////////////////
void CEditGravityVolumeTool::SetUserData(const char* key, void* userData)
{
	m_GravityVolume = (CGravityVolumeObject*)userData;
	assert(m_GravityVolume != 0);

	// Modify GravityVolume undo.
	if (!CUndo::IsRecording())
	{
		CUndo("Modify GravityVolume");
		m_GravityVolume->StoreUndo("GravityVolume Modify");
	}

	m_GravityVolume->SelectPoint(-1);
}

//////////////////////////////////////////////////////////////////////////
CEditGravityVolumeTool::~CEditGravityVolumeTool()
{
	if (m_GravityVolume)
	{
		m_GravityVolume->SelectPoint(-1);
	}
	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
		GetIEditorImpl()->GetIUndoManager()->Cancel();

	m_pManipulator->signalBeginDrag.DisconnectAll();
	m_pManipulator->signalDragging.DisconnectAll();
	m_pManipulator->signalEndDrag.DisconnectAll();

	GetIEditorImpl()->GetGizmoManager()->RemoveManipulator(m_pManipulator);
}

bool CEditGravityVolumeTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (nChar == Qt::Key_Escape)
	{
		GetIEditorImpl()->SetEditTool(0);
	}
	else if (nChar == Qt::Key_Delete)
	{
		if (m_GravityVolume)
		{
			int sel = m_GravityVolume->GetSelectedPoint();
			if (!m_modifying && sel >= 0 && sel < m_GravityVolume->GetPointCount())
			{
				GetIEditorImpl()->GetIUndoManager()->Begin();
				if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
					m_GravityVolume->StoreUndo("Delete Point");

				m_GravityVolume->RemovePoint(sel);
				m_GravityVolume->SelectPoint(-1);
				m_GravityVolume->CalcBBox();
				m_pManipulator->Invalidate();

				if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
					GetIEditorImpl()->GetIUndoManager()->Accept("GravityVolume Modify");
			}
		}
	}
	return true;
}

void CEditGravityVolumeTool::OnManipulatorBeginDrag(IDisplayViewport* view, ITransformManipulator*, const Vec2i&, int flags)
{
	if (GetIEditorImpl()->GetEditMode() == eEditModeMove)
	{
		m_pointPos = m_GravityVolume->GetPoint(m_GravityVolume->GetSelectedPoint());

		Matrix34 tm;
		tm.SetIdentity();
		tm.SetTranslation(m_pointPos);
		view->SetConstructionMatrix(tm);

		m_modifying = true;
	}
}

void CEditGravityVolumeTool::OnManipulatorDrag(IDisplayViewport* view, ITransformManipulator*, const Vec2i&, const Vec3& offset, int)
{
	if (GetIEditorImpl()->GetEditMode() == eEditModeMove)
	{
		Matrix34 m = m_GravityVolume->GetWorldTM();
		m.Invert();
		Vec3 transformedOffset = m.TransformVector(offset);

		m_GravityVolume->SetPoint(m_GravityVolume->GetSelectedPoint(), m_pointPos + transformedOffset);
	}
}

void CEditGravityVolumeTool::OnManipulatorEndDrag(IDisplayViewport* view, ITransformManipulator*)
{
	if (GetIEditorImpl()->GetEditMode() == eEditModeMove)
	{
		m_modifying = false;
		m_GravityVolume->CalcBBox();
	}
}

bool CEditGravityVolumeTool::GetManipulatorMatrix(RefCoordSys coordSys, Matrix34& tm)
{
	if (!m_GravityVolume)
		return false;

	return m_GravityVolume->GetManipulatorMatrix(coordSys, tm);
}

void CEditGravityVolumeTool::GetManipulatorPosition(Vec3& position)
{
	if (m_GravityVolume->GetSelectedPoint() >= 0)
	{
		position = m_GravityVolume->GetWorldTM() * m_GravityVolume->GetPoint(m_GravityVolume->GetSelectedPoint());
	}
}

bool CEditGravityVolumeTool::IsManipulatorVisible()
{
	return m_GravityVolume->GetSelectedPoint() >= 0;
}

//////////////////////////////////////////////////////////////////////////
bool CEditGravityVolumeTool::MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	if (!m_GravityVolume)
		return false;

	if (event == eMouseLDown)
	{
		m_mouseDownPos = point;
	}

	if (event == eMouseLDown || event == eMouseMove || event == eMouseLDblClick || event == eMouseLUp)
	{
		const Matrix34& GravityVolumeTM = m_GravityVolume->GetWorldTM();

		float dist;

		Vec3 raySrc, rayDir;
		view->ViewToWorldRay(point, raySrc, rayDir);

		// Find closest point on the GravityVolume.
		int p1, p2;
		Vec3 intPnt;
		m_GravityVolume->GetNearestEdge(raySrc, rayDir, p1, p2, dist, intPnt);

		float fGravityVolumeCloseDistance = GravityVolume_CLOSE_DISTANCE * view->GetScreenScaleFactor(intPnt) * 0.01f;

		if ((flags & MK_CONTROL) && !m_modifying)
		{
			// If control we are editing edges..
			if (p1 >= 0 && p2 >= 0 && dist < fGravityVolumeCloseDistance + view->GetSelectionTolerance())
			{
				// Cursor near one of edited GravityVolume edges.
				view->ResetCursor();
				if (event == eMouseLDown)
				{
					m_modifying = true;
					GetIEditorImpl()->GetIUndoManager()->Begin();
					if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
						m_GravityVolume->StoreUndo("Make Point");

					// If last edge, insert at end.
					if (p2 == 0)
						p2 = -1;

					// Create new point between nearest edge.
					// Put intPnt into local space of GravityVolume.
					intPnt = GravityVolumeTM.GetInverted().TransformPoint(intPnt);

					int index = m_GravityVolume->InsertPoint(p2, intPnt);
					m_GravityVolume->SelectPoint(index);
					m_pManipulator->Invalidate();

					// Set construction plane for view.
					m_pointPos = GravityVolumeTM.TransformPoint(m_GravityVolume->GetPoint(index));
					Matrix34 tm;
					tm.SetIdentity();
					tm.SetTranslation(m_pointPos);
					view->SetConstructionMatrix(tm);
				}
			}
			return true;
		}

		int index = m_GravityVolume->GetNearestPoint(raySrc, rayDir, dist);
		if (index >= 0 && dist < fGravityVolumeCloseDistance + view->GetSelectionTolerance())
		{
			// Cursor near one of edited GravityVolume points.
			view->ResetCursor();
			if (event == eMouseLDown)
			{
				if (!m_modifying)
				{
					m_GravityVolume->SelectPoint(index);
					m_pManipulator->Invalidate();

					m_modifying = true;
					GetIEditorImpl()->GetIUndoManager()->Begin();

					// Set construction plance for view.
					m_pointPos = GravityVolumeTM.TransformPoint(m_GravityVolume->GetPoint(index));
					Matrix34 tm;
					tm.SetIdentity();
					tm.SetTranslation(m_pointPos);
					view->SetConstructionMatrix(tm);
				}
			}

			//GetNearestEdge

			if (event == eMouseLDblClick)
			{
				m_modifying = false;
				m_GravityVolume->RemovePoint(index);
				m_GravityVolume->SelectPoint(-1);
				m_pManipulator->Invalidate();
			}
		}
		else
		{
			if (event == eMouseLDown)
			{
				m_GravityVolume->SelectPoint(-1);
				m_pManipulator->Invalidate();
			}
		}

		if (m_modifying && event == eMouseLUp)
		{
			// Accept changes.
			m_modifying = false;
			//m_GravityVolume->SelectPoint( -1 );
			m_GravityVolume->CalcBBox();

			if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
				GetIEditorImpl()->GetIUndoManager()->Accept("GravityVolume Modify");
		}

		if (m_modifying && event == eMouseMove)
		{
			// Move selected point point.
			Vec3 p1 = view->MapViewToCP(m_mouseDownPos);
			Vec3 p2 = view->MapViewToCP(point);
			Vec3 v = view->GetCPVector(p1, p2);

			if (m_GravityVolume->GetSelectedPoint() >= 0)
			{
				Vec3 wp = m_pointPos;
				Vec3 newp = wp + v;
				if (GetIEditorImpl()->IsSnapToTerrainEnabled())
				{
					// Keep height.
					newp = view->MapViewToCP(point);
					//float z = wp.z - GetIEditorImpl()->GetTerrainElevation(wp.x,wp.y);
					//newp.z = GetIEditorImpl()->GetTerrainElevation(newp.x,newp.y) + z;
					//newp.z = GetIEditorImpl()->GetTerrainElevation(newp.x,newp.y) + GravityVolume_Z_OFFSET;
					newp.z += GravityVolume_Z_OFFSET;
				}

				if (newp.x != 0 && newp.y != 0 && newp.z != 0)
				{
					newp = view->SnapToGrid(newp);

					// Put newp into local space of GravityVolume.
					Matrix34 invGravityVolumeTM = GravityVolumeTM;
					invGravityVolumeTM.Invert();
					newp = invGravityVolumeTM.TransformPoint(newp);

					if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
						m_GravityVolume->StoreUndo("Move Point");
					m_GravityVolume->SetPoint(m_GravityVolume->GetSelectedPoint(), newp);
					m_pManipulator->Invalidate();
				}
			}
		}
		return true;
	}
	return false;
}

REGISTER_CLASS_DESC(CGravityVolumeObjectClassDesc);

//////////////////////////////////////////////////////////////////////////
// class CGravityVolume Sector

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CGravityVolumeObject, CEntityObject)

#define RAY_DISTANCE 100000.0f

//////////////////////////////////////////////////////////////////////////
CGravityVolumeObject::CGravityVolumeObject()
{
	m_bForce2D = false;
	mv_radius = 4.0f;
	mv_gravity = 10.0f;
	mv_falloff = 0.8f;
	mv_damping = 1.0f;
	mv_dontDisableInvisible = false;
	mv_step = 4.0f;

	m_bIgnoreParamUpdate = false;

	m_bbox.min = m_bbox.max = Vec3(0, 0, 0);
	m_selectedPoint = -1;
	m_entityClass = "AreaBezierVolume";

	SetColor(CMFCUtils::Vec2Rgb(Vec3(0, 0.8f, 1)));
}

//////////////////////////////////////////////////////////////////////////
void CGravityVolumeObject::Done()
{
	m_points.clear();
	m_bezierPoints.clear();
	__super::Done();
}

//////////////////////////////////////////////////////////////////////////
bool CGravityVolumeObject::Init(CBaseObject* prev, const string& file)
{
	bool res = CEntityObject::Init(prev, file);

	if (m_pEntity)
	{
		m_pEntity->CreateProxy(ENTITY_PROXY_AREA);
	}

	if (prev)
	{
		m_points = ((CGravityVolumeObject*)prev)->m_points;
		CalcBBox();
	}

	return res;
}

//////////////////////////////////////////////////////////////////////////
void CGravityVolumeObject::InitVariables()
{
	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(mv_radius, "Radius", functor(*this, &CGravityVolumeObject::OnParamChange));
	m_pVarObject->AddVariable(mv_gravity, "Gravity", functor(*this, &CGravityVolumeObject::OnParamChange));
	m_pVarObject->AddVariable(mv_falloff, "Falloff", functor(*this, &CGravityVolumeObject::OnParamChange));
	m_pVarObject->AddVariable(mv_damping, "Damping", functor(*this, &CGravityVolumeObject::OnParamChange));
	m_pVarObject->AddVariable(mv_step, "StepSize", functor(*this, &CGravityVolumeObject::OnParamChange));
	m_pVarObject->AddVariable(mv_dontDisableInvisible, "DontDisableInvisible", functor(*this, &CGravityVolumeObject::OnParamChange));
	mv_step.SetLimits(0.1f, 1000.f);
}

//////////////////////////////////////////////////////////////////////////
string CGravityVolumeObject::GetUniqueName() const
{
	char prefix[32];
	cry_sprintf(prefix, "%p ", this);
	return string(prefix) + GetName();
}

//////////////////////////////////////////////////////////////////////////
void CGravityVolumeObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CEntityObject::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CGravityVolumeObject>("Gravity Volume", [](CGravityVolumeObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_radius, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_gravity, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_falloff, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_damping, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_step, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_dontDisableInvisible, ar);

		if (ar.openBlock("Operators", "<Operators"))
		{
			size_t num_points = pObject->m_points.size();
			ar(num_points, "num_points", "!Number Of Points:");

			Serialization::SEditToolButton editShapeButton("");
			editShapeButton.SetToolClass(RUNTIME_CLASS(CEditGravityVolumeTool), "object", pObject);
			ar(editShapeButton, "edit_shape", "^Edit Shape");

			ar.closeBlock();
		}

		if (ar.isInput())
		{
			pObject->CalcBBox();
		}
	});
}

//////////////////////////////////////////////////////////////////////////
void CGravityVolumeObject::InvalidateTM(int nWhyFlags)
{
	__super::InvalidateTM(nWhyFlags);
	UpdateGameArea();
	//	CalcBBox();
}

//////////////////////////////////////////////////////////////////////////
void CGravityVolumeObject::GetBoundBox(AABB& box)
{
	box.SetTransformedAABB(GetWorldTM(), m_bbox);
	float s = 1.0f;
	box.min -= Vec3(s, s, s);
	box.max += Vec3(s, s, s);
}

//////////////////////////////////////////////////////////////////////////
void CGravityVolumeObject::GetLocalBounds(AABB& box)
{
	box = m_bbox;
}

//////////////////////////////////////////////////////////////////////////
bool CGravityVolumeObject::HitTest(HitContext& hc)
{
	// First check if ray intersect our bounding box.
	float tr = hc.distanceTolerance / 2 + GravityVolume_CLOSE_DISTANCE;

	// Find intersection of line with zero Z plane.
	float minDist = FLT_MAX;
	Vec3 intPnt(hc.raySrc);
	//GetNearestEdge( hc.raySrc,hc.rayDir,p1,p2,minDist,intPnt );

	bool bWasIntersection = false;
	Vec3 ip;
	Vec3 rayLineP1 = hc.raySrc;
	Vec3 rayLineP2 = hc.raySrc + hc.rayDir * RAY_DISTANCE;
	const Matrix34& wtm = GetWorldTM();

	for (int i = 0; i < m_points.size(); i++)
	{
		int kn = 6;
		for (int k = 0; k < kn; k++)
		{
			Vec3 pi = wtm.TransformPoint(GetSplinePos(m_points, i, float(k) / kn));
			Vec3 pj = wtm.TransformPoint(GetSplinePos(m_points, i, float(k) / kn + 1.0f / kn));

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

	float fGravityVolumeCloseDistance = GravityVolume_CLOSE_DISTANCE * hc.view->GetScreenScaleFactor(intPnt) * 0.01f;

	if (bWasIntersection && minDist < fGravityVolumeCloseDistance + hc.distanceTolerance)
	{
		hc.dist = hc.raySrc.GetDistance(intPnt);
		hc.object = this;
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
int CGravityVolumeObject::MouseCreateCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	if (event == eMouseMove || event == eMouseLDown || event == eMouseLDblClick)
	{
		Vec3 pos = ((CViewport*)view)->MapViewToCP(point, 0, true, GetCreationOffsetFromTerrain());

		if (m_points.size() < 2)
		{
			SetPos(pos);
		}

		pos.z += GravityVolume_Z_OFFSET;

		if (m_points.size() == 0)
		{
			InsertPoint(-1, Vec3(0, 0, 0));
		}
		else
		{
			SetPoint(m_points.size() - 1, pos - GetWorldPos());
		}

		if (event == eMouseLDblClick)
		{
			if (m_points.size() > GetMinPoints())
			{
				// Remove last unneeded point.
				m_points.pop_back();
				UpdateUIVars();
				return MOUSECREATE_OK;
			}
			else
				return MOUSECREATE_ABORT;
		}

		if (event == eMouseLDown)
		{
			Vec3 vlen = Vec3(pos.x, pos.y, 0) - Vec3(GetWorldPos().x, GetWorldPos().y, 0);
			if (m_points.size() > 2 && vlen.GetLength() < GravityVolume_CLOSE_DISTANCE)
			{
				UpdateUIVars();
				return MOUSECREATE_OK;
			}
			if (GetPointCount() >= GetMaxPoints())
			{
				UpdateUIVars();
				return MOUSECREATE_OK;
			}

			InsertPoint(-1, pos - GetWorldPos());
		}
		return MOUSECREATE_CONTINUE;
	}
	return __super::MouseCreateCallback(view, event, point, flags);
}

//////////////////////////////////////////////////////////////////////////
Vec3 CGravityVolumeObject::GetBezierPos(CGravityVolumePointVector& points, int index, float t)
{
	return points[index].pos * ((1 - t) * (1 - t) * (1 - t)) +
	       points[index].forw * (3 * t * (1 - t) * (1 - t)) +
	       points[index + 1].back * (3 * t * t * (1 - t)) +
	       points[index + 1].pos * (t * t * t);
}

//////////////////////////////////////////////////////////////////////////
Vec3 CGravityVolumeObject::GetSplinePos(CGravityVolumePointVector& points, int index, float t)
{
	int indprev = index - 1;
	int indnext = index + 1;
	if (indprev < 0)
		indprev = 0;
	if (indnext >= points.size())
		indnext = points.size() - 1;
	Vec3 p0 = points[indprev].pos;
	Vec3 p1 = points[index].pos;
	Vec3 p2 = points[indnext].pos;
	return ((p0 + p2) / 2 - p1) * t * t + (p1 - p0) * t + (p0 + p1) / 2;
}

//////////////////////////////////////////////////////////////////////////
Vec3 CGravityVolumeObject::GetBezierNormal(int index, float t)
{
	float kof = 0.0f;
	int i = index;
	if (index >= m_points.size() - 1)
	{
		kof = 1.0f;
		i = m_points.size() - 2;
		if (i < 0)
			return Vec3(0, 0, 0);
	}
	const Matrix34& wtm = GetWorldTM();
	Vec3 p0 = wtm.TransformPoint(GetSplinePos(m_points, i, t + 0.0001f + kof));
	Vec3 p1 = wtm.TransformPoint(GetSplinePos(m_points, i, t - 0.0001f + kof));

	Vec3 e = p0 - p1;
	Vec3 n = e.Cross(Vec3(0, 0, 1));

	//if(n.GetLength()>0.00001f)
	if (n.x > 0.00001f || n.x < -0.00001f || n.y > 0.00001f || n.y < -0.00001f || n.z > 0.00001f || n.z < -0.00001f)
		n.Normalize();
	return n;
}

//////////////////////////////////////////////////////////////////////////
float CGravityVolumeObject::GetBezierSegmentLength(int index, float t)
{
	const Matrix34& wtm = GetWorldTM();
	int kn = int(t * 20) + 1;
	float fRet = 0.0f;
	for (int k = 0; k < kn; k++)
	{
		Vec3 e = GetSplinePos(m_points, index, t * float(k) / kn) -
		         GetSplinePos(m_points, index, t * (float(k) / kn + 1.0f / kn));
		fRet += e.GetLength();
	}
	return fRet;
}

//////////////////////////////////////////////////////////////////////////
void CGravityVolumeObject::DrawBezierSpline(DisplayContext& dc, CGravityVolumePointVector& points, COLORREF col, bool isDrawJoints, bool isDrawGravityVolume)
{
	const Matrix34& wtm = GetWorldTM();
	float fPointSize = 0.5f;

	dc.SetColor(col);
	for (int i = 0; i < points.size(); i++)
	{
		Vec3 p0 = wtm.TransformPoint(points[i].pos);

		// Draw Bezier joints
		//if (isDrawJoints)
		if (0)
		{
			ColorB colf = dc.GetColor();

			Vec3 pf0 = wtm.TransformPoint(points[i].forw);
			float fScalef = fPointSize * dc.view->GetScreenScaleFactor(pf0) * 0.01f;
			Vec3 szf(fScalef, fScalef, fScalef);
			dc.SetColor(RGB(0, 200, 0));
			dc.DrawLine(p0, pf0);
			dc.SetColor(RGB(0, 255, 0));
			dc.DrawWireBox(pf0 - szf, pf0 + szf);

			Vec3 pb0 = wtm.TransformPoint(points[i].back);
			float fScaleb = fPointSize * dc.view->GetScreenScaleFactor(pb0) * 0.01f;
			Vec3 szb(fScaleb, fScaleb, fScaleb);
			dc.SetColor(RGB(0, 200, 0));
			dc.DrawLine(p0, pb0);
			dc.SetColor(RGB(0, 255, 0));
			dc.DrawWireBox(pb0 - szb, pb0 + szb);

			dc.SetColor(colf);
		}

		if (isDrawGravityVolume && i < points.size() - 1)
		{
			Vec3 p1 = wtm.TransformPoint(points[i + 1].pos);
			ColorB colf = dc.GetColor();

			dc.SetColor(RGB(0, 200, 0));
			dc.DrawLine(p0, p1);
			dc.SetColor(colf);
		}

		//if(i < points.size()-1)
		if (i < points.size())
		{
			int kn = int((GetBezierSegmentLength(i) + 0.5f) / mv_step);
			if (kn == 0)
				kn = 1;
			for (int k = 0; k < kn; k++)
			{
				Vec3 p = wtm.TransformPoint(GetSplinePos(points, i, float(k) / kn));
				Vec3 p1 = wtm.TransformPoint(GetSplinePos(points, i, float(k) / kn + 1.0f / kn));
				dc.DrawLine(p, p1);

				if (isDrawGravityVolume)
				{
					ColorB colf = dc.GetColor();
					dc.SetColor(RGB(127, 127, 255));
					Vec3 a = GetSplinePos(points, i, float(k) / kn);
					Vec3 a1 = GetSplinePos(points, i, float(k) / kn + 1.0f / kn);
					Vec3 e1 = GetBezierNormal(i, float(k) / kn);
					Vec3 e2 = e1.Cross(a1 - a);
					if (e2.x > 0.00001f || e2.x < -0.00001f || e2.y > 0.00001f || e2.y < -0.00001f || e2.z > 0.00001f || e2.z < -0.00001f)
					{
						e2.Normalize();
						for (int i = 0; i < 16; i++)
						{
							float ang = float(i) / 16 * 3.141593f * 2;
							float ang2 = float(i + 1) / 16 * 3.141593f * 2;
							Vec3 f = e1 * cos(ang) + e2 * sin(ang);
							Vec3 f2 = e1 * cos(ang2) + e2 * sin(ang2);
							Vec3 b = wtm.TransformPoint(mv_radius * f + a);
							Vec3 b2 = wtm.TransformPoint(mv_radius * f2 + a);
							dc.DrawLine(b, b2);
						}
					}
					dc.SetColor(colf);
				}
			}
		}

		float fScale = fPointSize * dc.view->GetScreenScaleFactor(p0) * 0.01f;
		Vec3 sz(fScale, fScale, fScale);
		dc.DrawWireBox(p0 - sz, p0 + sz);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGravityVolumeObject::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	//dc.renderer->EnableDepthTest(false);

	const Matrix34& wtm = GetWorldTM();
	COLORREF col = 0;

	float fPointSize = 0.5f;

	bool bPointSelected = false;
	if (m_selectedPoint >= 0 && m_selectedPoint < m_points.size())
	{
		bPointSelected = true;
	}

	if (m_points.size() > 1)
	{
		if ((IsSelected() || IsHighlighted()))
		{
			col = dc.GetSelectedColor();
			dc.SetColor(col);
		}
		else
		{
			if (IsFrozen())
				dc.SetFreezeColor();
			else
				dc.SetColor(GetColor());
			col = GetColor();
		}

		DrawBezierSpline(dc, m_points, col, false, IsSelected());

		if (IsSelected())
		{
			AABB box;
			GetBoundBox(box);
			dc.DrawWireBox(box.min, box.max);
		}

		// Draw selected point.
		if (bPointSelected)
		{
			dc.SetColor(RGB(0, 0, 255));
			//dc.SetSelectedColor( 0.5f );

			Vec3 selPos = wtm.TransformPoint(m_points[m_selectedPoint].pos);
			//dc.renderer->DrawBall( selPos,1.0f );
			float fScale = fPointSize * dc.view->GetScreenScaleFactor(selPos) * 0.01f;
			Vec3 sz(fScale, fScale, fScale);
			dc.DrawWireBox(selPos - sz, selPos + sz);
		}
	}

	DrawDefault(dc, GetColor());
}

//////////////////////////////////////////////////////////////////////////
void CGravityVolumeObject::Serialize(CObjectArchive& ar)
{
	m_bIgnoreParamUpdate = true;
	__super::Serialize(ar);

	XmlNodeRef xmlNode = ar.node;
	if (ar.bLoading)
	{
		m_points.clear();
		XmlNodeRef points = xmlNode->findChild("Points");
		if (points)
		{
			for (int i = 0; i < points->getChildCount(); i++)
			{
				XmlNodeRef pnt = points->getChild(i);
				CGravityVolumePoint pt;
				pnt->getAttr("Pos", pt.pos);
				//pnt->getAttr( "Back",pt.back );
				//pnt->getAttr( "Forw",pt.forw );
				pnt->getAttr("Angle", pt.angle);
				pnt->getAttr("Width", pt.width);
				pnt->getAttr("IsDefaultWidth", pt.isDefaultWidth);
				m_points.push_back(pt);
			}
		}

		CalcBBox();

		UpdateUIVars();
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
				pnt->setAttr("Pos", m_points[i].pos);
				//pnt->setAttr( "Back",m_points[i].back );
				//pnt->setAttr( "Forw",m_points[i].forw );
				pnt->setAttr("Angle", m_points[i].angle);
				pnt->setAttr("Width", m_points[i].width);
				pnt->setAttr("IsDefaultWidth", m_points[i].isDefaultWidth);
			}
		}
	}
	m_bIgnoreParamUpdate = false;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CGravityVolumeObject::Export(const string& levelPath, XmlNodeRef& xmlNode)
{
	XmlNodeRef objNode = __super::Export(levelPath, xmlNode);
	return objNode;
}

//////////////////////////////////////////////////////////////////////////
void CGravityVolumeObject::CalcBBox()
{
	if (m_points.empty())
		return;

	// Reposition GravityVolume, so that GravityVolume object position is in the middle of the GravityVolume.
	Vec3 center = m_points[0].pos;
	if (center.x != 0 || center.y != 0 || center.z != 0)
	{
		// May not work correctly if GravityVolume is transformed.
		for (int i = 0; i < m_points.size(); i++)
		{
			m_points[i].pos -= center;
		}
		Matrix34 ltm = GetLocalTM();
		SetPos(GetPos() + ltm.TransformVector(center));
	}

	for (int i = 0; i < m_points.size(); i++)
		SplineCorrection(i);

	// First point must always be 0,0,0.
	m_bbox.Reset();
	for (int i = 0; i < m_points.size(); i++)
		m_bbox.Add(m_points[i].pos);
	if (m_bbox.min.x > m_bbox.max.x)
		m_bbox.min = m_bbox.max = Vec3(0, 0, 0);
	//BBox box;
	//box.SetTransformedAABB( GetWorldTM(),m_bbox );

	//m_bbox.max.z = max( m_bbox.max.z,(float)mv_height );
	//m_bbox.max.z = max( m_bbox.max.z, 0.0f);
	m_bbox.min -= Vec3(mv_radius, mv_radius, mv_radius);
	m_bbox.max += Vec3(mv_radius, mv_radius, mv_radius);
}

//////////////////////////////////////////////////////////////////////////
int CGravityVolumeObject::InsertPoint(int index, const Vec3& point)
{
	if (GetPointCount() >= GetMaxPoints())
		return GetPointCount() - 1;
	int newindex;
	StoreUndo("Insert Point");

	newindex = m_points.size();

	if (index < 0 || index >= m_points.size())
	{
		CGravityVolumePoint pt;
		pt.pos = point;
		pt.back = point;
		pt.forw = point;
		m_points.push_back(pt);
		newindex = m_points.size() - 1;
	}
	else
	{
		CGravityVolumePoint pt;
		pt.pos = point;
		pt.back = point;
		pt.forw = point;
		m_points.insert(m_points.begin() + index, pt);
		newindex = index;
	}
	SetPoint(newindex, point);
	CalcBBox();
	UpdateUIVars();
	return newindex;
}

//////////////////////////////////////////////////////////////////////////
void CGravityVolumeObject::RemovePoint(int index)
{
	if ((index >= 0 || index < m_points.size()) && m_points.size() > 3)
	{
		if (index == 0)
		{
			Vec3 center = m_points[1].pos;
			Matrix34 ltm = GetLocalTM();
			Vec3 newp = GetPos() + ltm.TransformVector(center);
		}

		StoreUndo("Remove Point");
		m_points.erase(m_points.begin() + index);
		CalcBBox();
		UpdateUIVars();
	}
}

//////////////////////////////////////////////////////////////////////////
void CGravityVolumeObject::SelectPoint(int index)
{
	if (m_selectedPoint == index)
		return;
	m_selectedPoint = index;
}

//////////////////////////////////////////////////////////////////////////
float CGravityVolumeObject::GetPointAngle()
{
	int index = m_selectedPoint;
	if (0 <= index && index < m_points.size())
		return m_points[index].angle;
	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////
void CGravityVolumeObject::SetPointAngle(float angle)
{
	int index = m_selectedPoint;
	if (0 <= index && index < m_points.size())
		m_points[index].angle = angle;
}

float CGravityVolumeObject::GetPointWidth()
{
	int index = m_selectedPoint;
	if (0 <= index && index < m_points.size())
		return m_points[index].width;
	return 0.0f;
}

void CGravityVolumeObject::SetPointWidth(float width)
{
	int index = m_selectedPoint;
	if (0 <= index && index < m_points.size())
		m_points[index].width = width;
}

bool CGravityVolumeObject::IsPointDefaultWidth()
{
	int index = m_selectedPoint;
	if (0 <= index && index < m_points.size())
		return m_points[index].isDefaultWidth;
	return true;
}

void CGravityVolumeObject::PointDafaultWidthIs(bool isDefault)
{
	int index = m_selectedPoint;
	if (0 <= index && index < m_points.size())
	{
		m_points[index].isDefaultWidth = isDefault;
		m_points[index].width = mv_radius;
	}
}

//////////////////////////////////////////////////////////////////////////
void CGravityVolumeObject::BezierAnglesCorrection(CGravityVolumePointVector& points, int index)
{
	int maxindex = points.size() - 1;
	if (index < 0 || index > maxindex)
		return;

	Vec3& p2 = points[index].pos;
	Vec3& back = points[index].back;
	Vec3& forw = points[index].forw;

	if (index == 0)
	{
		back = p2;
		if (maxindex == 1)
		{
			Vec3& p3 = points[index + 1].pos;
			forw = p2 + (p3 - p2) / 3;
		}
		else if (maxindex > 0)
		{
			Vec3& p3 = points[index + 1].pos;
			Vec3& pb3 = points[index + 1].back;

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
			Vec3& p1 = points[index - 1].pos;
			Vec3& pf1 = points[index - 1].forw;

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
		Vec3& p1 = points[index - 1].pos;
		Vec3& p3 = points[index + 1].pos;

		float lenOsn = (p3 - p1).GetLength();
		float lenb = (p1 - p2).GetLength();
		float lenf = (p3 - p2).GetLength();

		back = p2 + (p1 - p3) / (lenOsn / lenb * 3);
		forw = p2 + (p3 - p1) / (lenOsn / lenf * 3);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGravityVolumeObject::SplinePointsCorrection(CGravityVolumePointVector& points, int index)
{
	int maxindex = points.size() - 1;
	if (index < 0 || index > maxindex)
		return;

	Vec3& p2 = points[index].pos;
	Vec3& back = points[index].back;
	Vec3& forw = points[index].forw;

	if (index == 0)
	{
		back = p2;
		if (maxindex == 1)
		{
			Vec3& p3 = points[index + 1].pos;
			forw = p2 + (p3 - p2) / 3;
		}
		else if (maxindex > 0)
		{
			Vec3& p3 = points[index + 1].pos;
			Vec3& pb3 = points[index + 1].back;

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
			Vec3& p1 = points[index - 1].pos;
			Vec3& pf1 = points[index - 1].forw;

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
		Vec3& p1 = points[index - 1].pos;
		Vec3& p3 = points[index + 1].pos;

		float lenOsn = (p3 - p1).GetLength();
		float lenb = (p1 - p2).GetLength();
		float lenf = (p3 - p2).GetLength();

		back = p2 + (p1 - p3) / (lenOsn / lenb * 2);
		forw = p2 + (p3 - p1) / (lenOsn / lenf * 2);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGravityVolumeObject::BezierCorrection(int index)
{
	BezierAnglesCorrection(m_points, index - 1);
	BezierAnglesCorrection(m_points, index);
	BezierAnglesCorrection(m_points, index + 1);
	BezierAnglesCorrection(m_points, index - 2);
	BezierAnglesCorrection(m_points, index + 2);
}

//////////////////////////////////////////////////////////////////////////
void CGravityVolumeObject::SplineCorrection(int index)
{
	/*
	   SplinePointsCorrection(m_points, index-1);
	   SplinePointsCorrection(m_points, index);
	   SplinePointsCorrection(m_points, index+1);
	   SplinePointsCorrection(m_points, index-2);
	   SplinePointsCorrection(m_points, index+2);
	 */
}

//////////////////////////////////////////////////////////////////////////
void CGravityVolumeObject::SetPoint(int index, const Vec3& pos)
{
	Vec3 p = pos;
	if (m_bForce2D)
	{
		if (!m_points.empty())
			p.z = m_points[0].pos.z; // Keep original Z.
	}

	if (0 <= index && index < m_points.size())
	{
		m_points[index].pos = p;
		SplineCorrection(index);
	}

	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
int CGravityVolumeObject::GetNearestPoint(const Vec3& raySrc, const Vec3& rayDir, float& distance)
{
	int index = -1;
	float minDist = FLT_MAX;
	Vec3 rayLineP1 = raySrc;
	Vec3 rayLineP2 = raySrc + rayDir * RAY_DISTANCE;
	Vec3 intPnt;
	const Matrix34& wtm = GetWorldTM();
	for (int i = 0; i < m_points.size(); i++)
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
void CGravityVolumeObject::GetNearestEdge(const Vec3& pos, int& p1, int& p2, float& distance, Vec3& intersectPoint)
{
	p1 = -1;
	p2 = -1;
	float minDist = FLT_MAX;
	Vec3 intPnt;

	const Matrix34& wtm = GetWorldTM();

	for (int i = 0; i < m_points.size(); i++)
	{
		int j = (i < m_points.size() - 1) ? i + 1 : 0;

		if (j == 0 && i != 0)
			continue;

		int kn = 6;
		for (int k = 0; k < kn; k++)
		{
			Vec3 a1 = wtm.TransformPoint(GetSplinePos(m_points, i, float(k) / kn));
			Vec3 a2 = wtm.TransformPoint(GetSplinePos(m_points, i, float(k) / kn + 1.0f / kn));

			float d = PointToLineDistance(a1, a2, pos, intPnt);
			if (d < minDist)
			{
				minDist = d;
				p1 = i;
				p2 = j;
				intersectPoint = intPnt;
			}
		}
	}
	distance = minDist;
}

//////////////////////////////////////////////////////////////////////////
bool CGravityVolumeObject::RayToLineDistance(const Vec3& rayLineP1, const Vec3& rayLineP2, const Vec3& pi, const Vec3& pj, float& distance, Vec3& intPnt)
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
void CGravityVolumeObject::GetNearestEdge(const Vec3& raySrc, const Vec3& rayDir, int& p1, int& p2, float& distance, Vec3& intersectPoint)
{
	p1 = -1;
	p2 = -1;
	float minDist = FLT_MAX;
	Vec3 intPnt;
	Vec3 rayLineP1 = raySrc;
	Vec3 rayLineP2 = raySrc + rayDir * RAY_DISTANCE;

	const Matrix34& wtm = GetWorldTM();

	for (int i = 0; i < m_points.size(); i++)
	{
		int j = (i < m_points.size() - 1) ? i + 1 : 0;

		if (j == 0 && i != 0)
			continue;

		int kn = 6;
		for (int k = 0; k < kn; k++)
		{
			Vec3 pi = wtm.TransformPoint(GetSplinePos(m_points, i, float(k) / kn));
			Vec3 pj = wtm.TransformPoint(GetSplinePos(m_points, i, float(k) / kn + 1.0f / kn));

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
	distance = minDist;
}

//////////////////////////////////////////////////////////////////////////
void CGravityVolumeObject::OnParamChange(IVariable* var)
{
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
bool CGravityVolumeObject::HitTestRect(HitContext& hc)
{
	//BBox box;
	// Retrieve world space bound box.
	//GetBoundBox( box );

	// Project all edges to viewport.
	const Matrix34& wtm = GetWorldTM();
	for (int i = 0; i < m_points.size(); i++)
	{
		int j = (i < m_points.size() - 1) ? i + 1 : 0;
		if (j == 0 && i != 0)
			continue;

		Vec3 p0 = wtm.TransformPoint(m_points[i].pos);
		Vec3 p1 = wtm.TransformPoint(m_points[j].pos);

		CPoint pnt0 = hc.view->WorldToView(p0);
		CPoint pnt1 = hc.view->WorldToView(p1);

		// See if pnt0 to pnt1 line intersects with rectangle.
		// check see if one point is inside rect and other outside, or both inside.
		bool in0 = hc.rect.PtInRect(pnt0);
		bool in1 = hc.rect.PtInRect(pnt0);
		if ((in0 && in1) || (in0 && !in1) || (!in0 && in1))
		{
			hc.object = this;
			return true;
		}
	}

	return false;
}

void CGravityVolumeObject::SetSelected(bool bSelect)
{
	__super::SetSelected(bSelect);
}

bool CGravityVolumeObject::CreateGameObject()
{
	bool bRes = CEntityObject::CreateGameObject();

	UpdateGameArea();
	return bRes;
}

//////////////////////////////////////////////////////////////////////////
void CGravityVolumeObject::UpdateGameArea()
{
	if (!m_pEntity)
		return;

	IEntityAreaComponent* pArea = m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();
	if (!pArea)
		return;
	const Matrix34& wtm = GetWorldTM();
	m_bezierPoints.resize(m_points.size());
	for (int i = 0; i < m_points.size(); i++)
		m_bezierPoints[i] = wtm.TransformPoint(m_points[i].pos);
	pArea->SetGravityVolume(&m_bezierPoints[0], m_points.size(), mv_radius, mv_gravity, mv_dontDisableInvisible, mv_falloff, mv_damping);

	m_pEntity->SetFlags(m_pEntity->GetFlags() | ENTITY_FLAG_SEND_RENDER_EVENT);

	AABB box;
	GetLocalBounds(box);
	m_pEntity->GetRenderInterface()->SetLocalBounds(box, true);
}

//////////////////////////////////////////////////////////////////////////
void CGravityVolumeObject::OnEvent(ObjectEvent event)
{
	if (event == EVENT_INGAME)
	{
		if(m_pLuaProperties == nullptr)
			return;
		
		bool bEnabled = false;
		IVariable* pVarEn = m_pLuaProperties->FindVariable("bEnabled");
		if (pVarEn)
		{
			pVarEn->Get(bEnabled);
		}

		SEntityEvent ev;
		ev.event = ENTITY_EVENT_SCRIPT_EVENT;
		const char* pComEn = "Enable";
		const char* pComDi = "Disable";
		static const bool cbTrue = true;
		if (bEnabled)
			ev.nParam[0] = (INT_PTR)pComEn;
		else
			ev.nParam[0] = (INT_PTR)pComDi;
		ev.nParam[1] = (INT_PTR)IEntityClass::EVT_BOOL;
		ev.nParam[2] = (INT_PTR)&cbTrue;
		m_pEntity->SendEvent(ev);
	}
}

