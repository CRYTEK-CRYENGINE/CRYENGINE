// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "IEditorMaterial.h"
#include "PreviewModelCtrl.h"

#include <Cry3DEngine/I3DEngine.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryParticleSystem/IParticles.h>

#include "IIconManager.h"
#include "ViewportInteraction.h"
#include <Preferences/ViewportPreferences.h>
#include "RenderLock.h"

CPreviewModelCtrl::CPreviewModelCtrl()
{
	m_bShowObject = true;
	m_pRenderer = 0;
	m_pObj = 0;
	m_pCharacter = 0;
	m_pEntity = 0;
	m_nTimer = 0;
	m_pEmitter = 0;
	m_size(0, 0, 0);

	m_bRotate = false;
	m_rotateAngle = 0;

	m_backgroundTextureId = 0;

	m_pRenderer = GetIEditor()->GetRenderer();
	m_pAnimationSystem = GetIEditor()->GetSystem()->GetIAnimationSystem();

	m_fov = 60;
	m_camera.SetFrustum(800, 600, DEG2RAD(m_fov), 0.02f, 10000.0f);

	m_bInRotateMode = false;
	m_bInMoveMode = false;

	SRenderLight l;

	float L = 1.0f;
	l.m_Flags |= DLF_SUN | DLF_DIRECTIONAL;
	l.SetLightColor(ColorF(L, L, L, 1));
	l.SetPosition(Vec3(100, 100, 100));
	l.SetRadius(10000);
	m_lights.push_back(l);

	m_bUseBacklight = false;
	m_renderContextCreated = false;
	m_bHaveAnythingToRender = false;
	m_bGrid = true;
	m_bAxis = true;
	m_bUpdate = false;
	m_bShowNormals = false;
	m_bShowPhysics = false;
	m_bShowRenderInfo = false;

	m_cameraAngles.Set(0, 0, 0);

	m_clearColor.set(0.5f, 0.5f, 0.5f, 1.0f);
	m_ambientColor.set(1.0f, 1.0f, 1.0f, 1.0f);
	m_ambientMultiplier = 0.5f;

	m_cameraChangeCallback = NULL;
	m_bPrecacheMaterial = false;
	m_bDrawWireFrame = false;

	m_tileX = 0.0f;
	m_tileY = 0.0f;
	m_tileSizeX = 1.0f;
	m_tileSizeY = 1.0f;

	m_aabb = AABB(2);
	FitToScreen();

	GetIEditor()->RegisterNotifyListener(this);
	m_physHelpers0 = gEnv->pPhysicalWorld->GetPhysVars()->iDrawHelpers;
}

CPreviewModelCtrl::~CPreviewModelCtrl()
{
	ReleaseObject();
	GetIEditor()->UnregisterNotifyListener(this);
	gEnv->pPhysicalWorld->GetPhysVars()->iDrawHelpers = m_physHelpers0;
}

BEGIN_MESSAGE_MAP(CPreviewModelCtrl, CWnd)
//{{AFX_MSG_MAP(CPreviewModelCtrl)
ON_WM_CREATE()
ON_WM_PAINT()
ON_WM_ERASEBKGND()
ON_WM_TIMER()
ON_WM_DESTROY()
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_MBUTTONDOWN()
ON_WM_MBUTTONUP()
ON_WM_MOUSEMOVE()
ON_WM_RBUTTONDOWN()
ON_WM_RBUTTONUP()
ON_WM_MOUSEWHEEL()
ON_WM_SIZE()
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CPreviewModelCtrl::Create(CWnd* pWndParent, const CRect& rc, DWORD dwStyle, UINT nID)
{
	BOOL bReturn = CreateEx(NULL, AfxRegisterWndClass(CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
	                                                  AfxGetApp()->LoadStandardCursor(IDC_ARROW), NULL, NULL), NULL, dwStyle,
	                        rc, pWndParent, nID, NULL);

	return bReturn;
}

int CPreviewModelCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	return 0;
}

bool CPreviewModelCtrl::CreateRenderContext()
{
	// Create context.
	if (m_pRenderer && !m_renderContextCreated)
	{
		CRect rc; GetClientRect(rc);
		IRenderer::SDisplayContextDescription desc;

		desc.handle = m_hWnd;
		desc.type = IRenderer::eViewportType_Secondary;
		desc.clearColor = m_clearColor;
		desc.renderFlags = FRT_CLEAR_COLOR | FRT_CLEAR_DEPTH | FRT_TEMPORARY_DEPTH;
		desc.superSamplingFactor.x = 1;
		desc.superSamplingFactor.y = 1;
		desc.screenResolution.x = rc.Width();
		desc.screenResolution.y = rc.Height();

		m_displayContextKey = m_pRenderer->CreateSwapChainBackedContext(desc);
		m_renderContextCreated = true;

		return true;
	}

	return false;
}

void CPreviewModelCtrl::DestroyRenderContext()
{
	// Destroy render context.
	if (m_pRenderer && m_renderContextCreated)
	{
		// Do not delete primary context.
		if (m_displayContextKey != reinterpret_cast<HWND>(m_pRenderer->GetHWND()))
			m_pRenderer->DeleteContext(m_displayContextKey);

		m_renderContextCreated = false;
	}
}

