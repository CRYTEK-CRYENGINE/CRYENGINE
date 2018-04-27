// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include <CryMath/Cry_Color.h>
#include "IPostRenderer.h"
#include "IDisplayViewport.h"
#include <CrySerialization/IArchive.h>

// Qt Headers
#include <QPoint>

// forward declarations.
class CBaseObject;
struct DisplayContext;
class CCryEditDoc;
class CViewManager;
class CEditTool;
class CBaseObjectsCache;
struct HitContext;
struct IRenderListener;
class CImageEx;
class CameraTransformEvent;
class CryInputEvent;
class QWidget;

/** Type of viewport.
 */
enum EViewportType
{
	ET_ViewportUnknown = 0,
	ET_ViewportXY,
	ET_ViewportXZ,
	ET_ViewportYZ,
	ET_ViewportCamera,
	ET_ViewportMap,
	ET_ViewportModel,
	ET_ViewportZ, //!< Z Only viewport.
	ET_ViewportUI,

	ET_ViewportLast,
};

//////////////////////////////////////////////////////////////////////////
// Standart cursors viewport can display.
//////////////////////////////////////////////////////////////////////////
enum EStdCursor
{
	STD_CURSOR_DEFAULT,
	STD_CURSOR_HIT,
	STD_CURSOR_MOVE,
	STD_CURSOR_ROTATE,
	STD_CURSOR_SCALE,
	STD_CURSOR_SEL_PLUS,
	STD_CURSOR_SEL_MINUS,
	STD_CURSOR_SUBOBJ_SEL,
	STD_CURSOR_SUBOBJ_SEL_PLUS,
	STD_CURSOR_SUBOBJ_SEL_MINUS,

	// TODO: remove hardcoded aplication name
	STD_CURSOR_CRYSIS,

	STD_CURSOR_LAST,
};

//! Base class for all Editor Viewports.
//! This still contains non-generic functionnality, please be careful to not add to it
class EDITOR_COMMON_API CViewport : public CObject, public _i_reference_target_t, public IDisplayViewport
{
	DECLARE_DYNCREATE(CViewport)
public:
	enum ViewMode
	{
		NothingMode = 0,
		ScrollZoomMode,
		ScrollMode,
		ZoomMode,
		ManipulatorDragMode,
	};

	enum EResolutionMode
	{
		// Internal resolution stays constant, window size changes to preserve aspect of internal resolution
		Stretch = 0,
		// Keep resolution changes to be the same as window size
		Window,
		// Internal resolution stays constant and viewport is centered so that only part of it is rendered
		Center,
		// Internal resolution stays constant and viewport's top right corner is snapped to the widget's top right corner
		// so debug information is visible
		TopRight,
		// Internal resolution stays constant and viewport's top left corner is snapped to the widget's top left corner
		// so debug information is visible
		TopLeft,
		// Internal resolution stays constant and viewport's bottom right corner is snapped to the widget's bottom right corner
		// so debug information is visible
		BottomRight,
		// Internal resolution stays constant and viewport's bottom left corner is snapped to the widget's bottom right corner
		// so debug information is visible
		BottomLeft,
	};

	CViewport();
	virtual ~CViewport();

	//! Called while window is idle.
	virtual void Update();

	/** Set name of this viewport.
	 */
	void SetName(const string& name);

	/** Get name of viewport
	 */
	string GetName() const;

	/** Set ID of this viewport
	 */
	void SetViewportId(int id) { m_nCurViewportID = id; };

	/** Get ID of this viewport
	 */
	int GetViewportId() { return m_nCurViewportID; };

	/** Must be overriden in derived classes.
	 */
	virtual void SetType(EViewportType type) {};

	// Return true if this is a RenderViewport based class.
	virtual bool IsRenderViewport() { return false; };

	// Is overridden by RenderViewport
	virtual void  SetFOV(float fov) {}
	virtual float GetFOV() const;

	// Must be overridden in derived classes.
	// Returns:
	//   e.g. 4.0/3.0
	virtual float GetAspectRatio() const { return 0; };
	virtual void  GetDimensions(int* pWidth, int* pHeight) const;
	virtual void  ScreenToClient(POINT* pPoint) const;

	/** Get type of this viewport.
	 */
	virtual EViewportType GetType() const                     { return ET_ViewportUnknown; }

