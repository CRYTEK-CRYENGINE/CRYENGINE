// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "MannPreferences.h"
#include "MannequinModelViewport.h"
#include "ICryMannequin.h"
#include <CryGame/IGameFramework.h>

#include "MannequinDialog.h"
#include "Util/MFCUtil.h"
#include "../../CryEngine/CryEntitySystem/RenderProxy.h"
#include "../objects/EntityObject.h"

#include "Dialogs/StringInputDialog.h"
#include "Preferences/ViewportPreferences.h"

const float CMannequinModelViewport::s_maxTweenTime = 0.5f;

/*
   The following code was in OnRender, commented out:

   static bool DEBUG_DRAW = true;

   if (DEBUG_DRAW && m_sequencePlayback)
   {
    m_sequencePlayback->DebugDraw();
   }

 */

CString GetUserOptionsRegKeyName(EMannequinEditorMode editorMode)
{
	CString keyName("Settings\\Mannequin\\");
	switch (editorMode)
	{
	case eMEM_FragmentEditor:
		keyName += "FragmentEditor";
		break;
	case eMEM_Previewer:
		keyName += "Previewer";
		break;
	case eMEM_TransitionEditor:
		keyName += "TransitionEditor";
		break;
	}
	return keyName + "UserOptions";
}

//////////////////////////////////////////////////////////////////////////
CMannequinModelViewport::CMannequinModelViewport(EMannequinEditorMode editorMode)
	:
	CModelViewport(GetUserOptionsRegKeyName(editorMode)),
	m_locMode(LM_Translate),
	m_selectedLocator(0xffffffff),
	m_viewmode(eVM_Unknown),
	m_draggingLocator(false),
	m_dragStartPoint(0, 0),
	m_LeftButtonDown(false),
	m_lookAtCamera(false),
	m_showSceneRoots(false),
	m_cameraKeyDown(false),
	m_playbackMultiplier(1.0f),
	m_tweenToFocusStart(ZERO),
	m_tweenToFocusDelta(ZERO),
	m_tweenToFocusTime(0.0f),
	m_editorMode(editorMode),
	m_pActionController(NULL),
	m_piGroundPlanePhysicalEntity(NULL),
	m_TickerMode(SEQTICK_INFRAMES),
	m_attachCameraToEntity(NULL),
	m_lastEntityPos(ZERO),
	m_pHoverBaseObject(NULL)
{
	m_camFOV = gViewportPreferences.defaultFOV;
	m_PhysicalLocation.SetIdentity();

	primitives::box box;
	box.center = Vec3(0.f, 0.f, -0.2f);
	box.size = Vec3(1000.f, 1000.f, 0.2f);
	box.Basis.SetIdentity();

	IGeometry* pPrimGeom = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::box::type, &box);
	phys_geometry* pGeom = gEnv->pPhysicalWorld->GetGeomManager()->RegisterGeometry(pPrimGeom, 0);

	m_piGroundPlanePhysicalEntity = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_STATIC);

	pe_geomparams geomParams;
	m_piGroundPlanePhysicalEntity->AddGeometry(pGeom, &geomParams);

	pe_params_pos posParams;
	posParams.pos = Vec3(-50.f, -50.f, 0.f);
	posParams.q = IDENTITY;
	m_piGroundPlanePhysicalEntity->SetParams(&posParams);

	pPrimGeom->Release();

	gEnv->pParticleManager->AddEventListener(this);
	gEnv->pGameFramework->GetMannequinInterface().AddMannequinGameListener(this);

	m_alternateCamera = GetCamera();

	m_arrAnimatedCharacterPath.resize(0x200, ZERO);
	m_arrSmoothEntityPath.resize(0x200, ZERO);

	m_arrRunStrafeSmoothing.resize(0x100);
	SetPlayerPos();

	m_bCanDrawWithoutLevelLoaded = true;
}

//////////////////////////////////////////////////////////////////////////
CMannequinModelViewport::~CMannequinModelViewport()
{
	gEnv->pParticleManager->RemoveEventListener(this);
	m_piGroundPlanePhysicalEntity->Release();
	gEnv->pPhysicalWorld->RemoveEventClient(EventPhysPostStep::id, OnPostStepLogged, 1);
	gEnv->pGameFramework->GetMannequinInterface().RemoveMannequinGameListener(this);
}

//////////////////////////////////////////////////////////////////////////
int CMannequinModelViewport::OnPostStepLogged(const EventPhys* pEvent)
{
	const EventPhysPostStep* pPostStep = static_cast<const EventPhysPostStep*>(pEvent);
	IEntity* piEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pPostStep->pEntity);
	piEntity->SetPosRotScale(pPostStep->pos, pPostStep->q, Vec3(1.f, 1.f, 1.f));
	return 1;
}

//////////////////////////////////////////////////////////////////////////
bool CMannequinModelViewport::UseAnimationDrivenMotionForEntity(const IEntity* piEntity)
{
	static bool addedClientEvent = false;
	if (piEntity)
	{
		IPhysicalEntity* piPhysicalEntity = piEntity->GetPhysics();
		if (piPhysicalEntity && (piPhysicalEntity->GetType() == PE_ARTICULATED))
		{
			if (!addedClientEvent)
			{
				addedClientEvent = true;
				piPhysicalEntity->GetWorld()->AddEventClient(EventPhysPostStep::id, OnPostStepLogged, 1);
			}

			return false;
		}
	}

	addedClientEvent = false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CMannequinModelViewport::Update()
{
	__super::Update();

	if (Deprecated::CheckVirtualKey('E'))
		m_locMode = LM_Rotate;
	if (Deprecated::CheckVirtualKey('W'))
		m_locMode = LM_Translate;
	if (Deprecated::CheckVirtualKey('L'))
	{
		m_cameraKeyDown = true;
	}
	else if (m_cameraKeyDown)
	{
		m_lookAtCamera = !m_lookAtCamera;
		m_cameraKeyDown = false;
	}
	if (Deprecated::CheckVirtualKey('P'))
	{
		AttachToEntity();
	}
	else if (Deprecated::CheckVirtualKey('O'))
	{
		DetachFromEntity();
	}

	if (m_tweenToFocusTime > 0.0f)
	{
		m_tweenToFocusTime = MAX(0.0f, m_tweenToFocusTime - gEnv->pTimer->GetFrameTime());
		float fProgress = (s_maxTweenTime - m_tweenToFocusTime) / s_maxTweenTime;
		m_Camera.SetPosition((m_tweenToFocusDelta * fProgress) + m_tweenToFocusStart);
	}
}

void CMannequinModelViewport::OnLButtonDown(UINT nFlags, CPoint point)
{

	m_HitContext.view = this;
	m_HitContext.b2DViewport = false;
	m_HitContext.point2d = point;
	ViewToWorldRay(point, m_HitContext.raySrc, m_HitContext.rayDir);
	m_HitContext.distanceTolerance = 0;
	m_LeftButtonDown = true;

	if (m_locMode == LM_Translate)
	{
		const uint32 numLocators = m_locators.size();
		for (uint32 i = 0; i < numLocators; ++i)
		{
			SLocator& locator = m_locators[i];

			Matrix34 m34 = GetLocatorWorldMatrix(locator);

			SetConstructionMatrix(m34);

			if (locator.m_AxisHelper.HitTest(m34, gGizmoPreferences, m_HitContext))
			{
				m_draggingLocator = true;
				m_selectedLocator = i;
				m_dragStartPoint = point;
			}
		}
	}
}

void CMannequinModelViewport::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (!m_draggingLocator)
	{
		CPoint mousePoint;
		GetCursorPos(&mousePoint);
		ScreenToClient(&mousePoint);
		HitContext hc;
		hc.view = this;
		hc.point2d = mousePoint;
		hc.camera = &m_Camera;
		Vec3 raySrc(0, 0, 0), rayDir(1, 0, 0);
		ViewToWorldRay(mousePoint, hc.raySrc, hc.rayDir);
		HitTest(hc, true);
	}
	m_draggingLocator = false;
	m_LeftButtonDown = false;

	m_selectedLocator = 0xffffffff;
}