void CPreviewModelCtrl::InitDisplayContext(HWND hWnd)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	// Draw all objects.
	SDisplayContextKey displayContextKey;
	displayContextKey.key.emplace<HWND>(hWnd);

	DisplayContext& dctx = m_displayContext;
	dctx.SetDisplayContext(displayContextKey, IRenderer::eViewportType_Secondary);
//	dctx.SetView(m_pViewportAdapter.get());
	dctx.SetCamera(&m_camera);
	dctx.renderer = m_pRenderer;
	dctx.engine = nullptr;
	dctx.box.min = Vec3(-100000, -100000, -100000);
	dctx.box.max = Vec3(100000, 100000, 100000);
	dctx.flags = 0;
}

void CPreviewModelCtrl::PreSubclassWindow()
{
	CWnd::PreSubclassWindow();
}

void CPreviewModelCtrl::ReleaseObject()
{
	m_pObj = NULL;
	SAFE_RELEASE(m_pCharacter);
	if (m_pEmitter)
	{
		m_pEmitter->Activate(false);
		m_pEmitter->Release();
		m_pEmitter = 0;
	}
	m_pEntity = 0;
	m_bHaveAnythingToRender = false;
}

void CPreviewModelCtrl::LoadFile(const char* modelFile, bool changeCamera)
{
	m_bHaveAnythingToRender = false;
	if (!m_hWnd)
		return;
	if (!m_pRenderer)
		return;

	ReleaseObject();

	if (!modelFile || !*modelFile)
	{
		if (m_nTimer != 0)
			KillTimer(m_nTimer);
		m_nTimer = 0;
		Invalidate();
		return;
	}

	m_loadedFile = modelFile;

	CString strFileExt = PathUtil::GetExt(modelFile);
	uint32 isSKEL = stricmp(strFileExt, CRY_SKEL_FILE_EXT) == 0;
	uint32 isSKIN = stricmp(strFileExt, CRY_SKIN_FILE_EXT) == 0;
	uint32 isCDF = stricmp(strFileExt, CRY_CHARACTER_DEFINITION_FILE_EXT) == 0;
	uint32 isCGA = stricmp(strFileExt, CRY_ANIM_GEOMETRY_FILE_EXT) == 0;
	uint32 isCGF = stricmp(strFileExt, CRY_GEOMETRY_FILE_EXT) == 0;

	if (isCGA)
	{
		// Load CGA animated object.
		m_pCharacter = m_pAnimationSystem->CreateInstance(modelFile);
		if (!m_pCharacter)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Loading of geometry object %s failed.", modelFile);
			if (m_nTimer != 0)
				KillTimer(m_nTimer);
			m_nTimer = 0;
			Invalidate();
			return;
		}
		m_pCharacter->AddRef();
		m_aabb = m_pCharacter->GetAABB();
	}

	if (isSKEL || isSKIN || isCDF)
	{
		// Load character.
		m_pCharacter = m_pAnimationSystem->CreateInstance(modelFile, CA_PreviewMode | CA_CharEditModel);
		if (!m_pCharacter)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Loading of character %s failed.", modelFile);
			if (m_nTimer != 0)
				KillTimer(m_nTimer);
			m_nTimer = 0;
			Invalidate();
			return;
		}
		m_pCharacter->AddRef();
		m_aabb = m_pCharacter->GetAABB();
	}

	if (isCGF)
	{
		// Load object.
		m_pObj = GetIEditor()->Get3DEngine()->LoadStatObj(modelFile, NULL, NULL, false);
		if (!m_pObj)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Loading of geometry object %s failed.", (const char*)modelFile);
			if (m_nTimer != 0)
				KillTimer(m_nTimer);
			m_nTimer = 0;
			Invalidate();
			return;
		}
		m_aabb.min = m_pObj->GetBoxMin();
		m_aabb.max = m_pObj->GetBoxMax();
	}
	else
	{
		if (m_nTimer != 0)
			KillTimer(m_nTimer);
		m_nTimer = 0;
		Invalidate();
		return;
	}

	m_bHaveAnythingToRender = true;

	if (changeCamera)
		FitToScreen();

	Invalidate();
}

void CPreviewModelCtrl::LoadParticleEffect(IParticleEffect* pEffect)
{
	m_bHaveAnythingToRender = false;
	if (!m_hWnd)
		return;
	if (!m_pRenderer)
		return;

	ReleaseObject();

	if (!pEffect)
		return;

	RECT rc;
	GetClientRect(&rc);
	if (rc.bottom - rc.top < 2 || rc.right - rc.left < 2)
		return;

	Matrix34 tm;
	tm.SetIdentity();
	tm.SetRotationXYZ(Ang3(DEG2RAD(90.0f), 0, 0));

	m_bHaveAnythingToRender = true;
	SpawnParams sp;
	sp.bNowhere = true;
	m_pEmitter = pEffect->Spawn(tm, sp);
	if (m_pEmitter)
	{
		m_pEmitter->AddRef();
		m_pEmitter->Update();
		m_aabb = m_pEmitter->GetBBox();
		if (m_aabb.IsReset())
			m_aabb = AABB(1);
		FitToScreen();
	}

	Invalidate();
}

