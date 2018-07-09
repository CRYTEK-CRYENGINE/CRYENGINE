// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include <CryMath/Cry_Camera.h>
#include "Viewport.h"
#include "Objects\DisplayContext.h"
#include "IUndoManager.h"
#include <EditorFramework/Preferences.h>
#include <CryExtension/CryGUID.h>

// forward declarations.
class CBaseObject;
class CPopupMenuItem;
struct IPhysicalEntity;
typedef IPhysicalEntity* PIPhysicalEntity;
struct ICameraDelegate;
struct ray_hit;
struct IRenderMesh;

struct SCameraPreferences : public SPreferencePage
{
	SCameraPreferences()
		: SPreferencePage("General", "Viewport/Movement")
		, terrainCollisionEnabled(false)
		, objectCollisionEnabled(false)
		, speedHeightRelativeEnabled(false)
	{

	}

	virtual bool Serialize(yasli::Archive& ar) override
	{
		ar.openBlock("general", "General");
		ar(speedHeightRelativeEnabled, "speedHeightRelativeEnabled", "Speed Height Relative");
		ar.closeBlock();

		ar.openBlock("general", "Collisions");
		ar(terrainCollisionEnabled, "terrainCollisionEnabled", "Terrain Collision");
		ar(objectCollisionEnabled, "objectCollisionEnabled", "Object Collision");
		ar.closeBlock();

		return true;
	}

	bool terrainCollisionEnabled;
	bool objectCollisionEnabled;
	bool speedHeightRelativeEnabled;
};

//! More concrete viewport class. This used to be This used to be the level editor viewport
//! however it was moved to EditorCommon as ModelViewport inherits from this and needs to be exposed.
//! In the future generic parts of this should go in CViewport and specific in CLevelEditorViewport and CModelViewport
//! in order to only have one generic viewport from which all others are derived
//! This still contains non-generic functionality, please be careful to not add to it
class EDITOR_COMMON_API CRenderViewport : public CViewport, public IEditorNotifyListener, public IUndoManagerListener
{
public:
	struct SResolution
	{
		SResolution() :
			width(0), height(0)
		{
		}

		SResolution(int w, int h) :
			width(w), height(h)
		{
		}

		int width;
		int height;
	};

public:
	CRenderViewport();

	/** Get type of this viewport.
	 */
	virtual EViewportType GetType() const             { return ET_ViewportCamera; }
	virtual void          SetType(EViewportType type) { assert(type == ET_ViewportCamera); };

	virtual bool          IsRenderViewport()          { return true; };

	// Implementation
public:
	virtual ~CRenderViewport();
	void           InitCommon();

	void           OnPaintSafe();
	virtual void   Update();

	virtual void   ResetContent();
	virtual void   UpdateContent(int flags);

	void           SetCamera(const CCamera& camera);
	const CCamera& GetCamera() const { return m_Camera; };
	virtual void   SetViewTM(const Matrix34& tm)
	{ SetViewTM(tm, false); }

	//! Map world space position to viewport position.
	virtual POINT WorldToView(const Vec3& wp) const;
	virtual Vec3  WorldToView3D(const Vec3& wp, int nFlags = 0) const;

	//! Map viewport position to world space position.
	virtual void        ViewToWorldRay(POINT vp, Vec3& raySrc, Vec3& rayDir) const;
	virtual Vec3        ViewToAxisConstraint(POINT& point, Vec3 axis, Vec3 origin) const;
	virtual Vec3        ViewDirection() const;
	virtual Vec3        UpViewDirection() const;
	virtual Vec3        CameraToWorld(Vec3 worldPoint) const;

	virtual float       GetScreenScaleFactor(const Vec3& worldPoint) const;
	virtual float       GetAspectRatio() const;
	virtual bool        HitTest(CPoint point, HitContext& hitInfo);
	virtual bool        IsBoundsVisible(const AABB& box) const;

	virtual void            SetResolution(int x, int y) override;
	virtual void            GetResolution(int& x, int& y) override;
	virtual void            SetResolutionMode(EResolutionMode mode) override;
	virtual EResolutionMode GetResolutionMode() override { return m_eResolutionMode; }

	CCrySignal<void()> signalResolutionChanged;

	virtual bool        IsSequenceCamera() const            { return m_pCameraDelegate != nullptr; }

	void                SetDefaultCamera();
	bool                IsDefaultCamera() const;

	void                SetCameraObject(CBaseObject* cameraObject);
	void                SetCameraDelegate(const ICameraDelegate* pDelegate);

	virtual float       GetCameraMoveSpeed() const           { return m_moveSpeed; }
	virtual float       GetCameraMoveSpeedIncrements() const { return m_moveSpeedIncrements; }
	virtual void        SetCameraMoveSpeedIncrements(int sp, bool bnotify = false);
	virtual void		OnCameraSpeedChanged() {}

	virtual const char* GetCameraMenuName() const override;

	void                  LockCameraMovement(bool bLock) { m_bLockCameraMovement = bLock;  }
	bool                  IsCameraMovementLocked() const { return m_bLockCameraMovement; }

	const DisplayContext& GetDisplayContext() const      { return m_displayContext;  }
	CBaseObject*          GetCameraObject() const;

	static SCameraPreferences s_cameraPreferences;
	CCamera m_Camera;

	virtual void  OnResize();
	virtual void  OnPaint();
	virtual void  OnFilterCryInputEvent(CryInputEvent* evt) override;
	virtual void  OnCameraTransformEvent(CameraTransformEvent* msg) override;


protected:
	struct SScopedCurrentContext;
	struct SPreviousContext
	{

	};
	// From CViewport
	virtual bool  MouseCallback(EMouseEvent event, CPoint& point, int flags);

