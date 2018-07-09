// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "CharacterEditor\ModelViewportCE.h"
#include "CharacterEditor\CharPanel_Animation.h"

#include <CryAnimation/ICryAnimation.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryPhysics/IPhysics.h>
#include <CrySystem/ITimer.h>
#include <CryRenderer/IRenderAuxGeom.h>

extern uint32 g_ypos;
int32 stance = 0;

//------------------------------------------------------------------------------
//---                          IdleStep  UnitTest                            ---
//------------------------------------------------------------------------------
void CModelViewportCE::IdleStep_UnitTest(ICharacterInstance* pInstance, const SRendParams& rRP, const SRenderingPassInfo& passInfo)
{

	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);

	IAnimationSet* pIAnimationSet = pInstance->GetIAnimationSet();
	ISkeletonAnim* pISkeletonAnim = pInstance->GetISkeletonAnim();
	ISkeletonPose* pISkeletonPose = pInstance->GetISkeletonPose();

	IdleStep(pIAnimationSet);

	m_relCameraRotZ = 0;
	m_relCameraRotX = 0;

	uint32 useFootAnchoring = 0;
	if (m_pCharPanel_Animation)
	{
		useFootAnchoring = m_pCharPanel_Animation->GetFootAnchoring();
	}

	//update animation
	pISkeletonAnim->SetAnimationDrivenMotion(1);
	pISkeletonPose->SetPostProcessCallback(NULL, NULL);

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
	m_PhysicalLocation.q.Normalize();
	pInstance->FinishAnimationComputations();

	Vec3 absAxisX = m_PhysicalLocation.GetColumn0();
	Vec3 absAxisY = m_PhysicalLocation.GetColumn1();
	Vec3 absAxisZ = m_PhysicalLocation.GetColumn2();
	Vec3 absRootPos = m_PhysicalLocation.t + pISkeletonPose->GetAbsJointByID(0).t;
	pAuxGeom->DrawLine(absRootPos, RGBA8(0xff, 0x00, 0x00, 0x00), absRootPos + m_PhysicalLocation.GetColumn0() * 2.0f, RGBA8(0x00, 0x00, 0x00, 0x00));
	pAuxGeom->DrawLine(absRootPos, RGBA8(0x00, 0xff, 0x00, 0x00), absRootPos + m_PhysicalLocation.GetColumn1() * 2.0f, RGBA8(0x00, 0x00, 0x00, 0x00));
	pAuxGeom->DrawLine(absRootPos, RGBA8(0x00, 0x00, 0xff, 0x00), absRootPos + m_PhysicalLocation.GetColumn2() * 2.0f, RGBA8(0x00, 0x00, 0x00, 0x00));

	//use updated animations to render character
	m_LocalEntityMat = Matrix34(m_PhysicalLocation);
	SRendParams rp = rRP;
	rp.pMatrix = &m_LocalEntityMat;
	rp.fDistance = (m_PhysicalLocation.t - m_absCameraPos).GetLength();
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

extern const char* strAnimName;

f32 StrafRadIS = 0;
Vec3 StrafeVectorWorldIS = Vec3(0, 1, 0);
Vec3 LocalStrafeAnimIS = Vec3(0, 1, 0);
Vec3 LastIdlePos = Vec3(0, 0, 0);

Lineseg g_MoveDirectionWorldIS = Lineseg(Vec3(ZERO), Vec3(ZERO));

f32 g_StepScale = 1.0f;
uint32 rcr_key_Pup = 0;
uint32 rcr_key_Pdown = 0;
uint64 rcr_key_return = 0;

