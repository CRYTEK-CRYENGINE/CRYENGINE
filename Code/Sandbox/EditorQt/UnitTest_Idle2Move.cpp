// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Controls\AnimationToolBar.h"

#include "CharacterEditor\ModelViewportCE.h"
#include "CharacterEditor\CharPanel_Animation.h"
#include <CryAnimation/ICryAnimation.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryPhysics/IPhysics.h>
#include <CrySystem/ITimer.h>
#include <CryRenderer/IRenderAuxGeom.h>

extern uint32 g_ypos;

//------------------------------------------------------------------------------
//---                         Idle 2 MOve  UnitTest                          ---
//------------------------------------------------------------------------------
void CModelViewportCE::Idle2Move_UnitTest(ICharacterInstance* pInstance, const SRendParams& rRP, const SRenderingPassInfo& passInfo)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);

	IAnimationSet* pIAnimationSet = pInstance->GetIAnimationSet();
	ISkeletonAnim* pISkeletonAnim = pInstance->GetISkeletonAnim();
	ISkeletonPose* pISkeletonPose = pInstance->GetISkeletonPose();

	Idle2Move(pIAnimationSet);

	m_relCameraRotZ = 0;
	m_relCameraRotX = 0;
	uint32 useFootAnchoring = 0;
	if (m_pCharPanel_Animation)
	{
		useFootAnchoring = m_pCharPanel_Animation->GetFootAnchoring();
	}

	//update animation
	pISkeletonAnim->SetAnimationDrivenMotion(1);

	int PostProcessCallback(ICharacterInstance * pInstance, void* pPlayer);
	pISkeletonPose->SetPostProcessCallback(0, 0);

	//	pISkeleton->SetAnimEventCallback(AnimEventCallback,this);

	const CCamera& camera = GetCamera();
	float fDistance = (camera.GetPosition() - m_PhysicalLocation.t).GetLength();
	float fZoomFactor = 0.001f + 0.999f * (RAD2DEG(camera.GetFov()) / 60.f);

	SAnimationProcessParams params;
	params.locationAnimation = m_PhysicalLocation;
	params.bOnRender = 0;
	params.zoomAdjustedDistanceFromCamera = fDistance * fZoomFactor;
	ExternalPostProcessing(pInstance);
	pInstance->StartAnimationProcessing(params);
	const QuatT& relmove = pISkeletonAnim->GetRelMovement();
	m_PhysicalLocation = m_PhysicalLocation * relmove;
	m_PhysicalLocation.t.z = 0;
	m_PhysicalLocation.q.Normalize();
	pInstance->SetAttachmentLocation_DEPRECATED(m_PhysicalLocation);
	pInstance->FinishAnimationComputations();

	//use updated animations to render character
	m_LocalEntityMat = Matrix34(m_PhysicalLocation);
	//	m_EntityMat   = Matrix34(m_PhysicalLocation);
	SRendParams rp = rRP;
	rp.pMatrix = &m_LocalEntityMat;
	rp.fDistance = (m_PhysicalLocation.t - m_absCameraPos).GetLength();
	pInstance->SetViewdir(GetCamera().GetViewdir());
	gEnv->p3DEngine->PrecacheCharacter(NULL, 1.f, pInstance, pInstance->GetIMaterial(), m_LocalEntityMat, 0, 1.f, 4, true, passInfo);
	pInstance->Render(rp, QuatTS(IDENTITY), passInfo);

	if (mv_showGrid)
		DrawHeightField();

	//------------------------------------------------------
	//---   draw the path of the past
	//------------------------------------------------------
	uint32 numEntries = m_arrAnimatedCharacterPath.size();
	for (int32 i = (numEntries - 2); i > -1; i--)
		m_arrAnimatedCharacterPath[i + 1] = m_arrAnimatedCharacterPath[i];
	m_arrAnimatedCharacterPath[0] = m_PhysicalLocation.t;
	AABB aabb;
	for (uint32 i = 0; i < numEntries; i++)
	{
		aabb.min = Vec3(-0.01f, -0.01f, -0.01f) + m_arrAnimatedCharacterPath[i];
		aabb.max = Vec3(+0.01f, +0.01f, +0.01f) + m_arrAnimatedCharacterPath[i];
		pAuxGeom->DrawAABB(aabb, 1, RGBA8(0x00, 0x00, 0xff, 0x00), eBBD_Extremes_Color_Encoded);
	}

	GetIEditor()->GetSystem()->GetIConsole()->GetCVar("ca_DrawSkeleton")->Set(mv_showSkeleton);
}