void CMannequinModelViewport::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_bInMoveMode)
	{
		// End the focus tween if in progress and the middle mouse button has been pressed
		m_tweenToFocusTime = 0.0f;
	}

	if (m_locMode == LM_Rotate)
	{
		//calculate the Ray for camera-pos to mouse-cursor
		CRect rct;
		GetClientRect(rct);
		f32 RresX = (f32)(rct.right - rct.left);
		f32 RresY = (f32)(rct.bottom - rct.top);
		f32 MiddleX = RresX / 2.0f;
		f32 MiddleY = RresY / 2.0f;
		Vec3 MoursePos3D = Vec3(point.x - MiddleX, m_Camera.GetEdgeP().y, -point.y + MiddleY) * Matrix33(m_Camera.GetViewMatrix()) + m_Camera.GetPosition();
		Ray mray(m_Camera.GetPosition(), (MoursePos3D - m_Camera.GetPosition()).GetNormalized());

		const uint32 numLocators = m_locators.size();
		for (uint32 i = 0; i < numLocators; i++)
		{
			SLocator& locator = m_locators[i];

			if (locator.m_ArcBall.ArcControl(GetLocatorReferenceMatrix(locator), mray, m_LeftButtonDown))
			{
				Matrix34 m34 = GetLocatorWorldMatrix(locator);

				SetConstructionMatrix(m34);

				CMannequinDialog::GetCurrentInstance()->OnMoveLocator(m_editorMode, locator.m_refID, locator.m_name, QuatT(locator.m_ArcBall.ObjectRotation, locator.m_ArcBall.sphere.center));
			}
		}
	}
	else
	{
		if (m_draggingLocator && m_selectedLocator < m_locators.size())
		{
			SLocator& locator = m_locators[m_selectedLocator];

			int axis = locator.m_AxisHelper.GetHighlightAxis();

			if (m_locMode == LM_Translate)
			{
				Matrix34 constructionMatrix = GetLocatorWorldMatrix(locator);

				SetConstructionMatrix(constructionMatrix);

				Vec3 p1 = MapViewToCP(m_dragStartPoint, axis);
				Vec3 p2 = MapViewToCP(point, axis);
				if (!p1.IsZero() && !p2.IsZero())
				{
					Vec3 v = GetCPVector(p1, p2, axis);

					m_dragStartPoint = point;

					Vec3 pos = constructionMatrix.GetTranslation() + v;

					Matrix34 mInvert = GetLocatorReferenceMatrix(locator).GetInverted();
					pos = mInvert * pos;
					locator.m_ArcBall.sphere.center = pos;

					CMannequinDialog::GetCurrentInstance()->OnMoveLocator(m_editorMode, locator.m_refID, locator.m_name, QuatT(locator.m_ArcBall.ObjectRotation, locator.m_ArcBall.sphere.center));
				}
			}
		}
		else
		{
			CPoint mousePoint;
			GetCursorPos(&mousePoint);
			ScreenToClient(&mousePoint);
			HitContext hc;
			hc.view = this;
			hc.point2d = mousePoint;
			hc.camera = &m_Camera;
			Vec3 raySrc(0, 0, 0), rayDir(1, 0, 0);
			ViewToWorldRay(mousePoint, hc.raySrc, hc.rayDir);
			HitTest(hc, false);
		}
	}
}

