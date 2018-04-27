// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "CharacterEditor\ModelViewportCE.h"
#include "CharacterEditor\CharPanel_Animation.h"
#include "CharacterEditor\AnimationBrowser.h"

#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IAttachment.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryPhysics/IPhysics.h>
#include <CrySystem/ITimer.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryAction/IMaterialEffects.h> // for footstep sound preview
#include "CharacterEditor/MotionAdaptorDlg.h"
#include <CryRenderer/IShaderParamCallback.h>

extern uint32 g_ypos;

f32 g_TimeCount = 0;

f32 g_AddRealMoveSpeed = 0;
f32 g_AddRealTurnSpeed = 0;
f32 g_AddRealTravelDir = 0;
f32 g_AddRealSlopeAngl = 0;

f32 g_RealMoveSpeedSec = 0;
f32 g_RealTurnSpeedSec = 0;
f32 g_RealTravelDirSec = 0;
f32 g_RealSlopeAnglSec = 0;

uint32 g_AnimEventCounter = 0;
AnimEventInstance g_LastAnimEvent;
f32 g_fGroundHeight = 0;

//------------------------------------------------------------------------------
//---              animation previewer test-application                     ---
//------------------------------------------------------------------------------
void CModelViewportCE::AnimPreview_UnitTest(ICharacterInstance* pInstanceBase, const SRendParams& rRP, const SRenderingPassInfo& passInfo)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	float color1[4] = { 1, 1, 1, 1 };

	f32 FrameTime = GetIEditor()->GetSystem()->GetITimer()->GetFrameTime();
	//m_AverageFrameTime = pInstance->GetAverageFrameTime();

	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
	pAuxGeom->SetRenderFlags(renderFlags);

	//m_renderer->Draw2dLabel(12,g_ypos,1.2f,color1,false, "pInstanceBase: %s",pInstanceBase->GetFilePath());
	//g_ypos+=20;

	pInstanceBase->SetCharEditMode(CA_CharacterTool);
	pInstanceBase->GetISkeletonPose()->SetForceSkeletonUpdate(1);

	IInput* pIInput = GetISystem()->GetIInput(); // Cache IInput pointer.
	pIInput->Update(true);

	uint32 nGroundAlign = 0;
	if (m_pCharPanel_Animation && m_pAnimationBrowserPanel)
	{
		ICharacterInstance* pSelectedSkel = m_pAnimationBrowserPanel->GetSelected_SKEL();
		if (pSelectedSkel == 0 || pSelectedSkel->GetISkeletonAnim()->GetNumAnimsInFIFO(0) == 0)
		{
			m_pCharPanel_Animation->EnableWindow_BlendSpaceSlider_Para1(0);
			m_pCharPanel_Animation->EnableWindow_BlendSpaceSlider_Para2(0);
			m_pCharPanel_Animation->EnableWindow_BlendSpaceSlider_Para3(0);
			m_pCharPanel_Animation->EnableWindow_BlendSpaceSlider_Para4(0);
			m_pCharPanel_Animation->EnableWindow_BlendSpaceSlider_Para5(0);
			m_pCharPanel_Animation->EnableWindow_BlendSpaceSlider_Para6(0);
			m_pCharPanel_Animation->EnableWindow_BlendSpaceSlider_Para7(0);
		}

		if (pSelectedSkel)
		{
			ISkeletonAnim* pISkeletonAnim = pSelectedSkel->GetISkeletonAnim();
			ISkeletonPose* pISkeletonPose = pSelectedSkel->GetISkeletonPose();

			int AnimEventCallback(ICharacterInstance * pInstance, void* pPlayer);
			pISkeletonAnim->SetEventCallback(AnimEventCallback, this);

			uint32 IsParametricSampler = 0;
			for (int layer = 0; layer < ISkeletonAnim::LayerCount; ++layer)
			{
				uint32 nAnimsInLayer = pISkeletonAnim->GetNumAnimsInFIFO(layer);
				if (nAnimsInLayer)
				{
					const CAnimation& animation = pISkeletonAnim->GetAnimFromFIFO(layer, 0);
					IsParametricSampler |= (animation.GetParametricSampler() != NULL);
				}
			}

			m_pCharPanel_Animation->EnableWindow_BlendSpaceSlider_Para1(IsParametricSampler);
			m_pCharPanel_Animation->EnableWindow_BlendSpaceSlider_Para2(IsParametricSampler);
			m_pCharPanel_Animation->EnableWindow_BlendSpaceSlider_Para3(IsParametricSampler);
			m_pCharPanel_Animation->EnableWindow_BlendSpaceSlider_Para4(IsParametricSampler);
			m_pCharPanel_Animation->EnableWindow_BlendSpaceSlider_Para5(IsParametricSampler);
			m_pCharPanel_Animation->EnableWindow_BlendSpaceSlider_Para6(IsParametricSampler);
			m_pCharPanel_Animation->EnableWindow_BlendSpaceSlider_Para7(IsParametricSampler);

			uint32 nAnimLayer0 = pISkeletonAnim->GetNumAnimsInFIFO(0);
			if (nAnimLayer0)
			{
				static uint32 rcr_key_R = 0;
				rcr_key_R <<= 1;
				if (CheckVirtualKey('R')) rcr_key_R |= 1;
				if ((rcr_key_R & 3) == 1)
					pISkeletonPose->ApplyRecoilAnimation(0.50f, 0.10f, 0.8f, 3);

				if (mv_showStartLocation)
				{
					const CAnimation& animation = pISkeletonAnim->GetAnimFromFIFO(0, 0);
					IAnimationSet* pIAnimationSet = pSelectedSkel->GetIAnimationSet();
					const char* aname = pIAnimationSet->GetNameByAnimID(animation.GetAnimationId());

					QuatT startLocation;
					const bool success = pIAnimationSet->GetAnimationDCCWorldSpaceLocation(aname, startLocation);
					const QuatT& invStartLocation = startLocation.GetInverted();
					m_renderer->Draw2dLabel(12, g_ypos, 1.6f, color1, false, "InvStartRot: %f (%f %f %f)", invStartLocation.q.w, invStartLocation.q.v.x, invStartLocation.q.v.y, invStartLocation.q.v.z);
					g_ypos += 15;
					m_renderer->Draw2dLabel(12, g_ypos, 1.6f, color1, false, "InvStartPos: %f %f %f", invStartLocation.t.x, invStartLocation.t.y, invStartLocation.t.z);
					g_ypos += 15;

					static Ang3 angle = Ang3(ZERO);
					angle.x += 0.1f;
					angle.y += 0.01f;
					angle.z += 0.001f;
					AABB sAABB = AABB(Vec3(-0.05f, -0.05f, -0.05f), Vec3(+0.05f, +0.05f, +0.05f));
					OBB obb = OBB::CreateOBBfromAABB(Matrix33::CreateRotationXYZ(angle), sAABB);
					pAuxGeom->DrawOBB(obb, invStartLocation.t, 1, RGBA8(0xff, 0xff, 0xff, 0xff), eBBD_Extremes_Color_Encoded);

					//draw start coordinates;
					Vec3 xaxis = invStartLocation.q.GetColumn0() * 0.5f;
					Vec3 yaxis = invStartLocation.q.GetColumn1() * 0.5f;
					Vec3 zaxis = invStartLocation.q.GetColumn2() * 0.5f;
					pAuxGeom->DrawLine(invStartLocation.t + Vec3(0, 0, 0.02f), RGBA8(0xff, 0x00, 0x00, 0x00), invStartLocation.t + xaxis + Vec3(0, 0, 0.02f), RGBA8(0xff, 0x00, 0x00, 0x00));
					pAuxGeom->DrawLine(invStartLocation.t + Vec3(0, 0, 0.02f), RGBA8(0x00, 0xff, 0x00, 0x00), invStartLocation.t + yaxis + Vec3(0, 0, 0.02f), RGBA8(0x00, 0xff, 0x00, 0x00));
					pAuxGeom->DrawLine(invStartLocation.t + Vec3(0, 0, 0.02f), RGBA8(0x00, 0x00, 0xff, 0x00), invStartLocation.t + zaxis + Vec3(0, 0, 0.02f), RGBA8(0x00, 0x00, 0xff, 0x00));
				}
			}

			if (m_bPaused == false)
			{
				f32 fPlaybackScale = m_pCharPanel_Animation->GetPlaybackScaleSmooth(0.2f, m_AverageFrameTime);
				pInstanceBase->SetPlaybackScale(fPlaybackScale);
			}
			f32 fBlendSpace_Para1 = m_pCharPanel_Animation->GetBlendSpaceFlex_Para1(0.5f, m_AverageFrameTime);
			f32 fBlendSpace_Para2 = m_pCharPanel_Animation->GetBlendSpaceFlex_Para2(0.5f, m_AverageFrameTime);
			f32 fBlendSpace_Para3 = m_pCharPanel_Animation->GetBlendSpaceFlex_Para3(0.5f, m_AverageFrameTime);
			f32 fBlendSpace_Para4 = m_pCharPanel_Animation->GetBlendSpaceFlex_Para4(0.5f, m_AverageFrameTime);
			f32 fBlendSpace_Para5 = m_pCharPanel_Animation->GetBlendSpaceFlex_Para5(0.5f, m_AverageFrameTime);
			f32 fBlendSpace_Para6 = m_pCharPanel_Animation->GetBlendSpaceFlex_Para6(0.5f, m_AverageFrameTime);
			f32 fBlendSpace_Para7 = m_pCharPanel_Animation->GetBlendSpaceFlex_Para7(0.5f, m_AverageFrameTime);
			f32 fBlendSpace_Para8 = m_pCharPanel_Animation->GetBlendSpaceFlex_Para8(0.5f, m_AverageFrameTime);
			f32 fBlendSpace_Para9 = m_pCharPanel_Animation->GetBlendSpaceFlex_Para9(0.5f, m_AverageFrameTime);
			f32 fBlendSpace_Para10 = m_pCharPanel_Animation->GetBlendSpaceFlex_Para10(0.5f, m_AverageFrameTime);
			f32 fBlendSpace_Para11 = m_pCharPanel_Animation->GetBlendSpaceFlex_Para11(0.5f, m_AverageFrameTime);
			f32 fBlendSpace_Para12 = m_pCharPanel_Animation->GetBlendSpaceFlex_Para12(0.5f, m_AverageFrameTime);
			f32 fBlendSpace_Para13 = m_pCharPanel_Animation->GetBlendSpaceFlex_Para13(0.5f, m_AverageFrameTime);

			m_absCurrentSlope = fBlendSpace_Para4;

			if (mv_StrafingControl)
			{
				Vec3 p0 = m_PhysicalLocation.t;
				p0.z = 0.0f;
				Vec3 p1 = GetViewTM().GetTranslation();
				p1.z = 0.0f;
				Vec3 vMoveDir = (p0 - p1).GetNormalized();
				pAuxGeom->DrawLine(m_PhysicalLocation.t, RGBA8(0x1f, 0x1f, 0x1f, 0x00), m_PhysicalLocation.t + vMoveDir, RGBA8(0xff, 0xff, 0xff, 0x00));

				//adjust travel direction
				Vec2 vDesiredTravelDir(ZERO); //if no key pressed, the speed is always zero
				if (CheckVirtualKey(VK_CONTROL))
				{
					if (CheckVirtualKey(VK_RIGHT)) { vDesiredTravelDir -= Vec2(-vMoveDir.y, vMoveDir.x); }
					if (CheckVirtualKey(VK_LEFT))  { vDesiredTravelDir += Vec2(-vMoveDir.y, vMoveDir.x); }
					if (CheckVirtualKey(VK_UP))    { vDesiredTravelDir += Vec2(vMoveDir); }
					if (CheckVirtualKey(VK_DOWN))  { vDesiredTravelDir -= Vec2(vMoveDir); }
				}
				Vec2 vWorldDesiredMoveDirection = (vDesiredTravelDir | vDesiredTravelDir) ? vDesiredTravelDir.GetNormalized() * fBlendSpace_Para1 : ZERO;
				SmoothCD(m_vWorldDesiredMoveDirectionSmooth, m_vWorldDesiredMoveDirectionSmoothRate, FrameTime, vWorldDesiredMoveDirection, 0.40f);

				const f32 maxf = (fBlendSpace_Para1 < 6.0f) ? fBlendSpace_Para1 : 6.0f;
				const f32 maxr = (fBlendSpace_Para1 < 4.0f) ? fBlendSpace_Para1 : 4.0f;
				const f32 maxl = (fBlendSpace_Para1 < 4.0f) ? fBlendSpace_Para1 : 4.0f;
				const f32 maxb = (fBlendSpace_Para1 < 3.0f) ? fBlendSpace_Para1 : 3.0f;

				if (1)
				{
					Vec3 p;
					const f32 s = 0.01f;
					const f32 step = gf_PI * 0.5f / 128.0f;
					for (f32 r = 0.0f; r < gf_PI * 0.5f; r += step)
						p = m_PhysicalLocation * Vec3(-sin_tpl(r) * maxl, cos_tpl(r) * maxf, 0), pAuxGeom->DrawAABB(AABB(Vec3(-s, -s, -s) + p, Vec3(s, s, s) + p), 1, RGBA8(0xff, 0x00, 0x00, 0x00), eBBD_Extremes_Color_Encoded);
					for (f32 r = -gf_PI * 0.5f; r < 0; r += step)
						p = m_PhysicalLocation * Vec3(-sin_tpl(r) * maxr, cos_tpl(r) * maxf, 0), pAuxGeom->DrawAABB(AABB(Vec3(-s, -s, -s) + p, Vec3(s, s, s) + p), 1, RGBA8(0xff, 0x00, 0x00, 0x00), eBBD_Extremes_Color_Encoded);
					for (f32 r = gf_PI * 0.5f; r < gf_PI; r += step)
						p = m_PhysicalLocation * Vec3(-sin_tpl(r) * maxl, cos_tpl(r) * maxb, 0), pAuxGeom->DrawAABB(AABB(Vec3(-s, -s, -s) + p, Vec3(s, s, s) + p), 1, RGBA8(0xff, 0x00, 0x00, 0x00), eBBD_Extremes_Color_Encoded);
					for (f32 r = -gf_PI; r < -gf_PI * 0.5f; r += step)
						p = m_PhysicalLocation * Vec3(-sin_tpl(r) * maxr, cos_tpl(r) * maxb, 0), pAuxGeom->DrawAABB(AABB(Vec3(-s, -s, -s) + p, Vec3(s, s, s) + p), 1, RGBA8(0xff, 0x00, 0x00, 0x00), eBBD_Extremes_Color_Encoded);
				}

				//thats our desired move speed
				fBlendSpace_Para1 = m_vWorldDesiredMoveDirectionSmooth.GetLength();

				const Vec3 vDesiredLocalMoveDir = Vec3(m_vWorldDesiredMoveDirectionSmooth) * m_PhysicalLocation.q;
				fBlendSpace_Para3 = -atan2_tpl(vDesiredLocalMoveDir.x, vDesiredLocalMoveDir.y);

				//Check at what speed we are allowed to move in that direction
				const f32 fSpeed = vDesiredLocalMoveDir.GetLength();
				if (fSpeed)
				{
					const Vec3 n = vDesiredLocalMoveDir / fSpeed; //normalize the move-vector

					//calculate the spherical speed limit
					const f32 xs = n.x < 0 ? maxl : maxr; //select x-axis for quadrant
					const f32 ys = n.y < 0 ? maxb : maxf; //select y-axis for quadrant
					const f32 x = n.x / xs;
					const f32 y = n.y / ys;
					const f32 len = isqrt_tpl(x * x + y * y);
					const f32 fMaxSpeed = sqrt_tpl(sqr(x * xs * len) + sqr(y * ys * len)); //the the maximum speed for the desired move direction

					//and now clamp the speed if unnecessary
					float fColorRed[4] = { 1, 0, 0, 1 };
					if (fBlendSpace_Para1 > fMaxSpeed)
						fBlendSpace_Para1 = fMaxSpeed, m_renderer->Draw2dLabel(12, g_ypos, 2.5f, fColorRed, false, "fSpeed Clamping: %f", fBlendSpace_Para1), g_ypos += 26;
				}
			}

			//--------------------------------------------------------------
			//----   set the motion parameters                           ---
			//--------------------------------------------------------------
			SetDesiredMotionParam(pISkeletonAnim, eMotionParamID_TravelSpeed, fBlendSpace_Para1);
			SetDesiredMotionParam(pISkeletonAnim, eMotionParamID_TurnSpeed, fBlendSpace_Para2);
			SetDesiredMotionParam(pISkeletonAnim, eMotionParamID_TravelAngle, fBlendSpace_Para3);
			SetDesiredMotionParam(pISkeletonAnim, eMotionParamID_TravelSlope, fBlendSpace_Para4);
			SetDesiredMotionParam(pISkeletonAnim, eMotionParamID_TurnAngle, fBlendSpace_Para5);
			SetDesiredMotionParam(pISkeletonAnim, eMotionParamID_TravelDist, fBlendSpace_Para6);
			SetDesiredMotionParam(pISkeletonAnim, eMotionParamID_BlendWeight, fBlendSpace_Para7);
			SetDesiredMotionParam(pISkeletonAnim, eMotionParamID_BlendWeight2, fBlendSpace_Para8);
			SetDesiredMotionParam(pISkeletonAnim, eMotionParamID_BlendWeight3, fBlendSpace_Para9);
			SetDesiredMotionParam(pISkeletonAnim, eMotionParamID_BlendWeight4, fBlendSpace_Para10);
			SetDesiredMotionParam(pISkeletonAnim, eMotionParamID_BlendWeight5, fBlendSpace_Para11);
			SetDesiredMotionParam(pISkeletonAnim, eMotionParamID_BlendWeight6, fBlendSpace_Para12);
			SetDesiredMotionParam(pISkeletonAnim, eMotionParamID_BlendWeight7, fBlendSpace_Para13);

			//------------------------------------------------------------------
			//---- check if animation in layer is an aimpose    ----------------
			//------------------------------------------------------------------
			uint32 nLayer = m_pCharPanel_Animation->GetLayer();
			uint32 numAnimsLayer = pISkeletonAnim->GetNumAnimsInFIFO(nLayer);
			if (nLayer && numAnimsLayer)
				m_pCharPanel_Animation->EnableLayerBlendWeightWindow(1);
			else
				m_pCharPanel_Animation->EnableLayerBlendWeightWindow(0);

			m_pCharPanel_Animation->SmoothLayerBlendWeights(m_AverageFrameTime);
			for (uint32 i = 0; i < ISkeletonAnim::LayerCount; i++)
			{
				f32 fAddWeight = m_pCharPanel_Animation->GetLayerBlendWeight(i);
				pISkeletonAnim->SetLayerBlendWeight(i, fAddWeight);
			}

			IAnimationPoseBlenderDir* pIPoseBlenderAim = pISkeletonPose->GetIPoseBlenderAim();
			if (pIPoseBlenderAim)
			{
				//		m_renderer->Draw2dLabel(12,g_ypos,1.2f,color1,false,"pIPoseBlenderAim Initialized" );
				//		g_ypos+=10;
				uint32 nAimIKLayer = (m_pCharPanel_Animation ? m_pCharPanel_Animation->GetAimIKLayerValue() : ISkeletonAnim::LayerCount);
				uint32 nAimIK = m_pCharPanel_Animation->GetAimIK();
				pIPoseBlenderAim->SetState(nAimIK);
				pIPoseBlenderAim->SetPolarCoordinatesSmoothTimeSeconds(0.2f);
				pIPoseBlenderAim->SetLayer(nAimIKLayer);
				pIPoseBlenderAim->SetTarget(m_vCamPos);
				pIPoseBlenderAim->SetFadeoutAngle(DEG2RAD(180));
			}

			IAnimationPoseBlenderDir* pIPoseBlenderLook = pISkeletonPose->GetIPoseBlenderLook();
			if (pIPoseBlenderLook)
			{
				//	m_renderer->Draw2dLabel(12,g_ypos,1.2f,color1,false,"pIPoseBlenderLook Initialized" );
				//	g_ypos+=10;
				uint32 nLookIKLayer = (m_pCharPanel_Animation ? m_pCharPanel_Animation->GetLookIKLayerValue() : ISkeletonAnim::LayerCount);
				int32 LookIK = m_pCharPanel_Animation->GetLookIK();
				pIPoseBlenderLook->SetState(LookIK);
				pIPoseBlenderLook->SetPolarCoordinatesSmoothTimeSeconds(0.2f);
				pIPoseBlenderLook->SetLayer(nLookIKLayer);
				pIPoseBlenderLook->SetTarget(m_vCamPos);
				pIPoseBlenderLook->SetFadeoutAngle(DEG2RAD(120));
			}

			//------------------------------------------------------------------
			//parameterization for uphill/downhill
			//------------------------------------------------------------------
			g_fGroundHeight = 0;
			f32 fGroundRadian = 0;
			f32 fGroundRadianMoveDir = 0;
			f32 fGroundDegreeMoveDir = 0;
			nGroundAlign = m_pCharPanel_Animation->GetGroundAlign();
			if (nGroundAlign)
			{
				Lineseg ls_middle;
				ls_middle.start = Vec3(m_PhysicalLocation.t.x, m_PhysicalLocation.t.y, +9999.0f);
				ls_middle.end = Vec3(m_PhysicalLocation.t.x, m_PhysicalLocation.t.y, -9999.0f);

				Vec3 vGroundIntersection(0, 0, -9999.0f);
				int8 ground = Intersect::Lineseg_OBB(ls_middle, m_GroundOBBPos, m_GroundOBB, vGroundIntersection);
				assert(ground);

				f32 fDesiredSpeed = fBlendSpace_Para1;
				f32 t = clamp_tpl((fDesiredSpeed - 1) / 7.0f, 0.0f, 1.0f);
				t = t * t;
				f32 fScaleLimit = clamp_tpl(0.8f * (1.0f - t) + 0.0f * t, 0.2f, 1.0f) - 0.2f;

				Vec3 vGroundNormal = m_GroundOBB.m33.GetColumn2() * m_PhysicalLocation.q;
				fGroundRadian = acos_tpl(vGroundNormal.z);
				g_fGroundHeight = (vGroundIntersection.z - (fGroundRadian * 0.15f));//*fScaleLimit;

				Vec3 gnormal = Vec3(0, vGroundNormal.y, vGroundNormal.z);
				f32 cosine = Vec3(0, 0, 1) | gnormal;
				Vec3 sine = Vec3(0, 0, 1) % gnormal;
				fGroundRadianMoveDir = atan2(sgn(sine.x) * sine.GetLength(), cosine);
				fGroundDegreeMoveDir = RAD2DEG(fGroundRadianMoveDir);

				m_renderer->Draw2dLabel(12, g_ypos, 1.5f, color1, false, "vGroundIntersection_z: %f", vGroundIntersection.z);
				//	m_renderer->Draw2dLabel(12,g_ypos,1.5f,color1,false,"GroundAngle: rad:%f  degree:%f  fGroundDegreeMoveDir:%f", fGroundAngle, RAD2DEG(fGroundAngle), fGroundDegreeMoveDir );
				g_ypos += 14;
			}
		}
	}  //char panel animation

	const CCamera& camera = GetCamera();
	float fDistance = sqrt_tpl(Distance::Point_AABBSq(m_PhysicalLocation.GetInverted() * camera.GetPosition(), pInstanceBase->GetAABB()));
	float fZoomFactor = 0.001f + 0.999f * (RAD2DEG(camera.GetFov()) / 60.f);

	SAnimationProcessParams params;
	params.locationAnimation = m_PhysicalLocation;
	params.bOnRender = 0;
	params.zoomAdjustedDistanceFromCamera = fDistance * fZoomFactor;

	ExternalPostProcessing(pInstanceBase);
	const bool useAnimationDrivenMotion = UseAnimationDrivenMotion();
	pInstanceBase->GetISkeletonAnim()->SetAnimationDrivenMotion(useAnimationDrivenMotion);
	pInstanceBase->StartAnimationProcessing(params);
	QuatT relMove = pInstanceBase->GetISkeletonAnim()->GetRelMovement();

	//for scrubbing, its important to set the new time AFTER the update of the animation queue and BEFORE we query the new locator movement.
	//if we don't do that, then we mess up some internal flags

	const f32 fSmoothTime = m_bPaused ? 0.2f : 0.0f;
	SmoothCD(m_NormalizedTimeSmooth, m_NormalizedTimeRate, FrameTime, m_NormalizedTime, fSmoothTime);
	const int layerId = GetCurrentActiveAnimationLayer();
	CAnimation* pAnimation = GetAnimationByLayer(layerId);
	if (pAnimation)
	{
		if (m_bPaused)
		{
			//At this point the animation pose was already fully updated by StartAnimationProcessing(params)
			relMove = m_lastCalculateRelativeMovement;  //if we set a new time with 'scrubbing' then we have to work with one frame delay (or we will see foot-sliding at low framerate)
			//Set a new animation-time. We will see the effect one frame later.
			pInstanceBase->GetISkeletonAnim()->SetAnimationNormalizedTime(pAnimation, m_NormalizedTimeSmooth, m_isEntireClipNormalizedTime);
			//Read back the new 'locator movement' immediately and use it next frame.
			m_lastCalculateRelativeMovement = pInstanceBase->GetISkeletonAnim()->CalculateRelativeMovement(-1.0f); //there is no "delta-time" in scrub-mode
		}
		else
		{
			m_isEntireClipNormalizedTime = true;
			m_NormalizedTime = pInstanceBase->GetISkeletonAnim()->GetAnimationNormalizedTime(pAnimation);
		}
	}

	if (mv_InPlaceMovement)
	{
		Matrix34 m = GetViewTM();
		m.SetTranslation(m.GetTranslation() - m_PhysicalLocation.t);
		SetViewTM(m);
		relMove.t.zero();
		m_PhysicalLocation.t.zero();
	}

	m_PhysicalLocation.s = mv_UniformScaling;
	m_PhysicalLocation = m_PhysicalLocation * relMove;
	m_PhysicalLocation.q.Normalize();
	pInstanceBase->SetAttachmentLocation_DEPRECATED(m_PhysicalLocation);
	pInstanceBase->FinishAnimationComputations();

	f32 fDistanceFromOrigin = m_PhysicalLocation.t.GetLength();
	if (fDistanceFromOrigin > 2000)
	{
		Matrix34 m = GetViewTM();
		m.SetTranslation(m.GetTranslation() - m_PhysicalLocation.t);
		SetViewTM(m);
		m_PhysicalLocation.t.zero();
	}

	//move the ViewPort camera together with the character
	Matrix34 m = GetViewTM();
	m.SetTranslation(m_PhysicalLocation.q * relMove.t * mv_UniformScaling + m.GetTranslation());
	SetViewTM(m);

	//m_renderer->Draw2dLabel(12,g_ypos,1.5f,color1,false,"relMove: (%f %f %f %f)   (%f %f %f)",relMove.q.w,relMove.q.v.x,relMove.q.v.y,relMove.q.v.z,  relMove.t.x,relMove.t.y,relMove.t.z  );
	//g_ypos+=14;
	//m_renderer->Draw2dLabel(12,g_ypos,1.5f,color1,false,"m_PhysicalLocation: (%f %f %f %f)   (%f %f %f) (%f)",m_PhysicalLocation.q.w,m_PhysicalLocation.q.v.x,m_PhysicalLocation.q.v.y,m_PhysicalLocation.q.v.z,  m_PhysicalLocation.t.x,m_PhysicalLocation.t.y,m_PhysicalLocation.t.z,m_PhysicalLocation.s  );
	//g_ypos+=14;

	GetIEditor()->GetSystem()->GetIConsole()->GetCVar("ca_DrawLocator")->Set(mv_showLocator);
	assert(rRP.pMatrix);
	assert(rRP.pPrevMatrix);

	if (mv_showGrid)
		DrawGrid(m_PhysicalLocation.q, m_PhysicalLocation.t, Vec3(ZERO), m_GroundOBB.m33);
	if (mv_showBase)
		DrawCoordSystem(IDENTITY, 7.0f);    //DrawCoordSystem( QuatT(m_PhysicalLocation) ,10.0f);

	m_LocalEntityMat = Matrix34(m_PhysicalLocation);

	if (m_Callbacks.size() > 0)
	{
		IMaterial* pMaterial = 0;
		if (m_object)
			pMaterial = m_object->GetMaterial();
		else if (GetCharacterBase())
			pMaterial = GetCharacterBase()->GetIMaterial();

		TCallbackVector::iterator itEnd = m_Callbacks.end();
		for (TCallbackVector::iterator it = m_Callbacks.begin(); it != itEnd; ++it)
		{
			(*it)->SetupShaderParams(m_pShaderPublicParams, pMaterial);
		}
	}

	SRendParams rp = rRP;
	rp.pMatrix = &m_LocalEntityMat;
	rp.pPrevMatrix = &m_PrevLocalEntityMat;
	rp.fDistance = fDistance * fZoomFactor;
	IAttachmentManager* pIAttachmentManager = pInstanceBase->GetIAttachmentManager();
	uint32 numAttachments = pIAttachmentManager->GetAttachmentCount();
	for (uint32 i = 0; i < numAttachments; i++)
	{
		IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByIndex(i);
		if (pIAttachment)
		{
			IAttachmentSkin* pIAttachmentSkin = pIAttachment->GetIAttachmentSkin();
			if (pIAttachmentSkin)
				pIAttachmentSkin->TriggerMeshStreaming(mv_forceLODNum, passInfo);
		}
	}

	if (m_pShaderPublicParams)
		m_pShaderPublicParams->AssignToRenderParams(rp);

	AABB aabb = pInstanceBase->GetAABB();
	AABB taabb = AABB::CreateTransformedAABB(m_LocalEntityMat, aabb);
	uint32 visible = GetCamera().IsAABBVisible_E(taabb);
	if (visible)
	{
		pInstanceBase->SetViewdir(GetCamera().GetViewdir());
		gEnv->p3DEngine->PrecacheCharacter(NULL, 1.f, pInstanceBase, pInstanceBase->GetIMaterial(), m_LocalEntityMat, 0, 1.f, 4, true, passInfo);
		rp.lodValue = pInstanceBase->ComputeLod(mv_forceLODNum, passInfo);
		pInstanceBase->Render(rp, QuatTS(IDENTITY), passInfo);

		if (0)
		{
			CTimeValue tv = gEnv->pTimer->GetAsyncTime();
			static f64 fMilliseconds = 0;

			SkelExtensionsTest(pInstanceBase);

			float colorR[4] = { 1.0f, 0.8f, 0.4f, 1 };
			f64 dif = (gEnv->pTimer->GetAsyncTime() - tv).GetMilliSeconds();
			if (dif > 0.1f)
				fMilliseconds = dif;
			m_renderer->Draw2dLabel(12, g_ypos, 2.5f, colorR, false, "dif: %f  fMilliseconds: %f", dif, fMilliseconds), g_ypos += 24;

		}

	}
	ICharacterManager* pICharManager = GetAnimationSystem();
	pICharManager->RenderDebugInstances(passInfo);

	//-------------------------------------------------
	//---      draw path of the past
	//-------------------------------------------------
	uint32 numEntries = m_arrAnimatedCharacterPath.size();
	for (int32 i = (numEntries - 2); i > -1; i--)
		m_arrAnimatedCharacterPath[i + 1] = m_arrAnimatedCharacterPath[i];
	m_arrAnimatedCharacterPath[0] = m_PhysicalLocation.t;

	Vec3 axis = m_PhysicalLocation.q.GetColumn0();
	Matrix33 SlopeMat33 = m_GroundOBB.m33;
	uint32 GroundAlign = (m_pCharPanel_Animation ? m_pCharPanel_Animation->GetGroundAlign() : 1);
	if (GroundAlign == 0)
		SlopeMat33 = Matrix33::CreateRotationAA(m_absCurrentSlope, axis);

	//gdc
	for (uint32 i = 0; i < numEntries; i++)
	{
		AABB aabb;
		aabb.min = Vec3(-0.01f, -0.01f, -0.01f) + m_arrAnimatedCharacterPath[i];
		aabb.max = Vec3(+0.01f, +0.01f, +0.01f) + m_arrAnimatedCharacterPath[i];
		pAuxGeom->DrawAABB(aabb, 1, RGBA8(0x00, 0x00, 0xff, 0x00), eBBD_Extremes_Color_Encoded);
	}

	GetIEditor()->GetSystem()->GetIConsole()->GetCVar("ca_DrawSkeleton")->Set(mv_showSkeleton);
	if (m_pCharPanel_Animation)
	{
		int32 mt = m_pCharPanel_Animation->GetUseMorphTargets();
		GetIEditor()->GetSystem()->GetIConsole()->GetCVar("ca_UseMorph")->Set(mt);
	}
	//	f32 last_time;
	//	const char* last_AnimPath;
	//	const char* last_EventName;
	//	const char* last_SoundName;

	if (mv_printDebugText)
	{
		g_ypos += 20;
		m_renderer->Draw2dLabel(12, g_ypos, 1.2f, color1, false, "AnimEventCounter: %d", g_AnimEventCounter);
		g_ypos += 10;
		m_renderer->Draw2dLabel(12, g_ypos, 1.2f, color1, false, "time: %f     EventName:''%s''     Parameter:''%s''    PathName:''%s''", g_LastAnimEvent.m_time, g_LastAnimEvent.m_EventName, g_LastAnimEvent.m_CustomParameter, g_LastAnimEvent.m_AnimPathName);
		g_ypos += 10;
	}

	g_TimeCount += FrameTime;

	g_AddRealMoveSpeed += relMove.t.GetLength();
	g_AddRealTurnSpeed += Ang3::GetAnglesXYZ(relMove.q).z;
	g_AddRealTravelDir = Ang3::CreateRadZ(Vec3(0, 1, 0), relMove.t);
	Vec3 v = relMove.t * Matrix33::CreateRotationZ(g_AddRealTravelDir);
	g_AddRealSlopeAngl = atan2_tpl(v.z, v.y);

	if (g_TimeCount > 1.00f)
	{
		g_RealMoveSpeedSec = g_AddRealMoveSpeed;
		g_RealTurnSpeedSec = g_AddRealTurnSpeed;
		g_RealTravelDirSec = g_AddRealTravelDir;
		g_RealSlopeAnglSec = g_AddRealSlopeAngl;
		g_AddRealMoveSpeed = 0;
		g_AddRealTurnSpeed = 0;
		g_AddRealTravelDir = 0;
		g_TimeCount = FrameTime;
	}

	if (mv_showMotionParam)
	{
		static float colorRed[4] = { 1.0f, .0f, .0f, 1.0f };
		m_renderer->Draw2dLabel(1, 1, 1.5f, colorRed, false, "                                            Real MoveSpeed: %f ", g_RealMoveSpeedSec);
		m_renderer->Draw2dLabel(1, 15, 1.5f, colorRed, false, "                                            Real TurnSpeed: %f ", g_RealTurnSpeedSec);
		m_renderer->Draw2dLabel(1, 30, 1.5f, colorRed, false, "                                            Real TravelDir: %f ", g_RealTravelDirSec);
		m_renderer->Draw2dLabel(1, 45, 1.5f, colorRed, false, "                                            Real SlopeAngl: %f (%f)", g_RealSlopeAnglSec, RAD2DEG(g_RealSlopeAnglSec));
	}

	if (m_pMotionAdaptorDlg)
	{
		for (uint32 i = 0; i < ISkeletonAnim::LayerCount; i++)
		{
			uint32 nAnimsInQueue = pInstanceBase->GetISkeletonAnim()->GetNumAnimsInFIFO(i);
			if (nAnimsInQueue)
			{
				m_pMotionAdaptorDlg->m_dlgMain.ClearAnimation();
				break;
			}
		}
		uint32 numJoints = m_pMotionAdaptorDlg->m_dlgMain.GetViconJointNum();
		uint32 numFrames = 0;

		if (numJoints > 0)
			numFrames = m_pMotionAdaptorDlg->m_dlgMain.GetViconFrameCount();
		if (numFrames)
		{
			m_renderer->Draw2dLabel(12, g_ypos, 1.2f, color1, false, "Motion Adaptor Updateloop:  numJoints: %d     numFrames: %d", numJoints, numFrames);
			g_ypos += 10;
			m_pMotionAdaptorDlg->m_dlgMain.MotionPlayback(GetCharacterBase());
		}
	}

	//------------------------------------------------------------------------
	//---        Attach camera to socket                                   ---
	//------------------------------------------------------------------------
	if (mv_AttachCamera)
	{
		static Vec3 vCamPos = Vec3(0.0f, -4.3f, 3.0f);
		static Quat vCamRot = Quat(IDENTITY);

		IAttachmentManager* pIAttachmentManager = pInstanceBase->GetIAttachmentManager();
		if (pIAttachmentManager)
		{
			IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByName("#camera");
			if (pIAttachment)
			{
				QuatTS qt = pIAttachment->GetAttWorldAbsolute();
				qt = m_PhysicalLocation.GetInverted() * qt;
				vCamRot = qt.q;
				vCamPos = qt.t;
				SetViewTM(Matrix34(Matrix33(m_PhysicalLocation.q * vCamRot), m_PhysicalLocation * vCamPos));
			}
		}
	}

	//------------------------------------------------------------------------
	//---  camera control
	//------------------------------------------------------------------------
	if (0)
	{
		m_nFrameCounter++;
		CryCharAnimationParams AParams;

		m_renderer->Draw2dLabel(12, g_ypos, 1.0f, color1, false, "%d", m_nFrameCounter);
		g_ypos += 10;

		//ivo
		static Vec3 vLookAt = Vec3(0.0f, 0.0f, 1.2f);
		static Vec3 vLookAtSmooth = Vec3(0.0f, 0.0f, 1.2f);
		static Vec3 vLookAtRate = Vec3(0.0f, 0.0f, 1.2f);

		static Vec3 vLookFrom = Vec3(0.0f, -4.3f, 3.0f);
		static Vec3 vLookFromSmooth = Vec3(0.0f, -4.3f, 3.0f);
		static Vec3 vLookFromRate = Vec3(0.0f, -4.3f, 3.0f);

		if (m_nFrameCounter < 0)
		{
			vLookAt = Vec3(0, 0, 1.3f);
			vLookFrom = Vec3(-2.5f, 5.0f, 1.4f);

			AParams.m_nFlags = CA_LOOP_ANIMATION | CA_START_AFTER;
			AParams.m_fTransTime = 0.15f;
			AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion
			pInstanceBase->GetISkeletonAnim()->StartAnimation("stand_stepRotate_com_idle_90_01", AParams);
		}

		if (m_nFrameCounter == 200)
		{
			AParams.m_nFlags = CA_REPEAT_LAST_KEY;
			AParams.m_fTransTime = 0.10f;
			AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion
			pInstanceBase->GetISkeletonAnim()->StartAnimation("stand_stepRotate_com_rgt_01", AParams);

			AParams.m_nFlags = CA_LOOP_ANIMATION | CA_START_AFTER;
			AParams.m_fTransTime = 0.15f;
			AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion
			pInstanceBase->GetISkeletonAnim()->StartAnimation("stand_stepRotate_com_idle_90_01", AParams);
		}

		if (m_nFrameCounter == 400)
		{

			AParams.m_nFlags = CA_REPEAT_LAST_KEY;
			AParams.m_fTransTime = 0.10f;
			AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion
			pInstanceBase->GetISkeletonAnim()->StartAnimation("stand_stepRotate_com_lft_01", AParams);

			AParams.m_nFlags = CA_LOOP_ANIMATION | CA_START_AFTER;
			AParams.m_fTransTime = 0.15f;
			AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion
			pInstanceBase->GetISkeletonAnim()->StartAnimation("stand_stepRotate_com_idle_90_01", AParams);
		}

		if (m_nFrameCounter == 600)
		{
			AParams.m_nFlags = CA_REPEAT_LAST_KEY;
			AParams.m_fTransTime = 0.10f;
			AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion
			pInstanceBase->GetISkeletonAnim()->StartAnimation("stand_stepRotate_com_rgt_01", AParams);

			AParams.m_nFlags = CA_ALLOW_ANIM_RESTART | CA_REPEAT_LAST_KEY | CA_START_AFTER;
			AParams.m_fTransTime = 0.10f;
			AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion
			pInstanceBase->GetISkeletonAnim()->StartAnimation("stand_stepRotate_com_rgt_01", AParams);

			AParams.m_nFlags = CA_LOOP_ANIMATION | CA_START_AFTER;
			AParams.m_fTransTime = 0.15f;
			AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion
			pInstanceBase->GetISkeletonAnim()->StartAnimation("stand_stepRotate_com_idle_90_01", AParams);
		}

		if (m_nFrameCounter == 900)
		{
			vLookAt = Vec3(0, 0, 1.3f);
			vLookFrom = Vec3(-3.5f, 1.0f, 1.4f);
		}

		if (m_nFrameCounter == 1000)
		{
			AParams.m_nFlags = CA_LOOP_ANIMATION;
			AParams.m_fTransTime = 0.55f;
			AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion
			AParams.m_nLayerID = 0;
			pInstanceBase->GetISkeletonAnim()->StartAnimation("stand_walk_com_fwd_fast_01", AParams);

			AParams.m_nFlags = CA_LOOP_ANIMATION;
			AParams.m_fTransTime = 0.15f;
			AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion
			AParams.m_nLayerID = 1;
			pInstanceBase->GetISkeletonAnim()->StartAnimation("stand_walk_com_fwd_fast_search_lft_alr_add_01", AParams);
		}

		if (m_nFrameCounter == 1400)
		{
			AParams.m_nFlags = CA_TRANSITION_TIMEWARPING | CA_LOOP_ANIMATION;
			AParams.m_fTransTime = 5.15f;
			AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion
			AParams.m_nLayerID = 0;
			pInstanceBase->GetISkeletonAnim()->StartAnimation("stand_run_com_fwd_fast_01", AParams);

			AParams.m_nFlags = CA_LOOP_ANIMATION;
			AParams.m_fTransTime = 2.15f;
			AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion
			AParams.m_nLayerID = 1;
			pInstanceBase->GetISkeletonAnim()->StartAnimation("stand_walk_com_fwd_fast_search_lft_alr_add_01", AParams);
		}

		if (m_nFrameCounter == 2000)
		{
			AParams.m_nFlags = CA_REPEAT_LAST_KEY;
			AParams.m_fTransTime = 0.55f;
			AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion
			AParams.m_nLayerID = 0;
			pInstanceBase->GetISkeletonAnim()->StartAnimation("stand_hitrun_fwd_back_01", AParams);

			AParams.m_nFlags = CA_LOOP_ANIMATION | CA_START_AFTER;
			AParams.m_fTransTime = 0.15f;
			AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion
			AParams.m_nLayerID = 0;
			pInstanceBase->GetISkeletonAnim()->StartAnimation("stand_run_com_fwd_fast_01", AParams);
		}

		if (m_nFrameCounter == 2200)
		{
			AParams.m_nFlags = CA_LOOP_ANIMATION;
			AParams.m_fTransTime = 2.15f;
			AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion
			AParams.m_nLayerID = 1;
			pInstanceBase->GetISkeletonAnim()->StartAnimation("stand_walk_com_fwd_fast_search_rgt_alr_add_01", AParams);
		}

		if (m_nFrameCounter == 2600)
		{
			AParams.m_nFlags = CA_REPEAT_LAST_KEY;
			AParams.m_fTransTime = 0.55f;
			AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion
			AParams.m_nLayerID = 0;
			pInstanceBase->GetISkeletonAnim()->StartAnimation("stand_hitrun_fwd_front_01", AParams);

			AParams.m_nFlags = CA_LOOP_ANIMATION | CA_START_AFTER;
			AParams.m_fTransTime = 0.15f;
			AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion
			AParams.m_nLayerID = 0;
			pInstanceBase->GetISkeletonAnim()->StartAnimation("stand_run_com_fwd_fast_01", AParams);
		}

		if (m_nFrameCounter == 2800)
		{
			AParams.m_nFlags = CA_LOOP_ANIMATION;
			AParams.m_fTransTime = 2.15f;
			AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion
			AParams.m_nLayerID = 1;
			pInstanceBase->GetISkeletonAnim()->StartAnimation("stand_walk_com_fwd_fast_search_lft_alr_add_01", AParams);
		}

		if (m_nFrameCounter == 3000)
		{
			AParams.m_nFlags = CA_TRANSITION_TIMEWARPING | CA_LOOP_ANIMATION;
			AParams.m_fTransTime = 4.15f;
			AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion
			pInstanceBase->GetISkeletonAnim()->StartAnimation("stand_walk_com_fwd_fast_01", AParams);

			AParams.m_nFlags = CA_LOOP_ANIMATION;
			AParams.m_fTransTime = 2.15f;
			AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion
			AParams.m_nLayerID = 1;
			pInstanceBase->GetISkeletonAnim()->StartAnimation("stand_walk_com_fwd_fast_search_rgt_alr_add_01", AParams);
		}

		if (m_nFrameCounter == 3300)
		{
			vLookAt = Vec3(0, 0, 1.7f);
			vLookFrom = Vec3(+1.5f, -4.0f, 1.8f);
		}

		if (m_nFrameCounter == 3600)
		{
			AParams.m_nFlags = CA_LOOP_ANIMATION;
			AParams.m_fTransTime = 2.15f;
			AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion
			AParams.m_nLayerID = 1;
			pInstanceBase->GetISkeletonAnim()->StartAnimation("stand_walk_com_fwd_fast_search_lft_alr_add_01", AParams);
		}

		SmoothCD(vLookAtSmooth, vLookAtRate, FrameTime, vLookAt, 0.45f);
		SmoothCD(vLookFromSmooth, vLookFromRate, FrameTime, vLookFrom, 2.00f);
		m_absCameraPos = vLookFromSmooth;
		Matrix33 orientation = Matrix33::CreateRotationVDir((vLookAtSmooth - vLookFromSmooth).GetNormalized());
		SetViewTM(Matrix34(orientation, vLookFromSmooth));
	}

}

