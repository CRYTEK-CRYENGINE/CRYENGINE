// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Viewport.h"
#include "IViewportManager.h"
#include "IUndoManager.h"
#include "IObjectManager.h"
#include <Preferences/ViewportPreferences.h>
#include <CrySystem/ITimer.h>

#include "Gizmos/ITransformManipulator.h"
#include "Include/IRenderListener.h"

#include <CryPhysics/IPhysics.h>
#include <Cry3DEngine/I3DEngine.h>

#include "Objects/BaseObject.h"
#include <CryInput/IHardwareMouse.h>

#include "Util/Math.h"
#include "QtUtil.h"
#include "DragDrop.h"
#include "AssetSystem/Asset.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/AssetType.h"

#include "QToolWindowManager/QToolWindowManager.h"
#include "Grid.h"

#include "Gizmos/IGizmoManager.h"
#include "Gizmos/Gizmo.h"
#include "EditTool.h"

#include <QDrag>
#include <QDropEvent>
#include <QDesktopWidget>

IMPLEMENT_DYNCREATE(CViewport, CObject)

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
bool CViewport::m_bDegradateQuality = false;
//Matrix34 CViewport::m_constructionMatrix[LAST_COORD_SYSTEM];

CViewport::CViewport()
{
	m_viewWidget = 0;

	m_selectionTollerance = 0;
	m_fGridZoom = 1.0f;

	m_nLastUpdateFrame = 0;
	m_nLastMouseMoveFrame = 0;

	// View mode
	m_eViewMode = NothingMode;

	// TODO: move to common cursor library instead of loading per viewport
	m_hCursor[STD_CURSOR_DEFAULT] = QCursor(Qt::ArrowCursor);
	QPixmap pointerHit_pixmap = QPixmap(":/icons/Viewport/pointerHit.png");
	m_hCursor[STD_CURSOR_HIT] = QCursor(pointerHit_pixmap, 15, 15);
	QPixmap object_move_pixmap = QPixmap(":/icons/Viewport/object_move.png");
	m_hCursor[STD_CURSOR_MOVE] = QCursor(object_move_pixmap, 15, 15);
	QPixmap object_rotate_pixmap = QPixmap(":/icons/Viewport/object_rotate.png");
	m_hCursor[STD_CURSOR_ROTATE] = QCursor(object_rotate_pixmap, 15, 15);
	QPixmap object_scale_pixmap = QPixmap(":/icons/Viewport/object_scale.png");
	m_hCursor[STD_CURSOR_SCALE] = QCursor(object_scale_pixmap, 15, 15);
	QPixmap pointer_plus_pixmap = QPixmap(":/icons/Viewport/pointer_plus.png");
	m_hCursor[STD_CURSOR_SEL_PLUS] = QCursor(pointer_plus_pixmap, 1, 1);
	QPixmap pointer_minus_pixmap = QPixmap(":/icons/Viewport/pointer_minus.png");
	m_hCursor[STD_CURSOR_SEL_MINUS] = QCursor(pointer_minus_pixmap, 1, 1);
	QPixmap pointer_so_select_pixmap = QPixmap(":/icons/Viewport/pointer_so_select.png");
	m_hCursor[STD_CURSOR_SUBOBJ_SEL] = QCursor(pointer_so_select_pixmap, 11, 9);
	QPixmap pointer_so_sel_plus_pixmap = QPixmap(":/icons/Viewport/pointer_so_sel_plus.png");
	m_hCursor[STD_CURSOR_SUBOBJ_SEL_PLUS] = QCursor(pointer_so_sel_plus_pixmap, 6, 6);
	QPixmap pointer__pixmap = QPixmap(":/icons/Viewport/pointer_.png");
	m_hCursor[STD_CURSOR_SUBOBJ_SEL_MINUS] = QCursor(pointer__pixmap, 6, 6);
	QPixmap Mouse_pixmap = QPixmap(":/icons/Viewport/Mouse.png");
	m_hCursor[STD_CURSOR_CRYSIS] = QCursor(Mouse_pixmap, 0, 0);

	m_activeAxis = AXIS_XY;

	m_snappingMatrix.SetIdentity();
	m_viewTM.SetIdentity();

	m_pMouseOverObject = 0;

	m_bAdvancedSelectMode = false;

	m_pVisibleObjectsCache = new CBaseObjectsCache;

	m_constructionPlane.SetPlane(Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, 0.0f, 0.0f));
	m_constructionPlaneAxisX.Set(0, 0, 0);
	m_constructionPlaneAxisY.Set(0, 0, 0);

	GetIEditor()->GetViewportManager()->RegisterViewport(this);

	m_pLocalEditTool = 0;

	m_nCurViewportID = MAX_NUM_VIEWPORTS - 1;

	m_bFocused = false;

	m_bMouseInside = false;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::SetViewWidget(QWidget* view)
{
	m_viewWidget = view;

	this->CreateRenderContext(GetSafeHwnd());
}

