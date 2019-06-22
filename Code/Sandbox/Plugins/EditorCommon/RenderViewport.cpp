// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RenderViewport.h"

#include "Controls/DynamicPopupMenu.h"
#include "EditorFramework/Events.h"
#include "Gizmos/IGizmoManager.h"
#include "LevelEditor/Tools/EditTool.h"
#include "Objects/BaseObject.h"
#include "Preferences/SnappingPreferences.h"
#include "Preferences/ViewportPreferences.h"
#include "ILevelEditor.h"
#include "IObjectManager.h"
#include "IUndoManager.h"
#include "IViewportManager.h"
#include "QtUtil.h"
#include "RenderLock.h"
#include "ViewportInteraction.h"

#include <IEditor.h>

#include <Cry3DEngine/I3DEngine.h>
#include <CryAISystem/IAISystem.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryInput/IHardwareMouse.h>
#include <CryInput/IInput.h>
#include <CryMath/Cry_GeoIntersect.h>
#include <CryMath/Cry_Math.h>
#include <CryPhysics/IPhysics.h>
#include <CryPhysics/IPhysicsDebugRenderer.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySystem/IConsole.h>
#include <CrySystem/ITimer.h>

void* CRenderViewport::m_currentContextWnd = nullptr;
CRenderViewport* CRenderViewport::m_pPrimaryViewport = nullptr;

namespace
{
const char renderViewportTitleName[] = "Perspective";
const float g_renderMeshTestDistance = 0.2f;
}

SCameraPreferences CRenderViewport::s_cameraPreferences;
REGISTER_PREFERENCES_PAGE_PTR(SCameraPreferences, &CRenderViewport::s_cameraPreferences);

YASLI_ENUM_BEGIN_NESTED(SCameraPreferences, EViewportBehavior, "ViewportBehaviour")
YASLI_ENUM(SCameraPreferences::eViewport_Push, "push", "Push Content");
YASLI_ENUM(SCameraPreferences::eViewport_Drag, "drag", "Drag Content");
YASLI_ENUM_END()

bool SCameraPreferences::Serialize(yasli::Archive& ar)
{
	ar.openBlock("general", "General");
	ar(viewportPanningStyle, "viewportPanningStyle", "Viewport Panning Style");
	ar(speedHeightRelativeEnabled, "speedHeightRelativeEnabled", "Speed Height Relative");
	ar.closeBlock();

	ar.openBlock("collisions", "Collisions");
	ar(terrainCollisionEnabled, "terrainCollisionEnabled", "Terrain Collision");
	ar(objectCollisionEnabled, "objectCollisionEnabled", "Object Collision");
	ar.closeBlock();

	return true;
}

bool SCameraPreferences::IsTerrainCollisionEnabled() const
{
	return terrainCollisionEnabled;
}

bool SCameraPreferences::IsObjectCollisionEnabled() const
{
	return objectCollisionEnabled;
}

bool SCameraPreferences::IsSpeedHeightRelativeEnabled() const
{
	return speedHeightRelativeEnabled;
}

SCameraPreferences::EViewportBehavior SCameraPreferences::GetViewportPanningStyle() const
{
	return viewportPanningStyle;
}

void SCameraPreferences::SetTerrainCollision(bool enabled)
{
	terrainCollisionEnabled = enabled;
	GetIEditor()->GetPreferences()->Save();
}

void SCameraPreferences::SetObjectCollision(bool enabled)
{
	objectCollisionEnabled = enabled;
	GetIEditor()->GetPreferences()->Save();
}

void SCameraPreferences::SetSpeedHeightRelative(bool enabled)
{
	speedHeightRelativeEnabled = enabled;
	GetIEditor()->GetPreferences()->Save();
}

void SCameraPreferences::SetViewportPanningStyle(EViewportBehavior behavior)
{
	viewportPanningStyle = behavior;
}

//////////////////////////////////////////////////////////////////////////
// CRenderViewport
//////////////////////////////////////////////////////////////////////////
CRenderViewport::CRenderViewport() : m_pCameraDelegate(nullptr)
{
	m_pSkipEnts = 0;
	m_numSkipEnts = 0;

	m_bRenderStats = true;
	m_bUpdateViewport = false;

	m_pSkipEnts = new PIPhysicalEntity[1024];

	m_isOnPaint = false;

	m_hadLastOrbitTarget = false;

	m_bFakeMouseMove = false;
	m_bCanDrawWithoutLevelLoaded = false;

	InitCommon();

	m_prevViewTM.SetIdentity();

	if (GetIEditor()->GetViewportManager()->GetSelectedViewport() == NULL)
		GetIEditor()->GetViewportManager()->SelectViewport(this);

	GetIEditor()->RegisterNotifyListener(this);

	GetIEditor()->GetIUndoManager()->AddListener(this);

	m_currentResolution.width = m_currentResolution.height = 0;
	m_eResolutionMode = EResolutionMode::Window;
	m_eLastCustomResolutionMode = EResolutionMode::Stretch;

	SetName(renderViewportTitleName);

	m_bSuspendResizeNotifications = false;
	m_bPendingResizeNotification = false;
	m_lastCameraSpeedScale = 1.0f;
}

void CRenderViewport::OnFilterCryInputEvent(CryInputEvent* evt)
{
	SInputEvent* pInput = evt->GetInputEvent();

	if (pInput->deviceType == eIDT_EyeTracker)
	{
		CPoint point;
		if (pInput->keyId == eKI_EyeTracker_X)
		{
			point.x = pInput->value;
			ScreenToClient(&point);
			pInput->value = point.x / (float)m_viewWidget->width();
			evt->setAccepted(true);
		}
		if (pInput->keyId == eKI_EyeTracker_Y)
		{
			point.y = pInput->value;
			ScreenToClient(&point);
			pInput->value = point.y / (float)m_viewWidget->height();
			evt->setAccepted(true);
		}
	}
}

CRenderViewport::~CRenderViewport()
{
	DestroyRenderContext();
	GetIEditor()->GetIUndoManager()->RemoveListener(this);
	GetIEditor()->UnregisterNotifyListener(this);
	delete[] m_pSkipEnts;
}

void CRenderViewport::InitCommon()
{
	m_Camera = GetIEditor()->GetSystem()->GetViewCamera();
	SetViewTM(m_Camera.GetMatrix());

	m_bRenderContextCreated = false;

	m_bInRotateMode = false;
	m_bInMoveMode = false;
	m_bInOrbitMode = false;
	m_bInZoomMode = false;
	m_bAttachCameraToSelected = false;
	m_bCaptureAttachOffset = false;

	m_mousePos.SetPoint(0, 0);

	m_cameraObjectId = CryGUID::Null();

	m_moveSpeed = 0.1f;
	m_moveSpeedIncrements = log((m_moveSpeed - 0.01f) * 100) / log(1.1) + 0.5;

	m_bLockCameraMovement = true;

	m_bCursorHidden = false;

	m_eCameraMoveState = ECameraMoveState::NoMove;
	m_prevViewTM.SetIdentity();

	m_renderer = GetIEditor()->GetRenderer();
	m_engine = GetIEditor()->Get3DEngine();
}

static int OnPaintFilter(unsigned int code, struct _EXCEPTION_POINTERS* ep)
{
	// Win32 handles first-chance exceptions generated during the execution of WM_PAINT.
	// Since the IDE debugger usually intercepts only second-change exceptions, this error could be overlooked.
	OutputDebugStringA("\nException was triggered during WM_PAINT.\n");

	if (IsDebuggerPresent())
	{
		// In order to make sure that the IDE breaks whenever the exception is generated, please do the following steps:
		// 1. Open the 'Exceptions' dialog by going to 'Debug->Exceptions...'
		// 2. Make sure that the 'Thrown' check box next to 'Win32 Exceptions' is checked.

		// In order to return the default settings, press the 'Reset All' button in the 'Exceptions' dialog.

		DebugBreak();
	}

	return EXCEPTION_CONTINUE_SEARCH;
}

void CRenderViewport::OnPaintSafe()
{
	__try
	{
		static bool bUpdating = false;
		if (!bUpdating)
		{
			bUpdating = true;
			Update();
			bUpdating = false;
		}
	}
	__except (OnPaintFilter(GetExceptionCode(), GetExceptionInformation()))
	{
	}
}