void CModelViewportCE::SkelExtensionsTest(ICharacterInstance* pInstanceBase)
{
	const IDefaultSkeleton& rIDefaultSkeleton = pInstanceBase->GetIDefaultSkeleton();
	IAttachmentManager* pIAttachmentManager = pInstanceBase->GetIAttachmentManager();

	float color1[4] = { 1.0f, 1.0f, 1.0f, 1 };
	uint32 numJoints = rIDefaultSkeleton.GetJointCount();
	m_renderer->Draw2dLabel(12, g_ypos, 3.5f, color1, false, "numJoints: %d", numJoints), g_ypos += 34;
	static uint32 framecounter = 0;
	static uint32 lshift1 = 0;
	static uint32 lshift2 = 0;
	static uint32 lshift4 = 0;
	static uint32 lshift8 = 0;
	static uint32 shift = 0;

	framecounter++;

	float colorR[4] = { 1.0f, 0.8f, 0.4f, 1 };
	float colorD[4] = { 0.5f, 0.4f, 0.2f, 1 };

	shift = (framecounter >> 8) & 0x0f;
	if (shift & 1)
		m_renderer->Draw2dLabel(12, g_ypos, 2.5f, colorR, false, "1-NanoArm on"), g_ypos += 24;
	else
		m_renderer->Draw2dLabel(12, g_ypos, 2.5f, colorD, false, "1-NanoArm off"), g_ypos += 24;

	if (shift & 2)
		m_renderer->Draw2dLabel(12, g_ypos, 2.5f, colorR, false, "2-ShoulderPlates on"), g_ypos += 24;
	else
		m_renderer->Draw2dLabel(12, g_ypos, 2.5f, colorD, false, "2-ShoulderPlates off"), g_ypos += 24;

	if (shift & 4)
		m_renderer->Draw2dLabel(12, g_ypos, 2.5f, colorR, false, "3-Helmet on"), g_ypos += 24;
	else
		m_renderer->Draw2dLabel(12, g_ypos, 2.5f, colorD, false, "3-Helmet off"), g_ypos += 24;

	if (shift & 8)
		m_renderer->Draw2dLabel(12, g_ypos, 2.5f, colorR, false, "4-StrapsBelt on"), g_ypos += 24;
	else
		m_renderer->Draw2dLabel(12, g_ypos, 2.5f, colorD, false, "4-StrapsBelt off"), g_ypos += 24;

	uint32 chanage1 = (shift & 1) != (lshift1 & 1);
	if (chanage1)
	{
		lshift1 = shift;
		IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByName("skel_extension1");
		if (pIAttachment)
		{
			if (shift & 1)
			{
				ISkin* pISkin = GetAnimationSystem()->LoadModelSKIN("objects/characters/human/us/nanosuit_v2/_nano_arm.skin", CA_CharEditModel);
				if (pISkin)
				{
					IAttachmentSkin* pIAttachmentSkin = pIAttachment->GetIAttachmentSkin();
					CSKINAttachment* pSkinAttachment = new CSKINAttachment();
					pSkinAttachment->m_pIAttachmentSkin = pIAttachmentSkin; //this is a skin-attachment
					IAttachmentObject* pIAttachmentObject = (IAttachmentObject*)pSkinAttachment;
					pIAttachment->AddBinding(pIAttachmentObject, pISkin, CA_CharEditModel), Physicalize();
					return;
				}
			}
			else
			{
				pIAttachment->ClearBinding(CA_CharEditModel), Physicalize();
				return;
			}
		}
	}

	uint32 chanage2 = (shift & 2) != (lshift2 & 2);
	if (chanage2)
	{
		lshift2 = shift;
		IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByName("skel_extension2");
		if (pIAttachment)
		{
			if (shift & 2)
			{
				ISkin* pISkin = GetAnimationSystem()->LoadModelSKIN("objects/characters/human/us/nanosuit_v2/_ryse_plates.skin", CA_CharEditModel);
				if (pISkin)
				{
					IAttachmentSkin* pIAttachmentSkin = pIAttachment->GetIAttachmentSkin();
					CSKINAttachment* pSkinAttachment = new CSKINAttachment();
					pSkinAttachment->m_pIAttachmentSkin = pIAttachmentSkin; //this is a skin-attachment
					IAttachmentObject* pIAttachmentObject = (IAttachmentObject*)pSkinAttachment;
					pIAttachment->AddBinding(pIAttachmentObject, pISkin, CA_CharEditModel), Physicalize();
					return;
				}
			}
			else
			{
				pIAttachment->ClearBinding(CA_CharEditModel), Physicalize();
				return;
			}
		}
	}

	uint32 chanage4 = (shift & 4) != (lshift4 & 4);
	if (chanage4)
	{
		lshift4 = shift;
		IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByName("skel_extension3");
		if (pIAttachment)
		{
			if (shift & 4)
			{
				ISkin* pISkin = GetAnimationSystem()->LoadModelSKIN("objects/characters/human/us/nanosuit_v2/_ryse_helmet.skin", CA_CharEditModel);
				if (pISkin)
				{
					IAttachmentSkin* pIAttachmentSkin = pIAttachment->GetIAttachmentSkin();
					CSKINAttachment* pSkinAttachment = new CSKINAttachment();
					pSkinAttachment->m_pIAttachmentSkin = pIAttachmentSkin; //this is a skin-attachment
					IAttachmentObject* pIAttachmentObject = (IAttachmentObject*)pSkinAttachment;
					pIAttachment->AddBinding(pIAttachmentObject, pISkin, CA_CharEditModel), Physicalize();
					return;
				}
			}
			else
			{
				pIAttachment->ClearBinding(CA_CharEditModel), Physicalize();
				return;
			}
		}
	}

	uint32 chanage8 = (shift & 8) != (lshift8 & 8);
	if (chanage8)
	{
		lshift8 = shift;
		IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByName("skel_extension4");
		if (pIAttachment)
		{
			if (shift & 8)
			{
				ISkin* pISkin = GetAnimationSystem()->LoadModelSKIN("objects/characters/human/us/nanosuit_v2/_ryse_belt.skin", CA_CharEditModel);
				if (pISkin)
				{
					IAttachmentSkin* pIAttachmentSkin = pIAttachment->GetIAttachmentSkin();
					CSKINAttachment* pSkinAttachment = new CSKINAttachment();
					pSkinAttachment->m_pIAttachmentSkin = pIAttachmentSkin; //this is a skin-attachment
					IAttachmentObject* pIAttachmentObject = (IAttachmentObject*)pSkinAttachment;
					pIAttachment->AddBinding(pIAttachmentObject, pISkin, CA_CharEditModel), Physicalize();
					return;
				}
			}
			else
			{
				pIAttachment->ClearBinding(CA_CharEditModel), Physicalize();
				return;
			}
		}
	}
}