//////////////////////////////////////////////////////////////////////////
CViewport::~CViewport()
{
	m_pLocalEditTool = 0;
	delete m_pVisibleObjectsCache;

	GetIEditor()->GetViewportManager()->UnregisterViewport(this);
}

//////////////////////////////////////////////////////////////////////////
void CViewport::ScreenToClient(POINT* pPoint) const
{
	QPoint p = m_viewWidget->mapToGlobal(QPoint(0, 0));
	QRect r = QApplication::desktop()->screenGeometry(m_viewWidget->window());
	QPoint p2 = QtUtil::PixelScale(m_viewWidget, p - r.topLeft()); //widget offset on screen, screen scale
	pPoint->x -= r.left() + p2.x();
	pPoint->y -= r.top() + p2.y();
}

//////////////////////////////////////////////////////////////////////////
void CViewport::GetDimensions(int* pWidth, int* pHeight) const
{
	if (pWidth)
		*pWidth = QtUtil::PixelScale(m_viewWidget, m_viewWidget->size().width());
	if (pHeight)
		*pHeight = QtUtil::PixelScale(m_viewWidget, m_viewWidget->size().height());
}

//////////////////////////////////////////////////////////////////////////
CEditTool* CViewport::GetLocalEditTool()
{
	return m_pLocalEditTool;
}

CEditTool* CViewport::GetEditTool()
{
	if (m_pLocalEditTool)
		return m_pLocalEditTool;
	else 
		return GetIEditor()->GetEditTool();
}

//////////////////////////////////////////////////////////////////////////
void CViewport::SetLocalEditTool(CEditTool* pEditTool)
{
	if (m_pLocalEditTool != pEditTool)
	{
		m_pLocalEditTool = pEditTool;
	}
}

//////////////////////////////////////////////////////////////////////////
void CViewport::RegisterRenderListener(IRenderListener* piListener)
{
#ifdef _DEBUG
	size_t nCount(0);
	size_t nTotal(0);

	nTotal = m_cRenderListeners.size();
	for (nCount = 0; nCount < nTotal; ++nCount)
	{
		if (m_cRenderListeners[nCount] == piListener)
		{
			assert(!"Registered the same RenderListener multiple times.");
			break;
		}
	}
#endif //_DEBUG
	m_cRenderListeners.push_back(piListener);
}