//0-combat walk nw
//1-combat walk pistol
//2-combat walk rifle
//3-combat walk mg

//4-combat run nw
//5-combat run pistol
//6-combat run rifle
//7-combat run mg

//8-stealth walk nw
//9-stealth walk pistol
//10-stealth walk rifle
//11-stealth walk mg

//12-stealth run nw
//13-stealth run pistol
//14-stealth run rifle
//15-stealth run mg

uint32 STANCE = 0;

#define STATE_STAND           (0)
#define STATE_WALK            (1)
#define STATE_WALKSTRAFELEFT  (2)
#define STATE_WALKSTRAFERIGHT (3)
#define STATE_RUN             (4)
#define STATE_MOVE2IDLE       (5)

#define RELAXED               (0)
#define COMBAT                (1)
#define STEALTH               (2)
#define CROUCH                (3)
#define PRONE                 (4)

const char* strAnimName;

f32 strafrad = 0;
Vec3 StrafeVectorWorld = Vec3(0, 1, 0);
Vec3 LocalStrafeAnim = Vec3(0, 1, 0);

Lineseg g_MoveDirectionWorld = Lineseg(Vec3(ZERO), Vec3(ZERO));

f32 g_LocomotionSpeed = 4.5f;
uint64 _rcr_key_Pup = 0;
uint64 _rcr_key_Pdown = 0;
uint64 _rcr_key_return = 0;

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
f32 CModelViewportCE::Idle2Move(IAnimationSet* pIAnimationSet)
{
	uint32 ypos = 380;

	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	f32 FrameTime = GetISystem()->GetITimer()->GetFrameTime();
	f32 TimeScale = GetISystem()->GetITimer()->GetTimeScale();
	ISkeletonAnim* pISkeletonAnim = GetCharacterBase()->GetISkeletonAnim();
	ISkeletonPose* pISkeletonPose = GetCharacterBase()->GetISkeletonPose();
	const IDefaultSkeleton& rIDefaultSkeleton = GetCharacterBase()->GetIDefaultSkeleton();
	GetCharacterBase()->SetCharEditMode(CA_CharacterTool);

	Vec3 absPlayerPos = m_PhysicalLocation.t;

	_rcr_key_Pup <<= 1;
	if (CheckVirtualKey(VK_PRIOR)) _rcr_key_Pup |= 1;
	if ((_rcr_key_Pup & 3) == 1) g_LocomotionSpeed += 0.10f;
	if (_rcr_key_Pup == 0xffffffffffffffff) g_LocomotionSpeed += FrameTime;

	_rcr_key_Pdown <<= 1;
	if (CheckVirtualKey(VK_NEXT)) _rcr_key_Pdown |= 1;
	if ((_rcr_key_Pdown & 3) == 1) g_LocomotionSpeed -= 0.10f;
	if (_rcr_key_Pdown == 0xffffffffffffffff) g_LocomotionSpeed -= FrameTime;

	if (g_LocomotionSpeed > 16.0f)
		g_LocomotionSpeed = 16.0f;
	if (g_LocomotionSpeed < 1.0f)
		g_LocomotionSpeed = 1.0f;

	_rcr_key_return <<= 1;
	if (CheckVirtualKey(VK_CONTROL) == 0 && CheckVirtualKey(VK_RETURN))
	{
		_rcr_key_return |= 1;
		if ((_rcr_key_return & 3) == 1) STANCE++;
	}
	if (CheckVirtualKey(VK_CONTROL) != 0 && CheckVirtualKey(VK_RETURN))
	{
		_rcr_key_return |= 1;
		if ((_rcr_key_return & 3) == 1) STANCE--;
	}

	if (STANCE > 1) STANCE = 0;
	if (STANCE < 0) STANCE = 1;

	float color1[4] = { 1, 1, 0, 1 };
	float color2[4] = { 0, 1, 0, 1 };

	if (STANCE == 0) m_renderer->Draw2dLabel(12, ypos, 2.0f, color2, false, "Stance: %02d Relaxed Idle-2-Walk", STANCE);
	if (STANCE == 1) m_renderer->Draw2dLabel(12, ypos, 2.0f, color2, false, "Stance: %02d Combat  Idle-2-Move", STANCE);
	if (STANCE == 2) m_renderer->Draw2dLabel(12, ypos, 2.0f, color2, false, "Stance: %02d _stand_tac_run2idle_nova_3p_01", STANCE);
	if (STANCE == 3) m_renderer->Draw2dLabel(12, ypos, 2.0f, color2, false, "Stance: %02d _stand_tac_run2idle_heavy_3p_01", STANCE);
	ypos += 20;

	int32 left = 0;
	int32 right = 0;
	m_key_W = 0;
	m_key_S = 0;
	if (CheckVirtualKey('W'))
		m_key_W = 1;
	if (CheckVirtualKey('S'))
		m_key_S = 1;

	if (CheckVirtualKey(VK_LEFT))
		left = +3;
	if (CheckVirtualKey(VK_RIGHT))
		right = -3;

	//	static f32 SmoothFrameTime = 0.00f;
	//	static f32 SmoothFrameTimeRate = 0.00f;
	//	SmoothCD(SmoothFrameTime, SmoothFrameTimeRate, FrameTime, FrameTime*left+FrameTime*right, 0.25f);
	strafrad += FrameTime * left + FrameTime * right;

	StrafeVectorWorld = Matrix33::CreateRotationZ(strafrad).GetColumn1();
	LocalStrafeAnim = StrafeVectorWorld * m_PhysicalLocation.q;

	Vec3 wpos = m_PhysicalLocation.t + Vec3(0, 0, 1);
	pAuxGeom->DrawLine(wpos, RGBA8(0xff, 0xff, 0x00, 0x00), wpos + StrafeVectorWorld * 2, RGBA8(0x00, 0x00, 0x00, 0x00));

	CAnimation animationT;
	uint32 numAnimsLayer0 = pISkeletonAnim->GetNumAnimsInFIFO(0);
	if (numAnimsLayer0)
		animationT = pISkeletonAnim->GetAnimFromFIFO(0, numAnimsLayer0 - 1);

	CryCharAnimationParams AParams;

	if (m_State < 0)
		m_State = STATE_STAND;

	if (m_key_S && m_State == STATE_RUN && numAnimsLayer0 <= 2)
	{
		m_State = STATE_MOVE2IDLE;
	}

	static f32 LastTurnRadianSec7 = 0;
	static f32 LastTurnRadianSec6 = 0;
	static f32 LastTurnRadianSec5 = 0;
	static f32 LastTurnRadianSec4 = 0;
	static f32 LastTurnRadianSec3 = 0;
	static f32 LastTurnRadianSec2 = 0;
	static f32 LastTurnRadianSec1 = 0;
	static f32 TurnRadianSec = 0;

	pISkeletonAnim->SetLayerPlaybackScale(0, 1); //no scaling

	f32 fTravelSpeed = g_LocomotionSpeed;
	f32 fStartTurnAngle = -atan2f(LocalStrafeAnim.x, LocalStrafeAnim.y);
	f32 fTurnSpeed = 0;

	//standing idle
	if (m_State == STATE_STAND && numAnimsLayer0 <= 1)
	{
		//blend into idle after walk-stop
		AParams.m_nFlags = CA_LOOP_ANIMATION | CA_START_AFTER;
		AParams.m_fTransTime = 0.15f;
		AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion
		if (STANCE == 0) strAnimName = "relaxed_tac_idle_rifle_3p_01";
		if (STANCE == 1) strAnimName = "stand_tac_step_rifle_idle_3p_01";
		if (STANCE == 2) strAnimName = "_stand_tac_step_nova_3p_01";
		if (STANCE == 3) strAnimName = "_stand_tac_step_heavy_3p_01";
		pISkeletonAnim->StartAnimation(strAnimName, AParams);

		g_MoveDirectionWorld.start = m_PhysicalLocation.t + Vec3(0, 0, 0.01f);
		g_MoveDirectionWorld.end = m_PhysicalLocation.t + Vec3(0, 0, 0.01f) + StrafeVectorWorld * 10.0f;

		m_State = STATE_STAND;
		m_Stance = 0;
	}

	//start "idle2move" and "running"
	if (m_key_W && m_State == STATE_STAND && numAnimsLayer0 <= 1)
	{
		AParams.m_nFlags = CA_REPEAT_LAST_KEY;  //CA_LOOP_ANIMATION;
		AParams.m_fTransTime = 0.00f;           //start immediately
		AParams.m_fKeyTime = -1;                //use keytime of previous motion to start this motion
		AParams.m_fPlaybackSpeed = 1.0f;
		if (STANCE == 0)  strAnimName = "relaxed_tac_idle2move_rifle_3p_01";
		if (STANCE == 1)  strAnimName = "stand_tac_idle2move_rifle_3p_01";
		if (STANCE == 2)  strAnimName = "_stand_tac_idle2move_nova_3p_01";
		if (STANCE == 3)  strAnimName = "_stand_tac_idle2move_heavy_3p_01";
		pISkeletonAnim->StartAnimation(strAnimName, AParams);

		AParams.m_nFlags = CA_LOOP_ANIMATION | CA_IDLE2MOVE | CA_TRANSITION_TIMEWARPING;
		f32 fTransTime = (1.00f / fTravelSpeed) * 1.3f;    //average time for transition
		AParams.m_fTransTime = clamp_tpl(fTransTime, 0.3f, 0.9f);

		AParams.m_fKeyTime = -1;        //use keytime of previous motion to start this motion
		AParams.m_fPlaybackSpeed = 1.0f;
		if (STANCE == 0)  strAnimName = "relaxed_tac_walk_nw_3p_01";
		if (STANCE == 1)  strAnimName = "stand_tac_move_rifle_3p_01";
		if (STANCE == 2)  strAnimName = "_stand_tac_run_nova_3p_01";
		if (STANCE == 3)  strAnimName = "_stand_tac_run_heavy_3p_01";
		pISkeletonAnim->StartAnimation(strAnimName, AParams);

		g_MoveDirectionWorld.start = m_PhysicalLocation.t + Vec3(0, 0, 0.01f);
		g_MoveDirectionWorld.end = m_PhysicalLocation.t + Vec3(0, 0, 0.01f) + StrafeVectorWorld * 10.0f;

		m_State = STATE_RUN;
		m_Stance = 0;
	}

	//stopping
	static f32 s_leg = 0;
	f32 fAnimTime = animationT.GetCurrentSegmentNormalizedTime();
	uint32 isActivated = animationT.IsActivated();

	uint32 nTransPossible = 0;
	nTransPossible |= (m_State == STATE_MOVE2IDLE && numAnimsLayer0 == 2 && isActivated);
	nTransPossible |= (m_State == STATE_MOVE2IDLE && numAnimsLayer0 == 1 && isActivated);

	if (nTransPossible)
	{
		if (STANCE == 0) strAnimName = "relaxed_tac_walk2idle_rifle_3p_01";
		if (STANCE == 1) strAnimName = "stand_tac_move2idle_rifle_3p_01";
		if (STANCE == 2) strAnimName = "_stand_tac_run2idle_nova_3p_01";
		if (STANCE == 3) strAnimName = "_stand_tac_run2idle_heavy_3p_01";
		//		if (animation.m_fAnimTime<0.25f || animation.m_fAnimTime>0.80f)
		if (fAnimTime < 0.10f || fAnimTime > 0.90f)
		{
			//stop with LEFT leg on ground
			s_leg = -0.5f;  //left
			AParams.m_nFlags = CA_REPEAT_LAST_KEY;
			AParams.m_fTransTime = 0.50f; //average time for transition
			AParams.m_fKeyTime = -1;      //use keytime of previous motion to start this motion
			pISkeletonAnim->StartAnimation(strAnimName, AParams);
			m_State = STATE_STAND;
			m_Stance = 0;
		}

		if (fAnimTime > 0.45f && fAnimTime < 0.55f)
		{
			//stop with RIGHT leg on ground
			s_leg = 0.5f;
			AParams.m_nFlags = CA_REPEAT_LAST_KEY;
			AParams.m_fTransTime = 0.50f; //average time for transition
			AParams.m_fKeyTime = -1;      //use keytime of previous motion to start this motion
			pISkeletonAnim->StartAnimation(strAnimName, AParams);
			m_State = STATE_STAND;
			m_Stance = 0;
		}
	}

	numAnimsLayer0 = pISkeletonAnim->GetNumAnimsInFIFO(0);
	if (m_State == STATE_RUN && numAnimsLayer0 <= 2)
	{
		f32 FramesPerSecond = TimeScale / m_AverageFrameTime;

		LastTurnRadianSec7 = LastTurnRadianSec6;
		LastTurnRadianSec6 = LastTurnRadianSec5;
		LastTurnRadianSec5 = LastTurnRadianSec4;
		LastTurnRadianSec4 = LastTurnRadianSec3;
		LastTurnRadianSec3 = LastTurnRadianSec2;
		LastTurnRadianSec2 = LastTurnRadianSec1;
		LastTurnRadianSec1 = TurnRadianSec;

		TurnRadianSec = Ang3::CreateRadZ(m_PhysicalLocation.q.GetColumn1(), StrafeVectorWorld) * FramesPerSecond;
		TurnRadianSec /= (FramesPerSecond / 12.0f);

		f32 avrg = (TurnRadianSec + LastTurnRadianSec1) * 0.5f;
		TurnRadianSec = avrg;
		//	fStartTurnAngle	=	0;
		fTurnSpeed = TurnRadianSec;

		if (fTurnSpeed > 5.0f)
			fTurnSpeed = 5.0f;
		if (fTurnSpeed < -5.0f)
			fTurnSpeed = -5.0f;
		//		fTravelSpeed *= (5.0f-fabsf(fTurnSpeed))/5.0f;
		//		if (fTravelSpeed<1.0f)
		//			fTravelSpeed=1.0f;
		m_renderer->Draw2dLabel(12, ypos, 1.2f, color1, false, "TurnRadianSec: %f", TurnRadianSec);
		ypos += 10;
	}

	m_renderer->Draw2dLabel(12, ypos, 2.5f, color1, false, "fStartTurnAngle: %f     fTravelVector: %f %f", fStartTurnAngle, LocalStrafeAnim.x, LocalStrafeAnim.y);
	ypos += 30;
	m_renderer->Draw2dLabel(12, ypos, 2.5f, color1, false, "fTravelSpeed: %f", fTravelSpeed);
	ypos += 40;
	m_renderer->Draw2dLabel(12, ypos, 2.5f, color1, false, "Leg: %f", s_leg);
	ypos += 40;

	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelAngle, 0, FrameTime);
	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelSpeed, fTravelSpeed, FrameTime);
	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelDist, -1, FrameTime);
	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TurnSpeed, fTurnSpeed, FrameTime);
	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TurnAngle, fStartTurnAngle, FrameTime);
	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_StopLeg, s_leg, FrameTime);

	pAuxGeom->DrawLine(g_MoveDirectionWorld.start, RGBA8(0xff, 0xff, 0xff, 0x00), g_MoveDirectionWorld.end, RGBA8(0x00, 0xff, 0x00, 0x00));

	//------------------------------------------------------------------------
	//---  camera control
	//------------------------------------------------------------------------
	//	uint32 AttachedCamera = m_pCharPanel_Animation->GetAttachedCamera();
	//	if (AttachedCamera)
	{

		m_absCameraHigh -= m_relCameraRotX * 0.002f;
		if (m_absCameraHigh > 3)
			m_absCameraHigh = 3;
		if (m_absCameraHigh < 0)
			m_absCameraHigh = 0;

		SmoothCD(m_LookAt, m_LookAtRate, m_AverageFrameTime, Vec3(absPlayerPos.x, absPlayerPos.y, 0.7f), 0.05f);
		Vec3 absCameraPos = m_LookAt + Vec3(0, 0, m_absCameraHigh);
		absCameraPos.x = m_LookAt.x;
		absCameraPos.y = m_LookAt.y + 4.0f;

		SmoothCD(m_vCamPos, m_vCamPosRate, m_AverageFrameTime, absCameraPos, 0.25f);
		m_absCameraPos = m_vCamPos;

		Matrix33 orientation = Matrix33::CreateRotationVDir((m_LookAt - m_absCameraPos).GetNormalized(), 0);
		if (m_pCharPanel_Animation->GetFixedCamera())
		{
			m_absCameraPos = Vec3(0, 3, 1);
			orientation = Matrix33::CreateRotationVDir((m_LookAt - m_absCameraPos).GetNormalized());
		}
		m_absCameraPos = m_vCamPos;
		SetViewTM(Matrix34(orientation, m_vCamPos));

		const char* RootName = rIDefaultSkeleton.GetJointNameByID(0);
		if (RootName[0] == 'B' && RootName[1] == 'i' && RootName[2] == 'p')
		{
			uint32 AimIKState = m_pCharPanel_Animation->GetAimIK();
			uint32 AimIKLayer = m_pCharPanel_Animation->GetAimIKLayerValue();

			if (IAnimationPoseBlenderDir* pPoseBlenderAim = pISkeletonPose->GetIPoseBlenderAim())
			{
				pPoseBlenderAim->SetTarget(m_absCameraPos);
				pPoseBlenderAim->SetLayer(AimIKLayer);
				pPoseBlenderAim->SetState(AimIKState);
			}

			//m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"AimStatus: %d %d",AimIK, status );	ypos+=10;
			//m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"m_absCameraPos: %f %f %f",m_absCameraPos.x,m_absCameraPos.y,m_absCameraPos.z );	ypos+=10;
		}

	}

	return 0;

}