void CModelViewportCE::SetDesiredMotionParam(ISkeletonAnim* pISkeletonAnim, EMotionParamID nParameterID, float fParameter)
{
	if (nParameterID >= eMotionParamID_COUNT)
		return; //not a valid parameter

	for (int layer = 0; layer < ISkeletonAnim::LayerCount; ++layer)
	{
		uint32 numAnimsLayer = pISkeletonAnim->GetNumAnimsInFIFO(layer);
		for (uint32 a = 0; a < numAnimsLayer; a++)
		{
			//we store the parameters in the run-time structure of the ParametricSampler
			int animCount = pISkeletonAnim->GetNumAnimsInFIFO(layer);
			for (int i = 0; i < animCount; i++)
			{
				CAnimation& anim = pISkeletonAnim->GetAnimFromFIFO(layer, i);
				if (anim.GetParametricSampler() == 0)
					continue;
				uint32 IsEOC = anim.GetEndOfCycle();

				//It's a Parametric Animation
				SParametricSampler& lmg = *anim.GetParametricSampler();
				for (uint32 d = 0; d < lmg.m_numDimensions; d++)
				{
					if (lmg.m_MotionParameterID[d] == nParameterID)
					{
						uint32 locked = lmg.m_MotionParameterFlags[d] & CA_Dim_LockedParameter;
						uint32 init = lmg.m_MotionParameterFlags[d] & CA_Dim_Initialized;
						if (IsEOC || init == 0 || locked == 0)    //if already initialized and locked, then we can't change the parameter any more
						{
							lmg.m_MotionParameter[d] = fParameter;
							lmg.m_MotionParameterFlags[d] |= CA_Dim_Initialized;
						}
					}
				}
			}
		}
	}
}