uint32 m_key_R = 0;

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
f32 CModelViewportCE::IdleStep(IAnimationSet* pIAnimationSet)
{

	uint32 ypos = 382;

	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);

	f32 fFrameTime = GetISystem()->GetITimer()->GetFrameTime();
	ISkeletonAnim* pISkeletonAnim = GetCharacterBase()->GetISkeletonAnim();
	ISkeletonPose* pISkeletonPose = GetCharacterBase()->GetISkeletonPose();
	const IDefaultSkeleton& rIDefaultSkeleton = GetCharacterBase()->GetIDefaultSkeleton();
	GetCharacterBase()->SetCharEditMode(CA_CharacterTool);

	//	m_PhysicalLocation.q.SetRotationZ(0.0f);

	Vec3 absPlayerPos = m_PhysicalLocation.t;

	rcr_key_Pup <<= 1;
	if (CheckVirtualKey(VK_PRIOR))  rcr_key_Pup |= 1;
	if ((rcr_key_Pup & 3) == 1) g_StepScale += 0.05f;
	rcr_key_Pdown <<= 1;
	if (CheckVirtualKey(VK_NEXT)) rcr_key_Pdown |= 1;
	if ((rcr_key_Pdown & 3) == 1) g_StepScale -= 0.05f;

	rcr_key_return <<= 1;
	if (CheckVirtualKey(VK_CONTROL) == 0 && CheckVirtualKey(VK_RETURN))
	{
		rcr_key_return |= 1;
		if ((rcr_key_return & 3) == 1) stance++;
	}
	if (CheckVirtualKey(VK_CONTROL) != 0 && CheckVirtualKey(VK_RETURN))
	{
		rcr_key_return |= 1;
		if ((rcr_key_return & 3) == 1) stance--;
	}

	if (stance > 0) stance = 0;
	if (stance < 0) stance = 0;

	if (g_StepScale > 1.5f)
		g_StepScale = 1.5f;
	if (g_StepScale < 0.0f)
		g_StepScale = 0.0f;

	uint32 left = 0;
	uint32 right = 0;

	m_key_W = 0;
	m_key_R = 0;
	m_key_S = 0;

	if (CheckVirtualKey('W'))
		m_key_W = 1;
	if (CheckVirtualKey('R'))
		m_key_R = 1;
	if (CheckVirtualKey('S'))
		m_key_S = 1;

	if (CheckVirtualKey(VK_LEFT))
		left = 1;
	if (CheckVirtualKey(VK_RIGHT))
		right = 1;

	if (left)
		StrafRadIS += 1.5f * fFrameTime;
	if (right)
		StrafRadIS -= 1.5f * fFrameTime;

	//always make sure we deactivate the overrides
	pISkeletonAnim->SetLayerPlaybackScale(0, 1); //no scaling

	StrafeVectorWorldIS = Matrix33::CreateRotationZ(StrafRadIS).GetColumn1();
	LocalStrafeAnimIS = StrafeVectorWorldIS * m_PhysicalLocation.q;

	Vec3 absPelvisPos = pISkeletonPose->GetAbsJointByID(1).t;

	//	Vec3 CurrMoveDir =  (m_absEntityMat) * pISkeleton->GetCurrentMoveDirection();
	Vec3 wpos = m_PhysicalLocation.t;
	pAuxGeom->DrawLine(wpos + absPelvisPos, RGBA8(0xff, 0xff, 0x00, 0x00), wpos + absPelvisPos + StrafeVectorWorldIS * 2, RGBA8(0x00, 0x00, 0x00, 0x00));
	//pAuxGeom->DrawLine( absRootPos+Vec3(0,0,0.1f),RGBA8(0xff,0xff,0x00,0x00), absRootPos+Vec3(0,0,0.1f)-CurrMoveDir*4,RGBA8(0x00,0x00,0x00,0x00) );

	CAnimation animation;
	uint32 numAnimsLayer0 = pISkeletonAnim->GetNumAnimsInFIFO(0);
	if (numAnimsLayer0)
		animation = pISkeletonAnim->GetAnimFromFIFO(0, 0);

	CryCharAnimationParams AParams;

	if (m_State < 0)
		m_State = STATE_STAND;

	if (m_key_S && m_State == STATE_RUN && numAnimsLayer0 <= 1)
	{
		m_State = STATE_MOVE2IDLE;
	}

	float color1[4] = { 1, 1, 0, 1 };

	if (stance == 0) m_renderer->Draw2dLabel(12, ypos, 2.0f, color1, false, "Stance: %02d stand_tac_step_rifle_idle_3p_01", stance);
	if (stance == 1) m_renderer->Draw2dLabel(12, ypos, 2.0f, color1, false, "Stance: %02d _stand_tac_step_nova_3p_01", stance);
	if (stance == 2) m_renderer->Draw2dLabel(12, ypos, 2.0f, color1, false, "Stance: %02d _stand_tac_step_jaw_3p_01", stance);
	if (stance == 3) m_renderer->Draw2dLabel(12, ypos, 2.0f, color1, false, "Stance: %02d _stand_tac_step_hmg_3p_01", stance);
	if (stance == 4) m_renderer->Draw2dLabel(12, ypos, 2.0f, color1, false, "Stance: %02d _stand_tac_step_nw_3p_01", stance);
	if (stance == 5) m_renderer->Draw2dLabel(12, ypos, 2.0f, color1, false, "Stance: %02d _RELAXED_IDLESTEP_NW_01", stance);
	ypos += 35;

	static f32 fTurnAngle = 0;
	//	f32 fMaxStepDistance = caps.m_vMaxVelocity.GetLength()*caps.m_fFastDuration;
	//	f32 fDesiredDistance = caps.m_vMaxVelocity.GetLength()*caps.m_fFastDuration*g_StepScale;
	//	m_renderer->Draw2dLabel(12,ypos,2.0f,color1,false,"StepScale: %f   fMaxStepDistance: %f  fDesiredDistance: %f",g_StepScale,fMaxStepDistance,fDesiredDistance );
	//	ypos+=15;

	float color2[4] = { 0, 1, 0, 1 };
	ypos += 20;

	//standing idle
	if (m_State == STATE_STAND && numAnimsLayer0 <= 1)
	{
		AParams.m_fTransTime = 0.8312f;  //transition-time in seconds

		//blend into idle after walk-stop
		AParams.m_nFlags = CA_LOOP_ANIMATION | CA_START_AFTER;
		AParams.m_fTransTime = 0.15f;
		AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion

		if (stance == 0) strAnimName = "stand_tac_step_rifle_idle_3p_01";
		if (stance == 1) strAnimName = "stand_tac_step_nova_idle_3p_01";
		if (stance == 2) strAnimName = "stand_tac_step_rifle_idle_3p_01";
		if (stance == 3) strAnimName = "stand_tac_step_hmg_idle_3p_01";
		if (stance == 4) strAnimName = "stand_tac_step_nw_idle_3p_01";
		if (stance == 5) strAnimName = "relaxed_step_nw_idle_01";

		pISkeletonAnim->StartAnimation(strAnimName, AParams);

		g_MoveDirectionWorldIS.start = m_PhysicalLocation.t + Vec3(0, 0, 0.01f);
		g_MoveDirectionWorldIS.end = m_PhysicalLocation.t + Vec3(0, 0, 0.01f) + StrafeVectorWorldIS * 10.0f;

		LastIdlePos = m_PhysicalLocation.t;

		Vec3 blend;
		Vec3 forward = m_PhysicalLocation.GetColumn1();
		Vec3 desiredforward = StrafeVectorWorldIS;
		fTurnAngle = Ang3::CreateRadZ(forward, desiredforward);
		for (f32 t = 0.0f; t < 1.0f; t += 0.01f)
		{
			blend.SetSlerp(forward, desiredforward, t);
			pAuxGeom->DrawLine(wpos + Vec3(0, 0, 0.01f), RGBA8(0x00, 0x00, 0x00, 0x00), wpos + blend + Vec3(0, 0, 0.01f), RGBA8(0x7f, 0x7f, 0xff, 0x00));
		}

		m_State = STATE_STAND;
		m_Stance = 0;
	}

	//make a rotation step
	if (m_key_R && m_State == STATE_STAND && numAnimsLayer0 <= 1)
	{
		AParams.m_nFlags = CA_REPEAT_LAST_KEY | CA_FULL_ROOT_PRIORITY; //CA_LOOP_ANIMATION;
		AParams.m_fTransTime = 0.15f;                                  //start immediately
		AParams.m_fKeyTime = -1;                                       //use keytime of previous motion to start this motion
		AParams.m_fPlaybackSpeed = 1.0f;
		if (stance == 0)  strAnimName = "1D-BSpace_StepRot";
		if (stance == 1)  strAnimName = "_stand_tac_stepRot_nova_3p_01";
		if (stance == 2)  strAnimName = "_stand_tac_stepRot_jaw_3p_01";
		if (stance == 3)  strAnimName = "_stand_tac_stepRot_hmg_3p_01";
		if (stance == 4)  strAnimName = "_stand_tac_stepRot_nw_3p_01";
		if (stance == 5)  strAnimName = "_RELAXED_IDLESTEPROTATE_NW_01";

		pISkeletonAnim->StartAnimation(strAnimName, AParams);

		AParams.m_nFlags = CA_LOOP_ANIMATION | CA_START_AFTER;
		AParams.m_fTransTime = 0.20f;   //average time for transition
		AParams.m_fKeyTime = -1;        //use keytime of previous motion to start this motion
		AParams.m_fPlaybackSpeed = 1.0f;
		if (stance == 0) strAnimName = "stand_tac_step_rifle_idle_3p_01";
		if (stance == 1) strAnimName = "stand_tac_step_pistol_idle_3p_01";
		if (stance == 2) strAnimName = "stand_tac_step_rifle_idle_3p_01";
		if (stance == 3) strAnimName = "stand_tac_step_hmg_idle_3p_01";
		if (stance == 4) strAnimName = "stand_tac_step_nw_idle_3p_01";
		if (stance == 5) strAnimName = "relaxed_step_nw_idle_01";

		pISkeletonAnim->StartAnimation(strAnimName, AParams);

		g_MoveDirectionWorldIS.start = m_PhysicalLocation.t + Vec3(0, 0, 0.01f);
		g_MoveDirectionWorldIS.end = m_PhysicalLocation.t + Vec3(0, 0, 0.01f) + StrafeVectorWorldIS * 10.0f;

		m_State = STATE_STAND;
		m_Stance = 0;
	}

	//make a direction step
	if (m_key_W && m_State == STATE_STAND && numAnimsLayer0 <= 1)
	{
		AParams.m_nFlags = CA_REPEAT_LAST_KEY;  //CA_LOOP_ANIMATION;
		AParams.m_fTransTime = 0.20f;           //start immediately
		AParams.m_fKeyTime = -1;                //use keytime of previous motion to start this motion
		AParams.m_fPlaybackSpeed = 1.0f;

		if (stance == 0)  strAnimName = "_stand_tac_step_scar_3p_01";
		if (stance == 1)  strAnimName = "_stand_tac_step_nova_3p_01";
		if (stance == 2)  strAnimName = "_stand_tac_step_jaw_3p_01";
		if (stance == 3)  strAnimName = "_stand_tac_step_hmg_3p_01";
		if (stance == 4)  strAnimName = "_stand_tac_step_nw_3p_01";
		if (stance == 5)  strAnimName = "_RELAXED_IDLESTEP_NW_01";

		pISkeletonAnim->StartAnimation(strAnimName, AParams);

		AParams.m_nFlags = CA_LOOP_ANIMATION | CA_START_AFTER;
		AParams.m_fTransTime = 0.20f;   //average time for transition
		AParams.m_fKeyTime = -1;        //use keytime of previous motion to start this motion
		AParams.m_fPlaybackSpeed = 1.0f;

		if (stance == 0)  strAnimName = "stand_tac_step_rifle_idle_3p_01";
		if (stance == 1)  strAnimName = "stand_tac_step_pistol_idle_3p_01";
		if (stance == 2)  strAnimName = "stand_tac_step_rifle_idle_3p_01";
		if (stance == 3)  strAnimName = "stand_tac_step_hmg_idle_3p_01";
		if (stance == 4)  strAnimName = "stand_tac_step_nw_idle_3p_01";
		if (stance == 5)  strAnimName = "relaxed_step_nw_idle_01";

		pISkeletonAnim->StartAnimation(strAnimName, AParams);

		g_MoveDirectionWorldIS.start = m_PhysicalLocation.t + Vec3(0, 0, 0.01f);
		g_MoveDirectionWorldIS.end = m_PhysicalLocation.t + Vec3(0, 0, 0.01f) + StrafeVectorWorldIS * 10.0f;

		m_State = STATE_STAND;
		m_Stance = 0;
	}

	pAuxGeom->DrawLine(g_MoveDirectionWorldIS.start, RGBA8(0xff, 0xff, 0xff, 0x00), g_MoveDirectionWorldIS.end, RGBA8(0x00, 0xff, 0x00, 0x00));

	f32 fTravelAngle = -atan2f(LocalStrafeAnimIS.x, LocalStrafeAnimIS.y);

	//	m_renderer->Draw2dLabel(12,ypos,2.0f,color1,false,"fTurnAngle: %f   fTravelAngle: %f   fDesiredDistance: %f",fTurnAngle,fTravelAngle,fDesiredDistance);
	//	ypos+=40;

	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelAngle, fTravelAngle, fFrameTime);
	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelSpeed, 0, fFrameTime);
	//	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelDist,  fDesiredDistance, fFrameTime) ;
	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelDist, 0, fFrameTime);

	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TurnSpeed, 0, fFrameTime);
	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TurnAngle, fTurnAngle, fFrameTime);

	//pISkeleton->SetBlendSpaceOverride(eMotionParamID_TravelAngle, -atan2f(LocalStrafeAnimIS.x,LocalStrafeAnimIS.y), 1) ;
	//pISkeleton->SetBlendSpaceOverride(eMotionParamID_TravelDistScale, g_StepScale, 1) ;

	//	pISkeleton->SetBlendSpaceOverride(eMotionParamID_TravelDist, 2.0f, 1); //this will  overwrite the scale value

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
			uint32 AimIK = m_pCharPanel_Animation->GetAimIK();
			if (IAnimationPoseBlenderDir* pPoseBlenderAim = pISkeletonPose->GetIPoseBlenderAim())
			{
				pPoseBlenderAim->SetTarget(m_absCameraPos);
				pPoseBlenderAim->SetState(AimIK);
			}

			//m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"AimStatus: %d %d",AimIK, status );	ypos+=10;
			//m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"m_absCameraPos: %f %f %f",m_absCameraPos.x,m_absCameraPos.y,m_absCameraPos.z );	ypos+=10;
		}

	}

	return 0;

}