void CPreviewModelCtrl::SetEntity(IRenderNode* entity)
{
	m_bHaveAnythingToRender = false;
	if (m_pEntity != entity)
	{
		m_pEntity = entity;
		if (m_pEntity)
		{
			m_bHaveAnythingToRender = true;
			m_aabb = m_pEntity->GetBBox();
		}
		Invalidate();
	}
}

void CPreviewModelCtrl::SetObject(IStatObj* pObject)
{
	if (m_pObj != pObject)
	{
		m_bHaveAnythingToRender = false;
		m_pObj = pObject;
		if (m_pObj)
		{
			m_bHaveAnythingToRender = true;
			m_aabb = m_pObj->GetAABB();
		}
		Invalidate();
	}
}

void CPreviewModelCtrl::SetCameraRadius(float fRadius)
{
	m_cameraRadius = fRadius;

	Matrix34 m = m_camera.GetMatrix();
	Vec3 dir = m.TransformVector(Vec3(0, 1, 0));
	Matrix34 tm = Matrix33::CreateRotationVDir(dir, 0);
	tm.SetTranslation(m_cameraTarget - dir * m_cameraRadius);
	m_camera.SetMatrix(tm);
	if (m_cameraChangeCallback)
		m_cameraChangeCallback(m_pCameraChangeUserData, this);
}

void CPreviewModelCtrl::SetCameraLookAt(float fRadiusScale, const Vec3& fromDir)
{
	m_cameraTarget = m_aabb.GetCenter();
	m_cameraRadius = m_aabb.GetRadius() * fRadiusScale;

	Vec3 dir = fromDir.GetNormalized();
	Matrix34 tm = Matrix33::CreateRotationVDir(dir, 0);
	tm.SetTranslation(m_cameraTarget - dir * m_cameraRadius);
	m_camera.SetMatrix(tm);
	if (m_cameraChangeCallback)
		m_cameraChangeCallback(m_pCameraChangeUserData, this);
}

CCamera& CPreviewModelCtrl::GetCamera()
{
	return m_camera;
}

void CPreviewModelCtrl::UseBackLight(bool bEnable)
{
	if (bEnable)
	{
		m_lights.resize(1);
		SRenderLight l;
		l.m_Flags |= DLF_POINT;
		l.SetPosition(Vec3(-100, 100, -100));
		float L = 0.5f;
		l.SetLightColor(ColorF(L, L, L, 1));
		l.SetRadius(1000);
		m_lights.push_back(l);
	}
	else
	{
		m_lights.resize(1);
	}
	m_bUseBacklight = bEnable;
}

void CPreviewModelCtrl::OnPaint()
{
	CPaintDC dc(this);
	bool res = Render();
	if (!res)
	{
		RECT rect;
		// Get the rect of the client window
		GetClientRect(&rect);
		// Create the brush
		CBrush cFillBrush;
		cFillBrush.CreateSolidBrush(RGB(128, 128, 128));
		// Fill the entire client area
		dc.FillRect(&rect, &cFillBrush);
	}
}

BOOL CPreviewModelCtrl::OnEraseBkgnd(CDC* pDC)
{
	if (m_bHaveAnythingToRender)
		return TRUE;

	return CWnd::OnEraseBkgnd(pDC);
}

void CPreviewModelCtrl::SetCamera(CCamera& cam)
{
	m_camera.SetPosition(cam.GetPosition());

	CRect rc;
	GetClientRect(rc);
	int w = rc.Width() * m_tileSizeX;
	int h = rc.Height() * m_tileSizeY;
	m_camera.SetFrustum(w, h, DEG2RAD(m_fov), m_camera.GetNearPlane(), m_camera.GetFarPlane());

	if (m_cameraChangeCallback)
		m_cameraChangeCallback(m_pCameraChangeUserData, this);
}

void CPreviewModelCtrl::SetOrbitAngles(const Ang3& ang)
{
	assert(0);
}

bool CPreviewModelCtrl::Render()
{
	bool result = false;

	// lock while we are rendering to prevent any recursive rendering across the application
	if (CScopedRenderLock lock = CScopedRenderLock())
	{
		if (!m_renderContextCreated)
		{
			if (!CreateRenderContext())
				return false;
		}

		// Configures Aux to draw to the current display-context
		InitDisplayContext(m_hWnd);

		m_pRenderer->BeginFrame(m_displayContextKey);

		result = RenderInternal();

		m_pRenderer->EndFrame();
	}

	return result;
}

