// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __2dviewport_h__
#define __2dviewport_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "Viewport.h"
#include "Objects\DisplayContext.h"

class CPopupMenuItem;

/** 2D Viewport used mostly for indoor editing, Front/Top/Left viewports.
 */
class C2DViewport : public CViewport
{
	DECLARE_DYNCREATE(C2DViewport)
public:
	C2DViewport();
	virtual ~C2DViewport();

	virtual void          SetType(EViewportType type);
	virtual EViewportType GetType() const        { return m_viewType; }
	virtual float         GetAspectRatio() const { return 1.0f; };

	virtual void          ResetContent();
	virtual void          UpdateContent(int flags);

	// Called every frame to update viewport.
	virtual void Update();

	//! Map world space position to viewport position.
	virtual POINT WorldToView(const Vec3& wp) const;
	//! Map viewport position to world space position.
	virtual Vec3  ViewToWorld(POINT vp, bool* collideWithTerrain = 0, bool onlyTerrain = false, bool bSkipVegetation = false, bool bTestRenderMesh = false) const;
	//! Map viewport position to world space ray from camera.
	virtual void  ViewToWorldRay(POINT vp, Vec3& raySrc, Vec3& rayDir) const;
	//! Project a point as close as possible to a world space axis based on screen space projection
	virtual Vec3  ViewToAxisConstraint(POINT& point, Vec3 axis, Vec3 origin) const;
	//! Get view up direction - basically this is hack to compensate for fact of inconsistency between 2d-perspective view matrices
	virtual Vec3 UpViewDirection() const;
	//! Get view direction
	virtual Vec3 ViewDirection() const;
	//! Get direction from camera to a world point - not necessarily normalized
	virtual Vec3 CameraToWorld(Vec3 worldPoint) const;

	virtual bool  HitTest(CPoint point, HitContext& hitInfo);
	virtual bool  IsBoundsVisible(const AABB& box) const;

	// ovverided from CViewport.
	float GetScreenScaleFactor(const Vec3& worldPoint) const;

	// Overrided from CViewport.
	void OnDragSelectRectangle(CPoint p1, CPoint p2, bool bNormilizeRect = false);
	void CenterOnSelection();

	/** Get 2D viewports origin.
	 */
	Vec3 GetOrigin2D() const;

	/** Assign 2D viewports origin.
	 */
	void SetOrigin2D(const Vec3& org);

	void SetShowViewMarker(bool bEnable)                { m_bShowViewMarker = bEnable; }

	void SetShowGrid(bool bShowGrid)                    { m_bShowGrid = bShowGrid; }
	bool GetShowGrid() const                            { return m_bShowGrid; };

	void SetShowObjectsInfo(bool bShowObjectsInfo)      { m_bShowObjectsInfo = bShowObjectsInfo; }
	bool GetShowObjectsInfo() const                     { return m_bShowObjectsInfo; }

	void SetGridLines(bool bShowMinor, bool bShowMajor) { m_bShowMinorGridLines = bShowMinor; m_bShowMajorGridLines = bShowMajor; }
	void SetGridLineNumbers(bool bShowNumbers)          { m_bShowNumbers = bShowNumbers; }
	void SetAutoAdjust(bool bAuto)                      { m_bAutoAdjustGrids = bAuto; }

protected:
	enum EViewportAxis
	{
		VPA_XY,
		VPA_XZ,
		VPA_YZ,
		VPA_YX,
	};
	virtual bool CreateRenderContext(HWND hWnd) override;

	void         SetAxis(EViewportAxis axis);

	// Scrolling / zooming related
	virtual void SetScrollOffset(float x, float y, bool bLimits = true);
	virtual void GetScrollOffset(float& x, float& y);  // Only x and y components used.

	virtual void SetZoom(float fZoomFactor, CPoint center);

	// overrides from CViewport.
	virtual void            MakeSnappingGridPlane(int axis);

	//! Calculate view transformation matrix.
	virtual void CalculateViewTM();

	// Render
	void Render();

	// Draw everything.
	virtual void Draw(CObjectRenderHelper& objRenderHelper);

	// Draw elements of viewport.
	void DrawGrid(DisplayContext& dc, bool bNoXNumbers = false);
	void DrawAxis(DisplayContext& dc);
	void DrawSelection(DisplayContext& dc);
	void DrawObjects(CObjectRenderHelper& objRenderHelper);
	void DrawViewerMarker(DisplayContext& dc);

	AABB GetWorldBounds(CPoint p1, CPoint p2);

	//////////////////////////////////////////////////////////////////////////
	//! Get current screen matrix.
	//! Screen matrix transform from World space to Screen space.
	const Matrix34& GetScreenTM() const { return m_screenTM; };

	void            OnResize() override;
	void            OnPaint() override;
	bool            MouseCallback(EMouseEvent event, CPoint& point, int flags) override;

	void            SerializeDisplayOptions(Serialization::IArchive& ar);

	const CViewManager* GetViewManager() const { return GetIEditor()->GetViewManager(); }
	CViewManager* GetViewManager() { return GetIEditor()->GetViewManager(); }

protected:
	//! XY/XZ/YZ mode of this 2D viewport.
	EViewportType m_viewType;
	EViewportAxis m_axis;

	//! Axis to cull normals with in this Viewport.
	int m_cullAxis;

	// Viewport origin point.
	Vec3 m_origin2D;

	// Scrolling / zooming related
	CPoint         m_cMousePos;
	CPoint         m_RMouseDownPos;

	float          m_prevZoomFactor;
	CSize          m_prevScrollOffset;

	CRect          m_rcSelect;
	CRect          m_rcClient;

	AABB           m_displayBounds;

	bool           m_bShowTerrain;
	bool           m_bShowViewMarker;
	bool           m_bShowGrid;
	bool           m_bShowObjectsInfo;
	bool           m_bShowMinorGridLines;
	bool           m_bShowMajorGridLines;
	bool           m_bShowNumbers;
	bool           m_bAutoAdjustGrids;

	Matrix34       m_screenTM;
	Matrix34       m_screenTM_Inverted;

	float          m_gridAlpha;
	COLORREF       m_colorGridText;
	COLORREF       m_colorAxisText;
	COLORREF       m_colorBackground;
	bool           m_bContentValid;

	DisplayContext m_displayContext;
	CPoint         m_cMouseDownPos;

	bool           m_bRenderContextCreated;

	CCamera        m_camera;

	SDisplayContextKey m_displayContextKey;
};

#endif // __2dviewport_h__

