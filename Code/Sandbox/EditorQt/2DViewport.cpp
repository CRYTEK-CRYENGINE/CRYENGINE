// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#ifndef EDITOR_2D_VIEWPORT_NOT_IMPLEMENTED

	#include "2DViewport.h"
	#include "CryEditDoc.h"
	#include "Util/MFCUtil.h"

	#include <Preferences/ViewportPreferences.h>
	#include "Grid.h"
	#include "ViewManager.h"
	#include "Gizmos/IGizmoManager.h"

	#include "Objects/BrushObject.h"
	#include "Controls/DynamicPopupMenu.h"

	#include "RenderLock.h"

	#include <CryRenderer/IRenderAuxGeom.h>

	#define MARKER_SIZE          6.0f
	#define MARKER_DIR_SIZE      10.0f
	#define SELECTION_RADIUS     30.0f

	#define GL_RGBA              0x1908
	#define GL_BGRA              0x80E1

	#define BACKGROUND_COLOR     Vec3(1.0f, 1.0f, 1.0f)
	#define SELECTION_RECT_COLOR Vec3(0.8f, 0.8f, 0.8f)
	#define MINOR_GRID_COLOR     Vec3(0.55f, 0.55f, 0.55f)
	#define MAJOR_GRID_COLOR     Vec3(0.6f, 0.6f, 0.6f)
	#define AXIS_GRID_COLOR      Vec3(0, 0, 0)
	#define GRID_TEXT_COLOR      Vec3(0, 0, 1.0f)

	#define MAX_WORLD_SIZE       10000

enum Viewport2dTitleMenuCommands
{
	ID_SHOW_LABELS,
	ID_SHOW_GRID
};

IMPLEMENT_DYNCREATE(C2DViewport, CViewport)
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
C2DViewport::C2DViewport()
{
	// Scroll offset equals origin
	m_rcSelect.SetRect(0, 0, 0, 0);

	m_viewType = ET_ViewportXY;
	m_axis = VPA_XY;

	m_bShowTerrain = true;
	m_bShowGrid = true;
	m_bShowObjectsInfo = true;
	m_bShowMinorGridLines = true;
	m_bShowMajorGridLines = true;
	m_bShowNumbers = true;
	m_bAutoAdjustGrids = true;
	m_gridAlpha = 1;

	m_bRenderContextCreated = false;

	m_origin2D.Set(0, 0, 0);

	m_colorGridText = RGB(220, 220, 220);
	m_colorAxisText = RGB(220, 220, 220);
	m_colorBackground = RGB(128, 128, 128);

	m_screenTM.SetIdentity();
	m_screenTM_Inverted.SetIdentity();

	m_fZoomFactor = 1;
	m_bContentValid = false;

	m_bShowViewMarker = true;

	m_displayContext.pIconManager = GetIEditorImpl()->GetIconManager();

	// Calculate the View transformation matrix.
	CalculateViewTM();
}