bool CPreviewModelCtrl::RenderInternal()
{
	DisplayContext& dc = m_displayContext;

	CRect rc;
	GetClientRect(rc);

	int width = rc.right - rc.left;
	int height = rc.bottom - rc.top;
	if (height < 2 || width < 2)
		return false;

	SetCamera(m_camera);

	DrawBackground();
	if (m_bGrid || m_bAxis)
		DrawGrid();

	// save some cvars
	int showNormals = gEnv->pConsole->GetCVar("r_ShowNormals")->GetIVal();
	int showPhysics = gEnv->pConsole->GetCVar("p_draw_helpers")->GetIVal();
	int showInfo = gEnv->pConsole->GetCVar("r_displayInfo")->GetIVal();

	gEnv->pConsole->GetCVar("r_ShowNormals")->Set((int)m_bShowNormals);
	gEnv->pConsole->GetCVar("p_draw_helpers")->Set((int)m_bShowPhysics);
	gEnv->pConsole->GetCVar("r_displayInfo")->Set((int)m_bShowRenderInfo);

	// Render object.
	SRenderingPassInfo passInfo = SRenderingPassInfo::CreateGeneralPassRenderingInfo(m_camera, SRenderingPassInfo::DEFAULT_FLAGS, true, dc.GetDisplayContextKey());
	m_pRenderer->EF_StartEf(passInfo);

	{
		CScopedWireFrameMode scopedWireFrame(m_pRenderer, m_bDrawWireFrame ? R_WIREFRAME_MODE : R_SOLID_MODE);

		// Add lights.
		for (size_t i = 0; i < m_lights.size(); i++)
		{
			m_pRenderer->EF_ADDDlight(&m_lights[i], passInfo);
		}

		if (m_pCurrentMaterial)
			m_pCurrentMaterial->DisableHighlight();

		_smart_ptr<IMaterial> pMaterial;
		if (m_pCurrentMaterial)
			pMaterial = m_pCurrentMaterial->GetMatInfo();

		if (m_bPrecacheMaterial)
		{
			IMaterial* pCurMat = pMaterial;
			if (!pCurMat)
			{
				if (m_pObj)
					pCurMat = m_pObj->GetMaterial();
				else if (m_pEntity)
					pCurMat = m_pEntity->GetMaterial();
				else if (m_pCharacter)
					pCurMat = m_pCharacter->GetIMaterial();
				else if (m_pEmitter)
					pCurMat = m_pEmitter->GetMaterial();
			}
			if (pCurMat)
			{
				pCurMat->PrecacheMaterial(0.0f, NULL, true, true);
			}
		}

		{
			// activate shader item
			IMaterial* pCurMat = pMaterial;
			if (!pCurMat)
			{
				if (m_pObj)
					pCurMat = m_pObj->GetMaterial();
				else if (m_pEntity)
					pCurMat = m_pEntity->GetMaterial();
				else if (m_pCharacter)
					pCurMat = m_pCharacter->GetIMaterial();
				else if (m_pEmitter)
					pCurMat = m_pEmitter->GetMaterial();
			}
			/*if (pCurMat)
			   {
			   pCurMat->ActivateAllShaderItem();
			   }*/
		}

		if (m_bShowObject)
			RenderObject(pMaterial, passInfo);

		m_pRenderer->EF_EndEf3D(SHDF_NOASYNC | SHDF_ALLOWHDR | SHDF_SECONDARY_VIEWPORT, -1, -1, passInfo);

		if (true)
			RenderEffect(pMaterial, passInfo);
	}

	m_pRenderer->RenderDebug(false);

	gEnv->pConsole->GetCVar("r_ShowNormals")->Set(showNormals);
	gEnv->pConsole->GetCVar("p_draw_helpers")->Set(showPhysics);
	gEnv->pConsole->GetCVar("r_displayInfo")->Set(showInfo);

	return true;
}

void CPreviewModelCtrl::RenderObject(IMaterial* pMaterial, const SRenderingPassInfo& passInfo)
{
	SRendParams rp;
	rp.dwFObjFlags = 0;
	rp.AmbientColor = m_ambientColor * m_ambientMultiplier;
	rp.dwFObjFlags |= FOB_TRANS_MASK /*| FOB_GLOBAL_ILLUMINATION*/ | FOB_NO_FOG /*| FOB_ZPREPASS*/;
	rp.pMaterial = pMaterial;

	Matrix34 tm;
	tm.SetIdentity();
	rp.pMatrix = &tm;

	if (m_bRotate)
	{
		tm.SetRotationXYZ(Ang3(0, 0, m_rotateAngle));
		m_rotateAngle += 0.1f;
	}

	if (m_pObj)
		m_pObj->Render(rp, passInfo);

	if (m_pEntity)
		m_pEntity->Render(rp, passInfo);

	if (m_pCharacter)
		m_pCharacter->Render(rp, passInfo);

	if (m_pEmitter)
	{
		m_pEmitter->Update();
		m_pEmitter->Render(rp, passInfo);
	}
}

void CPreviewModelCtrl::RenderEffect(IMaterial* pMaterial, const SRenderingPassInfo& passInfo)
{
}