void CRenderViewport::ProcessMouse()
{
	CPoint point;
	GetCursorPos(&point);
	ScreenToClient(&point);

	if (GetIEditor()->IsInGameMode())
	{
		if (gEnv->pHardwareMouse && m_lastMousePos != point)
		{
			// Ensure, viewport is set correctly, even after switching widgets while in game mode
			if (GetIEditor()->GetViewportManager()->GetSelectedViewport() != this)
			{
				GetIEditor()->GetViewportManager()->SelectViewport(this);
			}

			gEnv->pHardwareMouse->Event(point.x, point.y, HARDWAREMOUSEEVENT_MOVE);
		}
		m_lastMousePos = point;

		return;
	}
	m_lastMousePos = point;

	if (point == m_mousePos)
	{
		// still need to push that fake event
		if (m_bFakeMouseMove && !m_bCursorHidden && IsMouseInWindow(&point))
		{
			CViewport::MouseCallback(eMouseMove, point, 0);
			m_bFakeMouseMove = false;
		}
		return;
	}

	float speedScale = gViewportMovementPreferences.camMoveSpeed * m_moveSpeed;

	if (QtUtil::IsModifierKeyDown(Qt::ControlModifier))
	{
		speedScale *= gViewportMovementPreferences.camFastMoveSpeed;
	}

	speedScale *= GetCameraSpeedScale();

	//TODO : This may have broken player movement in mannequin but commenting it out for refactoring
	/*if (m_PlayerControl)
	   {
	   if (m_bInRotateMode)
	   {
	    f32 MousedeltaX = (m_mousePos.x - point.x);
	    f32 MousedeltaY = (m_mousePos.y - point.y);
	    m_relCameraRotZ += MousedeltaX;
	    m_relCameraRotX += MousedeltaY;
	    CPoint pnt = m_mousePos;
	    ClientToScreen(&pnt);
	    SetCursorPos(pnt.x, pnt.y);

	    if (m_bAttachCameraToSelected) m_bCaptureAttachOffset = true;
	   }
	   }
	   else*/if ((m_bInRotateMode && m_bInMoveMode) || m_bInZoomMode)
	{
		if (m_eCameraMoveState == ECameraMoveState::NoMove)
			m_eCameraMoveState = ECameraMoveState::MovingWithoutUndoPushed;

		// Zoom.
		Matrix34 m = GetViewTM();
		Vec3 xdir(0, 0, 0);

		Vec3 ydir = m.GetColumn1().GetNormalized();
		Vec3 pos = m.GetTranslation();

		const float posDelta = 0.2f * (m_mousePos.y - point.y) * speedScale;
		pos = pos - ydir * posDelta;

		m.SetTranslation(pos);
		SetViewTM(m);

		CPoint pnt = m_mousePos;
		ClientToScreen(&pnt);
		SetCursorPos(pnt.x, pnt.y);

		if (m_bAttachCameraToSelected) m_bCaptureAttachOffset = true;
	}
	else if (m_bInRotateMode)
	{
		if (m_eCameraMoveState == ECameraMoveState::NoMove)
			m_eCameraMoveState = ECameraMoveState::MovingWithoutUndoPushed;

		Ang3 angles(-point.y + m_mousePos.y, 0, -point.x + m_mousePos.x);
		angles = angles * 0.002f * GetCameraRotateSpeed();

		Matrix34 camtm = GetViewTM();
		Ang3 ypr = CCamera::CreateAnglesYPR(Matrix33(camtm));
		ypr.x += angles.z;
		ypr.y += angles.x;

		ypr.y = CLAMP(ypr.y, -1.5f, 1.5f);    // to keep rotation in reasonable range
		// In the recording mode of a custom camera, the z rotation is allowed.
		ypr.z = 0;    // to have camera always upward

		camtm = Matrix34(CCamera::CreateOrientationYPR(ypr), camtm.GetTranslation());
		SetViewTM(camtm);

		CPoint pnt = m_mousePos;
		ClientToScreen(&pnt);
		SetCursorPos(pnt.x, pnt.y);

		if (m_bAttachCameraToSelected) m_bCaptureAttachOffset = true;
	}
	else if (m_bInMoveMode)
	{
		if (m_eCameraMoveState == ECameraMoveState::NoMove)
			m_eCameraMoveState = ECameraMoveState::MovingWithoutUndoPushed;

		// Slide.
		Matrix34 m = GetViewTM();
		Vec3 xdir = m.GetColumn0().GetNormalized();
		Vec3 zdir = m.GetColumn2().GetNormalized();

		if (s_cameraPreferences.GetViewportPanningStyle() == SCameraPreferences::eViewport_Drag)
		{
			xdir.Flip();
			zdir.Flip();
		}

		Vec3 pos = m.GetTranslation();
		pos += 0.1f * speedScale * (xdir * (point.x - m_mousePos.x) + zdir * (m_mousePos.y - point.y));
		m.SetTranslation(pos);
		SetViewTM(m, true);

		CPoint pnt = m_mousePos;
		ClientToScreen(&pnt);
		SetCursorPos(pnt.x, pnt.y);

		if (m_bAttachCameraToSelected) m_bCaptureAttachOffset = true;
	}
	else if (m_bInOrbitMode)
	{
		if (m_eCameraMoveState == ECameraMoveState::NoMove)
			m_eCameraMoveState = ECameraMoveState::MovingWithoutUndoPushed;

		m_totalMouseMove += point - m_mousePos;
		Ang3 angles(-m_totalMouseMove.y, 0, -m_totalMouseMove.x);
		angles = angles * 0.002f * GetCameraRotateSpeed();

		Vec3 initPos = m_orbitInitViewMatrix.GetTranslation();
		Vec3 vInitWorldDist = initPos - m_orbitTarget;
		Vec3 vInitViewProjection = Matrix33(m_orbitInitViewMatrix).GetTransposed() * vInitWorldDist;
		Vec3 viewDir = m_orbitInitViewMatrix.GetColumn1();
		Ang3 ypr(atan2(viewDir.y, viewDir.x) - 0.5f * g_PI, asin(viewDir.z), 0.0f);
		ypr.x += angles.z;
		ypr.y += angles.x;
		ypr.y = CLAMP(ypr.y, -1.5f, 1.5f);    // to keep rotation in reasonable range

		Matrix33 rotateTM = CCamera::CreateOrientationYPR(ypr);

		// Calc new source.
		Vec3 dst = m_orbitTarget + rotateTM * vInitViewProjection;

		Matrix34 camTM = Matrix34(rotateTM, dst);
		SetViewTM(camTM);
		if (m_bAttachCameraToSelected) m_bCaptureAttachOffset = true;

		m_totalMouseMove += point - m_mousePos;
		CPoint pnt = m_mousePos;
		ClientToScreen(&pnt);
		SetCursorPos(pnt.x, pnt.y);
	}

	// emit a fake mouse move event if we are translating on the screen.
	if (m_bFakeMouseMove && !m_bCursorHidden && IsMouseInWindow(&point))
	{
		CViewport::MouseCallback(eMouseMove, point, 0);
		m_bFakeMouseMove = false;
	}
}

void CRenderViewport::ResetContent()
{
	CViewport::ResetContent();
}

void CRenderViewport::UpdateContent(int flags)
{
	CViewport::UpdateContent(flags);
	if (flags & eUpdateObjects)
	{
		m_bUpdateViewport = true;
	}
}