	virtual void            SetResolution(int x, int y)               {}
	virtual void            GetResolution(int& x, int& y);
	virtual void            SetResolutionMode(EResolutionMode mode)   {}
	virtual EResolutionMode GetResolutionMode()                       { return EResolutionMode::Window; }

	virtual void          ResetContent();
	virtual void          UpdateContent(int flags);

	//! Set current zoom factor for this viewport.
	virtual void SetZoomFactor(float fZoomFactor);

	//! Get current zoom factor for this viewport.
	virtual float GetZoomFactor() const;

	virtual void  OnActivate();
	virtual void  OnDeactivate();
	virtual void  SetCursor(QCursor* hCursor) override;

	//! Map world space position to viewport position.
	virtual POINT WorldToView(const Vec3& wp) const;

	//! Map world space position to 3D viewport position.
	virtual Vec3 WorldToView3D(const Vec3& wp, int nFlags = 0) const;

	//! Map viewport position to world space position.
	virtual Vec3 ViewToWorld(POINT vp, bool* pCollideWithTerrain = 0, bool onlyTerrain = false, bool bSkipVegetation = false, bool bTestRenderMesh = false) const;
	//! Convert point on screen to world ray.
	virtual void ViewToWorldRay(POINT vp, Vec3& raySrc, Vec3& rayDir) const;
	//! Get normal for viewport position
	virtual Vec3 ViewToWorldNormal(POINT vp, bool onlyTerrain, bool bTestRenderMesh = false);
	//! Project a point as close as possible to a world space axis based on screen space projection
	virtual Vec3 ViewToAxisConstraint(POINT& point, Vec3 axis, Vec3 origin) const;
	//! Get view direction
	virtual Vec3 ViewDirection() const;
	//! Get view up direction - basically this is hack to compensate for fact of inconsistency between 2d-perspective view matrices
	virtual Vec3 UpViewDirection() const;
	//! Get direction from camera to a world point - not necessarily normalized
	virtual Vec3 CameraToWorld(Vec3 worldPoint) const;

	//! Map view point to world space using current construction plane.
	//TODO : decorrelate terrain logic from these methods, implementing like this for now to support backwards compatibility
	//hint: offset should be applied to the result of this method, which takes into account all snapping parameters, not before. right now it is applied
	//after constraint and terrain snapping but before grid snapping
	Vec3         MapViewToCP(CPoint point)                                            { return MapViewToCP(point, GetAxisConstrain(), false, 0.f); };
	Vec3         MapViewToCP(CPoint point, bool aSnapToTerrain, float aTerrainOffset) { return MapViewToCP(point, GetAxisConstrain(), aSnapToTerrain, aTerrainOffset); };
	virtual Vec3 MapViewToCP(CPoint point, int axis, bool aSnapToTerrain = false, float aTerrainOffset = 0.f);

	//! This method return a vector (p2-p1) in world space alligned to construction plane and restriction axises.
	//! p1 and p2 must be given in world space and lie on construction plane.
	virtual Vec3 GetCPVector(const Vec3& p1, const Vec3& p2) { return GetCPVector(p1, p2, GetAxisConstrain()); }
	virtual Vec3 GetCPVector(const Vec3& p1, const Vec3& p2, int axis);

	//! Snap any given 3D world position to grid lines if snap is enabled.
	virtual Vec3  SnapToGrid(Vec3 vec) override;
	virtual float GetGridStep() const;

	//! Returns the screen scale factor for a point given in world coordinates.
	//! This factor gives the width in world-space units at the point's distance of the viewport.
	virtual float GetScreenScaleFactor(const Vec3& worldPoint) const { return 1; };

	virtual void  SetViewMode(ViewMode eViewMode)                    { m_eViewMode = eViewMode; };
	ViewMode      GetViewMode()                                      { return m_eViewMode; };