void CPreviewModelCtrl::DrawGrid()
{
	// Draw grid.
	float step = 0.1f;
	float XR = 5;
	float YR = 5;

	IRenderAuxGeom* pRag = m_pRenderer->GetIRenderAuxGeom();
	SAuxGeomRenderFlags nRendFlags = pRag->GetRenderFlags();

	pRag->SetRenderFlags(e_Def3DPublicRenderflags);
	SAuxGeomRenderFlags nNewFlags = pRag->GetRenderFlags();
	nNewFlags.SetAlphaBlendMode(e_AlphaBlended);
	pRag->SetRenderFlags(nNewFlags);

	int nGridAlpha = 40;
	if (m_bGrid)
	{
		// Draw grid.
		for (float x = -XR; x < XR; x += step)
		{
			if (fabs(x) > 0.01)
				pRag->DrawLine(Vec3(x, -YR, 0), ColorB(150, 150, 150, nGridAlpha), Vec3(x, YR, 0), ColorB(150, 150, 150, nGridAlpha));
		}
		for (float y = -YR; y < YR; y += step)
		{
			if (fabs(y) > 0.01)
				pRag->DrawLine(Vec3(-XR, y, 0), ColorB(150, 150, 150, nGridAlpha), Vec3(XR, y, 0), ColorB(150, 150, 150, nGridAlpha));
		}

	}

	nGridAlpha = 60;
	if (m_bAxis)
	{
		// Draw axis.
		pRag->DrawLine(Vec3(0, 0, 0), ColorB(255, 0, 0, nGridAlpha), Vec3(XR, 0, 0), ColorB(255, 0, 0, nGridAlpha));
		pRag->DrawLine(Vec3(0, 0, 0), ColorB(0, 255, 0, nGridAlpha), Vec3(0, YR, 0), ColorB(0, 255, 0, nGridAlpha));
		pRag->DrawLine(Vec3(0, 0, 0), ColorB(0, 0, 255, nGridAlpha), Vec3(0, 0, YR), ColorB(0, 0, 255, nGridAlpha));
	}
	pRag->SetRenderFlags(nRendFlags);
}

void CPreviewModelCtrl::UpdateAnimation()
{
	if (m_pCharacter)
	{
		GetISystem()->GetIAnimationSystem()->Update(false);
		m_pCharacter->GetISkeletonPose()->SetForceSkeletonUpdate(0);

		const CCamera& camera = GetCamera();
		float fDistance = (camera.GetPosition()).GetLength();
		float fZoomFactor = 0.001f + 0.999f * (RAD2DEG(camera.GetFov()) / 60.f);

		SAnimationProcessParams params;
		params.locationAnimation = QuatTS(IDENTITY);
		params.bOnRender = 0;
		params.zoomAdjustedDistanceFromCamera = fDistance * fZoomFactor;
		m_pCharacter->StartAnimationProcessing(params);
		m_pCharacter->FinishAnimationComputations();

		m_aabb = m_pCharacter->GetAABB();
	}
}

void CPreviewModelCtrl::OnTimer(UINT_PTR nIDEvent)
{
	if (IsWindowVisible())
	{
		if (m_bHaveAnythingToRender)
			Invalidate();
	}

	CWnd::OnTimer(nIDEvent);
}

void CPreviewModelCtrl::SetCameraTM(const Matrix34& cameraTM)
{
	m_camera.SetMatrix(cameraTM);
	if (m_cameraChangeCallback)
		m_cameraChangeCallback(m_pCameraChangeUserData, this);
}

void CPreviewModelCtrl::GetCameraTM(Matrix34& cameraTM)
{
	cameraTM = m_camera.GetMatrix();
}

void CPreviewModelCtrl::OnDestroy()
{
	ReleaseObject();
	DestroyRenderContext();

	CWnd::OnDestroy();

	if (m_nTimer)
		KillTimer(m_nTimer);
}

void CPreviewModelCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	m_bInRotateMode = true;
	m_mousePosition = point;
	SetFocus();
	if (!m_bInMoveMode)
		SetCapture();
	Invalidate();
}

void CPreviewModelCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
	m_bInRotateMode = false;
	if (!m_bInMoveMode)
		ReleaseCapture();
	Invalidate();
}

void CPreviewModelCtrl::OnMButtonDown(UINT nFlags, CPoint point)
{
	m_bInRotateMode = true;
	m_bInMoveMode = true;
	m_mousePosition = point;
	SetCapture();
	Invalidate();
}

void CPreviewModelCtrl::OnMButtonUp(UINT nFlags, CPoint point)
{
	m_bInRotateMode = false;
	m_bInMoveMode = false;
	ReleaseCapture();
	Invalidate();
}

void CPreviewModelCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default
	CWnd::OnMouseMove(nFlags, point);

	if (point == m_mousePosition)
		return;

	if (m_bInMoveMode)
	{
		// Zoom.
		Matrix34 m = m_camera.GetMatrix();
		Vec3 xdir(0, 0, 0);
		Vec3 zdir = m.GetColumn1().GetNormalized();

		float step = 0.002f;
		float dx = (point.x - m_mousePosition.x);
		float dy = (point.y - m_mousePosition.y);
		m_camera.SetPosition(m_camera.GetPosition() + step * xdir * dx + step * zdir * dy);
		SetCamera(m_camera);

		CPoint pnt = m_mousePosition;
		ClientToScreen(&pnt);
		SetCursorPos(pnt.x, pnt.y);
		Invalidate();
	}
	else if (m_bInRotateMode)
	{
		Vec3 pos = m_camera.GetMatrix().GetTranslation();
		m_cameraRadius = Vec3(m_camera.GetMatrix().GetTranslation() - m_cameraTarget).GetLength();
		// Look
		Ang3 angles(-point.y + m_mousePosition.y, 0, -point.x + m_mousePosition.x);
		angles = angles * 0.002f;

		Matrix34 camtm = m_camera.GetMatrix();
		Matrix33 Rz = Matrix33::CreateRotationXYZ(Ang3(0, 0, angles.z));        // Rotate around vertical axis.
		Matrix33 Rx = Matrix33::CreateRotationAA(angles.x, camtm.GetColumn0()); // Rotate with angle around x axis in camera space.

		Vec3 dir = camtm.TransformVector(Vec3(0, 1, 0));
		Vec3 newdir = (Rx * Rz).TransformVector(dir).GetNormalized();
		camtm = Matrix34(Matrix33::CreateRotationVDir(newdir, 0), m_cameraTarget - newdir * m_cameraRadius);
		m_camera.SetMatrix(camtm);
		if (m_cameraChangeCallback)
			m_cameraChangeCallback(m_pCameraChangeUserData, this);

		CPoint pnt = m_mousePosition;
		ClientToScreen(&pnt);
		SetCursorPos(pnt.x, pnt.y);
		Invalidate();
	}
	else if (m_bInMoveMode)
	{
		// Slide.
		float speedScale = 0.001f;
		Matrix34 m = m_camera.GetMatrix();
		Vec3 xdir = m.GetColumn0().GetNormalized();
		Vec3 zdir = m.GetColumn2().GetNormalized();

		Vec3 pos = m_cameraTarget;
		pos += 0.1f * xdir * (point.x - m_mousePosition.x) * speedScale + 0.1f * zdir * (m_mousePosition.y - point.y) * speedScale;
		m_cameraTarget = pos;

		Vec3 dir = m.TransformVector(Vec3(0, 1, 0));
		m.SetTranslation(m_cameraTarget - dir * m_cameraRadius);
		m_camera.SetMatrix(m);
		if (m_cameraChangeCallback)
			m_cameraChangeCallback(m_pCameraChangeUserData, this);

		m_mousePosition = point;
		CPoint pnt = m_mousePosition;
		ClientToScreen(&pnt);
		SetCursorPos(pnt.x, pnt.y);
		Invalidate();
	}
}

void CPreviewModelCtrl::OnRButtonDown(UINT nFlags, CPoint point)
{
	m_bInMoveMode = true;
	m_mousePosition = point;
	if (!m_bInRotateMode)
		SetCapture();
	Invalidate();
}

void CPreviewModelCtrl::OnRButtonUp(UINT nFlags, CPoint point)
{
	m_bInMoveMode = false;
	m_mousePosition = point;
	if (!m_bInRotateMode)
		ReleaseCapture();
	Invalidate();
}

BOOL CPreviewModelCtrl::OnMouseWheel(UINT nFlags, short zDelta, CPoint point)
{
	// TODO: Add your message handler code here and/or call default
	Matrix34 m = m_camera.GetMatrix();
	Vec3 zdir = m.GetColumn1().GetNormalized();

	//m_camera.SetPosition( m_camera.GetPos() + ydir*(m_mousePos.y-point.y),xdir*(m_mousePos.x-point.x) );
	m_camera.SetPosition(m_camera.GetPosition() + 0.002f * zdir * (zDelta));
	SetCamera(m_camera);
	Invalidate();

	return TRUE;
}

void CPreviewModelCtrl::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	RedrawWindow();

	//m_pRenderer->ResizeContext(GetSafeHwnd(),cx,cy);
}

void CPreviewModelCtrl::EnableUpdate(bool bUpdate)
{
	m_bUpdate = bUpdate;
}

void CPreviewModelCtrl::Update(bool bForceUpdate)
{
	ProcessKeys();

	if (m_bUpdate && m_bHaveAnythingToRender || bForceUpdate)
	{
		if (IsWindowVisible())
			Render();
	}
}

void CPreviewModelCtrl::SetRotation(bool bEnable)
{
	m_bRotate = bEnable;
}

void CPreviewModelCtrl::SetMaterial(IEditorMaterial* pMaterial)
{
	if (pMaterial)
	{
		if ((pMaterial->GetFlags() & MTL_FLAG_NOPREVIEW))
		{
			m_pCurrentMaterial = 0;
			Invalidate();
			return;
		}
	}
	m_pCurrentMaterial = pMaterial;
	Invalidate();
}

IEditorMaterial* CPreviewModelCtrl::GetMaterial()
{
	return m_pCurrentMaterial;
}

void CPreviewModelCtrl::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnIdleUpdate:
		Update();
		break;
	case eNotify_OnClearLevelContents:
		ReleaseObject();
		break;
	}
}

void CPreviewModelCtrl::GetImageOffscreen(CImageEx& image, const CSize& customSize)
{
	m_pRenderer->EnableSwapBuffers(false);
	Render();
	m_pRenderer->EnableSwapBuffers(true);

	CRect rc;
	int width;
	int height;

	GetClientRect(rc);

	if (customSize.cx == 0 && customSize.cy == 0)
	{
		width = rc.Width();
		height = rc.Height();
	}
	else
	{
		width = customSize.cx;
		height = customSize.cy;
	}

	image.Allocate(width, height);
	m_pRenderer->ReadFrameBuffer(image.GetData(), width, height);

	// At this point the image is upside-down, so we need to flip it.
	unsigned int* data = image.GetData();
	for (int row = 0; row < (height - 1) / 2; ++row)
	{
		for (int col = 0; col < width; ++col)
		{
			unsigned int pixelUp = data[row * width + col];
			unsigned int pixelDn = data[(height - row - 1) * width + col];

			data[row * width + col] = pixelDn;
			data[(height - row - 1) * width + col] = pixelUp;
		}
	}
}