void CRenderViewport::Update()
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	if (!m_renderer || !m_engine)
		return;

	if (!IsWindowVisible())
		return;

	// Don't wait for changes to update the focused viewport.
	if (IsFocused())
		m_bUpdateViewport = true;

	// While Renderer doesn't support fast rendering of the scene to more then 1 viewport
	// render only focused viewport if more then 1 are opened and always update is off.
	if (!m_isOnPaint && GetIEditor()->GetViewportManager()->GetNumberOfGameViewports() > 1 && GetType() == ET_ViewportCamera)
	{
		if (m_pPrimaryViewport != this)
		{
			if (IsFocused()) // If this is the focused window, set primary viewport.
				m_pPrimaryViewport = this;
			else if (!m_bUpdateViewport) // Skip this viewport.
				return;
		}
	}

	if (IsFocused())
	{
		ProcessMouse();
		ProcessKeys();
	}

	if (GetIEditor()->IsInGameMode())
	{
		if (CScopedRenderLock lock = CScopedRenderLock())
		{
			gEnv->pRenderer->GetIRenderAuxGeom()->SetCurrentDisplayContext(m_displayContextKey);

			if (m_renderer)
			{
				CRect rcClient;
				GetClientRect(&rcClient);

				// Why 16? Internal effects can use half or quarter renderbuffer size so this ensures some leeway in resolution (see CE-8648)
				if (rcClient.right > 16 && rcClient.bottom > 16)
				{
					CCamera ccam = gEnv->pSystem->GetViewCamera();
					ccam.SetFrustum(m_currentResolution.width, m_currentResolution.height, ccam.GetFov(), ccam.GetNearPlane(), ccam.GetFarPlane(), ccam.GetPixelAspectRatio());
					gEnv->pSystem->SetViewCamera(ccam);
				}
			}

			CViewport::Update();
		}
		return;
	}

	if (CScopedRenderLock lock = CScopedRenderLock())
	{
		m_viewTM = m_Camera.GetMatrix(); // synchronize.

		// Render
		if (!m_bRenderContextCreated)
		{
			CRY_HWND hWnd = GetSafeHwnd();
			if (!CreateRenderContext(hWnd))
				return;
		}

		// Why 16? Internal effects can use half or quarter render buffer size so this ensures some leeway in resolution (see CE-8648)
		CRect rcClient;
		GetClientRect(&rcClient);
		if (rcClient.right < 16 || rcClient.bottom < 16)
			return;

		m_currentContextWnd = 0;
		SScopedCurrentContext context(this);

		// Configures Aux to draw to the current display-context
		SDisplayContext displayContext = InitDisplayContext(m_displayContextKey);

		// 3D engine stats
		GetIEditor()->GetSystem()->RenderBegin(m_displayContextKey, m_graphicsPipelineKey);

		bool bRenderStats = m_bRenderStats;

		auto le = GetIEditor()->GetLevelEditor();
		if (!m_bCanDrawWithoutLevelLoaded && (!le || !le->IsLevelLoaded() || !GetIEditor()->IsDocumentReady()))
		{
			bRenderStats = false;
			DrawBackground(displayContext);
		}
		else
		{
			OnRender(displayContext);
		}

		ProcessRenderListeners(displayContext);

		displayContext.Flush2D();

		// draw gizmos on top of 2d icons.
		GetIEditor()->GetGizmoManager()->Display(displayContext);

		CCamera CurCamera = gEnv->pSystem->GetViewCamera();
		gEnv->pSystem->SetViewCamera(m_Camera);

		// Post Render Callback
		{
			PostRenderers::iterator itr = m_postRenderers.begin();
			PostRenderers::iterator end = m_postRenderers.end();
			for (; itr != end; ++itr)
			{
				(*itr)->OnPostRender();
			}
		}

		GetIEditor()->GetSystem()->RenderEnd(bRenderStats);
		gEnv->pSystem->SetViewCamera(CurCamera);

		CViewport::Update();
		m_bUpdateViewport = false;
	}
}

void CRenderViewport::SetCameraObject(CBaseObject* cameraObject)
{
	m_pCameraDelegate = NULL;

	if (cameraObject)
	{
		if (m_cameraObjectId == CryGUID::Null())
		{
			m_prevViewTM = GetViewTM();
		}

		m_cameraObjectId = cameraObject->GetId();
		SetName((const char*)cameraObject->GetName());
		GetIEditor()->GetViewportManager()->SetCameraObjectId(m_cameraObjectId);
	}
	else
	{
		bool bHadCamObj = (m_cameraObjectId != CryGUID::Null());

		// Switch to normal view.
		m_cameraObjectId = CryGUID::Null();

		GetIEditor()->GetViewportManager()->SetCameraObjectId(m_cameraObjectId);
		if (bHadCamObj)
		{
			SetViewTM(m_prevViewTM);
		}

		SetName(renderViewportTitleName);
	}

	GetIEditor()->Notify(eNotify_CameraChanged);
}

void CRenderViewport::SetCameraDelegate(const ICameraDelegate* pDelegate)
{
	SetCameraObject(NULL);
	m_pCameraDelegate = pDelegate;

	if (pDelegate)
	{
		SetName(pDelegate->GetName());
	}

	GetIEditor()->Notify(eNotify_CameraChanged);
}

CBaseObject* CRenderViewport::GetCameraObject() const
{
	CBaseObject* pCameraObject = NULL;

	if (m_pCameraDelegate)
	{
		CryGUID delegateGUID = m_pCameraDelegate->GetActiveCameraObjectGUID();
		pCameraObject = GetIEditor()->GetObjectManager()->FindObject(delegateGUID);
	}
	else if (m_cameraObjectId != CryGUID::Null())
	{
		// Find camera object from id.
		pCameraObject = GetIEditor()->GetObjectManager()->FindObject(m_cameraObjectId);
	}

	return pCameraObject;
}

void CRenderViewport::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnBeginGameMode:
		ShowCursor();
		SetCurrentContext();
		SetCurrentCursor(STD_CURSOR_CRYSIS);
		break;

	case eNotify_OnEndGameMode:
		SetCurrentCursor(STD_CURSOR_DEFAULT);
		m_bInRotateMode = false;
		m_bInMoveMode = false;
		m_bInOrbitMode = false;
		m_bInZoomMode = false;
		m_bAttachCameraToSelected = false;
		break;

	case eNotify_OnClearLevelContents:
		SetDefaultCamera();
		break;

	case eNotify_OnBeginSceneClose:
		m_hadLastOrbitTarget = false;
		break;

	case eNotify_OnBeginNewScene:
		CScopedRenderLock::Lock();
		break;

	case eNotify_OnEndNewScene:
		CScopedRenderLock::Unlock();
		break;

	case eNotify_OnBeginLayerExport:
	case eNotify_OnBeginSceneSave:
		CScopedRenderLock::Lock();
		break;
	case eNotify_OnEndLayerExport:
	case eNotify_OnEndSceneSave:
		CScopedRenderLock::Unlock();
		break;

	case eNotify_OnBeginLayoutResize:
		m_bSuspendResizeNotifications = true;
		break;
	case eNotify_OnEndLayoutResize:
		if (m_bPendingResizeNotification)
		{
			OnResizeInternal(m_pendingResizeWidth, m_pendingResizeHeight);
			m_bPendingResizeNotification = false;
		}
		m_bSuspendResizeNotifications = false;
		break;
	}
}

void CRenderViewport::RenderSelectionRectangle(SDisplayContext& context)
{
	if (m_selectedRect.IsRectEmpty())
	{
		return;
	}

	CPoint topLeft(m_selectedRect.left, m_selectedRect.top);
	CPoint bottomRight(m_selectedRect.right, m_selectedRect.bottom);

	context.SetColor(1, 1, 1, 0.4f);
	context.DrawWireQuad2d(topLeft, bottomRight, 1, true, false);
}

SDisplayContext CRenderViewport::InitDisplayContext(const SDisplayContextKey& displayContextKey)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	SDisplayContext dctx;
	dctx.SetDisplayContext(displayContextKey, IRenderer::eViewportType_Default);
	dctx.SetView(this);
	dctx.SetCamera(&m_Camera);
	dctx.renderer = m_renderer;
	dctx.engine = m_engine;
	dctx.box.min = Vec3(-100000, -100000, -100000);
	dctx.box.max = Vec3(100000, 100000, 100000);
	dctx.pIconManager = GetIEditor()->GetIconManager();

	if (m_helperSettings.enabled)
	{
		dctx.enabled = m_helperSettings.enabled;

		dctx.showIcons = m_helperSettings.showIcons;
		dctx.showMesh = m_helperSettings.showMesh;
		dctx.showAnimationTracks = m_helperSettings.showAnimationTracks;
		dctx.showBoundingBoxes = m_helperSettings.showBoundingBoxes;
		dctx.showComponentHelpers = m_helperSettings.showComponentHelpers;
		dctx.showDimensions = m_helperSettings.showDimensions;
		dctx.fillSelectedShapes = m_helperSettings.fillSelectedShapes;
		dctx.showFrozenObjectsHelpers = m_helperSettings.showFrozenObjectsHelpers;
		dctx.showTextLabels = m_helperSettings.showTextLabels;
		dctx.showEntityObjectsTextLabels = m_helperSettings.showEntityObjectsTextLabels;
		dctx.showLinks = m_helperSettings.showLinks;
		dctx.showMeshStatsOnMouseOver = m_helperSettings.showMeshStatsOnMouseOver;
		dctx.showRadii = m_helperSettings.showRadii;
		dctx.showSelectedObjectOrientation = m_helperSettings.showSelectedObjectOrientation;
		dctx.showSnappingGridGuide = m_helperSettings.showSnappingGridGuide;
		dctx.showTriggerBounds = m_helperSettings.showTriggerBounds;

		dctx.showAreaHelper = m_helperSettings.showAreaHelper;
		dctx.showBrushHelper = m_helperSettings.showBrushHelper;
		dctx.showDecalHelper = m_helperSettings.showDecalHelper;
		dctx.showEntityObjectHelper = m_helperSettings.showEntityObjectHelper;
		dctx.showEnviromentProbeHelper = m_helperSettings.showEnviromentProbeHelper;
		dctx.showGroupHelper = m_helperSettings.showGroupHelper;
		dctx.showPrefabHelper = m_helperSettings.showPrefabHelper;
		dctx.showPrefabBounds = m_helperSettings.showPrefabBounds;
		dctx.showPrefabChildrenHelpers = m_helperSettings.showPrefabChildrenHelpers;
		dctx.showRoadHelper = m_helperSettings.showRoadHelper;
		dctx.showShapeHelper = m_helperSettings.showShapeHelper;
	}

	// Selection helpers should be visible even if helpers are disabled
	dctx.displaySelectionHelpers = m_bAdvancedSelectMode;

	return dctx;
}