	void          SetAxisConstrain(int axis);
	int           GetAxisConstrain() const { return GetIEditor()->GetAxisConstrains(); };

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CViewport)
	//}}AFX_VIRTUAL

	//////////////////////////////////////////////////////////////////////////
	// Selection.
	//////////////////////////////////////////////////////////////////////////
	//! Resets current selection region.
	virtual void  ResetSelectionRegion();
	//! Set 2D selection rectangle.
	virtual void  SetSelectionRectangle(CPoint p1, CPoint p2);
	//! Return 2D selection rectangle.
	virtual CRect GetSelectionRectangle() const { return m_selectedRect; };
	//! Called when dragging selection rectangle.
	virtual void  OnDragSelectRectangle(CPoint p1, CPoint p2, bool bNormilizeRect = false);
	//! Get selection procision tollerance.
	float         GetSelectionTolerance() const { return m_selectionTollerance; }
	//! Center viewport on selection.
	virtual void  CenterOnSelection()           {};

	//! Center on AABB
	virtual void  CenterOnAABB(AABB* const aabb) {};

	//! Performs hit testing of 2d point in view to find which object hit.
	virtual bool HitTest(CPoint point, HitContext& hitInfo);

	//! Do 2D hit testing of line in world space.
	// pToCameraDistance is an optional output parameter in which distance from the camera to the line is returned.
	virtual bool HitTestLine(const Vec3& lineP1, const Vec3& lineP2, const CPoint& hitpoint, int pixelRadius, float* pToCameraDistance = 0) const;

	// Access to the member m_bAdvancedSelectMode so interested modules can know its value.
	bool                 GetAdvancedSelectModeFlag() const;

	virtual void         GetPerpendicularAxis(EAxis* pAxis, bool* pIs2D) const;

	//////////////////////////////////////////////////////////////////////////
	// View matrix.
	//////////////////////////////////////////////////////////////////////////
	//! Set current view matrix,
	//! This is a matrix that transforms from world to view space.
	virtual void SetViewTM(const Matrix34& tm) { m_viewTM = tm; };

	//! Get current view matrix.
	//! This is a matrix that transforms from world space to view space.
	virtual const Matrix34& GetViewTM() const { return m_viewTM; };

	//////////////////////////////////////////////////////////////////////////
	//! Set construction plane from given position construction matrix refrence coord system and axis settings.
	//////////////////////////////////////////////////////////////////////////
	virtual void            MakeSnappingGridPlane(int axis);
	virtual void            SetConstructionMatrix(const Matrix34& xform) override;
	//////////////////////////////////////////////////////////////////////////

	void DegradateQuality(bool bEnable);

	//////////////////////////////////////////////////////////////////////////
	// Undo for viewpot operations.
	void BeginUndo();
	void AcceptUndo(const string& undoDescription);
	void CancelUndo();
	void RestoreUndo();
	bool IsUndoRecording() const;
	//////////////////////////////////////////////////////////////////////////

	//! Get prefered original size for this viewport.
	//! if 0, then no preference.
	virtual CSize GetIdealSize() const;

	//! Check if world space bounding box is visible in this view.
	virtual bool IsBoundsVisible(const AABB& box) const;

	//////////////////////////////////////////////////////////////////////////

	virtual void ToggleCameraObject()     {};
	virtual bool IsSequenceCamera() const { return false; }

	// Set`s current cursor string.
	void SetCurrentCursor(EStdCursor stdCursor, const string& cursorString);
	void SetCurrentCursor(EStdCursor stdCursor);
	void SetCursorString(const string& cursorString);
	void ResetCursor();

	//////////////////////////////////////////////////////////////////////////
	// Drag and drop support on viewports.
	// To be overrided in derived classes.
	//////////////////////////////////////////////////////////////////////////
	virtual bool       CanDrop(CPoint point, IDataBaseItem* pItem) { return false; };
	virtual void       Drop(CPoint point, IDataBaseItem* pItem)    {};

	//! Return this viewport's edit tool otherwise returns the global edit tool
	virtual CEditTool* GetEditTool();

	//! Only returns the edit tool if set in this viewport
	virtual CEditTool* GetLocalEditTool();

	//! Assign an edit tool to viewport
	virtual void       SetLocalEditTool(CEditTool* pEditTool);

	//////////////////////////////////////////////////////////////////////////
	// Return visble objects cache.
	CBaseObjectsCache* GetVisibleObjectsCache() { return m_pVisibleObjectsCache; };

	void               RegisterRenderListener(IRenderListener* piListener);
	bool               UnregisterRenderListener(IRenderListener* piListener);
	bool               IsRenderListenerRegistered(IRenderListener* piListener);

	void               AddPostRenderer(IPostRenderer* pPostRenderer);
	bool               RemovePostRenderer(IPostRenderer* pPostRenderer);

	virtual void       SerializeDisplayOptions(Serialization::IArchive& ar) {}

	//////////////////////////////////////////////////////////////////////////
	// MFC Window Emulation
	bool         IsFocused() const              { return m_bFocused; };
	void         SetFocus(bool bFocused = true) { m_bFocused = bFocused; }
	void         SetActiveWindow()              {}
	HWND         GetSafeHwnd() const;
	CWnd*        GetCWnd() const;
	void         ClientToScreen(CPoint* pnt);
	bool         IsWindowVisible() const;
	void         GetClientRect(RECT* rect) const;
	//! Check if point is inside window. Point may be expressed in local widget coordinates
	bool         IsMouseInWindow(CPoint* pnt, bool bIsLocal = true);
	virtual void Invalidate(bool bInvalid = true) {};
	//////////////////////////////////////////////////////////////////////////

	// Access to QWidget.
	virtual void SetViewWidget(QWidget* view);
	QWidget*     GetViewWidget() const { return m_viewWidget; }

	// called to process mouse callback inside the viewport.
	virtual bool MouseCallback(EMouseEvent event, CPoint& point, int flags);
	// Process key events in viewport
	virtual bool OnKeyDown(uint32 nChar, uint32 nRepCnt, uint32 nFlags);
	virtual bool OnKeyUp(uint32 nChar, uint32 nRepCnt, uint32 nFlags);

	virtual bool DragEvent(EDragEvent eventId, QEvent* event, int flags);

	// Override if viewport resize event needs to be handled
	virtual void OnResize() {};
	// Override if viewport paint event needs to be handled
	virtual void OnPaint()  {};

	// Get camera menu name for header
	virtual const char* GetCameraMenuName() const
	{
		return m_name.c_str();
	};

	// Support for 3dconnexion mouse
	virtual void OnCameraTransformEvent(CameraTransformEvent* msg) {};
	
	// Allows for input devices to be modified prior to input event
	virtual void OnFilterCryInputEvent(CryInputEvent* evt) {};