int AnimEventCallback(ICharacterInstance* pInstance, void* pPlayer)
{

	//process bones specific stuff (IK, torso rotation, etc)
	((CModelViewportCE*)pPlayer)->AnimEventProcessing(pInstance);
	return 1;
}

void CModelViewportCE::AnimEventProcessing(ICharacterInstance* pInstance)
{

	ISkeletonAnim* pISkeletonAnim = pInstance->GetISkeletonAnim();
	ISkeletonPose* pISkeletonPose = pInstance->GetISkeletonPose();
	const IDefaultSkeleton& rIDefaultSkeleton = pInstance->GetIDefaultSkeleton();

	g_AnimEventCounter++;
	g_LastAnimEvent = pISkeletonAnim->GetLastAnimEvent();

	// If the event is a sound event, play the sound.
	//uint32 s0 = stricmp(g_LastAnimEvent.m_EventName, "sound") == 0;
	//uint32 s1 = stricmp(g_LastAnimEvent.m_EventName, "sound_tp") == 0;
	//if (g_LastAnimEvent.m_EventName && (s0 || s1))
	//{
	//	// TODO verify looping sounds not get "lost" and continue to play forever
	//	_smart_ptr<ISound> pSound = gEnv->pAudioSystem->CreateSound( g_LastAnimEvent.m_CustomParameter, FLAG_SOUND_DEFAULT_3D|FLAG_SOUND_LOAD_SYNCHRONOUSLY );
	//	if (pSound != 0)
	//	{
	//		if (!(pSound->GetFlags() & FLAG_SOUND_EVENT))
	//			pSound->GetInterfaceDeprecated()->SetMinMaxDistance( 1, 50 );

	//		pSound->Play();
	//		m_SoundIDs.push_back(pSound->GetId());
	//	}
	//}
	//// If the event is an footstep event, play a generic footstep sound.
	//else if (g_LastAnimEvent.m_EventName && stricmp(g_LastAnimEvent.m_EventName, "footstep") == 0)
	//{

	//	// setup sound params
	//	SMFXRunTimeEffectParams params;
	//	params.angle = 0;
	//	params.soundSemantic = eSoundSemantic_Physics_Footstep;

	//	IMaterialEffects* pMaterialEffects = gEnv->pMaterialEffects;
	//	TMFXEffectId effectId = InvalidEffectId;

	//	// note: for some reason material libraries are named "footsteps" when queried by name
	//	// and "footstep" when queried by ID
	//	if (pMaterialEffects)
	//	{
	//		if (g_LastAnimEvent.m_CustomParameter[0])
	//		{
	//			effectId = pMaterialEffects->GetEffectIdByName("footstep", g_LastAnimEvent.m_CustomParameter);
	//		}
	//		else
	//		{
	//			effectId = pMaterialEffects->GetEffectIdByName("footstep", "default");
	//		}
	//	}

	//	if (effectId != InvalidEffectId)
	//		pMaterialEffects->ExecuteEffect(effectId, params);
	//	else
	//		gEnv->pSystem->Warning(VALIDATOR_MODULE_EDITOR,VALIDATOR_WARNING,VALIDATOR_FLAG_SOUND,0,"Failed to find material for footstep sounds");

	//}
	//else if (g_LastAnimEvent.m_EventName && stricmp(g_LastAnimEvent.m_EventName, "groundEffect") == 0)
	//{
	//	// setup sound params
	//	SMFXRunTimeEffectParams params;
	//	params.angle = 0;
	//	params.soundSemantic = eSoundSemantic_Animation;

	//	IMaterialEffects* pMaterialEffects = gEnv->pMaterialEffects;
	//	TMFXEffectId effectId = InvalidEffectId;
	//	if (pMaterialEffects)
	//	{
	//		effectId = pMaterialEffects->GetEffectIdByName(g_LastAnimEvent.m_CustomParameter, "default");
	//	}

	//	if (effectId != 0)
	//		pMaterialEffects->ExecuteEffect(effectId, params);
	//	else
	//		gEnv->pSystem->Warning(VALIDATOR_MODULE_EDITOR,VALIDATOR_WARNING,VALIDATOR_FLAG_SOUND,0,"Failed to find material for groundEffect anim event");

	//}
	//// If the event is an footstep event, play a generic footstep sound.
	//else if (g_LastAnimEvent.m_EventName && stricmp(g_LastAnimEvent.m_EventName, "foley") == 0)
	//{

	//	// setup sound params
	//	SMFXRunTimeEffectParams params;
	//	params.angle = 0;
	//	params.soundSemantic = eSoundSemantic_Animation;

	//	IMaterialEffects* pMaterialEffects = gEnv->pMaterialEffects;
	//	TMFXEffectId effectId = InvalidEffectId;

	//	// replace with "foley"
	//	if (pMaterialEffects)
	//	{
	//		if (g_LastAnimEvent.m_CustomParameter[0])
	//		{
	//			effectId = pMaterialEffects->GetEffectIdByName("foley", g_LastAnimEvent.m_CustomParameter);
	//		}
	//		else
	//		{
	//			effectId = pMaterialEffects->GetEffectIdByName("foley", "default");
	//		}

	//		if (effectId != InvalidEffectId)
	//			pMaterialEffects->ExecuteEffect(effectId, params);
	//		else
	//			gEnv->pSystem->Warning(VALIDATOR_MODULE_EDITOR,VALIDATOR_WARNING,VALIDATOR_FLAG_SOUND,0,"Failed to find effect entry for foley sounds");
	//	}

	//}

	// If the event is an effect event, spawn the event.
	if (g_LastAnimEvent.m_EventName && stricmp(g_LastAnimEvent.m_EventName, "effect") == 0)
	{
		if (GetCharacterBase())
		{
			ISkeletonAnim* pSkeletonAnim = GetCharacterBase()->GetISkeletonAnim();
			ISkeletonPose* pSkeletonPose = GetCharacterBase()->GetISkeletonPose();
			const IDefaultSkeleton& rIDefaultSkeleton = GetCharacterBase()->GetIDefaultSkeleton();
			m_effectManager.SetSkeleton(pSkeletonAnim, pSkeletonPose, &rIDefaultSkeleton);
			m_effectManager.SpawnEffect(g_LastAnimEvent.m_AnimID, g_LastAnimEvent.m_AnimPathName, g_LastAnimEvent.m_CustomParameter,
			                            g_LastAnimEvent.m_BonePathName, g_LastAnimEvent.m_vOffset, g_LastAnimEvent.m_vDir);
		}
	}

	//----------------------------------------------------------------------------------------

	IAttachmentManager* pIAttachmentManager = pInstance->GetIAttachmentManager();
	IAttachment* pSAtt1 = pIAttachmentManager->GetInterfaceByName("riflepos01");
	IAttachment* pSAtt2 = pIAttachmentManager->GetInterfaceByName("riflepos02");
	IAttachment* pWAtt = pIAttachmentManager->GetInterfaceByName("weapon");
	if (g_LastAnimEvent.m_EventName)
	{

		int32 nWeaponIdx = rIDefaultSkeleton.GetJointIDByName("weapon_bone");
		int32 nSpine2Idx = rIDefaultSkeleton.GetJointIDByName("Bip01 Spine2");
		if (nWeaponIdx < 0)
			return;
		if (nSpine2Idx < 0)
			return;

		QuatT absWeaponBone = pISkeletonPose->GetAbsJointByID(nWeaponIdx);
		QuatT absSpine2 = pISkeletonPose->GetAbsJointByID(nSpine2Idx);
		QuatT defSpine2 = rIDefaultSkeleton.GetDefaultAbsJointByID(nSpine2Idx);

		//QuatT relative1           = (absWeaponBone.GetInverted()*absSpine2)*defSpine2;
		QuatT AttachmentLocation = defSpine2 * (absSpine2.GetInverted() * absWeaponBone);

		if (stricmp(g_LastAnimEvent.m_EventName, "WeaponDrawRight") == 0)
		{
			pSAtt1->HideAttachment(1); //hide
			pSAtt2->HideAttachment(0); //show
			pWAtt->HideAttachment(0);  //show
		}

		if (stricmp(g_LastAnimEvent.m_EventName, "WeaponDrawLeft") == 0)
		{
			pSAtt1->HideAttachment(0);
			pSAtt2->HideAttachment(1);
			pWAtt->HideAttachment(0);  //show
		}

	}

	uint32 i = 0;
}