void CRenderViewport::DrawAxis(SDisplayContext& context)
{
	if (!m_helperSettings.enabled)
		return;

	Vec3 colX(1, 0, 0), colY(0, 1, 0), colZ(0, 0, 1), colW(1, 1, 1);
	Vec3 pos(50, 50, 0.1f); // Bottom-left corner

	Vec3 posInWorld(0, 0, 0);
	if (!GetCamera().Unproject(pos, posInWorld))
		return;

	float screenScale = GetScreenScaleFactor(posInWorld);
	float length = 0.03f * screenScale;
	float arrowSize = 0.02f * screenScale;
	float textSize = 1.1f;

	Vec3 x(length, 0, 0);
	Vec3 y(0, length, 0);
	Vec3 z(0, 0, length);

	int prevRState = context.GetState();

	context.DepthWriteOff();
	context.DepthTestOff();
	context.CullOff();
	context.SetLineWidth(1);
	context.SetColor(colX);
	context.DrawLine(posInWorld, posInWorld + x);
	context.DrawArrow(posInWorld + x * 0.9f, posInWorld + x, arrowSize);
	context.SetColor(colY);
	context.DrawLine(posInWorld, posInWorld + y);
	context.DrawArrow(posInWorld + y * 0.9f, posInWorld + y, arrowSize);
	context.SetColor(colZ);
	context.DrawLine(posInWorld, posInWorld + z);
	context.DrawArrow(posInWorld + z * 0.9f, posInWorld + z, arrowSize);
	context.SetColor(colW);
	context.DrawTextLabel(posInWorld + x, textSize, "x");
	context.DrawTextLabel(posInWorld + y, textSize, "y");
	context.DrawTextLabel(posInWorld + z, textSize, "z");
	context.DepthWriteOn();
	context.DepthTestOn();
	context.CullOn();
	context.SetState(prevRState);
}

void CRenderViewport::DrawBackground(SDisplayContext& context)
{
	if (!m_helperSettings.enabled)
	{
		return;
	}

	const float startX = 0.0f;
	const float endX = m_Camera.GetViewSurfaceX();
	const float startY = 0.0f;
	const float endY = m_Camera.GetViewSurfaceZ();

	CRect rcClient;
	GetClientRect(&rcClient);

	m_Camera.SetFrustum(endX, endY, m_Camera.GetFov(), 0.02f, 10000, m_Camera.GetPixelAspectRatio());
	int w = rcClient.right - rcClient.left;
	int h = rcClient.bottom - rcClient.top;
	m_Camera.SetFrustum(w, h, DEG2RAD(90), 0.0101f, 10000.0f);

	Vec3 bottomLeft(startX, startY, 0);
	Vec3 bottomRight(endX, startY, 0);
	Vec3 topLeft(startX, endY, 0);
	Vec3 topRight(endX, endY, 0);

	COLORREF topColor = ::GetSysColor(COLOR_MENU);
	COLORREF bottomColor = ::GetSysColor(COLOR_GRAYTEXT);

	ColorB secondC(GetRValue(topColor), GetGValue(topColor), GetBValue(topColor), 255.0f);
	ColorB firstC(GetRValue(bottomColor), GetGValue(bottomColor), GetBValue(bottomColor), 255.0f);

	context.SetState(e_Mode2D | e_AlphaBlended | e_FillModeSolid | e_CullModeBack | e_DepthWriteOn | e_DepthTestOn);
	context.DrawQuadGradient(bottomRight, bottomLeft, topLeft, topRight, secondC, firstC);
}

void CRenderViewport::RenderCursorString()
{
	if (m_cursorStr.IsEmpty())
		return;

	CPoint point;
	GetCursorPos(&point);
	ScreenToClient(&point);

	// Display hit object name.
	float col[4] = { 1, 1, 1, 1 };

	CRect rc;
	GetClientRect(&rc);

	point.x = point.x * m_currentResolution.width / (float)rc.Width();
	point.y = point.y * m_currentResolution.height / (float)rc.Height();

	IRenderAuxText::Draw2dLabel(point.x + 12, point.y + 4, 1.2f, col, false, "%s", (const char*)m_cursorStr);
}

void CRenderViewport::UpdateSafeFrame()
{
	CRect rcClient;
	GetClientRect(&rcClient);

	m_safeFrame = rcClient;

	if (m_safeFrame.Height() == 0)
		return;

	const bool allowSafeFrameBiggerThanViewport = false;

	float safeFrameAspectRatio = float(m_safeFrame.Width()) / m_safeFrame.Height();
	float targetAspectRatio = GetAspectRatio();
	bool viewportIsWiderThanSafeFrame = (targetAspectRatio <= safeFrameAspectRatio);
	if (viewportIsWiderThanSafeFrame || allowSafeFrameBiggerThanViewport)
	{
		float maxSafeFrameWidth = m_safeFrame.Height() * targetAspectRatio;
		float widthDifference = m_safeFrame.Width() - maxSafeFrameWidth;

		m_safeFrame.left = m_safeFrame.left + widthDifference * 0.5;
		m_safeFrame.right = m_safeFrame.right - widthDifference * 0.5;
	}
	else
	{
		float maxSafeFrameHeight = m_safeFrame.Width() / targetAspectRatio;
		float heightDifference = m_safeFrame.Height() - maxSafeFrameHeight;

		m_safeFrame.top = m_safeFrame.top + heightDifference * 0.5;
		m_safeFrame.bottom = m_safeFrame.bottom - heightDifference * 0.5;
	}

	m_safeFrame.DeflateRect(0, 0, 1, 1);   // <-- aesthetic improvement.

	const float SAFE_ACTION_SCALE_FACTOR = 0.05f;
	m_safeAction = m_safeFrame;
	m_safeAction.DeflateRect(m_safeFrame.Width() * SAFE_ACTION_SCALE_FACTOR, m_safeFrame.Height() * SAFE_ACTION_SCALE_FACTOR);

	const float SAFE_TITLE_SCALE_FACTOR = 0.1f;
	m_safeTitle = m_safeFrame;
	m_safeTitle.DeflateRect(m_safeFrame.Width() * SAFE_TITLE_SCALE_FACTOR, m_safeFrame.Height() * SAFE_TITLE_SCALE_FACTOR);
}

void CRenderViewport::RenderSafeFrame(SDisplayContext& context)
{
	RenderSafeFrame(context, m_safeFrame, 0.75f, 0.75f, 0, 0.8f);
	RenderSafeFrame(context, m_safeAction, 0, 0.85f, 0.80f, 0.8f);
	RenderSafeFrame(context, m_safeTitle, 0.80f, 0.60f, 0, 0.8f);
}

void CRenderViewport::RenderSafeFrame(SDisplayContext& context, const CRect& frame, float r, float g, float b, float a)
{
	context.SetColor(r, g, b, a);

	const int LINE_WIDTH = 2;
	for (int i = 0; i < LINE_WIDTH; i++)
	{
		Vec3 topLeft(frame.left + i, frame.top + i, 0);
		Vec3 bottomRight(frame.right - i, frame.bottom - i, 0);

		context.DrawWireBox(topLeft, bottomRight);
	}
}

float CRenderViewport::GetAspectRatio() const
{
	return gViewportPreferences.defaultAspectRatio;
}

void CRenderViewport::RenderSnapMarker(SDisplayContext& context)
{
	if (!gSnappingPreferences.markerDisplay())
		return;

	CPoint point;
	::GetCursorPos(&point);
	ScreenToClient(&point);
	Vec3 p = MapViewToCP(point);

	float fScreenScaleFactor = GetScreenScaleFactor(p);

	Vec3 x(1, 0, 0);
	Vec3 y(0, 1, 0);
	Vec3 z(0, 0, 1);
	x = x * gSnappingPreferences.markerSize() * fScreenScaleFactor * 0.1f;
	y = y * gSnappingPreferences.markerSize() * fScreenScaleFactor * 0.1f;
	z = z * gSnappingPreferences.markerSize() * fScreenScaleFactor * 0.1f;

	context.SetColor(gSnappingPreferences.markerColor());
	context.DrawLine(p - x, p + x);
	context.DrawLine(p - y, p + y);
	context.DrawLine(p - z, p + z);

	point = WorldToView(p);

	int s = 8;
	context.DrawLine2d(point + CPoint(-s, -s), point + CPoint(s, -s), 0);
	context.DrawLine2d(point + CPoint(-s, s), point + CPoint(s, s), 0);
	context.DrawLine2d(point + CPoint(-s, -s), point + CPoint(-s, s), 0);
	context.DrawLine2d(point + CPoint(s, -s), point + CPoint(s, s), 0);
}