//////////////////////////////////////////////////////////////////////////
bool CViewport::UnregisterRenderListener(IRenderListener* piListener)
{
	size_t nCount(0);
	size_t nTotal(0);

	nTotal = m_cRenderListeners.size();
	for (nCount = 0; nCount < nTotal; ++nCount)
	{
		if (m_cRenderListeners[nCount] == piListener)
		{
			m_cRenderListeners.erase(m_cRenderListeners.begin() + nCount);
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CViewport::IsRenderListenerRegistered(IRenderListener* piListener)
{
	size_t nCount(0);
	size_t nTotal(0);

	nTotal = m_cRenderListeners.size();
	for (nCount = 0; nCount < nTotal; ++nCount)
	{
		if (m_cRenderListeners[nCount] == piListener)
		{
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::AddPostRenderer(IPostRenderer* pPostRenderer)
{
	stl::push_back_unique(m_postRenderers, pPostRenderer);
}

//////////////////////////////////////////////////////////////////////////
bool CViewport::RemovePostRenderer(IPostRenderer* pPostRenderer)
{
	PostRenderers::iterator itr = m_postRenderers.begin();
	PostRenderers::iterator end = m_postRenderers.end();
	for (; itr != end; ++itr)
	{
		if (*itr == pPostRenderer)
		{
			break;
		}
	}

	if (itr != end)
	{
		m_postRenderers.erase(itr);
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
string CViewport::GetName() const
{
	return m_name;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::SetName(const string& name)
{
	m_name = name;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::OnActivate()
{
	////////////////////////////////////////////////////////////////////////
	// Make this edit window the current one
	////////////////////////////////////////////////////////////////////////
}

//////////////////////////////////////////////////////////////////////////
void CViewport::OnDeactivate()
{
}

void CViewport::GetResolution(int& x, int& y)
{
	CRect rectViewport;
	GetClientRect(rectViewport);

	x = rectViewport.Width();
	y = rectViewport.Height();
}

//////////////////////////////////////////////////////////////////////////
void CViewport::ResetContent()
{
	m_pMouseOverObject = 0;

	// Need to clear visual object cache.
	// Right after loading new level, some code(e.g. OnMouseMove) access invalid
	// previous level object before cache updated.
	GetVisibleObjectsCache()->ClearObjects();
}

//////////////////////////////////////////////////////////////////////////
void CViewport::UpdateContent(int flags)
{
	if (flags & eRedrawViewports)
		Invalidate(FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CViewport::Update()
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	m_bAdvancedSelectMode = false;
	bool bSpaceClick = false;
	CEditTool* pEditTool = GetIEditor()->GetEditTool();
	
	//TODO : this is deprecated
	bSpaceClick = CryGetAsyncKeyState(VK_SPACE);

	if (bSpaceClick && IsFocused())
		m_bAdvancedSelectMode = true;

	m_nLastUpdateFrame++;
}

//////////////////////////////////////////////////////////////////////////
POINT CViewport::WorldToView(const Vec3& wp) const
{
	CPoint p;
	p.x = wp.x;
	p.y = wp.y;
	return p;
}

//////////////////////////////////////////////////////////////////////////
Vec3 CViewport::WorldToView3D(const Vec3& wp, int nFlags) const
{
	CPoint p = WorldToView(wp);
	Vec3 out;
	out.x = p.x;
	out.y = p.y;
	out.z = wp.z;
	return out;
}

//////////////////////////////////////////////////////////////////////////
Vec3 CViewport::ViewToWorld(POINT vp, bool* pCollideWithTerrain, bool onlyTerrain, bool bSkipVegetation, bool bTestRenderMesh) const  //TODO : remove all parameters that are unused!!
{
	Vec3 wp;
	wp.x = vp.x;
	wp.y = vp.y;
	wp.z = 0;
	if (pCollideWithTerrain)
		*pCollideWithTerrain = true;
	return wp;
}

//////////////////////////////////////////////////////////////////////////
Vec3 CViewport::ViewToWorldNormal(POINT vp, bool onlyTerrain, bool bTestRenderMesh)
{
	return Vec3(0, 0, 0);
}

//////////////////////////////////////////////////////////////////////////
void CViewport::ViewToWorldRay(POINT vp, Vec3& raySrc, Vec3& rayDir) const
{
	raySrc(0, 0, 0);
	rayDir(0, 0, -1);
}

Vec3 CViewport::ViewToAxisConstraint(POINT& point, Vec3 axis, Vec3 origin) const
{
	return Vec3(0, 0, 0);
}

Vec3 CViewport::ViewDirection() const
{
	return Vec3(0, 0, -1);
}

Vec3 CViewport::UpViewDirection() const
{
	return Vec3(0, 1, 0);
}

Vec3 CViewport::CameraToWorld(Vec3 worldPoint) const
{
	return -worldPoint;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::ResetSelectionRegion()
{
	AABB box(Vec3(0, 0, 0), Vec3(0, 0, 0));
	GetIEditor()->SetSelectedRegion(box);
	m_selectedRect.SetRectEmpty();
}

void CViewport::SetSelectionRectangle(CPoint p1, CPoint p2)
{
	m_selectedRect = CRect(p1, p2);
	m_selectedRect.NormalizeRect();
}

//////////////////////////////////////////////////////////////////////////
void CViewport::OnDragSelectRectangle(CPoint pnt1, CPoint pnt2, bool bNormilizeRect)
{
	Vec3 org;
	AABB box;
	box.Reset();

	Vec3 p1 = ViewToWorld(pnt1);
	Vec3 p2 = ViewToWorld(pnt2);
	org = p1;
	// Calculate selection volume.
	if (!bNormilizeRect)
	{
		box.Add(p1);
		box.Add(p2);
	}
	else
	{
		CRect rc(pnt1, pnt2);
		rc.NormalizeRect();
		box.Add(ViewToWorld(CPoint(rc.left, rc.top)));
		box.Add(ViewToWorld(CPoint(rc.right, rc.top)));
		box.Add(ViewToWorld(CPoint(rc.left, rc.bottom)));
		box.Add(ViewToWorld(CPoint(rc.right, rc.bottom)));
	}

	box.min.z = -10000;
	box.max.z = 10000;
	GetIEditor()->SetSelectedRegion(box);

	// Show marker position in the status bar
	float w = box.max.x - box.min.x;
	float h = box.max.y - box.min.y;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::SetCursor(QCursor* hCursor)
{
	if (hCursor)
		m_viewWidget->setCursor(*hCursor);
	else
		m_viewWidget->setCursor(m_hCursor[STD_CURSOR_DEFAULT]);
}

//////////////////////////////////////////////////////////////////////////
void CViewport::SetCurrentCursor(EStdCursor stdCursor, const string& cursorString)
{
	SetCurrentCursor(stdCursor);
	m_cursorStr = cursorString;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::SetCurrentCursor(EStdCursor stdCursor)
{
	SetCursor(&m_hCursor[stdCursor]);
}

//////////////////////////////////////////////////////////////////////////
void CViewport::SetCursorString(const string& cursorString)
{
	m_cursorStr = cursorString;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::ResetCursor()
{
	SetCurrentCursor(STD_CURSOR_DEFAULT, "");
}

//////////////////////////////////////////////////////////////////////////
void CViewport::SetConstructionMatrix(const Matrix34& xform)
{
	m_snappingMatrix = xform;
	m_snappingMatrix.OrthonormalizeFast(); // Remove scale component from matrix.
	MakeSnappingGridPlane(GetAxisConstrain());
}

//////////////////////////////////////////////////////////////////////////
void CViewport::AssignConstructionPlane(const Vec3& p1, const Vec3& p2, const Vec3& p3)
{
	m_constructionPlane.SetPlane(p1, p2, p3);
	m_constructionPlaneAxisX = p3 - p1;
	m_constructionPlaneAxisY = p2 - p1;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::MakeSnappingGridPlane(int axis)
{
	CPoint cursor;
	GetCursorPos(&cursor);
	ScreenToClient(&cursor);
	Vec3 raySrc(0, 0, 0), rayDir(1, 0, 0);
	ViewToWorldRay(cursor, raySrc, rayDir);

	Vec3 xAxis(1, 0, 0);
	Vec3 yAxis(0, 1, 0);
	Vec3 zAxis(0, 0, 1);

	xAxis = m_snappingMatrix.TransformVector(xAxis);
	yAxis = m_snappingMatrix.TransformVector(yAxis);
	zAxis = m_snappingMatrix.TransformVector(zAxis);

	Vec3 pos = m_snappingMatrix.GetTranslation();

	if (axis == AXIS_X)
	{
		// X direction.
		Vec3 n;
		float d1 = fabs(rayDir.Dot(yAxis));
		float d2 = fabs(rayDir.Dot(zAxis));
		if (d1 > d2) n = yAxis; else n = zAxis;
		if (rayDir.Dot(n) < 0) n = -n; // face construction plane to the ray.

		//Vec3 n = Vec3(0,0,1);
		Vec3 v1 = n.Cross(xAxis);
		Vec3 v2 = n.Cross(v1);
		AssignConstructionPlane(pos, pos + v2, pos + v1);
	}
	else if (axis == AXIS_Y)
	{
		// Y direction.
		Vec3 n;
		float d1 = fabs(rayDir.Dot(xAxis));
		float d2 = fabs(rayDir.Dot(zAxis));
		if (d1 > d2) n = xAxis; else n = zAxis;
		if (rayDir.Dot(n) < 0) n = -n; // face construction plane to the ray.

		Vec3 v1 = n.Cross(yAxis);
		Vec3 v2 = n.Cross(v1);
		AssignConstructionPlane(pos, pos + v2, pos + v1);
	}
	else if (axis == AXIS_Z)
	{
		// Z direction.
		Vec3 n;
		float d1 = fabs(rayDir.Dot(xAxis));
		float d2 = fabs(rayDir.Dot(yAxis));
		if (d1 > d2) n = xAxis; else n = yAxis;
		if (rayDir.Dot(n) < 0) n = -n; // face construction plane to the ray.

		Vec3 v1 = n.Cross(zAxis);
		Vec3 v2 = n.Cross(v1);
		AssignConstructionPlane(pos, pos + v2, pos + v1);
	}
	else if (axis == AXIS_XY)
	{
		AssignConstructionPlane(pos, pos + yAxis, pos + xAxis);
	}
	else if (axis == AXIS_XZ)
	{
		AssignConstructionPlane(pos, pos + zAxis, pos + xAxis);
	}
	else if (axis == AXIS_YZ)
	{
		AssignConstructionPlane(pos, pos + zAxis, pos + yAxis);
	}
	else
	{
		AssignConstructionPlane(pos, pos + yAxis, pos + xAxis);
	}
}

//////////////////////////////////////////////////////////////////////////
Vec3 CViewport::MapViewToCP(CPoint point, int axis, bool aSnapToTerrain, float aTerrainOffset)
{
	if (((GetIEditor()->IsSnapToTerrainEnabled() || aSnapToTerrain) && axis != AXIS_Z) || GetIEditor()->IsSnapToGeometryEnabled())
	{
		bool hasCollidedWithTerrain;
		Vec3 pos = ViewToWorld(point, &hasCollidedWithTerrain);

		if (hasCollidedWithTerrain)
			pos.z += aTerrainOffset;//TODO : should this offset be the offset against any construction plane ?

		return SnapToGrid(pos);
	}

	MakeSnappingGridPlane(axis);

	Vec3 raySrc(0, 0, 0), rayDir(1, 0, 0);
	ViewToWorldRay(point, raySrc, rayDir);

	Vec3 v;

	Ray ray(raySrc, rayDir);
	if (!Intersect::Ray_Plane(ray, m_constructionPlane, v))
	{
		Plane inversePlane = m_constructionPlane;
		inversePlane.n = -inversePlane.n;
		inversePlane.d = -inversePlane.d;
		if (!Intersect::Ray_Plane(ray, inversePlane, v))
		{
			v.Set(0, 0, 0);
		}
	}

	// Snap value to grid.
	v = SnapToGrid(v);

	return v;
}

//////////////////////////////////////////////////////////////////////////
Vec3 CViewport::GetCPVector(const Vec3& p1, const Vec3& p2, int axis)
{
	Vec3 v = p2 - p1;

	Vec3 xAxis(1, 0, 0);
	Vec3 yAxis(0, 1, 0);
	Vec3 zAxis(0, 0, 1);
	// In local coordinate system transform axises by construction matrix.

	xAxis = m_snappingMatrix.TransformVector(xAxis);
	yAxis = m_snappingMatrix.TransformVector(yAxis);
	zAxis = m_snappingMatrix.TransformVector(zAxis);

	if (axis == AXIS_X || axis == AXIS_Y || axis == AXIS_Z)
	{
		// Project vector v on transformed axis x,y or z.
		Vec3 axisVector;
		if (axis == AXIS_X)
			axisVector = xAxis;
		if (axis == AXIS_Y)
			axisVector = yAxis;
		if (axis == AXIS_Z)
			axisVector = zAxis;

		// Project vector on construction plane into the one of axises.
		v = v.Dot(axisVector) * axisVector;
	}
	else if (axis == AXIS_XY)
	{
		// Project vector v on transformed plane x/y.
		Vec3 planeNormal = xAxis.Cross(yAxis);
		Vec3 projV = v.Dot(planeNormal) * planeNormal;
		v = v - projV;
	}
	else if (axis == AXIS_XZ)
	{
		// Project vector v on transformed plane x/y.
		Vec3 planeNormal = xAxis.Cross(zAxis);
		Vec3 projV = v.Dot(planeNormal) * planeNormal;
		v = v - projV;
	}
	else if (axis == AXIS_YZ)
	{
		// Project vector v on transformed plane x/y.
		Vec3 planeNormal = yAxis.Cross(zAxis);
		Vec3 projV = v.Dot(planeNormal) * planeNormal;
		v = v - projV;
	}

	if (GetIEditor()->IsSnapToTerrainEnabled() && axis != AXIS_Z)
	{
		v.z = 0;
	}
	return v;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::SetAxisConstrain(int axis)
{
	m_activeAxis = axis;
};

//////////////////////////////////////////////////////////////////////////
bool CViewport::HitTest(CPoint point, HitContext& hitInfo)
{
	if (hitInfo.bSkipIfGizmoHighlighted && GetIEditor()->GetGizmoManager()->GetHighlightedGizmo())
		return false;

	Vec3 raySrc(0, 0, 0), rayDir(1, 0, 0);
	ViewToWorldRay(point, hitInfo.raySrc, hitInfo.rayDir);
	hitInfo.view = this;
	hitInfo.point2d = point;
	if (m_bAdvancedSelectMode)
	{
		hitInfo.bUseSelectionHelpers = true;
	}

	return GetIEditor()->GetObjectManager()->HitTest(hitInfo);
}

//////////////////////////////////////////////////////////////////////////
void CViewport::SetZoomFactor(float fZoomFactor)
{
	m_fZoomFactor = fZoomFactor;
	if (gViewportPreferences.sync2DViews && GetType() != ET_ViewportCamera && GetType() != ET_ViewportModel)
		GetIEditor()->GetViewportManager()->SetZoom2D(fZoomFactor);
};

//////////////////////////////////////////////////////////////////////////
float CViewport::GetZoomFactor() const
{
	if (gViewportPreferences.sync2DViews && GetType() != ET_ViewportCamera && GetType() != ET_ViewportModel)
	{
		m_fZoomFactor = GetIEditor()->GetViewportManager()->GetZoom2D();
	}
	return m_fZoomFactor;
};

//////////////////////////////////////////////////////////////////////////
Vec3 CViewport::SnapToGrid(Vec3 vec)
{
	return gSnappingPreferences.Snap(vec, m_fGridZoom);
}

float CViewport::GetGridStep() const
{
	return gSnappingPreferences.gridScale() * gSnappingPreferences.gridSize();
}

//////////////////////////////////////////////////////////////////////////
void CViewport::BeginUndo()
{
	DegradateQuality(true);
	GetIEditor()->GetIUndoManager()->Begin();
}

//////////////////////////////////////////////////////////////////////////
void CViewport::AcceptUndo(const string& undoDescription)
{
	DegradateQuality(false);
	GetIEditor()->GetIUndoManager()->Accept(undoDescription);
	GetIEditor()->UpdateViews(eUpdateObjects);
}

//////////////////////////////////////////////////////////////////////////
void CViewport::CancelUndo()
{
	DegradateQuality(false);
	GetIEditor()->GetIUndoManager()->Cancel();
	GetIEditor()->UpdateViews(eUpdateObjects);
}

//////////////////////////////////////////////////////////////////////////
void CViewport::RestoreUndo()
{
	GetIEditor()->GetIUndoManager()->Restore();
}

//////////////////////////////////////////////////////////////////////////
bool CViewport::IsUndoRecording() const
{
	return GetIEditor()->GetIUndoManager()->IsUndoRecording();
}

//////////////////////////////////////////////////////////////////////////
void CViewport::DegradateQuality(bool bEnable)
{
	m_bDegradateQuality = bEnable;
}

//////////////////////////////////////////////////////////////////////////
CSize CViewport::GetIdealSize() const
{
	return CSize(0, 0);
}

//////////////////////////////////////////////////////////////////////////
bool CViewport::IsBoundsVisible(const AABB& box) const
{
	// Always visible in standard implementation.
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CViewport::HitTestLine(const Vec3& lineP1, const Vec3& lineP2, const CPoint& hitpoint, int pixelRadius, float* pToCameraDistance) const
{
	CPoint p1 = WorldToView(lineP1);
	CPoint p2 = WorldToView(lineP2);

	float dist = PointToLineDistance2D(Vec3(p1.x, p1.y, 0), Vec3(p2.x, p2.y, 0), Vec3(hitpoint.x, hitpoint.y, 0));
	if (dist <= pixelRadius)
	{
		if (pToCameraDistance)
		{
			Vec3 raySrc, rayDir;
			ViewToWorldRay(hitpoint, raySrc, rayDir);
			Vec3 rayTrg = raySrc + rayDir * 10000.0f;

			Vec3 pa, pb;
			float mua, mub;
			LineLineIntersect(lineP1, lineP2, raySrc, rayTrg, pa, pb, mua, mub);
			*pToCameraDistance = mub;
		}

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::GetPerpendicularAxis(EAxis* pAxis, bool* pIs2D) const
{
	EViewportType vpType = GetType();
	bool b2D = false;
	switch (vpType)
	{
	case ET_ViewportXY:
		if (pIs2D)
			*pIs2D = true;
		if (pAxis)
			*pAxis = AXIS_Z;
		break;
	case ET_ViewportXZ:
		if (pIs2D)
			*pIs2D = true;
		if (pAxis)
			*pAxis = AXIS_Y;
		break;
	case ET_ViewportYZ:
		if (pIs2D)
			*pIs2D = true;
		if (pAxis)
			*pAxis = AXIS_X;
		break;
	case ET_ViewportMap:
	case ET_ViewportZ:
		if (pIs2D)
			*pIs2D = true;
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CViewport::GetAdvancedSelectModeFlag() const
{
	return m_bAdvancedSelectMode;
}

//////////////////////////////////////////////////////////////////////////
bool CViewport::MouseCallback(EMouseEvent event, CPoint& point, int flags)
{
	// Ignore any mouse events in game mode.
	if (GetIEditor()->IsInGameMode())
	{
		if (gEnv->pHardwareMouse)
		{
			EHARDWAREMOUSEEVENT hwMouseEvent;
			switch (event)
			{
			case eMouseMove:
				hwMouseEvent = HARDWAREMOUSEEVENT_MOVE;
				break;
			case eMouseWheel:
				hwMouseEvent = HARDWAREMOUSEEVENT_WHEEL;
				break;
			case eMouseLDown:
				hwMouseEvent = HARDWAREMOUSEEVENT_LBUTTONDOWN;
				break;
			case eMouseLUp:
				hwMouseEvent = HARDWAREMOUSEEVENT_LBUTTONUP;
				break;
			case eMouseRDown:
				hwMouseEvent = HARDWAREMOUSEEVENT_RBUTTONDOWN;
				break;
			case eMouseRUp:
				hwMouseEvent = HARDWAREMOUSEEVENT_RBUTTONUP;
				break;
			case eMouseMDown:
				hwMouseEvent = HARDWAREMOUSEEVENT_MBUTTONDOWN;
				break;
			case eMouseMUp:
				hwMouseEvent = HARDWAREMOUSEEVENT_MBUTTONUP;
				break;
			case eMouseLDblClick:
				hwMouseEvent = HARDWAREMOUSEEVENT_LBUTTONDOUBLECLICK;
				break;
			case eMouseRDblClick:
				hwMouseEvent = HARDWAREMOUSEEVENT_RBUTTONDOUBLECLICK;
				break;
			case eMouseMDblClick:
				hwMouseEvent = HARDWAREMOUSEEVENT_MBUTTONDOUBLECLICK;
				break;
			}
			gEnv->pHardwareMouse->Event(point.x, point.y, hwMouseEvent);
		}
		return true;
	}

	// We must ignore mouse events when we are in the middle of an assert.
	// Reason: If we have an assert called from an engine module under the editor, if we call this function,
	// it may call the engine again and cause a deadlock.
	// Concrete example: CryPhysics called from Trackview causing an assert, and moving the cursor over the viewport
	// would cause the editor to freeze as it calls CryPhysics again for a raycast while it didn't release the lock.
	if (gEnv->pSystem->IsAssertDialogVisible())
	{
		return true;
	}

	if (event == eMouseLDown || event == eMouseRDown || event == eMouseMDown)
	{
		if (!IsFocused())
			SetFocus(true);
		GetIEditor()->GetViewportManager()->SelectViewport(this);
	}

	switch (event)
	{
	case eMouseMove:

		if (m_nLastUpdateFrame == m_nLastMouseMoveFrame)
		{
			// If mouse move event generated in the same frame, ignore it.
			return false;
		}
		m_nLastMouseMoveFrame = m_nLastUpdateFrame;

		break;

	case eMouseEnter:
		m_bMouseInside = true;
		break;

	case eMouseLeave:
		m_bMouseInside = false;
		break;
	}

	//////////////////////////////////////////////////////////////////////////
	// Handle viewport manipulators.
	//////////////////////////////////////////////////////////////////////////
	IGizmoManager* gizmoManager = GetIEditor()->GetGizmoManager();
	if (gizmoManager->HandleMouseInput(this, event, point, flags))
	{
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	// Asks current edit tool to handle mouse callback.
	CEditTool* pEditTool = GetEditTool();
	if (pEditTool && pEditTool->MouseCallback(this, event, point, flags))
	{
		if (event == eMouseMove && !m_viewWidget->hasFocus())
		{
			m_viewWidget->setFocus();
		}
		return true;
	}

	return false;
}

bool CViewport::DragEvent(EDragEvent eventId, QEvent* event, int flags)
{
	if (GetIEditor()->IsInGameMode())
	{
		return true;
	}

	CEditTool* pEditTool = GetEditTool();
	if (pEditTool && pEditTool->OnDragEvent(this, eventId, event, flags))
		return true;

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CViewport::OnKeyDown(uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (GetIEditor()->IsInGameMode())
	{
		// Ignore key downs while in game.
		return true;
	}
	if (GetEditTool())
	{
		if (GetEditTool()->OnKeyDown(this, nChar, nRepCnt, nFlags))
			return true;
		else if (nChar == Qt::Key_Escape)
		{
			GetIEditor()->SetEditTool(0);
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CViewport::OnKeyUp(uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (GetIEditor()->IsInGameMode())
	{
		// Ignore key downs while in game.
		return true;
	}
	if (GetEditTool())
	{
		if (GetEditTool()->OnKeyUp(this, nChar, nRepCnt, nFlags))
			return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::ProcessRenderListeners(DisplayContext& rstDisplayContext)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	size_t nCount(0);
	size_t nTotal(0);

	nTotal = m_cRenderListeners.size();
	for (nCount = 0; nCount < nTotal; ++nCount)
	{
		m_cRenderListeners[nCount]->Render(rstDisplayContext);
	}
}

//////////////////////////////////////////////////////////////////////////
float CViewport::GetFOV() const
{
	return gViewportPreferences.defaultFOV;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::ClientToScreen(CPoint* pnt)
{
	QPoint local_qt_space = QtUtil::QtScale(m_viewWidget,QPoint(pnt->x, pnt->y));
	QPoint global_qt_space = m_viewWidget->mapToGlobal(local_qt_space);
	QPoint global_pixel_space = QtUtil::PixelScale(m_viewWidget,global_qt_space);
	pnt->x = global_pixel_space.x();
	pnt->y = global_pixel_space.y();
}

bool CViewport::IsWindowVisible() const
{
	if (!m_viewWidget)
		return false;

	return m_viewWidget->isVisible();
	//QRegion region = visibleRegion();
	//return !region.isEmpty();
}

//////////////////////////////////////////////////////////////////////////
HWND CViewport::GetSafeHwnd() const
{
	//QWindow *window = m_viewWidget->windowHandle();
	//HWND h = static_cast<HWND>(QGuiApplication::platformNativeInterface()->nativeResourceForWindow("handle", window));
	HWND h = (HWND)m_viewWidget->winId();
	return h;
}

//////////////////////////////////////////////////////////////////////////
CWnd* CViewport::GetCWnd() const
{
	return CWnd::FromHandle((HWND)GetSafeHwnd());
}

void CViewport::GetClientRect(RECT* rc) const
{
	rc->left = 0;
	rc->top = 0;
	rc->right = m_viewWidget->size().width() * m_viewWidget->devicePixelRatioF();
	rc->bottom = m_viewWidget->size().height()  * m_viewWidget->devicePixelRatioF();
}

bool CViewport::IsMouseInWindow(CPoint* pnt, bool bIsLocal)
{
	QPoint p(pnt->x, pnt->y);
	QSize size = m_viewWidget->size();

	if (!bIsLocal)
	{
		p = m_viewWidget->mapFromGlobal(p);
	}

	return p.x() >= 0 && p.y() >= 0 &&
	       p.x() < size.width() &&
	       p.y() < size.height();
}

bool CViewport::MoreMouseEventProcessNeeded(EMouseEvent event)
{
	return event != eMouseWheel;
}