//////////////////////////////////////////////////////////////////////////
C2DViewport::~C2DViewport()
{
	if (m_bRenderContextCreated)
	{
		// Do not delete primary context.
		if (m_displayContextKey != reinterpret_cast<HWND>(gEnv->pRenderer->GetHWND()))
			gEnv->pRenderer->DeleteContext(m_displayContextKey);
		m_bRenderContextCreated = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::SetType(EViewportType type)
{
	m_viewType = type;

	switch (m_viewType)
	{
	case ET_ViewportXY:
		m_axis = VPA_XY;
		break;
	case ET_ViewportXZ:
		m_axis = VPA_XZ;
		break;
	case ET_ViewportYZ:
		m_axis = VPA_YZ;
		break;
	}
	;

	SetAxis(m_axis);
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::SetAxis(EViewportAxis axis)
{
	m_axis = axis;
	switch (m_axis)
	{
	case VPA_XY:
		m_cullAxis = 2;
		break;
	case VPA_YX:
		m_cullAxis = 2;
		break;
	case VPA_XZ:
		m_cullAxis = 1;
		break;
	case VPA_YZ:
		m_cullAxis = 0;
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::CalculateViewTM()
{
	Matrix34 viewTM;
	Matrix34 tm;
	tm.SetIdentity();
	viewTM.SetIdentity();
	float fScale = GetZoomFactor();
	Vec3 origin = GetOrigin2D();

	float height = m_rcClient.Height() / fScale;

	Vec3 v1;
	switch (m_axis)
	{
	case VPA_XY:
		tm = Matrix33::CreateScale(Vec3(fScale, -fScale, fScale)); // No fScale for Z
		tm.SetTranslation(Vec3(-origin.x, height + origin.y, 0) * fScale);
		break;
	case VPA_YX:
		tm.SetFromVectors(Vec3(0, 1, 0) * fScale, Vec3(1, 0, 0) * fScale, Vec3(0, 0, 1) * fScale, Vec3(0, 0, 0));
		tm.SetTranslation(Vec3(-origin.x, height + origin.y, 0) * fScale);
		break;
	case VPA_XZ:
		viewTM.SetFromVectors(Vec3(1, 0, 0), Vec3(0, 0, 1), Vec3(0, 1, 0), Vec3(0, 0, 0));
		tm.SetFromVectors(Vec3(1, 0, 0) * fScale, Vec3(0, 0, 1) * fScale, Vec3(0, -1, 0) * fScale, Vec3(0, 0, 0));
		tm.SetTranslation(Vec3(-origin.x, height + origin.z, 0) * fScale);
		break;

	case VPA_YZ:
		viewTM.SetFromVectors(Vec3(0, 1, 0), Vec3(0, 0, 1), Vec3(1, 0, 0), Vec3(0, 0, 0));
		tm.SetFromVectors(Vec3(0, 0, 1) * fScale, Vec3(1, 0, 0) * fScale, Vec3(0, -1, 0) * fScale, Vec3(0, 0, 0)); // No fScale for Z
		tm.SetTranslation(Vec3(-origin.y, height + origin.z, 0) * fScale);
		break;
	}
	SetViewTM(viewTM);
	m_screenTM = tm;
	m_screenTM_Inverted = m_screenTM.GetInverted();

	SetConstructionMatrix(GetViewTM());
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::ResetContent()
{
	CViewport::ResetContent();
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::UpdateContent(int flags)
{
	CViewport::UpdateContent(flags);
	if (flags & eUpdateObjects)
	{
		m_bContentValid = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::SetScrollOffset(float x, float y, bool bLimits)
{
	Vec3 org = GetOrigin2D();
	switch (m_axis)
	{
	case VPA_XY:
	case VPA_YX:
		org.x = x;
		org.y = y;
		break;
	case VPA_XZ:
		org.x = x;
		org.z = y;
		break;
	case VPA_YZ:
		org.y = x;
		org.z = y;
		break;
	}

	SetOrigin2D(org);

	CalculateViewTM();
	//GetViewManager()->UpdateViews(eUpdateObjects);
	m_bContentValid = false;
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::GetScrollOffset(float& x, float& y)
{
	Vec3 origin = GetOrigin2D();
	switch (m_axis)
	{
	case VPA_XY:
	case VPA_YX:
		x = origin.x;
		y = origin.y;
		break;
	case VPA_XZ:
		x = origin.x;
		y = origin.z;
		break;
	case VPA_YZ:
		x = origin.y;
		y = origin.z;
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::SetZoom(float fZoomFactor, CPoint center)
{
	if (fZoomFactor < 0.01f)
		fZoomFactor = 0.01f;

	float prevz = GetZoomFactor();

	// Zoom to mouse position.
	float ofsx, ofsy;
	GetScrollOffset(ofsx, ofsy);

	float s1 = GetZoomFactor();
	float s2 = fZoomFactor;

	SetZoomFactor(fZoomFactor);

	// Calculate new offset to center zoom on mouse.
	float x2 = center.x;
	float y2 = m_rcClient.Height() - center.y;
	ofsx = -(x2 / s2 - x2 / s1 - ofsx);
	ofsy = -(y2 / s2 - y2 / s1 - ofsy);
	SetScrollOffset(ofsx, ofsy, true);

	CalculateViewTM();

	m_bContentValid = false;
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::OnPaint()
{
	m_bContentValid = false;
}

//////////////////////////////////////////////////////////////////////////
bool C2DViewport::CreateRenderContext(HWND hWnd)
{
	Vec3 clCol = CMFCUtils::Rgb2Vec(m_colorBackground);

	int width = 0;
	int height = 0;
	GetDimensions(&width,&height);

	IRenderer::SDisplayContextDescription desc;

	desc.handle = hWnd;
	desc.type = IRenderer::eViewportType_Secondary;
	desc.clearColor = ColorF(clCol);
	desc.renderFlags = FRT_CLEAR | FRT_OVERLAY_DEPTH;
	desc.screenResolution.x = width;
	desc.screenResolution.y = height;

	m_displayContextKey = gEnv->pRenderer->CreateSwapChainBackedContext(desc);

	m_bRenderContextCreated = true;
	return true;
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::Update()
{
	if (!m_bContentValid)
	{
		m_bContentValid = true;
		Render();
	}
	CViewport::Update();
}

//////////////////////////////////////////////////////////////////////////
POINT C2DViewport::WorldToView(const Vec3& wp) const
{
	Vec3 sp = m_screenTM.TransformPoint(wp);
	CPoint p = CPoint(sp.x, sp.y);
	return p;
}

//////////////////////////////////////////////////////////////////////////
Vec3 C2DViewport::ViewToWorld(POINT vp, bool* collideWithTerrain, bool onlyTerrain, bool bSkipVegetation, bool bTestRenderMesh) const
{
	Vec3 wp = m_screenTM_Inverted.TransformPoint(Vec3(vp.x, vp.y, 0));
	switch (m_axis)
	{
	case VPA_XY:
	case VPA_YX:
		wp.z = 0;
		break;
	case VPA_XZ:
		wp.y = 0;
		break;
	case VPA_YZ:
		wp.x = 0;
		break;
	}
	return wp;
}

Vec3 C2DViewport::ViewToAxisConstraint(POINT& point, Vec3 axis, Vec3 origin) const
{
	Vec3 raySrc, rayDir;

	ViewToWorldRay(point, raySrc, rayDir);

	// find plane between camera and initial position and direction
	Vec3 cameraToOrigin = raySrc - origin;
	Vec3 lineViewPlane = rayDir ^ axis;
	lineViewPlane.Normalize();

	// Now we project the ray from origin to the source point to the screen space line plane
	Vec3 perpPlane = (rayDir ^ lineViewPlane);

	// finally, project along the axis to perpPlane
	return ((perpPlane * cameraToOrigin) / (perpPlane * axis)) * axis;
}

Vec3 C2DViewport::ViewDirection() const
{
	switch (m_axis)
	{
	case VPA_XY:
	case VPA_YX:
		return Vec3(0, 0, -1);
	case VPA_XZ:
		return Vec3(0, -1, 0);
	case VPA_YZ:
		return Vec3(-1, 0, 0);
	}

	// some sane default without much meaning
	return Vec3(0, 0, -1);
}

Vec3 C2DViewport::UpViewDirection() const
{
	switch (m_axis)
	{
	case VPA_XY:
		return Vec3(0, 1, 0);
	case VPA_YX:
		return Vec3(1, 0, 0);
	case VPA_XZ:
		return Vec3(0, 0, 1);
	case VPA_YZ:
		return Vec3(0, 0, 1);
	}

	// some sane default without much meaning
	return Vec3(0, 1, 0);
}

Vec3 C2DViewport::CameraToWorld(Vec3 worldPoint) const
{
	return ViewDirection();
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::ViewToWorldRay(POINT vp, Vec3& raySrc, Vec3& rayDir) const
{
	raySrc = ViewToWorld(vp);
	switch (m_axis)
	{
	case VPA_XY:
	case VPA_YX:
		raySrc.z = MAX_WORLD_SIZE;
		rayDir(0, 0, -1);
		break;
	case VPA_XZ:
		raySrc.y = MAX_WORLD_SIZE;
		rayDir(0, -1, 0);
		break;
	case VPA_YZ:
		raySrc.x = MAX_WORLD_SIZE;
		rayDir(-1, 0, 0);
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
float C2DViewport::GetScreenScaleFactor(const Vec3& worldPoint) const
{
	return 400.0f / GetZoomFactor();
	//return 100.0f / ;
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::Render()
{
	// lock while we are rendering to prevent any re-rendering across the application
	if (CScopedRenderLock lock = CScopedRenderLock())
	{
		if (!gEnv->pRenderer)
			return;

		if (!IsWindowVisible())
			return;

		CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

		CRect rc;
		GetClientRect(rc);

		CalculateViewTM();

		CScopedWireFrameMode scopedWireFrame(gEnv->pRenderer, R_SOLID_MODE);

		//////////////////////////////////////////////////////////////////////////
		// 2D Mode.
		//////////////////////////////////////////////////////////////////////////
		if (rc.right != 0 && rc.bottom != 0)
		{
			gEnv->pRenderer->GetIRenderAuxGeom()->SetOrthographicProjection(true, 0.0f, rc.right, rc.bottom, 0.0f);
			
			//////////////////////////////////////////////////////////////////////////
			// Draw viewport elements here.
			//////////////////////////////////////////////////////////////////////////
			// Calc world bounding box for objects rendering.
			m_displayBounds = GetWorldBounds(CPoint(0, 0), CPoint(rc.Width(), rc.Height()));

			// Draw all objects.
			DisplayContext& dc = m_displayContext;
			
			dc.view = this;
			dc.renderer = gEnv->pRenderer;
			dc.engine = GetIEditorImpl()->Get3DEngine();
			dc.flags = DISPLAY_2D;
			dc.box = m_displayBounds;
			m_camera = GetIEditorImpl()->GetSystem()->GetViewCamera();
			// Should be setting orthogonal camera
			m_camera.SetFrustum(rc.Width(), rc.Height(), m_camera.GetFov(), m_camera.GetNearPlane(), m_camera.GetFarPlane());
			dc.SetCamera(&m_camera);
			dc.SetDisplayContext(m_displayContextKey);

			if (!gViewportPreferences.displayLabels || !(GetIEditor()->IsHelpersDisplayed()))
			{
				dc.flags |= DISPLAY_HIDENAMES;
			}
			if (gViewportPreferences.displayLinks && GetIEditor()->IsHelpersDisplayed())
			{
				dc.flags |= DISPLAY_LINKS;
			}
			if (m_bDegradateQuality)
			{
				dc.flags |= DISPLAY_DEGRADATED;
			}

			// Render
			gEnv->pRenderer->BeginFrame(m_displayContextKey);

			// TODO: BeginFrame/EndFrame calls can be droped and replaced by RT_AuxRender
			auto oldCamera = gEnv->pRenderer->GetIRenderAuxGeom()->GetCamera();
			gEnv->pRenderer->UpdateAuxDefaultCamera(m_camera);

			SRenderingPassInfo passInfo = SRenderingPassInfo::CreateGeneralPassRenderingInfo(GetIEditorImpl()->GetSystem()->GetViewCamera(), SRenderingPassInfo::DEFAULT_FLAGS, false, m_displayContextKey);

			gEnv->pRenderer->EF_StartEf(passInfo);
			dc.SetState(e_Mode3D | e_AlphaBlended | e_FillModeSolid | e_CullModeBack | e_DepthWriteOff | e_DepthTestOn);
			Draw(CObjectRenderHelper{ dc, passInfo });
			gEnv->pRenderer->EF_EndEf3D(SHDF_ALLOWHDR | SHDF_SECONDARY_VIEWPORT, -1, -1, passInfo);

			// Return back from 2D mode.
			gEnv->pRenderer->GetIRenderAuxGeom()->SetOrthographicProjection(false);

			gEnv->pRenderer->RenderDebug(false);

			ProcessRenderListeners(m_displayContext);

			gEnv->pRenderer->EndFrame();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::Draw(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	dc.DepthTestOff();
	dc.DepthWriteOff();
	if (m_bShowGrid)
		DrawGrid(dc);

	DrawObjects(objRenderHelper);
	DrawSelection(dc);
	if (m_bShowViewMarker)
		DrawViewerMarker(dc);
	DrawAxis(dc);
}

//////////////////////////////////////////////////////////////////////////
inline Vec3 SnapToSize(Vec3 v, double size)
{
	Vec3 snapped;
	snapped.x = floor((v.x / size) + 0.5) * size;
	snapped.y = floor((v.y / size) + 0.5) * size;
	snapped.z = floor((v.z / size) + 0.5) * size;
	return snapped;
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::DrawGrid(DisplayContext& dc, bool bNoXNumbers)
{
	float gridSize = gSnappingPreferences.gridSize();

	if (gridSize < 0.00001f)
		return;

	float fZ = 0;

	//////////////////////////////////////////////////////////////////////////
	float fScale = GetZoomFactor();

	int width = m_rcClient.Width();
	int height = m_rcClient.Height();

	float origin[2];
	GetScrollOffset(origin[0], origin[1]);

	//////////////////////////////////////////////////////////////////////////
	// Draw Minor grid lines.
	//////////////////////////////////////////////////////////////////////////

	float pixelsPerGrid = gridSize * fScale;

	m_fGridZoom = 1.0f;

	if (m_bAutoAdjustGrids)
	{
		int griditers = 0;
		pixelsPerGrid = gridSize * fScale;
		while (pixelsPerGrid <= 5 && griditers++ < 20)
		{
			m_fGridZoom *= gSnappingPreferences.gridMajorLine();
			gridSize = gridSize * gSnappingPreferences.gridMajorLine();
			pixelsPerGrid = gridSize * fScale;
		}
	}

	int firstGridLineX = origin[0] / gridSize - 1;
	int firstGridLineY = origin[1] / gridSize - 1;

	int numGridLinesX = (m_rcClient.Width() / fScale) / gridSize + 1;
	int numGridLinesY = (m_rcClient.Height() / fScale) / gridSize + 1;

	Matrix34 viewTM = GetViewTM().GetInverted() * m_screenTM_Inverted;
	Matrix34 viewTM_Inv = m_screenTM * GetViewTM();

	Vec3 viewP0 = viewTM.TransformPoint(Vec3(0, 0, 0));
	Vec3 viewP1 = viewTM.TransformPoint(Vec3(m_rcClient.Width(), m_rcClient.Height(), 0));

	Vec3 viewP_Text = viewTM.TransformPoint(Vec3(0, m_rcClient.Height(), 0));

	if (m_bShowMinorGridLines && (!m_bAutoAdjustGrids || pixelsPerGrid > 5))
	{
		//////////////////////////////////////////////////////////////////////////
		// Draw Minor grid lines.
		//////////////////////////////////////////////////////////////////////////

		Vec3 vec0 = SnapToSize(viewP0, gridSize);
		Vec3 vec1 = SnapToSize(viewP1, gridSize);
		if (vec0.x > vec1.x)
			std::swap(vec0.x, vec1.x);
		if (vec0.y > vec1.y)
			std::swap(vec0.y, vec1.y);

		dc.SetColor(MINOR_GRID_COLOR, m_gridAlpha);
		for (float dy = vec0.y; dy < vec1.y; dy += gridSize)
		{
			Vec3 p0 = viewTM_Inv.TransformPoint(Vec3(viewP0.x, dy, 0));
			Vec3 p1 = viewTM_Inv.TransformPoint(Vec3(viewP1.x, dy, 0));
			dc.DrawLine(Vec3(p0.x, p0.y, fZ), Vec3(p1.x, p1.y, fZ));
		}
		for (float dx = vec0.x; dx < vec1.x; dx += gridSize)
		{
			Vec3 p0 = viewTM_Inv.TransformPoint(Vec3(dx, viewP0.y, 0));
			Vec3 p1 = viewTM_Inv.TransformPoint(Vec3(dx, viewP1.y, 0));
			dc.DrawLine(Vec3(p0.x, p0.y, fZ), Vec3(p1.x, p1.y, fZ));
		}
	}
	if (m_bShowMajorGridLines)
	{
		//////////////////////////////////////////////////////////////////////////
		// Draw Major grid lines.
		//////////////////////////////////////////////////////////////////////////
		gridSize = gridSize * gSnappingPreferences.gridMajorLine();

		if (m_bAutoAdjustGrids)
		{
			int iters = 0;
			pixelsPerGrid = gridSize * fScale;
			while (pixelsPerGrid < 20 && iters < 20)
			{
				gridSize = gridSize * 2;
				pixelsPerGrid = gridSize * fScale;
				iters++;
			}
		}
		if (!m_bAutoAdjustGrids || pixelsPerGrid > 20)
		{
			Vec3 vec0 = SnapToSize(viewP0, gridSize);
			Vec3 vec1 = SnapToSize(viewP1, gridSize);
			if (vec0.x > vec1.x)
				std::swap(vec0.x, vec1.x);
			if (vec0.y > vec1.y)
				std::swap(vec0.y, vec1.y);

			dc.SetColor(MAJOR_GRID_COLOR, m_gridAlpha);

			for (float dy = vec0.y; dy < vec1.y; dy += gridSize)
			{
				Vec3 p0 = viewTM_Inv.TransformPoint(Vec3(viewP0.x, dy, 0));
				Vec3 p1 = viewTM_Inv.TransformPoint(Vec3(viewP1.x, dy, 0));
				dc.DrawLine(Vec3(p0.x, p0.y, fZ), Vec3(p1.x, p1.y, fZ));
			}
			for (float dx = vec0.x; dx < vec1.x; dx += gridSize)
			{
				Vec3 p0 = viewTM_Inv.TransformPoint(Vec3(dx, viewP0.y, 0));
				Vec3 p1 = viewTM_Inv.TransformPoint(Vec3(dx, viewP1.y, 0));
				dc.DrawLine(Vec3(p0.x, p0.y, fZ), Vec3(p1.x, p1.y, fZ));
			}

			// Draw numbers.
			if (m_bShowNumbers)
			{
				char text[64];
				dc.SetColor(m_colorGridText);

				if (!bNoXNumbers)
				{
					// Draw horizontal grid text.
					for (float dx = vec0.x; dx <= vec1.x; dx += gridSize)
					{
						Vec3 p = viewTM_Inv.TransformPoint(Vec3(dx, viewP_Text.y, 0));
						cry_sprintf(text, "%i", (int)(dx));
						dc.Draw2dTextLabel(p.x - 8, p.y - 13, 1, text);
					}
				}
				for (float dy = vec0.y; dy <= vec1.y; dy += gridSize)
				{
					Vec3 p = viewTM_Inv.TransformPoint(Vec3(viewP_Text.x, dy, 0));
					cry_sprintf(text, "%i", (int)(dy));
					dc.Draw2dTextLabel(p.x + 3, p.y - 12, 1, text);
				}
			}
		}
	}

	// Draw Main Axis.
	{
		Vec3 org = m_screenTM.TransformPoint(Vec3(0, 0, 0));
		dc.SetColor(AXIS_GRID_COLOR);
		dc.DrawLine(Vec3(org.x, 0, fZ), Vec3(org.x, height, fZ));
		dc.DrawLine(Vec3(0, org.y, fZ), Vec3(width, org.y, fZ));
	}
	//////////////////////////////////////////////////////////////////////////
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::DrawAxis(DisplayContext& dc)
{
	int ix = 0;
	int iy = 0;
	float cl = 0.85f;
	char xstr[2], ystr[2], zstr[2];
	Vec3 colx, coly, colz;
	Vec3 colorX(cl, 0, 0);
	Vec3 colorY(0, cl, 0);
	Vec3 colorZ(0, 0, cl);
	switch (m_axis)
	{
	case VPA_XY:
		cry_strcpy(xstr, "x");
		cry_strcpy(ystr, "y");
		cry_strcpy(zstr, "z");
		colx = colorX;
		coly = colorY;
		colz = colorZ;
		break;
	case VPA_YX:
		cry_strcpy(xstr, "x");
		cry_strcpy(ystr, "y");
		cry_strcpy(zstr, "z");
		colx = colorY;
		coly = colorX;
		colz = colorZ;
		break;
	case VPA_XZ:
		cry_strcpy(xstr, "x");
		cry_strcpy(ystr, "z");
		cry_strcpy(zstr, "y");
		colx = colorX;
		coly = colorZ;
		colz = colorY;
		break;
	case VPA_YZ:
		cry_strcpy(xstr, "y");
		cry_strcpy(ystr, "z");
		cry_strcpy(zstr, "x");
		colx = colorY;
		coly = colorZ;
		colz = colorX;
		break;
	}

	int width = m_rcClient.Width();
	int height = m_rcClient.Height();

	int size = 25;
	Vec3 pos(30, height - 15, 1);

	dc.SetColor(colx.x, colx.y, colx.z, 1);
	dc.DrawLine(pos, pos + Vec3(size, 0, 0));

	dc.SetColor(coly.x, coly.y, coly.z, 1);
	dc.DrawLine(pos, pos - Vec3(0, size, 0));

	dc.SetColor(m_colorAxisText);
	pos.x -= 3;
	pos.y -= 4;
	pos.z = 2;
	dc.Draw2dTextLabel(pos.x + size + 4, pos.y - 2, 1, xstr);
	dc.Draw2dTextLabel(pos.x + 3, pos.y - size, 1, ystr);
	dc.Draw2dTextLabel(pos.x - 5, pos.y + 5, 1, zstr);
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::DrawSelection(DisplayContext& dc)
{
	AABB box;
	GetIEditorImpl()->GetSelectedRegion(box);

	if (box.min != box.max)
	{
		switch (m_axis)
		{
		case VPA_XY:
		case VPA_YX:
			box.min.z = box.max.z = 0;
			break;
		case VPA_XZ:
			box.min.y = box.max.y = 0;
			break;
		case VPA_YZ:
			box.min.x = box.max.x = 0;
			break;
		}

		dc.PushMatrix(GetScreenTM());
		dc.SetColor(SELECTION_RECT_COLOR.x, SELECTION_RECT_COLOR.y, SELECTION_RECT_COLOR.z, 1);
		dc.DrawWireBox(box.min, box.max);
		dc.PopMatrix();
	}

	if (!m_selectedRect.IsRectEmpty())
	{
		dc.SetColor(SELECTION_RECT_COLOR.x, SELECTION_RECT_COLOR.y, SELECTION_RECT_COLOR.z, 1);
		CPoint p1 = CPoint(m_selectedRect.left, m_selectedRect.top);
		CPoint p2 = CPoint(m_selectedRect.right, m_selectedRect.bottom);
		dc.DrawLine(Vec3(p1.x, p1.y, 0), Vec3(p2.x, p1.y, 0));
		dc.DrawLine(Vec3(p1.x, p2.y, 0), Vec3(p2.x, p2.y, 0));
		dc.DrawLine(Vec3(p1.x, p1.y, 0), Vec3(p1.x, p2.y, 0));
		dc.DrawLine(Vec3(p2.x, p1.y, 0), Vec3(p2.x, p2.y, 0));
	}
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::DrawViewerMarker(DisplayContext& dc)
{
	float noScale = 1.0f / GetZoomFactor();

	CViewport* pVP = GetViewManager()->GetGameViewport();
	if (!pVP)
		return;

	Ang3 viewAngles = Ang3::GetAnglesXYZ(Matrix33(pVP->GetViewTM()));

	Matrix34 tm;
	switch (m_axis)
	{
	case VPA_XY:
	case VPA_YX:
		tm = Matrix34::CreateRotationXYZ(Ang3(0, 0, viewAngles.z));
		break;
	case VPA_XZ:
		tm = Matrix34::CreateRotationXYZ(Ang3(viewAngles.x, viewAngles.y, viewAngles.z));
		break;
	case VPA_YZ:
		tm = Matrix34::CreateRotationXYZ(Ang3(viewAngles.x, 0, viewAngles.z));
		break;
	}
	tm.SetTranslation(pVP->GetViewTM().GetTranslation());

	dc.PushMatrix(GetScreenTM() * tm);

	Vec3 dim(MARKER_SIZE, MARKER_SIZE / 2, MARKER_SIZE);
	dc.SetColor(RGB(0, 0, 255)); // blue
	dc.DrawWireBox(-dim * noScale, dim * noScale);

	float fov = GetIEditorImpl()->GetSystem()->GetViewCamera().GetFov();

	Vec3 q[4];
	float dist = 30;
	float ta = (float)tan(0.5f * fov);
	float h = dist * ta;
	float w = h * gViewportPreferences.defaultAspectRatio; //  ASPECT ??
	//float h = w / GetAspect();
	q[0] = Vec3(w, dist, h) * noScale;
	q[1] = Vec3(-w, dist, h) * noScale;
	q[2] = Vec3(-w, dist, -h) * noScale;
	q[3] = Vec3(w, dist, -h) * noScale;

	// Draw frustum.
	dc.DrawLine(Vec3(0, 0, 0), q[0]);
	dc.DrawLine(Vec3(0, 0, 0), q[1]);
	dc.DrawLine(Vec3(0, 0, 0), q[2]);
	dc.DrawLine(Vec3(0, 0, 0), q[3]);

	// Draw quad.
	dc.DrawPolyLine(q, 4);

	dc.PopMatrix();
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::DrawObjects(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	dc.PushMatrix(GetScreenTM());
	if (m_bShowObjectsInfo)
	{
		GetIEditorImpl()->GetObjectManager()->Display(objRenderHelper);
		GetIEditorImpl()->GetGizmoManager()->Display(dc);
	}

	// Display editing tool.
	if (GetEditTool() && m_bMouseInside)
	{
		GetEditTool()->Display(dc);
	}
	dc.PopMatrix();
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::MakeSnappingGridPlane(int axis)
{
	RefCoordSys coordSys = GetIEditorImpl()->GetReferenceCoordSys();
	m_snappingMatrix = GetViewTM();

	Vec3 p(0, 0, 0);

	switch (m_axis)
	{
	case VPA_XY:
		AssignConstructionPlane(p, p + Vec3(0, 1, 0), p + Vec3(1, 0, 0));
		break;
	case VPA_YX:
		m_constructionPlane.SetPlane(p, p + Vec3(1, 0, 0), p + Vec3(0, 1, 0));
		break;
	case VPA_XZ:
		AssignConstructionPlane(p, p + Vec3(0, 0, 1), p + Vec3(1, 0, 0));
		break;
	case VPA_YZ:
		AssignConstructionPlane(p, p + Vec3(0, 0, 1), p + Vec3(0, 1, 0));
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
AABB C2DViewport::GetWorldBounds(CPoint pnt1, CPoint pnt2)
{
	Vec3 org;
	AABB box;
	box.Reset();
	box.Add(ViewToWorld(pnt1));
	box.Add(ViewToWorld(pnt2));

	int maxSize = MAX_WORLD_SIZE;
	switch (m_axis)
	{
	case VPA_XY:
	case VPA_YX:
		box.min.z = -maxSize;
		box.max.z = maxSize;
		break;
	case VPA_XZ:
		box.min.y = -maxSize;
		box.max.y = maxSize;
		break;
	case VPA_YZ:
		box.min.x = -maxSize;
		box.max.x = maxSize;
		break;
	}
	return box;
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::OnDragSelectRectangle(CPoint pnt1, CPoint pnt2, bool bNormilizeRect)
{
	Vec3 org;
	AABB box;
	box.Reset();

	Vec3 p1 = ViewToWorld(pnt1);
	Vec3 p2 = ViewToWorld(pnt2);
	org = p1;

	// Calculate selection volume.
	box.Add(p1);
	box.Add(p2);

	int maxSize = MAX_WORLD_SIZE;
	float w, h;

	switch (m_axis)
	{
	case VPA_XY:
		box.min.z = -maxSize;
		box.max.z = maxSize;

		w = box.max.x - box.min.x;
		h = box.max.y - box.min.y;
		break;
	case VPA_YX:
		box.min.z = -maxSize;
		box.max.z = maxSize;

		w = box.max.y - box.min.y;
		h = box.max.x - box.min.x;
		break;
	case VPA_XZ:
		box.min.y = -maxSize;
		box.max.y = maxSize;

		w = box.max.x - box.min.x;
		h = box.max.z - box.min.z;
		break;
	case VPA_YZ:
		box.min.x = -maxSize;
		box.max.x = maxSize;

		w = box.max.y - box.min.y;
		h = box.max.z - box.min.z;
		break;
	default:
		break;
	}

	GetIEditorImpl()->SetSelectedRegion(box);
}

//////////////////////////////////////////////////////////////////////////
bool C2DViewport::HitTest(CPoint point, HitContext& hitInfo)
{
	hitInfo.bounds = &m_displayBounds;
	return CViewport::HitTest(point, hitInfo);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool C2DViewport::IsBoundsVisible(const AABB& box) const
{
	// If at least part of bbox is visible then its visible.
	if (m_displayBounds.IsIntersectBox(box))
		return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::CenterOnSelection()
{
	const CSelectionGroup* sel = GetIEditorImpl()->GetSelection();
	if (sel->IsEmpty())
		return;

	AABB bounds = sel->GetBounds();
	Vec3 selPos = sel->GetCenter();

	float size = (bounds.max - bounds.min).GetLength();

	Vec3 v1 = ViewToWorld(CPoint(m_rcClient.left, m_rcClient.bottom));
	Vec3 v2 = ViewToWorld(CPoint(m_rcClient.right, m_rcClient.top));
	Vec3 vofs = (v2 - v1) * 0.5f;
	selPos -= vofs;
	SetOrigin2D(selPos);

	m_bContentValid = false;
}

//////////////////////////////////////////////////////////////////////////
Vec3 C2DViewport::GetOrigin2D() const
{
	if (gViewportPreferences.sync2DViews)
		return GetViewManager()->GetOrigin2D();
	else
		return m_origin2D;
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::SetOrigin2D(const Vec3& org)
{
	m_origin2D = org;
	if (gViewportPreferences.sync2DViews)
		GetViewManager()->SetOrigin2D(m_origin2D);
}

//////////////////////////////////////////////////////////////////////////
void C2DViewport::OnResize()
{
	int width = 0;
	int height = 0;
	GetDimensions(&width, &height);

	gEnv->pRenderer->ResizeContext(m_displayContextKey,width,height);

	GetClientRect(&m_rcClient);
	CalculateViewTM();
	OnPaint();
}

//////////////////////////////////////////////////////////////////////////
bool C2DViewport::MouseCallback(EMouseEvent event, CPoint& point, int flags)
{
	if (GetIEditorImpl()->IsInGameMode())
		return false;

	if (CViewport::MouseCallback(event, point, flags))
	{
		m_bContentValid = false;
		if (!MoreMouseEventProcessNeeded(event))
			return true;
	}

	if (event == eMouseRDown)
	{
		SetCurrentCursor(STD_CURSOR_MOVE, "");

		// Save the mouse down position
		m_RMouseDownPos = point;

		m_prevZoomFactor = GetZoomFactor();
		//m_prevScrollOffset = m_cScrollOffset;

		SetViewMode(ScrollZoomMode);

		m_bContentValid = false;
	}
	else if (event == eMouseRUp)
	{
		SetViewMode(NothingMode);
		GetViewManager()->UpdateViews(eUpdateObjects);
	}

	if (event == eMouseMDown)
	{
		// Save the mouse down position
		m_RMouseDownPos = point;

		SetCurrentCursor(STD_CURSOR_MOVE, "");

		// Save the mouse down position
		m_RMouseDownPos = point;

		m_prevZoomFactor = GetZoomFactor();

		SetViewMode(ScrollZoomMode);

		m_bContentValid = false;
	}
	else if (event == eMouseMUp)
	{
		SetViewMode(NothingMode);

		m_bContentValid = false;
		GetViewManager()->UpdateViews(eUpdateObjects);
	}

	if (event == eMouseLDown)
	{
		////////////////////////////////////////////////////////////////////////
		// User pressed the left mouse button
		////////////////////////////////////////////////////////////////////////
		if (GetViewMode() == NothingMode)
		{
			m_cMouseDownPos = point;
			m_prevZoomFactor = GetZoomFactor();
			//m_prevScrollOffset = m_cScrollOffset;

			m_bContentValid = false;
		}
	}
	if (event == eMouseLUp)
	{
		GetViewManager()->UpdateViews(eUpdateObjects);
	}

	if (event == eMouseWheel)
	{
		float z = GetZoomFactor();
		int zDelta = point.x;
		float scale = 1.2f * fabs(zDelta / 120.0f);
		if (zDelta >= 0)
		{
			z = z * scale;
		}
		else
		{
			z = z / scale;
		}
		SetZoom(z, m_cMousePos);

		GetViewManager()->UpdateViews(eUpdateObjects);
	}

	if (event == eMouseMove)
	{
		m_cMousePos = point;

		if (GetViewMode() == ScrollZoomMode)
		{
			CRect rc;
			// You can only scroll while the middle mouse button is down
			if (flags & MK_RBUTTON || flags & MK_MBUTTON)
			{
				if (flags & MK_SHIFT)
				{
					// Get the dimensions of the window
					GetClientRect(&rc);

					CRect rc;
					GetClientRect(rc);
					int w = rc.right;
					int h = rc.bottom;

					// Zoom to mouse position.
					float z = m_prevZoomFactor + (point.y - m_RMouseDownPos.y) * 0.02f;
					SetZoom(z, m_RMouseDownPos);
				}
				else
				{
					// Set the new scrolled coordinates
					float fScale = GetZoomFactor();
					float ofsx, ofsy;
					GetScrollOffset(ofsx, ofsy);
					ofsx -= (point.x - m_RMouseDownPos.x) / fScale;
					ofsy += (point.y - m_RMouseDownPos.y) / fScale;
					SetScrollOffset(ofsx, ofsy);
					m_RMouseDownPos = point;
				}
			}
		}

		m_bContentValid = false;
	}

	return false;
}

// Serialization struct for XY viewport
class ViewportXYSettings
{
public:
	ViewportXYSettings(C2DViewport* viewport)
		: m_pViewport(viewport)
	{}

	void Serialize(Serialization::IArchive& ar)
	{
		bool showGrid = m_pViewport->GetShowGrid();
		ar(showGrid, "showgrid", "Show Grid");

		if (ar.isInput())
		{
			m_pViewport->SetShowGrid(showGrid);
		}
	}

private:
	C2DViewport* m_pViewport;
};

// Serialize Properties for viewports
void C2DViewport::SerializeDisplayOptions(Serialization::IArchive& ar)
{
	ViewportXYSettings ser(this);

	ar(ser, "viewport2D", "2D Viewport");
}

#endif //EDITOR_2D_VIEWPORT_NOT_IMPLEMENTED