void CRenderViewport::SetCamera(const CCamera& camera)
{
	const CCamera& ccam = gEnv->pSystem->GetViewCamera();

	m_Camera = camera;
	m_Camera.SetFrustum(m_currentResolution.width, m_currentResolution.height, ccam.GetFov(), ccam.GetNearPlane(), ccam.GetFarPlane(), ccam.GetPixelAspectRatio());
	SetViewTM(m_Camera.GetMatrix());
}

float CRenderViewport::GetCameraRotateSpeed() const
{
	return gViewportMovementPreferences.camRotateSpeed;
}

void CRenderViewport::Accelerate(const int acceleration)
{
	SetCameraMoveSpeedIncrements(m_moveSpeedIncrements + acceleration, true);
}

void CRenderViewport::SetCameraMoveSpeedIncrements(int sp, bool bnotify)
{
	// we have 9999 steps from 0.01 to 100.0
	// add 0.5 to make sure we include last number (not rounded off due to floating point errors)
	int maxsteps = log(9999) / log(1.1) + 0.5;

	m_moveSpeedIncrements = clamp_tpl(sp, 0, maxsteps);
	m_moveSpeed = pow(1.1, m_moveSpeedIncrements) / 100.0f;

	if (bnotify)
	{
		OnCameraSpeedChanged();
	}
}

const char* CRenderViewport::GetCameraMenuName() const
{
	// Do explicit instead of using GetCameraObject because we are interested in the
	// name of the delegate, not of the object
	if (m_pCameraDelegate)
	{
		return m_pCameraDelegate->GetName();
	}
	else if (m_cameraObjectId != CryGUID::Null())
	{
		// Find camera object from id.
		CBaseObject* pCamObj = GetIEditor()->GetObjectManager()->FindObject(m_cameraObjectId);

		if (pCamObj)
		{
			return pCamObj->GetName();
		}
	}

	return "Camera";
}

float CRenderViewport::GetCameraSpeedScale() const
{
	if (s_cameraPreferences.IsSpeedHeightRelativeEnabled())
	{
		// Always check terrain height, regardless of terrain collision flag
		Vec3 pos = GetViewTM().GetTranslation();
		float h = pos.z - GetIEditor()->GetTerrainElevation(pos.x, pos.y);

		if ((s_cameraPreferences.IsObjectCollisionEnabled()) && h > 1.0f)
		{
			// Check objects below camera tool
			ray_hit hit;
			if (gEnv->pPhysicalWorld->RayWorldIntersection(pos, Vec3(0, 0, -h), ent_static, rwi_stop_at_pierceable | rwi_ignore_back_faces, &hit, 1))
				h = hit.dist;
		}
		const float maxAcceleration = 30.0f;
		h = clamp_tpl(h, 1.0f, m_lastCameraSpeedScale + maxAcceleration * gEnv->pTimer->GetRealFrameTime());
		return m_lastCameraSpeedScale = h;
	}
	return 1.0f;
}

void CRenderViewport::SetViewTM(const Matrix34& viewTM, bool bMoveOnly)
{
	Matrix34 camMatrix = viewTM;

	// If no collision flag set do not check for terrain elevation.
	if (GetType() == ET_ViewportCamera)
	{
		static const float terrain_offset = 0.5f;
		static const float object_offset = 1.0f;

		Vec3 p = camMatrix.GetTranslation();
		if (s_cameraPreferences.IsTerrainCollisionEnabled())
		{
			float z = GetIEditor()->GetTerrainElevation(p.x, p.y);
			if (p.z < z + terrain_offset)
			{
				p.z = z + terrain_offset;
				camMatrix.SetTranslation(p);
			}
		}
		if (s_cameraPreferences.IsObjectCollisionEnabled())
		{
			ray_hit hit;
			Vec3 p0 = GetViewTM().GetTranslation();
			Vec3 move = p - p0;
			float move_dist = move.GetLength();
			if (move_dist)
			{
				move /= move_dist;
				if (gEnv->pPhysicalWorld->RayWorldIntersection(p0, move * (move_dist + object_offset), ent_static, rwi_stop_at_pierceable | rwi_ignore_back_faces, &hit, 1))
				{
					float adjust = hit.dist - (move_dist + object_offset);
					adjust *= hit.n | move;
					if (adjust > 0.0f)
					{
						p += hit.n * adjust;
						camMatrix.SetTranslation(p);
					}
				}
			}
		}

		// Also force this position on game.
		GetIEditor()->SetPlayerViewMatrix(camMatrix);
	}

	CBaseObject* cameraObject = GetCameraObject();
	if (cameraObject)
	{
		// Ignore camera movement if locked.
		if (m_bLockCameraMovement)
		{
			return;
		}

		const bool bPushUndo = m_eCameraMoveState == ECameraMoveState::MovingWithoutUndoPushed;
		if (bPushUndo)
		{
			GetIEditor()->GetIUndoManager()->Begin();
		}

		if (bMoveOnly)
			cameraObject->SetWorldPos(camMatrix.GetTranslation(), eObjectUpdateFlags_IgnoreSelection);
		else
			cameraObject->SetWorldTM(camMatrix, eObjectUpdateFlags_IgnoreSelection);

		if (bPushUndo)
		{
			GetIEditor()->GetIUndoManager()->Accept("Move Camera");
			m_eCameraMoveState = ECameraMoveState::MovingWithUndoPushed;
		}
	}

	CViewport::SetViewTM(camMatrix);

	m_Camera.SetMatrix(camMatrix);
}