protected:
	friend class CViewManager;

	// Called to create viewport rendering context.
	virtual bool CreateRenderContext(HWND hCtx) { return true; };

	bool         IsVectorInValidRange(const Vec3& v) const { return fabs(v.x) < 1e+8 && fabs(v.y) < 1e+8 && fabs(v.z) < 1e+8; }
	void         AssignConstructionPlane(const Vec3& p1, const Vec3& p2, const Vec3& p3);

	void         ProcessRenderListeners(DisplayContext& rstDisplayContext);
	bool         MoreMouseEventProcessNeeded(EMouseEvent event);

protected:
	string        m_name;

	float         m_selectionTollerance;

	mutable float m_fZoomFactor;

	int           m_nCurViewportID;

	ViewMode      m_eViewMode;

	//! Current selected rectangle.
	CRect m_selectedRect;

	int   m_activeAxis;

	// Viewport matrix.
	Matrix34 m_viewTM;

	//! Current construction plane.
	Plane m_constructionPlane;
	Vec3  m_constructionPlaneAxisX;
	Vec3  m_constructionPlaneAxisY;

	// When true selection helpers will be shown and hit tested against.
	bool m_bAdvancedSelectMode;

	bool m_bFocused;

	//////////////////////////////////////////////////////////////////////////
	// Standart cursors.
	//////////////////////////////////////////////////////////////////////////
	QCursor  m_hCursor[STD_CURSOR_LAST];

	//! Mouse is over this object.
	CBaseObject* m_pMouseOverObject;
	string      m_cursorStr;

	static bool  m_bDegradateQuality;

	// Grid size modifier due to zoom.
	float              m_fGridZoom;

	int                m_nLastUpdateFrame;
	int                m_nLastMouseMoveFrame;

	CBaseObjectsCache* m_pVisibleObjectsCache;

	// Same construction matrix is shared by all viewports.
	Matrix34                      m_snappingMatrix;

	_smart_ptr<CEditTool>         m_pLocalEditTool;

	std::vector<IRenderListener*> m_cRenderListeners;

	typedef std::vector<_smart_ptr<IPostRenderer>> PostRenderers;
	PostRenderers m_postRenderers;

	// stores if mouse is inside the viewport
	bool             m_bMouseInside;

	QWidget*         m_viewWidget;
};