void CPreviewModelCtrl::GetImage(CImageEx& image)
{
	Render();

	CRect rc;
	GetClientRect(rc);
	int width = rc.Width();
	int height = rc.Height();
	image.Allocate(width, height);

	CBitmap bmp;
	CDC dcMemory;
	CDC* pDC = GetDC();
	dcMemory.CreateCompatibleDC(pDC);

	bmp.CreateCompatibleBitmap(pDC, width, height);

	CBitmap* pOldBitmap = dcMemory.SelectObject(&bmp);
	dcMemory.BitBlt(0, 0, width, height, pDC, 0, 0, SRCCOPY);

	BITMAP bmpInfo;
	bmp.GetBitmap(&bmpInfo);
	bmp.GetBitmapBits(width * height * (bmpInfo.bmBitsPixel / 8), image.GetData());
	int bpp = bmpInfo.bmBitsPixel / 8;

	dcMemory.SelectObject(pOldBitmap);

	ReleaseDC(pDC);
}

void CPreviewModelCtrl::SetClearColor(const ColorF& color)
{
	m_clearColor = color;
}

static int GetFaceCountRecursively(IStatObj* p)
{
	if (!p)
	{
		return 0;
	}
	int n = 0;
	if (p->GetRenderMesh())
	{
		n += p->GetRenderMesh()->GetIndicesCount() / 3;
	}
	for (int i = 0; i < p->GetSubObjectCount(); ++i)
	{
		IStatObj::SSubObject* const pS = p->GetSubObject(i);
		if (pS)
		{
			n += GetFaceCountRecursively(pS->pStatObj);
		}
	}
	return n;
}

static int GetVertexCountRecursively(IStatObj* p)
{
	if (!p)
	{
		return 0;
	}
	int n = 0;
	if (p->GetRenderMesh())
	{
		n += p->GetRenderMesh()->GetVerticesCount();
	}
	for (int i = 0; i < p->GetSubObjectCount(); ++i)
	{
		IStatObj::SSubObject* const pS = p->GetSubObject(i);
		if (pS)
		{
			n += GetVertexCountRecursively(pS->pStatObj);
		}
	}
	return n;
}

static int GetMaxLodRecursively(IStatObj* p)
{
	if (!p)
	{
		return 0;
	}
	int n = 0;
	for (int i = 1; i < 10; i++)
	{
		if (p->GetLodObject(i))
		{
			n = i;
		}
	}
	for (int i = 0; i < p->GetSubObjectCount(); ++i)
	{
		IStatObj::SSubObject* const pS = p->GetSubObject(i);
		if (pS)
		{
			const int n2 = GetMaxLodRecursively(pS->pStatObj);
			n = (n < n2) ? n2 : n;
		}
	}
	return n;
}

namespace
{
struct MaterialId
{
	const void* ptr;
	int         id;

	MaterialId(const void* a_ptr, int a_id)
		: ptr(a_ptr)
		, id(a_id)
	{
	}

	bool operator<(const MaterialId& a) const
	{
		return ptr < a.ptr || id < a.id;
	}
};
}

static void CollectMaterialsRecursively(std::set<MaterialId>& mats, IStatObj* p)
{
	if (!p)
	{
		return;
	}
	if (p->GetRenderMesh())
	{
		TRenderChunkArray& ch = p->GetRenderMesh()->GetChunks();
		for (size_t i = 0; i < ch.size(); ++i)
		{
			mats.insert(MaterialId(p->GetMaterial(), ch[i].m_nMatID));
		}
	}
	for (int i = 0; i < p->GetSubObjectCount(); ++i)
	{
		IStatObj::SSubObject* const pS = p->GetSubObject(i);
		if (pS)
		{
			CollectMaterialsRecursively(mats, pS->pStatObj);
		}
	}
}

int CPreviewModelCtrl::GetFaceCount()
{
	if (m_pObj)
	{
		return GetFaceCountRecursively(m_pObj);
	}
	else if (m_pCharacter)
	{

	}
	return 0;
}

int CPreviewModelCtrl::GetVertexCount()
{
	if (m_pObj)
	{
		return GetVertexCountRecursively(m_pObj);
	}
	else if (m_pCharacter)
	{

	}
	return 0;
}

int CPreviewModelCtrl::GetMaxLod()
{
	if (m_pObj)
	{
		return GetMaxLodRecursively(m_pObj);
	}
	else if (m_pCharacter)
	{
		return 1; //BaseModels have only 1 LOD
	}
	return 0;
}

int CPreviewModelCtrl::GetMtlCount()
{
	if (m_pObj)
	{
		std::set<MaterialId> mats;
		CollectMaterialsRecursively(mats, m_pObj);
		return (int)mats.size();
	}
	return 0;
}