void CRenderViewport::RenderSelectedRegion(SDisplayContext& context)
{
	if (!m_engine)
		return;

	AABB box = GetIEditor()->GetLevelEditorSharedState()->GetSelectedRegion();
	if (box.IsEmpty())
		return;

	float x1 = box.min.x;
	float y1 = box.min.y;
	float x2 = box.max.x;
	float y2 = box.max.y;

	float fMaxSide = MAX(y2 - y1, x2 - x1);
	if (fMaxSide < 0.1f)
		return;
	float fStep = fMaxSide / 100.0f;

	float fMinZ = 0;
	float fMaxZ = 0;

	// Draw yellow border lines.
	context.SetColor(1, 1, 0, 1);
	float offset = 0.01f;
	Vec3 p1, p2;

	for (float y = y1; y < y2; y += fStep)
	{
		p1.x = x1;
		p1.y = y;
		p1.z = m_engine->GetTerrainElevation(p1.x, p1.y) + offset;

		p2.x = x1;
		p2.y = y + fStep;
		p2.z = m_engine->GetTerrainElevation(p2.x, p2.y) + offset;
		context.DrawLine(p1, p2);

		p1.x = x2;
		p1.y = y;
		p1.z = m_engine->GetTerrainElevation(p1.x, p1.y) + offset;

		p2.x = x2;
		p2.y = y + fStep;
		p2.z = m_engine->GetTerrainElevation(p2.x, p2.y) + offset;
		context.DrawLine(p1, p2);

		fMinZ = min(fMinZ, min(p1.z, p2.z));
		fMaxZ = max(fMaxZ, max(p1.z, p2.z));
	}
	for (float x = x1; x < x2; x += fStep)
	{
		p1.x = x;
		p1.y = y1;
		p1.z = m_engine->GetTerrainElevation(p1.x, p1.y) + offset;

		p2.x = x + fStep;
		p2.y = y1;
		p2.z = m_engine->GetTerrainElevation(p2.x, p2.y) + offset;
		context.DrawLine(p1, p2);

		p1.x = x;
		p1.y = y2;
		p1.z = m_engine->GetTerrainElevation(p1.x, p1.y) + offset;

		p2.x = x + fStep;
		p2.y = y2;
		p2.z = m_engine->GetTerrainElevation(p2.x, p2.y) + offset;
		context.DrawLine(p1, p2);

		fMinZ = min(fMinZ, min(p1.z, p2.z));
		fMaxZ = max(fMaxZ, max(p1.z, p2.z));
	}

	{
		// Draw a box area
		float fBoxOver = fMaxSide / 5.0f;
		float fBoxHeight = fBoxOver + fMaxZ - fMinZ;

		ColorB boxColor(64, 64, 255, 128); // light blue
		ColorB transparent(boxColor.r, boxColor.g, boxColor.b, 0);

		Vec3 base[] = {
			Vec3(x1, y1, fMinZ),
			Vec3(x2, y1, fMinZ),
			Vec3(x2, y2, fMinZ),
			Vec3(x1, y2, fMinZ) };

		// Generate vertices
		static AABB boxPrev(0.0f);
		static std::vector<Vec3> verts;
		static std::vector<ColorB> colors;

		if (!IsEquivalent(boxPrev, box))
		{
			verts.resize(0);
			colors.resize(0);
			for (int i = 0; i < 4; ++i)
			{
				Vec3& p = base[i];

				verts.push_back(p);
				verts.push_back(Vec3(p.x, p.y, p.z + fBoxHeight));
				verts.push_back(Vec3(p.x, p.y, p.z + fBoxHeight + fBoxOver));

				colors.push_back(boxColor);
				colors.push_back(boxColor);
				colors.push_back(transparent);
			}
			boxPrev = box;
		}

		// Generate indices
		const int numInds = 4 * 12;
		static vtx_idx inds[numInds];
		static bool bNeedIndsInit = true;
		if (bNeedIndsInit)
		{
			vtx_idx* pInds = &inds[0];

			for (int i = 0; i < 4; ++i)
			{
				int over = 0;
				if (i == 3)
					over = -12;

				int ind = i * 3;
				*pInds++ = ind;
				*pInds++ = ind + 3 + over;
				*pInds++ = ind + 1;

				*pInds++ = ind + 1;
				*pInds++ = ind + 3 + over;
				*pInds++ = ind + 4 + over;

				ind = i * 3 + 1;
				*pInds++ = ind;
				*pInds++ = ind + 3 + over;
				*pInds++ = ind + 1;

				*pInds++ = ind + 1;
				*pInds++ = ind + 3 + over;
				*pInds++ = ind + 4 + over;
			}
			bNeedIndsInit = false;
		}

		// Draw lines
		for (int i = 0; i < 4; ++i)
		{
			Vec3& p = base[i];

			context.DrawLine(p, Vec3(p.x, p.y, p.z + fBoxHeight), ColorF(1, 1, 0, 1), ColorF(1, 1, 0, 1));
			context.DrawLine(Vec3(p.x, p.y, p.z + fBoxHeight), Vec3(p.x, p.y, p.z + fBoxHeight + fBoxOver), ColorF(1, 1, 0, 1), ColorF(1, 1, 0, 0));
		}

		// Draw volume
		context.DepthWriteOff();
		context.CullOff();
		context.pRenderAuxGeom->DrawTriangles(&verts[0], verts.size(), &inds[0], numInds, &colors[0]);
		context.CullOn();
		context.DepthWriteOn();
	}
}

void CRenderViewport::ProcessKeys()
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	if (GetIEditor()->IsInGameMode() || !IsFocused())
		return;

	//m_Camera.UpdateFrustum();
	Matrix34 m = GetViewTM();
	Vec3 ydir = m.GetColumn1().GetNormalized();
	Vec3 xdir = m.GetColumn0().GetNormalized();

	Vec3 pos = GetViewTM().GetTranslation();

	float speedScale = 60.0f * GetIEditor()->GetSystem()->GetITimer()->GetFrameTime();
	if (speedScale > 20) speedScale = 20;

	speedScale *= gViewportMovementPreferences.camMoveSpeed * m_moveSpeed;

	if (QtUtil::IsModifierKeyDown(Qt::ShiftModifier))
	{
		speedScale *= gViewportMovementPreferences.camFastMoveSpeed;
	}

	speedScale *= GetCameraSpeedScale();

	bool bCtrl = QtUtil::IsModifierKeyDown(Qt::ControlModifier);
	const bool pressedF = CryGetAsyncKeyState('F');
	if (bCtrl && CryGetAsyncKeyState('G'))
	{
		if (!m_bAttachCameraToSelected)
			m_bAttachCameraToSelected = m_bCaptureAttachOffset = true;
	}
	else if (bCtrl && pressedF)
	{
		if (m_bAttachCameraToSelected)
			m_bAttachCameraToSelected = m_bCaptureAttachOffset = false;
	}
	else if (pressedF)
	{
		SetViewFocus();
	}

	if (bCtrl)
		return;

	bool bIsPressedSome = false;

	if (ViewportInteraction::CheckPolledKey(ViewportInteraction::eKey_Forward))
	{
		// move forward
		bIsPressedSome = true;
		if (m_eCameraMoveState == ECameraMoveState::NoMove)
			m_eCameraMoveState = ECameraMoveState::MovingWithoutUndoPushed;
		pos = pos + speedScale * ydir;
		m.SetTranslation(pos);
		SetViewTM(m, true);
		m_bFakeMouseMove = true;
	}

	if (ViewportInteraction::CheckPolledKey(ViewportInteraction::eKey_Backward))
	{
		// move backward
		bIsPressedSome = true;
		if (m_eCameraMoveState == ECameraMoveState::NoMove)
			m_eCameraMoveState = ECameraMoveState::MovingWithoutUndoPushed;
		pos = pos - speedScale * ydir;
		m.SetTranslation(pos);
		SetViewTM(m, true);
		m_bFakeMouseMove = true;
	}

	if (ViewportInteraction::CheckPolledKey(ViewportInteraction::eKey_Left))
	{
		// move left
		bIsPressedSome = true;
		if (m_eCameraMoveState == ECameraMoveState::NoMove)
			m_eCameraMoveState = ECameraMoveState::MovingWithoutUndoPushed;
		pos = pos - speedScale * xdir;
		m.SetTranslation(pos);
		SetViewTM(m, true);
		m_bFakeMouseMove = true;
	}

	if (ViewportInteraction::CheckPolledKey(ViewportInteraction::eKey_Right))
	{
		// move right
		bIsPressedSome = true;
		if (m_eCameraMoveState == ECameraMoveState::NoMove)
			m_eCameraMoveState = ECameraMoveState::MovingWithoutUndoPushed;
		pos = pos + speedScale * xdir;
		m.SetTranslation(pos);
		SetViewTM(m, true);
		m_bFakeMouseMove = true;
	}

	if (QtUtil::IsMouseButtonDown(Qt::RightButton) || QtUtil::IsMouseButtonDown(Qt::MiddleButton))
	{
		bIsPressedSome = true;
	}

	if (!bIsPressedSome)
		m_eCameraMoveState = ECameraMoveState::NoMove;
}

Vec3 CRenderViewport::WorldToView3D(const Vec3& wp, int nFlags) const
{
	Vec3 out(0, 0, 0);
	GetCamera().Project(wp, out);

	return out;
}

POINT CRenderViewport::WorldToView(const Vec3& wp) const
{
	CPoint p(0, 0);

	Vec3 out(0, 0, 0);
	GetCamera().Project(wp, out);
	if (std::isfinite(out.x) && std::isfinite(out.y))
	{
		CRect rc;
		GetClientRect(&rc);
		p.x = out.x * ((float)rc.Width() / m_currentResolution.width);
		p.y = out.y * ((float)rc.Height() / m_currentResolution.height);
	}

	return p;
}

Vec3 CRenderViewport::ViewToAxisConstraint(POINT& point, Vec3 axis, Vec3 origin) const
{
	Vec3 raySrc, rayDir;
	Vec3 cameraPos = m_Camera.GetPosition();

	ViewToWorldRay(point, raySrc, rayDir);

	// find plane between camera and initial position and direction
	Vec3 cameraToOrigin = cameraPos - origin;
	Vec3 lineViewPlane = cameraToOrigin ^ axis;
	lineViewPlane.Normalize();

	// Now we project the ray from origin to the source point to the screen space line plane
	Vec3 cameraToSrc = raySrc - cameraPos;
	cameraToSrc.Normalize();

	Vec3 perpPlane = (cameraToSrc ^ lineViewPlane);

	// finally, project along the axis to perpPlane
	return ((perpPlane * cameraToOrigin) / (perpPlane * axis)) * axis;
}

Vec3 CRenderViewport::ViewDirection() const
{
	return m_Camera.GetViewdir();
}

Vec3 CRenderViewport::CameraToWorld(Vec3 worldPoint) const
{
	return worldPoint - m_Camera.GetPosition();
}

Vec3 CRenderViewport::UpViewDirection() const
{
	return m_Camera.GetUp();
}