	void          SetViewTM(const Matrix34& tm, bool bMoveOnly);

	virtual void  SetViewFocus() {}

	virtual float GetCameraRotateSpeed() const;

	// Called to render stuff.
	virtual void OnRender() = 0;

	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event) override;

	//! Get currently active camera object.
	void             ToggleCameraObject();

	void             RenderSnapMarker();
	void             RenderCursorString();
	void             ProcessMouse();
	void             ProcessKeys();

	void             DrawAxis();
	void             DrawBackground();
	void             InitDisplayContext(SDisplayContextKey displayContextKey);
	void             Accelerate(const int acceleration);
	float            GetCameraSpeedScale() const;

	SPreviousContext SetCurrentContext();
	void             RestorePreviousContext(const SPreviousContext& x) const;

	// Update the safe frame, safe action, safe title, and borders rectangles based on
	// viewport size and target aspect ratio.
	void UpdateSafeFrame();

	// Draw safe frame, safe action, safe title rectangles and borders.
	void RenderSafeFrame();

	// Draw one of the safe frame rectangles with the desired color.
	void RenderSafeFrame(const CRect& frame, float r, float g, float b, float a);

	// Draw the selection rectangle.
	void RenderSelectionRectangle();

	// Draw a selected region if it has been selected
	void         RenderSelectedRegion();

	virtual bool CreateRenderContext(HWND hWnd, IRenderer::EViewportType viewportType = IRenderer::EViewportType::eViewportType_Secondary);
	virtual void DestroyRenderContext();

	void         OnMenuCommandChangeAspectRatio(unsigned int commandId);

	bool         AdjustObjectPosition(const ray_hit& hit, Vec3& outNormal, Vec3& outPos) const;
	bool         RayRenderMeshIntersection(IRenderMesh* pRenderMesh, const Vec3& vInPos, const Vec3& vInDir, Vec3& vOutPos, Vec3& vOutNormal) const;

	void         ResizeView(int width, int height);

	void         HideCursor();
	void         ShowCursor();

protected:
	//! Assigned renderer.
	IRenderer* m_renderer;
	I3DEngine* m_engine;
	bool       m_bRenderContextCreated;

	// TODO: wrap all those in camera interaction classes...
	bool       m_bInRotateMode;
	bool       m_bInMoveMode;
	bool       m_bInOrbitMode;
	Matrix34   m_orbitInitViewMatrix;
	bool       m_bInZoomMode;

	CPoint     m_totalMouseMove;
	CPoint     m_mousePos;
	CPoint     m_lastMousePos;

	// speed is the actual moving speed, calculated from speed increments below with formula: 0.1f + pow (1.1f, m_moveSpeedIncrements)
	float m_moveSpeed;
	// this is increments of log scale for the above speed. Useful for UI and to make sure we
	// minimize floating point errors when increasing/decreasing speed.
	int   m_moveSpeedIncrements;

	Vec3  m_orbitTarget;

	bool  m_bAttachCameraToSelected;
	bool  m_bCaptureAttachOffset;
	bool  m_bFakeMouseMove;
	bool  m_hadLastOrbitTarget;

	//-------------------------------------------
	// Render options.
	bool m_bDisplayLabels;
	bool m_bShowSafeFrame;
	bool m_bRenderStats;

	// Index of camera objects.
	mutable CryGUID        m_cameraObjectId;
	bool                   m_bLockCameraMovement;
	bool                   m_bUpdateViewport;
	const ICameraDelegate* m_pCameraDelegate;

	// enum that describes the state of the camera motion.
	enum class ECameraMoveState
	{
		// Camera does not move right now
		NoMove,
		// Camera is moving and undo has not been pushed
		MovingWithoutUndoPushed,
		// Camera is moving and undo has been pushed
		MovingWithUndoPushed
	};

	ECameraMoveState          m_eCameraMoveState;

	Matrix34                  m_prevViewTM;
	string                    m_prevViewName;

	DisplayContext            m_displayContext;

	mutable PIPhysicalEntity* m_pSkipEnts;
	mutable int               m_numSkipEnts;

	bool                      m_isOnPaint;
	static CRenderViewport*   m_pPrimaryViewport;

	static void*              m_currentContextWnd;

	CRect                     m_safeFrame;
	CRect                     m_safeAction;
	CRect                     m_safeTitle;

	bool                      m_bCursorHidden;

	//TODO: We would like all viewports to render whether or not a level is loaded but this has to be further tested.
	bool					  m_bCanDrawWithoutLevelLoaded;

protected:

	virtual void OnResizeInternal(int width, int height);
	virtual void BeginUndoTransaction() override;
	virtual void EndUndoTransaction() override;

	SResolution   m_currentResolution;
	mutable float m_lastCameraSpeedScale;
	EResolutionMode m_eResolutionMode;
	// stores the last custom resolution mode so we can restore it when selecting another custom resolution
	EResolutionMode m_eLastCustomResolutionMode;

	// pending resize to gracefully handle viewport resize
	bool m_bSuspendResizeNotifications;
	bool m_bPendingResizeNotification;
	int  m_pendingResizeWidth;
	int  m_pendingResizeHeight;

	SDisplayContextKey            m_displayContextKey;
};


//////////////////////////////////////////////////////////////////////////

struct CRenderViewport::SScopedCurrentContext
{
	CRenderViewport*  viewport;
	CRenderViewport::SPreviousContext previousContext;

	SScopedCurrentContext(CRenderViewport* viewport)
		: viewport(viewport)
	{
		previousContext = viewport->SetCurrentContext();
	}

	~SScopedCurrentContext()
	{
		viewport->RestorePreviousContext(previousContext);
	}
};