void CPreviewModelCtrl::FitToScreen()
{
	SetCameraLookAt(2.0f, Vec3(1, 1, -0.5));
}

bool CPreviewModelCtrl::CheckVirtualKey(int virtualKey)
{
	GetAsyncKeyState(virtualKey);
	if (GetAsyncKeyState(virtualKey) & (1 << 15))
		return true;
	return false;
}

void CPreviewModelCtrl::ProcessKeys()
{
	if (GetFocus() != this)
		return;

	int moveSpeed = 1;

	Matrix34 m = m_camera.GetMatrix();

	Vec3 ydir = m.GetColumn2().GetNormalized();
	Vec3 xdir = m.GetColumn0().GetNormalized();

	Vec3 pos = m.GetTranslation();

	float speedScale = 60.0f * GetIEditor()->GetSystem()->GetITimer()->GetFrameTime();
	if (speedScale > 20) speedScale = 20;

	speedScale *= 0.04f;

	if (CheckVirtualKey(VK_SHIFT))
	{
		speedScale *= gViewportMovementPreferences.camFastMoveSpeed;
	}

	if (ViewportInteraction::CheckPolledKey(ViewportInteraction::eKey_Forward))
	{
		// move forward
		m_camera.SetPosition(pos + speedScale * moveSpeed * ydir);
		SetCamera(m_camera);
	}

	if (ViewportInteraction::CheckPolledKey(ViewportInteraction::eKey_Backward))
	{
		// move backward
		m_camera.SetPosition(pos - speedScale * moveSpeed * ydir);
		SetCamera(m_camera);
	}
}

BOOL CPreviewModelCtrl::PreTranslateMessage(MSG* pMsg)
{
	if (WM_KEYDOWN == pMsg->message || WM_KEYUP == pMsg->message)
		return TRUE;

	return CWnd::PreTranslateMessage(pMsg);
}

void CPreviewModelCtrl::SetBackgroundTexture(const CString& textureFilename)
{
	m_backgroundTextureId = GetIEditor()->GetIconManager()->GetIconTexture(textureFilename);
}

void CPreviewModelCtrl::DrawBackground()
{
	if (!m_backgroundTextureId)
		return;

	SVF_P3F_C4B_T2F tempVertices[6];
	SVF_P3F_C4B_T2F* pVertex = tempVertices;

	const float xpos = 0.0f;
	const float ypos = 0.0f;
	const float z = 0.0f;
	const uint32 color = 0xFFFFFFFF;
	const float w = 1.0f;
	const float h = 1.0f;
	const float s[4] = {0.0f, 1.0f, 1.0f, 0.0f};
	const float t[4] = {1.0f, 1.0f, 0.0f, 0.0f};

	pVertex->xyz.x = xpos;
	pVertex->xyz.y = ypos;
	pVertex->xyz.z = z;
	pVertex->st = Vec2(s[0], t[0]);
	pVertex->color.dcolor = color;

	++pVertex;

	pVertex->xyz.x = xpos + w;
	pVertex->xyz.y = ypos;
	pVertex->xyz.z = z;
	pVertex->st = Vec2(s[1], t[1]);
	pVertex->color.dcolor = color;

	++pVertex;

	pVertex->xyz.x = xpos;
	pVertex->xyz.y = ypos + h;
	pVertex->xyz.z = z;
	pVertex->st = Vec2(s[3], t[3]);
	pVertex->color.dcolor = color;

	++pVertex;

	pVertex->xyz.x = xpos;
	pVertex->xyz.y = ypos + h;
	pVertex->xyz.z = z;
	pVertex->st = Vec2(s[3], t[3]);
	pVertex->color.dcolor = color;

	++pVertex;

	pVertex->xyz.x = xpos + w;
	pVertex->xyz.y = ypos;
	pVertex->xyz.z = z;
	pVertex->st = Vec2(s[1], t[1]);
	pVertex->color.dcolor = color;

	++pVertex;

	pVertex->xyz.x = xpos + w;
	pVertex->xyz.y = ypos + h;
	pVertex->xyz.z = z;
	pVertex->st = Vec2(s[2], t[2]);
	pVertex->color.dcolor = color;

	SAuxGeomRenderFlags renderFlags;
	renderFlags.SetMode2D3DFlag(e_Mode2D);
	renderFlags.SetAlphaBlendMode(e_AlphaNone);
	renderFlags.SetDrawInFrontMode(e_DrawInFrontOff);
	renderFlags.SetFillMode(e_FillModeSolid);
	renderFlags.SetCullMode(e_CullModeNone);
	renderFlags.SetDepthWriteFlag(e_DepthWriteOff);
	renderFlags.SetDepthTestFlag(e_DepthTestOff);

	IRenderAuxGeom* aux = gEnv->pRenderer->GetIRenderAuxGeom();
	const SAuxGeomRenderFlags prevRenderFlags = aux->GetRenderFlags();
	aux->SetRenderFlags(renderFlags);
	aux->SetTexture(m_backgroundTextureId);

	aux->DrawBuffer(tempVertices, 6, true);

	aux->SetTexture(-1);
	aux->SetRenderFlags(prevRenderFlags);
}