bool CRenderViewport::AdjustObjectPosition(const ray_hit& hit, Vec3& outNormal, Vec3& outPos) const
{
	Matrix34A objMat, objMatInv;
	Matrix33 objRot, objRotInv;

	if (hit.pCollider->GetiForeignData() != PHYS_FOREIGN_ID_STATIC)
		return false;

	IRenderNode* pNode = (IRenderNode*) hit.pCollider->GetForeignData(PHYS_FOREIGN_ID_STATIC);
	if (!pNode || !pNode->GetEntityStatObj())
		return false;

	assert(hit.partid == 0);
	IStatObj* pEntObject = pNode->GetEntityStatObj(0, &objMat, false);
	if (!pEntObject || !pEntObject->GetRenderMesh())
		return false;

	objRot = Matrix33(objMat);
	objRot.NoScale(); // No scale.
	objRotInv = objRot;
	objRotInv.Invert();

	float fWorldScale = objMat.GetColumn(0).GetLength(); // GetScale
	float fWorldScaleInv = 1.0f / fWorldScale;

	// transform decal into object space
	objMatInv = objMat;
	objMatInv.Invert();

	// put into normal object space hit direction of projection
	Vec3 invhitn = -(hit.n);
	Vec3 vOS_HitDir = objRotInv.TransformVector(invhitn).GetNormalized();

	// put into position object space hit position
	Vec3 vOS_HitPos = objMatInv.TransformPoint(hit.pt);
	vOS_HitPos -= vOS_HitDir * g_renderMeshTestDistance * fWorldScaleInv;

	IRenderMesh* pRM = pEntObject->GetRenderMesh();

	AABB aabbRNode;
	pRM->GetBBox(aabbRNode.min, aabbRNode.max);
	Vec3 vOut(0, 0, 0);
	if (!Intersect::Ray_AABB(Ray(vOS_HitPos, vOS_HitDir), aabbRNode, vOut))
		return false;

	if (!pRM || !pRM->GetVerticesCount())
		return false;

	if (RayRenderMeshIntersection(pRM, vOS_HitPos, vOS_HitDir, outPos, outNormal))
	{
		outNormal = objRot.TransformVector(outNormal).GetNormalized();
		outPos = objMat.TransformPoint(outPos);
		return true;
	}
	return false;
}

bool CRenderViewport::RayRenderMeshIntersection(IRenderMesh* pRenderMesh, const Vec3& vInPos, const Vec3& vInDir, Vec3& vOutPos, Vec3& vOutNormal) const
{
	SRayHitInfo hitInfo;
	hitInfo.bUseCache = false;
	hitInfo.bInFirstHit = false;
	hitInfo.inRay.origin = vInPos;
	hitInfo.inRay.direction = vInDir.GetNormalized();
	hitInfo.inReferencePoint = vInPos;
	hitInfo.fMaxHitDistance = 0;
	bool bRes = GetIEditor()->Get3DEngine()->RenderMeshRayIntersection(pRenderMesh, hitInfo, NULL);
	vOutPos = hitInfo.vHitPos;
	vOutNormal = hitInfo.vHitNormal;
	return bRes;
}

void CRenderViewport::ViewToWorldRay(POINT vp, Vec3& raySrc, Vec3& rayDir) const
{
	if (!m_renderer)
		return;

	CRect rc;
	GetClientRect(&rc);

	vp.x = vp.x * m_currentResolution.width / (float)rc.Width();
	vp.y = vp.y * m_currentResolution.height / (float)rc.Height();

	Vec3 pos0(0, 0, 0), pos1(0, 0, 0);

	GetCamera().Unproject(Vec3(vp.x, m_currentResolution.height - vp.y, 0), pos0);
	GetCamera().Unproject(Vec3(vp.x, m_currentResolution.height - vp.y, 1), pos1);

	if (!IsVectorInValidRange(pos0))
		pos0 = Vec3(0, 0, 0);
	if (!IsVectorInValidRange(pos1))
		pos1 = Vec3(0, 0, 0);

	Vec3 v = (pos1 - pos0);
	v = v.GetNormalized();

	raySrc = pos0;
	rayDir = v;
}

float CRenderViewport::GetScreenScaleFactor(const Vec3& worldPoint) const
{
	Vec3 pointVec = worldPoint - m_Camera.GetPosition();
	// Only keep the projected part of the vector
	float dist = pointVec.Dot(m_Camera.GetViewdir());
	if (dist < m_Camera.GetNearPlane())
		dist = m_Camera.GetNearPlane();
	return dist;
}

bool CRenderViewport::HitTest(CPoint point, HitContext& hitInfo)
{
	hitInfo.camera = &m_Camera;

	return CViewport::HitTest(point, hitInfo);
}

bool CRenderViewport::IsBoundsVisible(const AABB& box) const
{
	// If at least part of bbox is visible then its visible.
	return m_Camera.IsAABBVisible_F(AABB(box.min, box.max));
}

void CRenderViewport::SetResolution(int x, int y)
{
	if (gEnv && gEnv->pRenderer)
	{
		gEnv->pRenderer->ResizeContext(m_displayContextKey, x, y);
	}

	m_currentResolution.width = x;
	m_currentResolution.height = y;

	SetCamera(m_Camera);

	m_eResolutionMode = m_eLastCustomResolutionMode;

	signalResolutionChanged();
}

void CRenderViewport::SetResolutionMode(EResolutionMode mode)
{
	m_eResolutionMode = mode;

	if (mode != EResolutionMode::Window)
	{
		m_eLastCustomResolutionMode = mode;
	}

	signalResolutionChanged();
}

void CRenderViewport::GetResolution(int& x, int& y)
{
	x = m_currentResolution.width;
	y = m_currentResolution.height;
}

bool CRenderViewport::CreateRenderContext(CRY_HWND hWnd, IRenderer::EViewportType viewportType)
{
	// Create context.
	if (hWnd && m_renderer && !m_bRenderContextCreated)
	{
		IRenderer::SDisplayContextDescription desc;

		desc.handle = hWnd;
		desc.type = viewportType;
		desc.clearColor = ColorF(0.4f, 0.4f, 0.4f, 1.0f);
		desc.renderFlags = FRT_CLEAR | FRT_OVERLAY_DEPTH;
		desc.superSamplingFactor.x = 1;
		desc.superSamplingFactor.y = 1;
		desc.screenResolution.x = m_currentResolution.width;
		desc.screenResolution.y = m_currentResolution.height;

		m_displayContextKey = m_renderer->CreateSwapChainBackedContext(desc);

		if (viewportType == IRenderer::eViewportType_Default)
		{
			m_graphicsPipelineDesc.type = EGraphicsPipelineType::Standard;
			m_graphicsPipelineDesc.shaderFlags = SHDF_ZPASS | SHDF_ALLOWHDR | SHDF_ALLOWPOSTPROCESS | SHDF_ALLOW_WATER | SHDF_ALLOW_AO | SHDF_ALLOW_SKY | SHDF_ALLOW_RENDER_DEBUG;
		}
		else
		{
			m_graphicsPipelineDesc.type = EGraphicsPipelineType::Minimum;
			m_graphicsPipelineDesc.shaderFlags = SHDF_SECONDARY_VIEWPORT | SHDF_ALLOWHDR | SHDF_FORWARD_MINIMAL;
		}
		m_graphicsPipelineKey = m_renderer->CreateGraphicsPipeline(m_graphicsPipelineDesc);

		m_bRenderContextCreated = true;

		// Make main context current.
		SetCurrentContext();
		return true;
	}

	return false;
}

void CRenderViewport::DestroyRenderContext()
{
	// Destroy render context.
	if (m_renderer && m_bRenderContextCreated)
	{
		// Do not delete primary context.
		if (m_displayContextKey != static_cast<HWND>(m_renderer->GetHWND()))
			m_renderer->DeleteContext(m_displayContextKey);

		m_renderer->ResetActiveGraphicsPipeline();
		m_renderer->DeleteGraphicsPipeline(m_graphicsPipelineKey);

		m_bRenderContextCreated = false;
	}
}

void CRenderViewport::SetDefaultCamera()
{
	if (IsDefaultCamera())
		return;
	gEnv->p3DEngine->SetPostEffectParam("Dof_Active", 0);
	m_pCameraDelegate = nullptr;
	SetCameraObject(0);
}

bool CRenderViewport::IsDefaultCamera() const
{
	if (m_cameraObjectId != CryGUID::Null() || GetCameraObject() || m_pCameraDelegate)
		return false;

	return true;
}

CRenderViewport::SPreviousContext CRenderViewport::SetCurrentContext()
{
	SPreviousContext x;
	m_currentContextWnd = GetSafeHwnd();

	GetISystem()->SetViewCamera(CCamera(m_Camera));

	return x;
}

void CRenderViewport::RestorePreviousContext(const SPreviousContext& x) const
{
}