void CMannequinModelViewport::UpdatePropEntities(SMannequinContexts* pContexts, SMannequinContexts::SProp& prop)
{
	for (uint32 em = 0; em < eMEM_Max; em++)
	{
		const SMannequinContextViewData::TBackgroundObjects& backgroundObjects = pContexts->viewData[em].backgroundObjects;
		const uint32 numBGObjects = backgroundObjects.size();
		for (uint32 i = 0; i < numBGObjects; i++)
		{
			const CBaseObject* pBaseObject = backgroundObjects[i];
			if (pBaseObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
			{
				if (prop.entity == pBaseObject->GetName().GetString())
				{
					CEntityObject* pEntObject = (CEntityObject*)pBaseObject;
					prop.entityID[em] = pEntObject->GetIEntity()->GetId();
					break;
				}
			}
		}
	}
}

bool CMannequinModelViewport::HitTest(HitContext& hc, const bool bIsClick)
{
	CMannequinDialog* pMannequinDialog = CMannequinDialog::GetCurrentInstance();
	SMannequinContexts* pContexts = pMannequinDialog->Contexts();
	SMannequinContextViewData::TBackgroundObjects& backgroundObjects = pContexts->viewData[m_editorMode].backgroundObjects;
	const uint32 numBGObjects = backgroundObjects.size();
	float minDist = FLT_MAX;
	CBaseObject* pBestObject = NULL;
	for (uint32 i = 0; i < numBGObjects; i++)
	{
		CBaseObject* pBaseObject = backgroundObjects[i];
		if (pBaseObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			if (pBaseObject->HitTest(hc))
			{
				if (hc.dist < minDist)
				{
					minDist = hc.dist;
					pBestObject = pBaseObject;
				}
			}
		}
	}

	if (pBestObject)
	{
		if (bIsClick)
		{
			CString initialProp = "";
			for (uint32 i = 0; i < pContexts->backgroundProps.size(); i++)
			{
				SMannequinContexts::SProp& prop = pContexts->backgroundProps[i];
				if (prop.entity == pBestObject->GetName().GetString())
				{
					initialProp = prop.name;
					break;
				}
			}

			CString pickerTitle("Set Prop Name for ");
			pickerTitle += pBestObject->GetName();
			CStringInputDialog dlgEnterPropName(initialProp, pickerTitle);
			if (dlgEnterPropName.DoModal())
			{
				CString newName = dlgEnterPropName.GetResultingText();
				bool inserted = newName == "";

				for (uint32 i = 0; i < pContexts->backgroundProps.size(); i++)
				{
					SMannequinContexts::SProp& prop = pContexts->backgroundProps[i];
					if (prop.entity == pBestObject->GetName().GetString())
					{
						if (inserted)
						{
							pContexts->backgroundProps.erase(pContexts->backgroundProps.begin() + i);
							break;
						}
						else
						{
							prop.name = newName;
							inserted = true;
						}
					}
					else if (prop.name == newName)
					{
						if (inserted)
						{
							pContexts->backgroundProps.erase(pContexts->backgroundProps.begin() + i);
							break;
						}
						else
						{
							prop.entity = pBestObject->GetName();
							inserted = true;
							UpdatePropEntities(pContexts, prop);
						}
					}
				}

				if (!inserted)
				{
					SMannequinContexts::SProp prop;
					prop.entity = pBestObject->GetName();
					prop.name = newName;
					UpdatePropEntities(pContexts, prop);
					pContexts->backgroundProps.push_back(prop);
				}

				pMannequinDialog->ResavePreviewFile();
			}
		}
	}

	m_pHoverBaseObject = pBestObject;

	return false;
}

void CMannequinModelViewport::OnRender()
{
	if (CMannequinDialog::GetCurrentInstance())
	{
		CMannequinDialog::GetCurrentInstance()->OnRender();
	}

	IRenderer* renderer = GetIEditorImpl()->GetRenderer();
	IRenderAuxGeom* pAuxGeom = renderer->GetIRenderAuxGeom();
	SAuxGeomRenderFlags previousFlags = pAuxGeom->GetRenderFlags();
	SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
	renderFlags.SetFillMode(e_FillModeSolid);
	pAuxGeom->SetRenderFlags(renderFlags);

	int mode = (m_locMode == LM_Translate) ? CAxisHelper::MOVE_FLAG : CAxisHelper::ROTATE_FLAG;
	const uint32 numLocators = m_locators.size();
	for (uint32 i = 0; i < numLocators; i++)
	{
		SLocator& locator = m_locators[i];

		if (locator.m_pEntity != NULL)
		{
			pAuxGeom->DrawSphere(locator.m_pEntity->GetWorldTM().GetTranslation(), 0.1f, ColorB());
		}

		if (locator.m_AxisHelper.GetHighlightAxis() == 0)
			locator.m_AxisHelper.SetHighlightAxis(GetAxisConstrain());

		locator.m_AxisHelper.SetMode(mode);

		if (m_locMode == LM_Rotate)
		{
			locator.m_ArcBall.DrawSphere(GetLocatorReferenceMatrix(locator), GetCamera(), pAuxGeom);
		}
		else
		{
			Matrix34 m34 = GetLocatorWorldMatrix(locator);

			if (GetIEditorImpl()->GetReferenceCoordSys() == COORDS_WORLD)
			{
				m34.SetRotation33(IDENTITY);
			}

			Vec3 groundProj(m34.GetTranslation());
			groundProj.z = 0.0f;
			pAuxGeom->DrawLine(m34.GetTranslation(), RGBA8(0x00, 0x00, 0xff, 0x00), groundProj, RGBA8(0xff, 0x00, 0x00, 0x00));
			pAuxGeom->DrawLine(groundProj - Vec3(0.2f, 0.0f, 0.0f), RGBA8(0xff, 0x00, 0x00, 0x00), groundProj + Vec3(0.2f, 0.0f, 0.0f), RGBA8(0xff, 0x00, 0x00, 0x00));
			pAuxGeom->DrawLine(groundProj - Vec3(0.0f, 0.2f, 0.0f), RGBA8(0xff, 0x00, 0x00, 0x00), groundProj + Vec3(0.0f, 0.2f, 0.0f), RGBA8(0xff, 0x00, 0x00, 0x00));

			locator.m_AxisHelper.DrawAxis(m34, gGizmoPreferences, m_displayContext);
		}
	}

	if (mv_AttachCamera)
	{
		const int screenWidth  = std::max(pAuxGeom->GetCamera().GetViewSurfaceX(), 1);
		const int screenHeight = std::max(pAuxGeom->GetCamera().GetViewSurfaceZ(), 1);
		static bool showCrosshair = true;
		static bool showSafeZone = true;
		const float targetRatio = 16.0f / 9.0f;
		static float crossHairLen = 25.0f;

		const ColorB crosshairColour = ColorB(192, 192, 192);
		const ColorB edgeColour = ColorB(192, 192, 192, 60);

		renderFlags.SetDepthTestFlag(e_DepthTestOff);
		renderFlags.SetMode2D3DFlag(e_Mode2D);
		pAuxGeom->SetRenderFlags(renderFlags);

		if (showCrosshair)
		{
			float adjustedHeight = crossHairLen / (float)screenHeight;
			float adjustedWidth = crossHairLen / (float)screenWidth;

			pAuxGeom->DrawLine(
			  Vec3(0.5f - adjustedWidth, 0.5f, 0.0f), crosshairColour,
			  Vec3(0.5f + adjustedWidth, 0.5f, 0.0f), crosshairColour);
			pAuxGeom->DrawLine(
			  Vec3(0.5f, 0.5f - adjustedHeight, 0.0f), crosshairColour,
			  Vec3(0.5f, 0.5f + adjustedHeight, 0.0f), crosshairColour);
		}

		if (showSafeZone)
		{
			float targetX = (float)screenHeight * targetRatio;
			float edgeWidth = (float)screenWidth - targetX;
			if (edgeWidth > 0.0f)
			{
				float edgePercent = (edgeWidth / (float)screenWidth) / 2.0f;

				float innerX = edgePercent;
				float upperX = 1.0f - edgePercent;
				pAuxGeom->DrawLine(
				  Vec3(innerX, 0.0f, 0.0f), crosshairColour,
				  Vec3(innerX, 1.0f, 0.0f), crosshairColour);
				pAuxGeom->DrawLine(
				  Vec3(upperX, 0.0f, 0.0f), crosshairColour,
				  Vec3(upperX, 1.0f, 0.0f), crosshairColour);

				renderFlags.SetAlphaBlendMode(e_AlphaBlended);
				pAuxGeom->SetRenderFlags(renderFlags);

				pAuxGeom->DrawTriangle(Vec3(0.0f, 1.0f, 0.0f), edgeColour, Vec3(innerX, 1.0f, 0.0f), edgeColour, Vec3(0.0f, 0.0f, 0.0f), edgeColour);
				pAuxGeom->DrawTriangle(Vec3(innerX, 1.0f, 0.0f), edgeColour, Vec3(innerX, 0.0f, 0.0f), edgeColour, Vec3(0.0f, 0.0f, 0.0f), edgeColour);

				pAuxGeom->DrawTriangle(Vec3(upperX, 1.0f, 0.0f), edgeColour, Vec3(1.0f, 1.0f, 0.0f), edgeColour, Vec3(upperX, 0.0f, 0.0f), edgeColour);
				pAuxGeom->DrawTriangle(Vec3(upperX, 0.0f, 0.0f), edgeColour, Vec3(1.0f, 1.0f, 0.0f), edgeColour, Vec3(1.0f, 0.0f, 0.0f), edgeColour);
			}
		}
	}
	pAuxGeom->SetRenderFlags(previousFlags);

	const float x = float(pAuxGeom->GetCamera().GetViewSurfaceX() - 5);
	const float y = float(pAuxGeom->GetCamera().GetViewSurfaceZ());

	if (mv_showGrid)
	{
		const bool bInstantCommit = false; // add grid to render-queue; i.e. disable instant rendering
		DrawFloorGrid(Quat(IDENTITY), Vec3(ZERO), Matrix33(IDENTITY), bInstantCommit);
	}

	if (m_TickerMode == SEQTICK_INFRAMES)
		gEnv->p3DEngine->DrawTextRightAligned(x, y - 20, "Timeline: Frames");
	else
		gEnv->p3DEngine->DrawTextRightAligned(x, y - 20, "Timeline: Seconds");

	float ypos = y - 20;
	if (m_lookAtCamera)
	{
		ypos -= 20;
		gEnv->p3DEngine->DrawTextRightAligned(x, ypos, "Looking at camera");
	}
	if (m_attachCameraToEntity)
	{
		ypos -= 20;
		gEnv->p3DEngine->DrawTextRightAligned(x, ypos, "Tracking %s", m_attachCameraToEntity->GetName());
	}

	if (m_pHoverBaseObject)
	{
		const char* currentPropName = "Click to Assign Prop";
		Vec3 pos = m_pHoverBaseObject->GetPos();
		if (m_pHoverBaseObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* pEntObject = (CEntityObject*)m_pHoverBaseObject;
			if (pEntObject->GetIEntity())
			{
				pos = pEntObject->GetIEntity()->GetPos();

				CMannequinDialog* pMannequinDialog = CMannequinDialog::GetCurrentInstance();
				SMannequinContexts* pContexts = pMannequinDialog->Contexts();
				const uint32 numProps = pContexts->backgroundProps.size();
				for (uint32 i = 0; i < numProps; i++)
				{
					if (pContexts->backgroundProps[i].entityID[m_editorMode] == pEntObject->GetEntityId())
					{
						currentPropName = pContexts->backgroundProps[i].name;
						break;
					}
				}

				ColorB boxCol(0, 255, 255, 255);
				AABB bbox;
				pEntObject->GetLocalBounds(bbox);
				pAuxGeom->DrawAABB(bbox, pEntObject->GetIEntity()->GetWorldTM(), false, boxCol, eBBD_Faceted);
			}
		}

		float colour[] = { 0.0f, 1.0f, 1.0f, 1.0f };
		IRenderAuxText::DrawLabelExF(pos, 1.5f, colour, true, true, "%s\n%s", m_pHoverBaseObject->GetName(), currentPropName);
	}

	UpdateAnimation(m_playbackMultiplier * gEnv->pTimer->GetFrameTime());

	__super::OnRender();
}

void CMannequinModelViewport::SetTimelineUnits(ESequencerTickMode mode)
{
	m_TickerMode = mode;
}

void CMannequinModelViewport::Focus(AABB& pBoundingBox)
{
	// Parent class handles focusing on bones...if about to happen, abort
	if (m_highlightedBoneID >= 0)
	{
		return;
	}

	// If currently panning the view, then abort
	if (m_bInMoveMode)
	{
		return;
	}

	// Don't want to move the first person camera
	if (IsCameraAttached())
	{
		ToggleCamera();
	}

	float fRadius = pBoundingBox.GetRadius();
	float fFov = m_Camera.GetFov();
	float fDistance = fRadius / (tan(fFov / 2));
	Vec3 center = pBoundingBox.GetCenter();
	Vec3 viewDir = m_Camera.GetViewdir();

	m_tweenToFocusStart = m_Camera.GetPosition();
	m_tweenToFocusDelta = (center + (-(viewDir.normalized()) * fDistance)) - m_tweenToFocusStart;
	m_tweenToFocusTime = s_maxTweenTime;
}

void CMannequinModelViewport::SetPlayerPos()
{
	Matrix34 m = GetViewTM();
	m.SetTranslation(m.GetTranslation() - m_PhysicalLocation.t);
	SetViewTM(m);

	m_AverageFrameTime = 0.14f;

	m_PhysicalLocation.SetIdentity();

	m_LocalEntityMat.SetIdentity();
	m_PrevLocalEntityMat.SetIdentity();

	m_absCameraHigh = 2.0f;
	m_absCameraPos = Vec3(0, 3, 2);
	m_absCameraPosVP = Vec3(0, -3, 1.5);

	m_absCurrentSlope = 0.0f;

	m_absLookDirectionXY = Vec2(0, 1);

	m_LookAt = Vec3(ZERO);
	m_LookAtRate = Vec3(ZERO);
	m_vCamPos = Vec3(ZERO);
	m_vCamPosRate = Vec3(ZERO);

	m_relCameraRotX = 0;
	m_relCameraRotZ = 0;

	uint32 numSample6 = m_arrAnimatedCharacterPath.size();
	for (uint32 i = 0; i < numSample6; i++)
		m_arrAnimatedCharacterPath[i] = Vec3(ZERO);

	numSample6 = m_arrSmoothEntityPath.size();
	for (uint32 i = 0; i < numSample6; i++)
		m_arrSmoothEntityPath[i] = Vec3(ZERO);

	uint32 numSample7 = m_arrRunStrafeSmoothing.size();
	for (uint32 i = 0; i < numSample7; i++)
	{
		m_arrRunStrafeSmoothing[i] = 0;
	}

	m_vWorldDesiredBodyDirection = Vec2(0, 1);
	m_vWorldDesiredBodyDirectionSmooth = Vec2(0, 1);
	m_vWorldDesiredBodyDirectionSmoothRate = Vec2(0, 1);

	m_vWorldDesiredBodyDirection2 = Vec2(0, 1);

	m_vWorldDesiredMoveDirection = Vec2(0, 1);
	m_vWorldDesiredMoveDirectionSmooth = Vec2(0, 1);
	m_vWorldDesiredMoveDirectionSmoothRate = Vec2(0, 1);
	m_vLocalDesiredMoveDirection = Vec2(0, 1);
	m_vLocalDesiredMoveDirectionSmooth = Vec2(0, 1);
	m_vLocalDesiredMoveDirectionSmoothRate = Vec2(0, 1);

	m_vWorldAimBodyDirection = Vec2(0, 1);

	m_MoveSpeedMSec = 5.0f;
	m_key_W = 0;
	m_keyrcr_W = 0;
	m_key_S = 0;
	m_keyrcr_S = 0;
	m_key_A = 0;
	m_keyrcr_A = 0;
	m_key_D = 0;
	m_keyrcr_D = 0;
	m_key_SPACE = 0;
	m_keyrcr_SPACE = 0;
	m_ControllMode = 0;

	m_State = -1;

	m_udGround = 0.0f;
	m_lrGround = 0.0f;
	AABB aabb = AABB(Vec3(-40.0f, -40.0f, -0.25f), Vec3(+40.0f, +40.0f, +0.0f));
	m_GroundOBB = OBB::CreateOBBfromAABB(Matrix33(IDENTITY), aabb);
	m_GroundOBBPos = Vec3(0, 0, -0.01f);
}

void CMannequinModelViewport::ClearLocators()
{
	m_locators.resize(0);
}

void CMannequinModelViewport::AddLocator(uint32 refID, const char* name, const QuatT& transform, IEntity* pEntity, int16 refJointId, IAttachment* pAttachment, uint32 paramCRC, string helperName)
{
	const uint32 numLocators = m_locators.size();
	SLocator* pLocator = NULL;
	for (uint32 i = 0; i < numLocators; i++)
	{
		SLocator& locator = m_locators[i];
		if (locator.m_refID == refID)
		{
			pLocator = &locator;
			break;
		}
	}

	if (pLocator == NULL)
	{
		m_locators.resize(numLocators + 1);
		pLocator = &m_locators[numLocators];
		pLocator->m_name = name;
		pLocator->m_refID = refID;
		pLocator->m_pEntity = pEntity;
		pLocator->m_jointId = refJointId;
		pLocator->m_pAttachment = pAttachment;
		pLocator->m_paramCRC = paramCRC;
		pLocator->m_helperName = helperName;
		pLocator->m_ArcBall.InitArcBall();
	}

	pLocator->m_ArcBall.DragRotation.SetIdentity();
	pLocator->m_ArcBall.ObjectRotation = transform.q;
	pLocator->m_ArcBall.sphere.center = transform.t;
}

Matrix34 CMannequinModelViewport::GetLocatorReferenceMatrix(const SLocator& locator)
{
	if (locator.m_pAttachment != NULL)
	{
		if (!locator.m_helperName.empty())
		{
			IAttachmentObject* pAttachObject = locator.m_pAttachment->GetIAttachmentObject();
			IStatObj* pStatObj = pAttachObject ? pAttachObject->GetIStatObj() : NULL;
			if (!pStatObj && pAttachObject && (pAttachObject->GetAttachmentType() == IAttachmentObject::eAttachment_Entity))
			{
				CEntityAttachment* pEntAttachment = (CEntityAttachment*)pAttachObject;
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(pEntAttachment->GetEntityId());
				if (pEntity)
				{
					pStatObj = pEntity->GetStatObj(0);
				}
			}
			if (pStatObj)
			{
				return Matrix34(locator.m_pAttachment->GetAttWorldAbsolute()) * pStatObj->GetHelperTM(locator.m_helperName);
			}
		}
		return Matrix34(locator.m_pAttachment->GetAttWorldAbsolute());
	}
	else if (locator.m_jointId > -1 && locator.m_pEntity != NULL)
	{
		ICharacterInstance* pCharInstance = locator.m_pEntity->GetCharacter(0);
		CRY_ASSERT(pCharInstance);

		ISkeletonPose& skeletonPose = *pCharInstance->GetISkeletonPose();

		Matrix34 world = Matrix34(locator.m_pEntity->GetWorldTM());
		Matrix34 joint = Matrix34(skeletonPose.GetAbsJointByID(locator.m_jointId));

		return world * joint;
	}
	else if (locator.m_paramCRC != 0)
	{
		if (!locator.m_helperName.empty())
		{
			EntityId attachEntityId;
			m_pActionController->GetParam(locator.m_paramCRC, attachEntityId);

			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(attachEntityId);
			if (pEntity)
			{
				IStatObj* pStatObj = pEntity->GetStatObj(0);

				return pEntity->GetWorldTM() * pStatObj->GetHelperTM(locator.m_helperName);
				;
			}
		}

		QuatT location;
		if (m_pActionController->GetParam(locator.m_paramCRC, location))
		{
			return Matrix34(location);
		}
		else
		{
			return Matrix34(IDENTITY);
		}
	}
	else
	{
		return Matrix34(IDENTITY);
	}
}

Matrix34 CMannequinModelViewport::GetLocatorWorldMatrix(const SLocator& locator)
{
	return GetLocatorReferenceMatrix(locator) * Matrix34(Matrix33(locator.m_ArcBall.DragRotation * locator.m_ArcBall.ObjectRotation), locator.m_ArcBall.sphere.center);
}

void CMannequinModelViewport::UpdateCharacter(IEntity* pEntity, ICharacterInstance* pInstance, float deltaTime)
{
	ISkeletonAnim& skeletonAnimation = *pInstance->GetISkeletonAnim();
	ISkeletonPose& skeletonPose = *pInstance->GetISkeletonPose();

	pInstance->SetCharEditMode(CA_CharacterTool);
	skeletonPose.SetForceSkeletonUpdate(1);

	//int AnimEventCallback(ICharacterInstance* pInstance,void* pPlayer);
	//skeletonAnimation.SetEventCallback(AnimEventCallback,this);

	const bool useAnimationDrivenMotion = UseAnimationDrivenMotionForEntity(pEntity);
	skeletonAnimation.SetAnimationDrivenMotion(useAnimationDrivenMotion);

	QuatT physicalLocation(pEntity->GetRotation(), pEntity->GetPos());

	const CCamera& camera = GetCamera();
	float fDistance = (camera.GetPosition() - physicalLocation.t).GetLength();
	float fZoomFactor = 0.001f + 0.999f * (RAD2DEG(camera.GetFov()) / 60.f);

	SAnimationProcessParams params;
	params.locationAnimation = physicalLocation;
	params.bOnRender = 0;
	params.zoomAdjustedDistanceFromCamera = fDistance * fZoomFactor;
	params.overrideDeltaTime = deltaTime;
	pInstance->StartAnimationProcessing(params);
	QuatT relMove = skeletonAnimation.GetRelMovement();
	pInstance->FinishAnimationComputations();

	physicalLocation = physicalLocation * relMove;
	physicalLocation.q.Normalize();

	pEntity->SetRotation(physicalLocation.q);
	pEntity->SetPos(physicalLocation.t);

	if (pEntity == m_attachCameraToEntity)
	{
		const Vec3 movement = pEntity->GetPos() - m_lastEntityPos;
		m_lastEntityPos = pEntity->GetPos();
		Matrix34 viewTM = GetViewTM();
		viewTM.SetTranslation(viewTM.GetTranslation() + movement);
		SetViewTM(viewTM);
	}
}

void CMannequinModelViewport::UpdateAnimation(float timePassed)
{
	gEnv->pGameFramework->GetMannequinInterface().SetSilentPlaybackMode(m_bPaused);
	if (m_pActionController)
	{
		m_pActionController->Update(timePassed);
	}
	CMannequinDialog* pMannequinDialog = CMannequinDialog::GetCurrentInstance();
	SMannequinContexts* pContexts = pMannequinDialog->Contexts();
	const uint32 numContextDatums = pContexts->m_contextData.size();
	for (uint32 i = 0; i < numContextDatums; i++)
	{
		SScopeContextData& contextData = pContexts->m_contextData[i];
		if (contextData.viewData[m_editorMode].enabled && contextData.viewData[m_editorMode].m_pActionController)
		{
			contextData.viewData[m_editorMode].m_pActionController->Update(timePassed);
		}
	}

	uint32 numChars = m_entityList.size();

	//--- Update the inputs after any animation installations
	if (numChars > 0)
	{
		IEntity* pEntity = m_entityList[0].GetEntity();
		if (pEntity != NULL)
		{
			ICharacterInstance* pInstance = pEntity->GetCharacter(0);
			if (pInstance)
			{
				UpdateInput(pInstance, timePassed, m_pActionController);
			}
		}
	}

	for (uint32 i = 0; i < numChars; i++)
	{
		IEntity* pEntity = m_entityList[i].GetEntity();
		if (pEntity && !pEntity->IsHidden())
		{
			ICharacterInstance* pInstance = pEntity->GetCharacter(0);
			if (pInstance)
			{
				UpdateCharacter(pEntity, pInstance, timePassed);

				IAttachmentManager* pIAttachmentManager = pInstance->GetIAttachmentManager();
				if (pIAttachmentManager && (i == 0))
				{
					IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByName("#camera");
					if (pIAttachment)
					{
						const QuatT physicalLocation(pEntity->GetRotation(), pEntity->GetPos());
						QuatTS qt = physicalLocation * pIAttachment->GetAttModelRelative();
						if (mv_AttachCamera)
						{
							SetViewTM(Matrix34(qt));
							SetFirstperson(pIAttachmentManager, eVM_FirstPerson);
						}
						else
						{
							//--- TODO: Add in an option to render this!
							//IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
							//pAuxGeom->DrawCone(qt.t, qt.q.GetColumn1(), 0.15f, 0.5f, RGBA8(0x00,0xff,0x00,0x60));

							SetFirstperson(pIAttachmentManager, eVM_ThirdPerson);
						}
					}
				}

				if (m_lookAtCamera && !mv_AttachCamera)
				{
					IAnimationPoseBlenderDir* pAim = pInstance->GetISkeletonPose()->GetIPoseBlenderAim();

					const Vec3 lookTarget = GetViewTM().GetTranslation();
					if (pAim)
					{
						pAim->SetTarget(lookTarget);
					}

					QuatT lookTargetQT(Quat(IDENTITY), lookTarget);
					m_pActionController->SetParam("AimTarget", lookTargetQT);
					m_pActionController->SetParam("LookTarget", lookTargetQT);

					for (uint32 i = 0; i < numContextDatums; i++)
					{
						SScopeContextData& contextData = pContexts->m_contextData[i];
						if (contextData.viewData[m_editorMode].enabled && contextData.viewData[m_editorMode].m_pActionController)
						{
							contextData.viewData[m_editorMode].m_pActionController->SetParam("AimTarget", lookTargetQT);
							contextData.viewData[m_editorMode].m_pActionController->SetParam("LookTarget", lookTargetQT);
						}
					}
				}
			}
		}
	}
	gEnv->pGameFramework->GetMannequinInterface().SetSilentPlaybackMode(false);
}

void CMannequinModelViewport::SetFirstperson(IAttachmentManager* pAttachmentManager, EMannequinViewMode viewmode)
{
	if (viewmode != m_viewmode)
	{
		m_viewmode = viewmode;
		bool isFirstPerson = viewmode == eVM_FirstPerson;
		int numAttachments = pAttachmentManager->GetAttachmentCount();

		for (int i = 0; i < numAttachments; i++)
		{
			if (IAttachment* pAttachment = pAttachmentManager->GetInterfaceByIndex(i))
			{
				const char* pAttachmentName = pAttachment->GetName();

				if (strstr(pAttachmentName, "_tp") != 0)
				{
					pAttachment->HideAttachment(isFirstPerson);
				}
				else if (strstr(pAttachmentName, "_fp") != 0)
				{
					pAttachment->HideAttachment(!isFirstPerson);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannequinModelViewport::DrawEntityAndChildren(CEntityObject* pEntityObject, const SRendParams& rp, const SRenderingPassInfo& passInfo)
{
	IEntity* pEntity = pEntityObject->GetIEntity();
	if (pEntity)
	{
		if (IEntityRender* pIEntityRender = pEntity->GetRenderInterface())
		{
			if (IRenderNode* pRenderNode = pIEntityRender->GetRenderNode())
			{
				pRenderNode->Render(rp, passInfo);
			}
		}

		const int childEntityCount = pEntity->GetChildCount();
		for (int idx = 0; idx < childEntityCount; ++idx)
		{
			if (IEntity* pChild = pEntity->GetChild(idx))
			{
				if (IEntityRender* pIEntityRender = pChild->GetRenderInterface())
				{
					if (IRenderNode* pRenderNode = pIEntityRender->GetRenderNode())
					{
						pRenderNode->Render(rp, passInfo);
					}
				}
			}
		}
	}

	const int childObjectCount = pEntityObject->GetChildCount();
	for (int idx = 0; idx < childObjectCount; ++idx)
	{
		if (CBaseObject* pObjectChild = pEntityObject->GetChild(idx))
		{
			if (pObjectChild->IsKindOf(RUNTIME_CLASS(CEntityObject)))
			{
				DrawEntityAndChildren((CEntityObject*)pObjectChild, rp, passInfo);
			}

			IRenderNode* pRenderNode = pObjectChild->GetEngineNode();
			if (pRenderNode)
			{
				pRenderNode->Render(rp, passInfo);
			}
		}
	}
}

void CMannequinModelViewport::DrawCharacter(ICharacterInstance* pInstance, const SRendParams& rRP, const SRenderingPassInfo& passInfo)
{
	f32 FrameTime = GetIEditorImpl()->GetSystem()->GetITimer()->GetFrameTime();
	m_AverageFrameTime = pInstance->GetAverageFrameTime();

	GetIEditorImpl()->GetSystem()->GetIConsole()->GetCVar("ca_DrawLocator")->Set(mv_showLocator);
	GetIEditorImpl()->GetSystem()->GetIConsole()->GetCVar("ca_DrawSkeleton")->Set(mv_showSkeleton);

	CMannequinDialog* pMannequinDialog = CMannequinDialog::GetCurrentInstance();
	SMannequinContexts* pContexts = pMannequinDialog->Contexts();
	SMannequinContextViewData::TBackgroundObjects& backgroundObjects = pContexts->viewData[m_editorMode].backgroundObjects;
	const uint32 numBGObjects = backgroundObjects.size();
	for (uint32 i = 0; i < numBGObjects; i++)
	{
		CBaseObject* pBaseObject = backgroundObjects[i];
		if (pBaseObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* pEntObject = (CEntityObject*)pBaseObject;
			DrawEntityAndChildren(pEntObject, rRP, passInfo);
		}
		else
		{
			IRenderNode* pRenderNode = pBaseObject->GetEngineNode();
			if (pRenderNode)
			{
				pRenderNode->Render(rRP, passInfo);
			}
		}
	}

	const uint32 numChars = m_entityList.size();
	for (uint32 i = 0; i < numChars; i++)
	{
		IEntity* pEntity = m_entityList[i].GetEntity();
		if (pEntity && !pEntity->IsHidden())
		{
			ICharacterInstance* charInst = pEntity->GetCharacter(0);
			IStatObj* statObj = pEntity->GetStatObj(0);
			if (charInst || statObj)
			{
				DrawCharacter(pEntity, charInst, statObj, rRP, passInfo);
			}
		}
	}

	if (m_particleEmitters.empty() == false)
	{
		for (std::vector<IParticleEmitter*>::iterator itEmitters = m_particleEmitters.begin(); itEmitters != m_particleEmitters.end(); )
		{
			IParticleEmitter* pEmitter = *itEmitters;
			if (pEmitter->IsAlive())
			{
				pEmitter->Update();
				pEmitter->Render(rRP, passInfo);
				++itEmitters;
			}
			else
			{
				itEmitters = m_particleEmitters.erase(itEmitters);
			}
		}
	}

}

uint32 g_ypos;

void CMannequinModelViewport::OnCreateEmitter(IParticleEmitter* pEmitter)
{
}

void CMannequinModelViewport::OnDeleteEmitter(IParticleEmitter* pEmitter)
{
	m_particleEmitters.erase(std::remove(m_particleEmitters.begin(), m_particleEmitters.end(), pEmitter), m_particleEmitters.end());
}

void CMannequinModelViewport::OnSpawnParticleEmitter(IParticleEmitter* pEmitter, IActionController& actionController)
{
	if (&actionController == m_pActionController)
	{
		auto sp = pEmitter->GetSpawnParams();
		sp.bNowhere = true;
		pEmitter->SetSpawnParams(sp);
		m_particleEmitters.push_back(pEmitter);
	}
}

void CMannequinModelViewport::DrawCharacter(IEntity* pEntity, ICharacterInstance* pInstance, IStatObj* pStatObj, const SRendParams& rRP, const SRenderingPassInfo& passInfo)
{
	const QuatT physicalLocation(pEntity->GetRotation(), pEntity->GetPos());

	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
	pAuxGeom->SetRenderFlags(renderFlags);

	if (mv_showBase)
		DrawCoordSystem(QuatT(physicalLocation), 7.0f);

	Matrix34 localEntityMat = Matrix34(physicalLocation);
	SRendParams rp = rRP;
	rp.pRenderNode = pEntity->GetRenderInterface() ? pEntity->GetRenderInterface()->GetRenderNode() :  nullptr;
	assert(rp.pRenderNode != NULL);
	rp.pMatrix = &localEntityMat;
	rp.pPrevMatrix = &localEntityMat;
	rp.fDistance = (physicalLocation.t - m_Camera.GetPosition()).GetLength();

	if (pInstance)
	{
		gEnv->p3DEngine->PrecacheCharacter(NULL, 1.f, pInstance, pInstance->GetIMaterial(), localEntityMat, 0, 1.f, 4, true, passInfo);
		pInstance->Render(rp, passInfo);

		if (m_showSceneRoots)
		{
			int numAnims = pInstance->GetISkeletonAnim()->GetNumAnimsInFIFO(0);
			if (numAnims > 0)
			{
				CAnimation& anim = pInstance->GetISkeletonAnim()->GetAnimFromFIFO(0, numAnims - 1);
				QuatT startLocation;
				if (pInstance->GetIAnimationSet()->GetAnimationDCCWorldSpaceLocation(&anim, startLocation, pInstance->GetIDefaultSkeleton().GetControllerIDByID(0)))
				{
					static float AXIS_LENGTH = 0.75f;
					static float AXIS_RADIUS = 0.01f;
					static float AXIS_CONE_RADIUS = 0.02f;
					static float AXIS_CONE_LENGTH = 0.1f;
					QuatT invStart = startLocation.GetInverted();
					QuatT location = physicalLocation * invStart;
					Vec3 xDir = location.q * Vec3(1.0f, 0.0f, 0.0f);
					Vec3 yDir = location.q * Vec3(0.0f, 1.0f, 0.0f);
					Vec3 zDir = location.q * Vec3(0.0f, 0.0f, 1.0f);
					Vec3 pos = location.t;
					pAuxGeom->DrawCylinder(pos + (xDir * (AXIS_LENGTH * 0.5f)), xDir, AXIS_RADIUS, AXIS_LENGTH, ColorB(255, 0, 0));
					pAuxGeom->DrawCylinder(pos + (yDir * (AXIS_LENGTH * 0.5f)), yDir, AXIS_RADIUS, AXIS_LENGTH, ColorB(0, 255, 0));
					pAuxGeom->DrawCylinder(pos + (zDir * (AXIS_LENGTH * 0.5f)), zDir, AXIS_RADIUS, AXIS_LENGTH, ColorB(0, 0, 255));
					pAuxGeom->DrawCone(pos + (xDir * AXIS_LENGTH), xDir, AXIS_CONE_RADIUS, AXIS_CONE_LENGTH, ColorB(255, 0, 0));
					pAuxGeom->DrawCone(pos + (yDir * AXIS_LENGTH), yDir, AXIS_CONE_RADIUS, AXIS_CONE_LENGTH, ColorB(0, 255, 0));
					pAuxGeom->DrawCone(pos + (zDir * AXIS_LENGTH), zDir, AXIS_CONE_RADIUS, AXIS_CONE_LENGTH, ColorB(0, 0, 255));
				}
			}
		}
	}
	else
	{
		pStatObj->Render(rp, passInfo);
	}

	//-------------------------------------------------
	//---      draw path of the past
	//-------------------------------------------------
	Matrix33 m33 = Matrix33(physicalLocation.q);
	Matrix34 m34 = Matrix34(physicalLocation);

	uint32 numEntries = m_arrAnimatedCharacterPath.size();
	for (int32 i = (numEntries - 2); i > -1; i--)
		m_arrAnimatedCharacterPath[i + 1] = m_arrAnimatedCharacterPath[i];
	m_arrAnimatedCharacterPath[0] = physicalLocation.t;

	Vec3 axis = physicalLocation.q.GetColumn0();
	Matrix33 SlopeMat33 = m_GroundOBB.m33;
	// [MichaelS 13/2/2007] Stop breaking the facial editor! Don't assume m_pCharPanel_Animation is valid! You (and I) know who you are!

	//gdc
	for (uint32 i = 0; i < numEntries; i++)
	{
		AABB aabb;
		aabb.min = Vec3(-0.01f, -0.01f, -0.01f) + SlopeMat33 * (m_arrAnimatedCharacterPath[i]);
		aabb.max = Vec3(+0.01f, +0.01f, +0.01f) + SlopeMat33 * (m_arrAnimatedCharacterPath[i]);
		pAuxGeom->DrawAABB(aabb, 1, RGBA8(0x00, 0x00, 0xff, 0x00), eBBD_Extremes_Color_Encoded);
	}

	if (pInstance)
	{
		ISkeletonAnim& skeletonAnim = *pInstance->GetISkeletonAnim();
		Vec3 vCurrentLocalMoveDirection = skeletonAnim.GetCurrentVelocity(); //.GetNormalizedSafe( Vec3(0,1,0) );;
		Vec3 CurrentVelocity = m_PhysicalLocation.q * skeletonAnim.GetCurrentVelocity();
		if (mv_printDebugText)
		{
			float color1[4] = { 1, 1, 1, 1 };
			IRenderAuxText::Draw2dLabel(12, g_ypos, 1.2f, color1, false, "CurrTravelDirection: %f %f", CurrentVelocity.x, CurrentVelocity.y);
			g_ypos += 10;
			IRenderAuxText::Draw2dLabel(12, g_ypos, 1.2f, color1, false, "vCurrentLocalMoveDirection: %f %f", vCurrentLocalMoveDirection.x, vCurrentLocalMoveDirection.y);
			g_ypos += 10;

			//draw the diagonal lines;
			//	pAuxGeom->DrawLine( absRoot.t+Vec3(0,0,0.02f),RGBA8(0xff,0x7f,0x00,0x00), absRoot.t+Vec3(0,0,0.02f)+Vec3( 1, 1, 0)*90,RGBA8(0x00,0x00,0x00,0x00) );
			//	pAuxGeom->DrawLine( absRoot.t+Vec3(0,0,0.02f),RGBA8(0xff,0x7f,0x00,0x00), absRoot.t+Vec3(0,0,0.02f)+Vec3(-1, 1, 0)*90,RGBA8(0x00,0x00,0x00,0x00) );
			//	pAuxGeom->DrawLine( absRoot.t+Vec3(0,0,0.02f),RGBA8(0xff,0x7f,0x00,0x00), absRoot.t+Vec3(0,0,0.02f)+Vec3( 1,-1, 0)*90,RGBA8(0x00,0x00,0x00,0x00) );
			//	pAuxGeom->DrawLine( absRoot.t+Vec3(0,0,0.02f),RGBA8(0xff,0x7f,0x00,0x00), absRoot.t+Vec3(0,0,0.02f)+Vec3(-1,-1, 0)*90,RGBA8(0x00,0x00,0x00,0x00) );
		}
	}
}

void CMannequinModelViewport::OnScrubTime(float timePassed)
{
}

void CMannequinModelViewport::OnSequenceRestart(float timePassed)
{
	OnScrubTime(timePassed);
	m_GridOrigin = Vec3(ZERO);
	m_PhysicalLocation.SetIdentity();
	SetPlayerPos();

	ResetLockedValues();

	const uint32 numChars = m_entityList.size();
	for (uint32 i = 0; i < numChars; i++)
	{
		IEntity* pEntity = m_entityList[i].GetEntity();
		if (pEntity)
		{
			pEntity->SetPosRotScale(m_entityList[i].startLocation.t, m_entityList[i].startLocation.q, Vec3(1.0f, 1.0f, 1.0f));
			ICharacterInstance* charInst = pEntity->GetCharacter(0);
			if (charInst)
			{
				if (pEntity->GetPhysics())
				{
					// Physicalizing with default params destroys the physical entity in the physical proxy.
					SEntityPhysicalizeParams params;
					pEntity->Physicalize(params);
				}

				charInst->GetISkeletonPose()->DestroyCharacterPhysics();
				charInst->GetISkeletonPose()->SetDefaultPose();

				SAnimationProcessParams params;
				params.locationAnimation = m_entityList[i].startLocation;
				params.bOnRender = 0;
				params.zoomAdjustedDistanceFromCamera = 0.0f;
				charInst->StartAnimationProcessing(params);
				charInst->FinishAnimationComputations();
			}
		}
	}

	CMannequinDialog* pMannequinDialog = CMannequinDialog::GetCurrentInstance();
	SMannequinContexts* pContexts = pMannequinDialog->Contexts();

	//--- Flush ActionControllers
	m_pActionController->Flush();
	const uint32 numContextDatums = pContexts->m_contextData.size();
	for (uint32 i = 0; i < numContextDatums; i++)
	{
		SScopeContextData& contextData = pContexts->m_contextData[i];
		SScopeContextViewData& viewData = contextData.viewData[m_editorMode];
		if (viewData.enabled && viewData.m_pActionController)
		{
			viewData.m_pActionController->Flush();

			//--- Temporary workaround for visualising lookposes in the editor
			if (contextData.database)
			{
				FragmentID fragID = contextData.pControllerDef->m_fragmentIDs.Find("LookPose");
				if (fragID != FRAGMENT_ID_INVALID)
				{
					TAction<SAnimationContext>* pAction = new TAction<SAnimationContext>(-1, fragID, TAG_STATE_EMPTY, IAction::Interruptable);
					viewData.m_pActionController->Queue(*pAction);
				}
			}
		}
	}

	//--- Reset background objects
	SMannequinContextViewData::TBackgroundObjects& backgroundObjects = pContexts->viewData[m_editorMode].backgroundObjects;
	const uint32 numBGObjects = backgroundObjects.size();
	for (uint32 i = 0; i < numBGObjects; i++)
	{
		CBaseObject* pBaseObject = backgroundObjects[i];
		if (pBaseObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* pEntObject = (CEntityObject*)pBaseObject;
			Vec3 oldPos = pEntObject->GetPos();
			pEntObject->SetPos(Vec3(ZERO));
			pEntObject->SetPos(oldPos);
		}
	}
}

void CMannequinModelViewport::UpdateDebugParams()
{
	CMannequinDialog* pMannequinDialog = CMannequinDialog::GetCurrentInstance();
	SMannequinContexts* pContexts = pMannequinDialog->Contexts();

	const uint32 numContextDatums = pContexts->m_contextData.size();
	for (uint32 i = 0; i < numContextDatums; i++)
	{
		const SScopeContextData& contextData = pContexts->m_contextData[i];
		const SScopeContextViewData& contextDataViewInfo = contextData.viewData[m_editorMode];
		if (contextDataViewInfo.enabled && contextDataViewInfo.entity)
		{
			if (contextData.contextID != CONTEXT_DATA_NONE)
			{
				m_pActionController->SetParam(pContexts->m_controllerDef->m_scopeContexts.GetTagName(contextData.contextID), contextDataViewInfo.entity->GetId());
			}
			else
			{
				m_pActionController->SetParam(contextData.name, contextDataViewInfo.entity->GetId());
			}
		}
	}
	const uint32 numProps = pContexts->backgroundProps.size();
	for (uint32 i = 0; i < numProps; i++)
	{
		m_pActionController->SetParam(pContexts->backgroundProps[i].name, pContexts->backgroundProps[i].entityID[m_editorMode]);
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CMannequinModelViewport::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		switch (pMsg->wParam)
		{
		case VK_UP:
		case VK_DOWN:
		case VK_LEFT:
		case VK_RIGHT:
		case 'W':
		case 'A':
		case 'S':
		case 'D':
			{
				return TRUE;
			}
		}
	}

	return TRUE;
}