//---------------------------------------------------------------------
//---------------------------------------------------------------------
//---------------------------------------------------------------------
uint32 CModelViewportCE::UseHumanLimbIK(ICharacterInstance* pInstance, const char* strLimb)
{

	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	ISkeletonAnim* pISkeletonAnim = pInstance->GetISkeletonAnim();
	ISkeletonPose* pISkeletonPose = pInstance->GetISkeletonPose();

	AABB aabb = pInstance->GetAABB();
	f32 fRadius = sqrt_tpl((aabb.max - aabb.min).GetLengthFast() * 0.8f);

	static Vec3 target = Vec3(ZERO);

	Matrix34 VMat = GetCamera().GetViewMatrix();
	Vec3 lr = Vec3(VMat.m00, VMat.m01, VMat.m02) * m_AverageFrameTime * fRadius;
	Vec3 fb = Vec3(VMat.m10, VMat.m11, VMat.m12) * m_AverageFrameTime * fRadius;
	Vec3 ud = Vec3(VMat.m20, VMat.m21, VMat.m22) * m_AverageFrameTime * fRadius;
	int32 STRG = CheckVirtualKey(VK_CONTROL);
	int32 np1 = CheckVirtualKey(VK_NUMPAD1);
	int32 np2 = CheckVirtualKey(VK_NUMPAD2);
	int32 np3 = CheckVirtualKey(VK_NUMPAD3);
	int32 np4 = CheckVirtualKey(VK_NUMPAD4);
	int32 np5 = CheckVirtualKey(VK_NUMPAD5);
	int32 np6 = CheckVirtualKey(VK_NUMPAD6);
	int32 np7 = CheckVirtualKey(VK_NUMPAD7);
	int32 np8 = CheckVirtualKey(VK_NUMPAD8);
	int32 np9 = CheckVirtualKey(VK_NUMPAD9);

	if (STRG && np8) target += ud;
	else if (STRG && np2)
		target -= ud;
	if (!STRG && np8) target += fb;
	else if (!STRG && np2)
		target -= fb;
	if (np6) target += lr;
	else if (np4)
		target -= lr;

	static Ang3 angle = Ang3(ZERO);
	angle.x += 0.1f;
	angle.y += 0.01f;
	angle.z += 0.001f;
	AABB sAABB = AABB(Vec3(-0.03f, -0.03f, -0.03f) * fRadius, Vec3(+0.03f, +0.03f, +0.03f) * fRadius);
	OBB obb = OBB::CreateOBBfromAABB(Matrix33::CreateRotationXYZ(angle), sAABB);
	pAuxGeom->DrawOBB(obb, target, 1, RGBA8(0xff, 0xff, 0xff, 0xff), eBBD_Extremes_Color_Encoded);

	pISkeletonPose->SetHumanLimbIK(target, strLimb);

	return 1;
}