/*
   //////////////////////////////////////////////////////////////////////////
   void CRenderViewport::OnCaptureChanged( CWnd* pWnd )
   {
   if(m_bInZoomMode || m_bInRotateMode || m_bInMoveMode || m_bInOrbitMode)
   {
    m_bInRotateMode = false;
    m_bInZoomMode = false;
    m_bInMoveMode = false;
    m_bInOrbitMode = false;
   }

   ShowCursor();
   }
 */

void CRenderViewport::HideCursor()
{
	if (m_bCursorHidden || !gViewportPreferences.hideMouseCursorWhenCaptured)
		return;

	if (gEnv->pInput)
		gEnv->pInput->ShowCursor(false);
	m_bCursorHidden = true;
}

void CRenderViewport::ShowCursor()
{
	if (!m_bCursorHidden || !gViewportPreferences.hideMouseCursorWhenCaptured)
		return;

	if (gEnv->pInput)
		gEnv->pInput->ShowCursor(true);
	m_bCursorHidden = false;
}

void CRenderViewport::BeginUndoTransaction()
{
	CScopedRenderLock::Lock();
}

void CRenderViewport::EndUndoTransaction()
{
	CScopedRenderLock::Unlock();
	Update();
}

void CRenderViewport::OnResizeInternal(int width, int height)
{
	if (m_eResolutionMode == EResolutionMode::Window)
	{
		if (gEnv && gEnv->pRenderer)
		{
			gEnv->pRenderer->ResizeContext(m_displayContextKey, width, height);
		}

		m_currentResolution.width = width;
		m_currentResolution.height = height;
	}

	gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_RESIZE, m_currentResolution.width, m_currentResolution.height);
}

void CRenderViewport::OnResize()
{
	CRect rcClient;
	GetClientRect(&rcClient);

	int width = rcClient.Width();
	int height = rcClient.Height();

	if (m_bSuspendResizeNotifications)
	{
		m_pendingResizeWidth = width;
		m_pendingResizeHeight = height;

		m_bPendingResizeNotification = true;
	}
	else
	{
		OnResizeInternal(width, height);
	}
}

void CRenderViewport::OnPaint()
{
	m_isOnPaint = true;
	OnPaintSafe();
	m_isOnPaint = false;
}

bool CRenderViewport::MouseCallback(EMouseEvent event, CPoint& point, int flags)
{
	if (event == eMouseLUp)
	{
		// Update viewports after done with actions.
		GetIEditor()->UpdateViews(eUpdateObjects);
	}

	// cancel fake mouse move if we have a real one
	if (event == eMouseMove)
	{
		m_bFakeMouseMove = false;
	}

	// accelerate only if camera is actually moving, but not if mouse wheel is pressed
	if (!(flags & (MK_CONTROL | MK_MBUTTON)) && (event == eMouseWheel) &&
	    ((gViewportMovementPreferences.mouseWheelBehavior == SViewportMovementPreferences::eWheel_SpeedOnly) ||
	     ((m_eCameraMoveState != ECameraMoveState::NoMove) && gViewportMovementPreferences.mouseWheelBehavior == SViewportMovementPreferences::eWheel_ZoomSpeed)))
	{
		Accelerate((point.x > 0) ? 1 : -1);
		return true;
	}

	// give a chance to the viewport to reset its semi-modal navigation modes before attempting anything else
	bool bMouseInputNeeded = m_bInMoveMode || m_bInOrbitMode || m_bInRotateMode || m_bInZoomMode;

	if (bMouseInputNeeded)
	{
		if (event == eMouseRUp)
		{
			m_bInRotateMode = false;
			m_bInZoomMode = false;

			ShowCursor();

			// Update viewports after done with rotating.
			//GetIEditor()->UpdateViews(eUpdateObjects);
		}
		else if (event == eMouseMUp)
		{
			m_bInMoveMode = false;
			m_bInOrbitMode = false;
			m_mousePos = point;

			ShowCursor();
		}
	}

	if (CViewport::MouseCallback(event, point, flags))
	{
		if (!MoreMouseEventProcessNeeded(event))
			return true;
	}

	if (event == eMouseRDown)
	{
		bool bAlt = (GetAsyncKeyState(VK_MENU) & (1 << 15)) != 0;
		if (bAlt)
			m_bInZoomMode = true;
		else
			m_bInRotateMode = true;
		m_mousePos = point;

		HideCursor();
	}

	if (event == eMouseWheel && gViewportMovementPreferences.mouseWheelBehavior != SViewportMovementPreferences::eWheel_SpeedOnly)
	{
		Matrix34 m = GetViewTM();
		Vec3 ydir = m.GetColumn1().GetNormalized();

		Vec3 pos = m.GetTranslation();

		int zDelta = point.y;
		const float posDelta = 0.01f * zDelta * gViewportMovementPreferences.wheelZoomSpeed * gViewportMovementPreferences.camMoveSpeed * m_moveSpeed * GetCameraSpeedScale();
		pos += ydir * posDelta;

		m.SetTranslation(pos);
		SetViewTM(m, true);
	}

	return false;
}

void CRenderViewport::OnCameraTransformEvent(CameraTransformEvent* msg)
{
	static float all6DOFs[6];

	all6DOFs[0] = msg->GetTranslation()[0];
	all6DOFs[1] = msg->GetTranslation()[1];
	all6DOFs[2] = msg->GetTranslation()[2];
	all6DOFs[3] = msg->GetRotation()[0];
	all6DOFs[4] = msg->GetRotation()[1];
	all6DOFs[5] = msg->GetRotation()[2];

	Matrix34 viewTM = GetViewTM();

	// Scale axis according to CVars
	ICVar* sys_scale3DMouseTranslation = gEnv->pConsole->GetCVar("sys_scale3DMouseTranslation");
	ICVar* sys_Scale3DMouseYPR = gEnv->pConsole->GetCVar("sys_Scale3DMouseYPR");
	float fScaleYPR = sys_Scale3DMouseYPR->GetFVal();

	float s = 0.01f * gViewportMovementPreferences.camMoveSpeed;
	Vec3 t = Vec3(s * all6DOFs[0], -s * all6DOFs[1], -s * all6DOFs[2] * 0.5f);
	t *= sys_scale3DMouseTranslation->GetFVal();

	float as = 0.001f * gViewportMovementPreferences.camMoveSpeed;
	Ang3 ypr = CCamera::CreateAnglesYPR(Matrix33(viewTM));
	ypr.x += -all6DOFs[5] * as * fScaleYPR;
	ypr.y = CLAMP(ypr.y + all6DOFs[3] * as * fScaleYPR, -1.5f, 1.5f); // to keep rotation in reasonable range
	ypr.z = 0;                                                        // to have camera always upward

	viewTM = Matrix34(CCamera::CreateOrientationYPR(ypr), viewTM.GetTranslation());
	viewTM = viewTM * Matrix34::CreateTranslationMat(t);

	SetViewTM(viewTM);

	// Since we are manipulating the view, don't forget to emit a fake mouse move for the tools
	m_bFakeMouseMove = true;
}

namespace
{
void PyToggleCameraTerrainCollisions()
{
	CRenderViewport::s_cameraPreferences.SetTerrainCollision(!CRenderViewport::s_cameraPreferences.IsTerrainCollisionEnabled());
}

void PyToggleCameraObjectCollisions()
{
	CRenderViewport::s_cameraPreferences.SetObjectCollision(!CRenderViewport::s_cameraPreferences.IsObjectCollisionEnabled());
}

void PyToggleCameraSpeedHeightRelative()
{
	CRenderViewport::s_cameraPreferences.SetSpeedHeightRelative(!CRenderViewport::s_cameraPreferences.IsSpeedHeightRelativeEnabled());
}
}

REGISTER_EDITOR_AND_SCRIPT_COMMAND(PyToggleCameraTerrainCollisions, camera, toggle_terrain_collisions,
                                   CCommandDescription("Camera Terrain Collisions"))
REGISTER_EDITOR_UI_COMMAND_DESC(camera, toggle_terrain_collisions, "Camera Terrain Collisions", "Q", "", true)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(PyToggleCameraObjectCollisions, camera, toggle_object_collisions,
                                   CCommandDescription("Camera Object Collisions"))
REGISTER_EDITOR_UI_COMMAND_DESC(camera, toggle_object_collisions, "Camera Object Collisions", "", "", true)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(PyToggleCameraSpeedHeightRelative, camera, toggle_speed_height_relative,
                                   CCommandDescription("Camera Speed Height-Relative"))
REGISTER_EDITOR_UI_COMMAND_DESC(camera, toggle_speed_height_relative, "Camera Speed Height-Relative", "Z", "", true)
