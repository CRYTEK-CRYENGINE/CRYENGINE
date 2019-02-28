// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "CharacterEditor\ModelViewportCE.h"
#include "CharacterEditor\CharPanel_Animation.h"
#include <CryAnimation/ICryAnimation.h>

#include <Cry3DEngine/I3DEngine.h>
#include <CryPhysics/IPhysics.h>
#include <CrySystem/ITimer.h>
#include <CryInput/IInput.h>
#include <CryRenderer/IRenderAuxGeom.h>

extern uint32 g_ypos;

//------------------------------------------------------------------------------
//---              simple player-control test-application                    ---
//------------------------------------------------------------------------------
void CModelViewportCE::PlayerControl_UnitTest(ICharacterInstance* pInstance, const SRendParams& rRP, const SRenderingPassInfo& passInfo)
{
	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);
	f32 FrameTime = GetIEditor()->GetSystem()->GetITimer()->GetFrameTime();

	ISkeletonAnim* pISkeletonAnim = pInstance->GetISkeletonAnim();
	ISkeletonPose* pISkeletonPose = pInstance->GetISkeletonPose();

	AABB aabb;
	Vec3 wpos;
	wpos = Vec3(2, 2, 0);
	aabb = AABB(Vec3(-0.2f, -0.2f, -0.00f) + wpos, Vec3(+0.2f, +0.2f, +2.0f) + wpos);
	pAuxGeom->DrawAABB(aabb, Matrix34(IDENTITY), 1, RGBA8(0x00, 0xff, 0x1f, 0xff), eBBD_Extremes_Color_Encoded);
	wpos = Vec3(4, 2, 0);
	aabb = AABB(Vec3(-0.2f, -0.2f, -0.00f) + wpos, Vec3(+0.2f, +0.2f, +2.0f) + wpos);
	pAuxGeom->DrawAABB(aabb, Matrix34(IDENTITY), 1, RGBA8(0x00, 0x0f, 0x4f, 0xff), eBBD_Extremes_Color_Encoded);
	wpos = Vec3(5.5f, 2, 0);
	aabb = AABB(Vec3(-0.2f, -0.2f, -0.00f) + wpos, Vec3(+0.2f, +0.2f, +2.0f) + wpos);
	pAuxGeom->DrawAABB(aabb, Matrix34(IDENTITY), 1, RGBA8(0x00, 0x0f, 0x7f, 0xff), eBBD_Extremes_Color_Encoded);

	wpos = Vec3(0, 5, 0);
	aabb = AABB(Vec3(-0.2f, -0.2f, -0.00f) + wpos, Vec3(+0.2f, +0.2f, +2.0f) + wpos);
	pAuxGeom->DrawAABB(aabb, Matrix34(IDENTITY), 1, RGBA8(0x7f, 0x7f, 0x7f, 0xff), eBBD_Extremes_Color_Encoded);
	wpos = Vec3(0, 8, 0);
	aabb = AABB(Vec3(-0.2f, -0.2f, -0.00f) + wpos, Vec3(+0.2f, +0.2f, +2.0f) + wpos);
	pAuxGeom->DrawAABB(aabb, Matrix34(IDENTITY), 1, RGBA8(0x7f, 0x7f, 0x7f, 0xff), eBBD_Extremes_Color_Encoded);
	wpos = Vec3(0, 11, 0);
	aabb = AABB(Vec3(-0.2f, -0.2f, -0.00f) + wpos, Vec3(+0.2f, +0.2f, +2.0f) + wpos);
	pAuxGeom->DrawAABB(aabb, Matrix34(IDENTITY), 1, RGBA8(0x7f, 0x7f, 0x7f, 0xff), eBBD_Extremes_Color_Encoded);

	uint32 ypos = 2;
	float color1[4] = { 1, 1, 1, 1 };

	//set the required callbacks
	int PostProcessCallback_PlayControl(ICharacterInstance * pInstance, void* pPlayer);
	pISkeletonPose->SetPostProcessCallback(PostProcessCallback_PlayControl, this);
	pISkeletonAnim->SetEventCallback(0, this);

	PlayerControlIO();
	PlayerControlHuman(m_absLookDirectionXY, 0, 0);
	PlayerControlCamera();

	m_relCameraRotZ = 0;
	m_relCameraRotX = 0;

	//update the skeleton pose
	const CCamera& camera = GetCamera();
	float fDistance = (camera.GetPosition() - m_PhysicalLocation.t).GetLength();
	float fZoomFactor = 0.001f + 0.999f * (RAD2DEG(camera.GetFov()) / 60.f);

	SAnimationProcessParams params;
	params.locationAnimation = m_PhysicalLocation;
	params.bOnRender = 0;
	params.zoomAdjustedDistanceFromCamera = fDistance * fZoomFactor;
	ExternalPostProcessing(pInstance);
	pInstance->StartAnimationProcessing(params);
	pInstance->FinishAnimationComputations();

	//render character
	m_AnimatedCharacterMat = Matrix34(m_PhysicalLocation);
	SRendParams rp = rRP;
	rp.pMatrix = &m_AnimatedCharacterMat;
	rp.pPrevMatrix = &m_AnimatedCharacterMat;
	rp.fDistance = (m_PhysicalLocation.t - m_absCameraPos).GetLength();
	pInstance->Render(rp, QuatTS(IDENTITY), passInfo);

	//just debugging
	Vec3 absAxisX = m_PhysicalLocation.GetColumn0();
	Vec3 absAxisY = m_PhysicalLocation.GetColumn1();
	Vec3 absAxisZ = m_PhysicalLocation.GetColumn2();
	Vec3 absRootPos = m_PhysicalLocation.t + pISkeletonPose->GetAbsJointByID(0).t;
	pAuxGeom->DrawLine(absRootPos, RGBA8(0xff, 0x00, 0x00, 0x00), absRootPos + m_PhysicalLocation.GetColumn0() * 2.0f, RGBA8(0x00, 0x00, 0x00, 0x00));
	pAuxGeom->DrawLine(absRootPos, RGBA8(0x00, 0xff, 0x00, 0x00), absRootPos + m_PhysicalLocation.GetColumn1() * 2.0f, RGBA8(0x00, 0x00, 0x00, 0x00));
	pAuxGeom->DrawLine(absRootPos, RGBA8(0x00, 0x00, 0xff, 0x00), absRootPos + m_PhysicalLocation.GetColumn2() * 2.0f, RGBA8(0x00, 0x00, 0x00, 0x00));

	//---------------------------------------
	//---   draw the path of the past
	//---------------------------------------
	uint32 numEntries = m_arrAnimatedCharacterPath.size();
	for (int32 i = (numEntries - 2); i > -1; i--)
		m_arrAnimatedCharacterPath[i + 1] = m_arrAnimatedCharacterPath[i];
	m_arrAnimatedCharacterPath[0] = m_PhysicalLocation.t;
	for (uint32 i = 0; i < numEntries; i++)
	{
		aabb.min = Vec3(-0.01f, -0.01f, -0.01f) + m_arrAnimatedCharacterPath[i];
		aabb.max = Vec3(+0.01f, +0.01f, +0.01f) + m_arrAnimatedCharacterPath[i];
		pAuxGeom->DrawAABB(aabb, 1, RGBA8(0x00, 0x00, 0xff, 0x00), eBBD_Extremes_Color_Encoded);
	}

	if (mv_showGrid)
		DrawHeightField();

	GetIEditor()->GetSystem()->GetIConsole()->GetCVar("ca_DrawSkeleton")->Set(mv_showSkeleton);
}

//--------------------------------------------------------------------------

void CModelViewportCE::PlayerControlIO()
{

	IInput* pIInput = GetISystem()->GetIInput(); // Cache IInput pointer.
	pIInput->Update(true);

	//-------------------------------------------------------------------
	//-------            Keyboard and Game-Pad handling         ---------
	//-------------------------------------------------------------------
	m_key_SPACE = m_key_W = m_key_A = m_key_D = m_key_S = 0;
	if (CheckVirtualKey(VK_UP) || CheckVirtualKey('W')) m_key_W = 1;
	if (CheckVirtualKey(VK_DOWN) || CheckVirtualKey('S')) m_key_S = 1;
	if (CheckVirtualKey(VK_LEFT) || CheckVirtualKey('A')) m_key_A = 1;
	if (CheckVirtualKey(VK_RIGHT) || CheckVirtualKey('D')) m_key_D = 1;
	if (m_key_W) m_key_S = 0; //forward has priority
	if (m_key_A) m_key_D = 0; //left has priority

	m_keyrcr_W <<= 1;
	m_keyrcr_W |= m_key_W;
	m_keyrcr_S <<= 1;
	m_keyrcr_S |= m_key_S;
	m_keyrcr_A <<= 1;
	m_keyrcr_A |= m_key_A;
	m_keyrcr_D <<= 1;
	m_keyrcr_D |= m_key_D;

	if (CheckVirtualKey(VK_RETURN)) { m_key_SPACE = 1; }
	m_keyrcr_SPACE <<= 1;
	m_keyrcr_SPACE |= m_key_SPACE;

	if ((m_keyrcr_A & 3) == 2 || (m_keyrcr_D & 3) == 2)
		m_LTHUMB.x = 0.0f;
	if ((m_keyrcr_W & 3) == 2 || (m_keyrcr_S & 3) == 2)
		m_LTHUMB.y = 0.0f;

	m_LTHUMB.x += -f32(m_key_A);
	m_LTHUMB.x += f32(m_key_D);
	m_LTHUMB.y += f32(m_key_W);
	m_LTHUMB.y += -f32(m_key_S);
	m_LTHUMB.x = clamp_tpl(m_LTHUMB.x, -1.0f, 1.0f);
	m_LTHUMB.y = clamp_tpl(m_LTHUMB.y, -1.0f, 1.0f);

}

//---------------------------------------------------------------------------

void CModelViewportCE::PlayerControlHuman(const Vec2& vViewDir2D, f32 fGroundRadian, f32 fGroundRadianMoveDir)
{
	float color1[4] = { 1, 1, 1, 1 };
	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	f32 FrameTime = GetISystem()->GetITimer()->GetFrameTime();

	ISkeletonAnim* pISkeletonAnim = GetCharacterBase()->GetISkeletonAnim();
	ISkeletonPose* pISkeletonPose = GetCharacterBase()->GetISkeletonPose();

	pISkeletonAnim->SetAnimationDrivenMotion(1);
	pISkeletonAnim->SetLayerPlaybackScale(0, 1); //no scaling

	//------------------------------------------------------------------------------
	//------------------------------------------------------------------------------
	//------------------------------------------------------------------------------

	if (m_LTHUMB.GetLength() > 1.0f)
		m_LTHUMB.Normalize();

	uint32 num = sizeof(m_arrLTHUMB) / sizeof(Vec2);
	for (int32 i = (num - 2); i > -1; i--)
		m_arrLTHUMB[i + 1] = m_arrLTHUMB[i];
	m_arrLTHUMB[0] = m_LTHUMB;

	uint32 index = 0;  //min(uint32(0.10f/m_AverageFrameTime),num-1);

	f32 fDesiredSpeed = m_arrLTHUMB[index].GetLength() * 5.0f;
	if (fDesiredSpeed > 0.1f)
	{
		if (fDesiredSpeed > 3.0f)
			fDesiredSpeed = 5.0f;
		else
			fDesiredSpeed = 1.3f;
	}

	f32 fSlopeSlowDown = 1.0f - fabsf(fGroundRadian);
	m_renderer->Draw2dLabel(12, g_ypos, 1.2f, color1, false, "fSlopeSlowDown: %f", fSlopeSlowDown);
	g_ypos += 15;

	static f32 fDesiredSpeedSmooth = 0;
	static f32 fDesiredSpeedRate = 0;
	SmoothCD(fDesiredSpeedSmooth, fDesiredSpeedRate, m_AverageFrameTime, fDesiredSpeed, 0.001f);

	//	uint32 SnapTurn=0;
	f32 rad = Ang3::CreateRadZ(m_arrLTHUMB[index], m_arrLTHUMB[0]);
	/*if ( fabsf(rad)>(gf_PI*0.5f) && m_arrLTHUMB[0].GetLength()>0.85f && m_arrLTHUMB[index].GetLength()>0.80f )
	   {
	   //		SnapTurn=1;
	   //fDesiredSpeed=0.0f;
	   for (int32 i=1; i<num; i++)
	    m_arrLTHUMB[i]=m_arrLTHUMB[0];
	   Vec2 dir = m_arrLTHUMB[0].GetNormalized();
	   m_vWorldDesiredBodyDirection.x	=	dir.x*vViewDir2D.y+dir.y*vViewDir2D.x;
	   m_vWorldDesiredBodyDirection.y	=	dir.y*vViewDir2D.y-dir.x*vViewDir2D.x;
	   }*/

	//use the GamePad radian to set the m_vWorldDesiredBodyDirection
	m_renderer->Draw2dLabel(12, g_ypos, 1.2f, color1, false, "vViewDir2D: %f %f    RAD:%f ", vViewDir2D.x, vViewDir2D.y, fabsf(rad));
	g_ypos += 15;
	m_renderer->Draw2dLabel(12, g_ypos, 1.2f, color1, false, "index: %d", index);
	g_ypos += 15;

	if (fDesiredSpeed > 0.1f)
	{
		Vec2 dir = m_arrLTHUMB[index].GetNormalized();
		m_vWorldDesiredBodyDirection.x = dir.x * vViewDir2D.y + dir.y * vViewDir2D.x;
		m_vWorldDesiredBodyDirection.y = dir.y * vViewDir2D.y - dir.x * vViewDir2D.x;
	}
	m_vWorldDesiredMoveDirection = m_vWorldDesiredBodyDirection;  //in this case we always travel in the body direction.

	uint32 LookIK = m_pCharPanel_Animation->GetLookIK();
	IAnimationPoseBlenderDir* pIPoseBlenderLook = pISkeletonPose->GetIPoseBlenderLook();
	if (pIPoseBlenderLook)
	{
		pIPoseBlenderLook->SetState(LookIK);
		pIPoseBlenderLook->SetFadeoutAngle(DEG2RAD(120));
		pIPoseBlenderLook->SetTarget(m_PhysicalLocation.t + m_vWorldDesiredBodyDirection * 20);
	}

	uint32 AimIK = m_pCharPanel_Animation->GetAimIK();
	Vec3 vAimPos = Vec3(12.0f, +10.0f, 1.0f);
	IAnimationPoseBlenderDir* pIPoseBlenderAim = pISkeletonPose->GetIPoseBlenderAim();
	if (pIPoseBlenderAim)
	{
		pIPoseBlenderAim->SetState(AimIK);
		pIPoseBlenderAim->SetTarget(vAimPos);
		pIPoseBlenderAim->SetFadeOutSpeed(0.4f);
	}
	pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);

	static uint32 keyrcr_R = 0;
	keyrcr_R = (keyrcr_R << 1) | uint32(m_RT > 0.8f);
	if ((keyrcr_R & 3) == 1)
		pISkeletonPose->ApplyRecoilAnimation(0.2f, 0.10f, 1);

	static Ang3 angle = Ang3(ZERO);
	angle.x += 0.1f;
	angle.y += 0.01f;
	angle.z += 0.001f;
	AABB sAABB = AABB(Vec3(-0.1f, -0.1f, -0.1f), Vec3(+0.1f, +0.1f, +0.1f));
	OBB obb = OBB::CreateOBBfromAABB(Matrix33::CreateRotationXYZ(angle), sAABB);
	pAuxGeom->DrawOBB(obb, vAimPos, 1, RGBA8(0xff, 0xff, 0xff, 0xff), eBBD_Extremes_Color_Encoded);

	static f32 fGroundRadianSmooth = 0;
	static f32 fGroundRadianRate = 0;
	SmoothCD(fGroundRadianSmooth, fGroundRadianRate, m_AverageFrameTime, fGroundRadian, 0.10f);

	static f32 fGroundRadianMoveDirSmooth = 0;
	static f32 fGroundRadianMoveDirRate = 0;
	SmoothCD(fGroundRadianMoveDirSmooth, fGroundRadianMoveDirRate, m_AverageFrameTime, fGroundRadianMoveDir, 0.10f);

	{
		Vec2 vLTHUMB = m_arrLTHUMB[0] * 5.0f;
		if (vLTHUMB.GetLength() > 0.001f)
		{
			Vec2 dir = vLTHUMB.GetNormalized();
			m_vWorldDesiredBodyDirection2.x = dir.x * vViewDir2D.y + dir.y * vViewDir2D.x;
			m_vWorldDesiredBodyDirection2.y = dir.y * vViewDir2D.y - dir.x * vViewDir2D.x;
		}

		//just for debugging
		f32 fRadBody = -atan2f(m_vWorldDesiredBodyDirection2.x, m_vWorldDesiredBodyDirection2.y);
		Matrix33 MatBody33 = Matrix33::CreateRotationZ(fRadBody);
		const f32 size = 0.009f;
		const f32 length = 2.2f;
		AABB yaabb = AABB(Vec3(-size, 0.0f, -size), Vec3(size, length, size));
		OBB obb = OBB::CreateOBBfromAABB(MatBody33, yaabb);
		pAuxGeom->DrawOBB(obb, m_PhysicalLocation.t, 1, RGBA8(0x00, 0xff, 0xff, 0x00), eBBD_Extremes_Color_Encoded);
		pAuxGeom->DrawCone(m_PhysicalLocation.t + MatBody33.GetColumn1() * length, MatBody33.GetColumn1(), 0.03f, 0.15f, RGBA8(0x00, 0xff, 0xff, 0x00));
	}

}

//------------------------------------------------------------------------
//---  camera control
//------------------------------------------------------------------------
void CModelViewportCE::PlayerControlCamera()
{

	m_relCameraRotZ = m_RTHUMB.x * m_AverageFrameTime;
	m_relCameraRotX = m_RTHUMB.y * m_AverageFrameTime;

	//update and clamp rotation-speed
	f32 YawRad = -m_relCameraRotZ * 2; //yaw-speed
	m_absLookDirectionXY = Matrix33::CreateRotationZ(YawRad) * m_absLookDirectionXY;
	m_absLookDirectionXY.Normalize();

	m_absCameraHigh -= m_relCameraRotX;
	if (m_absCameraHigh > 3)
		m_absCameraHigh = 3;
	if (m_absCameraHigh < 0)
		m_absCameraHigh = 0;

	SmoothCD(m_LookAt, m_LookAtRate, m_AverageFrameTime, Vec3(m_PhysicalLocation.t.x, m_PhysicalLocation.t.y, 0.7f), 0.02f);
	//	Vec3 dir = (m_absLookDirectionXY+vWorldCurrentBodyDirection).GetNormalizedSafe(Vec3(0,1,0));
	//	m_absCameraPos		=	-dir*2 + m_LookAt+Vec3(0,0,m_absCameraHigh);
	m_absCameraPos = -m_absLookDirectionXY * 2 + m_LookAt + Vec3(0, 0, m_absCameraHigh);

	Matrix33 orientation = Matrix33::CreateRotationVDir((m_LookAt - m_absCameraPos).GetNormalized(), 0);
	if (m_pCharPanel_Animation->GetFixedCamera())
	{
		m_absCameraPos = Vec3(0, 3, 1);
		orientation = Matrix33::CreateRotationVDir((m_LookAt - m_absCameraPos).GetNormalized());
	}
	SetViewTM(Matrix34(orientation, m_absCameraPos));
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

int PostProcessCallback_PlayControl(ICharacterInstance* pInstance, void* pPlayer)
{
	//process bones specific stuff (IK, torso rotation, etc)
	((CModelViewportCE*)pPlayer)->PostProcessCallback_PlayControl(pInstance);
	return 1;
}
void CModelViewportCE::PostProcessCallback_PlayControl(ICharacterInstance* pInstance)
{
	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	ISkeletonAnim* pISkeletonAnim = pInstance->GetISkeletonAnim();
	//float color1[4] = {1,1,1,1};
	//m_renderer->Draw2dLabel(12,g_ypos,1.2f,color1,false,"PlayerControl PostprocessCallback" );
	//g_ypos+=10;
	UseFootIKNew(pInstance);
}

//-------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------
uint32 CModelViewportCE::UseFootIKNew(ICharacterInstance* pInstance)
{
	float color1[4] = { 1, 1, 1, 1 };
	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	ISkeletonAnim* pISkeletonAnim = pInstance->GetISkeletonAnim();
	ISkeletonPose* pISkeletonPose = pInstance->GetISkeletonPose();
	const IDefaultSkeleton& rIDefaultSkeleton = pInstance->GetIDefaultSkeleton();

	f32 fSmoothTime = 0.07f;

	static Ang3 angle = Ang3(ZERO);
	angle.x += 0.01f;
	angle.y += 0.001f;
	angle.z += 0.0001f;
	AABB sAABB = AABB(Vec3(-0.02f, -0.02f, -0.02f), Vec3(+0.02f, +0.02f, +0.02f));
	OBB obb = OBB::CreateOBBfromAABB(Matrix33::CreateRotationXYZ(angle), sAABB);

	Matrix33 m33;
	AABB wAABB;

	BBox bbox;
	m_arrBBoxes.resize(0);

	m33 = Matrix33::CreateRotationXYZ(Ang3(-0.5f, 0, 0));
	wAABB = AABB(Vec3(-10.00f, -10.00f, -0.10f), Vec3(+10.00f, +10.00f, +0.20f));
	bbox.obb = OBB::CreateOBBfromAABB(m33, wAABB);
	bbox.pos = Vec3(0, -5.0f, 0);
	bbox.col = RGBA8(0xff, 0x00, 0x00, 0xff);
	m_arrBBoxes.push_back(bbox);

	m33 = Matrix33::CreateRotationXYZ(Ang3(0, 0, 0));
	wAABB = AABB(Vec3(-20.00f, -20.00f, -0.50f), Vec3(+20.00f, +20.00f, -0.01f));
	bbox.obb = OBB::CreateOBBfromAABB(m33, wAABB);
	bbox.pos = Vec3(0, -5.0f, 0);
	bbox.col = RGBA8(0x1f, 0x1f, 0x3f, 0xff);
	m_arrBBoxes.push_back(bbox);

	m33 = Matrix33::CreateRotationXYZ(Ang3(0, 0, 0));
	wAABB = AABB(Vec3(-2.00f, -2.00f, -0.20f), Vec3(+2.00f, +2.00f, +0.20f));
	bbox.obb = OBB::CreateOBBfromAABB(m33, wAABB);
	bbox.pos = Vec3(5, 4, 0);
	bbox.col = RGBA8(0x3f, 0x1f, 0x1f, 0xff);
	m_arrBBoxes.push_back(bbox);

	m33 = Matrix33::CreateRotationXYZ(Ang3(0, 0, 0));
	wAABB = AABB(Vec3(-1.00f, -1.00f, -0.20f), Vec3(+1.00f, +1.00f, +0.30f));
	bbox.obb = OBB::CreateOBBfromAABB(m33, wAABB);
	bbox.pos = Vec3(5, 4, 0);
	bbox.col = RGBA8(0x3f, 0x1f, 0x1f, 0xff);
	m_arrBBoxes.push_back(bbox);

	m33 = Matrix33::CreateRotationXYZ(Ang3(0, 0, 0));
	wAABB = AABB(Vec3(-1.00f, -1.00f, -0.20f), Vec3(+1.00f, +1.00f, +0.20f));
	bbox.obb = OBB::CreateOBBfromAABB(m33, wAABB);
	bbox.pos = Vec3(-5, 7, 0.1f);
	bbox.col = RGBA8(0x3f, 0x1f, 0x1f, 0xff);
	m_arrBBoxes.push_back(bbox);

	Vec3 vCharWorldPos = m_PhysicalLocation.t;
	IVec CWP = CheckFootIntersection(vCharWorldPos, vCharWorldPos);

	f32 fMinHigh = m_PhysicalLocation.t.z;
	if (fMinHigh < CWP.heel.z)
		fMinHigh = CWP.heel.z;
	Vec3 vGroundNormalRoot = CWP.nheel * m_PhysicalLocation.q;

	f32 fGroundAngle = acos_tpl(vGroundNormalRoot.z);
	f32 fGroundHeight = fMinHigh - (fGroundAngle * 0.3f);
	Vec3 gnormal = Vec3(0, vGroundNormalRoot.y, vGroundNormalRoot.z);
	f32 cosine = Vec3(0, 0, 1) | gnormal;
	Vec3 sine = Vec3(0, 0, 1) % gnormal;
	f32 fGroundAngleMoveDir = RAD2DEG(atan2(sgn(sine.x) * sine.GetLength(), cosine));

	//	m_renderer->Draw2dLabel(12,g_ypos,1.5f,color1,false,"GroundAngle: rad:%f  degree:%f  fGroundAngleMoveDir:%f", fGroundAngle, RAD2DEG(fGroundAngle), fGroundAngleMoveDir );
	//	g_ypos+=14;

	//take always the lowest leg for the body position
	static f32 fGroundHeightSmooth = 0.0f;
	static f32 fGroundHeightRate = 0.0f;
	SmoothCD(fGroundHeightSmooth, fGroundHeightRate, m_AverageFrameTime, fGroundHeight, fSmoothTime);

	//	m_renderer->Draw2dLabel(12,g_ypos,1.5f,color1,false,"fGroundHeightSmooth: %f", fGroundHeightSmooth );
	//	g_ypos+=14;
	//	m_PhysicalLocation.t.z = fGroundHeightSmooth; //stay on ground

	int32 LHeelIdx = rIDefaultSkeleton.GetJointIDByName("Bip01 L Heel");
	if (LHeelIdx < 0) return 0;
	int32 LToe0Idx = rIDefaultSkeleton.GetJointIDByName("Bip01 L Toe0");
	if (LToe0Idx < 0) return 0;
	uint32 FootplantL = 0;
	Vec3 Final_LHeel = m_PhysicalLocation * pISkeletonPose->GetAbsJointByID(LHeelIdx).t;
	Vec3 Final_LToe0 = m_PhysicalLocation * pISkeletonPose->GetAbsJointByID(LToe0Idx).t;

	int32 RHeelIdx = rIDefaultSkeleton.GetJointIDByName("Bip01 R Heel");
	if (RHeelIdx < 0) return 0;
	int32 RToe0Idx = rIDefaultSkeleton.GetJointIDByName("Bip01 R Toe0");
	if (RToe0Idx < 0) return 0;
	uint32 FootplantR = 0;
	Vec3 Final_RHeel = m_PhysicalLocation * pISkeletonPose->GetAbsJointByID(RHeelIdx).t;
	Vec3 Final_RToe0 = m_PhysicalLocation * pISkeletonPose->GetAbsJointByID(RToe0Idx).t;

	//-------------------------------------------------------------------------

	IVec L4 = CheckFootIntersection(Final_LHeel, Final_LToe0);
	pAuxGeom->DrawOBB(obb, L4.toe, 1, RGBA8(0xff, 0xff, 0xff, 0xff), eBBD_Extremes_Color_Encoded);
	pAuxGeom->DrawOBB(obb, L4.heel, 1, RGBA8(0x1f, 0xff, 0xff, 0xff), eBBD_Extremes_Color_Encoded);

	IVec R4 = CheckFootIntersection(Final_RHeel, Final_RToe0);
	pAuxGeom->DrawOBB(obb, R4.toe, 1, RGBA8(0xff, 0xff, 0xff, 0xff), eBBD_Extremes_Color_Encoded);
	pAuxGeom->DrawOBB(obb, R4.heel, 1, RGBA8(0x1f, 0xff, 0xff, 0xff), eBBD_Extremes_Color_Encoded);

	static Plane LSmoothGroundPlane(Vec3(0, 0, 1), 0);
	static Plane LSmoothGroundPlaneRate(Vec3(0, 0, 1), 0);
	{
		Plane LGroundPlane(Vec3(0, 0, 1), 0);
		if ((L4.heel.z - Final_LHeel.z) > -0.03f)
			LGroundPlane = Plane::CreatePlane(L4.nheel * m_PhysicalLocation.q, m_PhysicalLocation.GetInverted() * L4.heel);
		//m_renderer->Draw2dLabel(12,g_ypos,1.5f,color1,false,"LGroundPlane: %f %f %f  d:%f", LGroundPlane.n.x,LGroundPlane.n.y,LGroundPlane.n.z,LGroundPlane.d );
		//g_ypos+=14;

		SmoothCD(LSmoothGroundPlane, LSmoothGroundPlaneRate, m_AverageFrameTime, LGroundPlane, fSmoothTime);
		//	m_renderer->Draw2dLabel(12,g_ypos,1.5f,color1,false,"LSmoothGroundPlane: %f %f %f  d:%f", LSmoothGroundPlane.n.x,LSmoothGroundPlane.n.y,LSmoothGroundPlane.n.z,LSmoothGroundPlane.d );
		//	g_ypos+=14;
	}

	static Plane RSmoothGroundPlane(Vec3(0, 0, 1), 0);
	static Plane RSmoothGroundPlaneRate(Vec3(0, 0, 1), 0);
	{
		Plane RGroundPlane(Vec3(0, 0, 1), 0);
		if ((R4.heel.z - Final_RToe0.z) > -0.03f)
			RGroundPlane = Plane::CreatePlane(R4.nheel * m_PhysicalLocation.q, m_PhysicalLocation.GetInverted() * R4.heel);
		//m_renderer->Draw2dLabel(12,g_ypos,1.5f,color1,false,"RGroundPlane: %f %f %f  d:%f", RGroundPlane.n.x,RGroundPlane.n.y,RGroundPlane.n.z,RGroundPlane.d );
		//g_ypos+=14;

		SmoothCD(RSmoothGroundPlane, RSmoothGroundPlaneRate, m_AverageFrameTime, RGroundPlane, fSmoothTime);
		//	m_renderer->Draw2dLabel(12,g_ypos,1.5f,color1,false,"RSmoothGroundPlane: %f %f %f  d:%f", RSmoothGroundPlane.n.x,RSmoothGroundPlane.n.y,RSmoothGroundPlane.n.z,RSmoothGroundPlane.d );
		//	g_ypos+=14;
	}

	return 1;
}

IVec CModelViewportCE::CheckFootIntersection(const Vec3& Final_Heel, const Vec3& Final_Toe0)
{
	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();

	Vec3 out;
	uint32 ival = 0;
	f32 HeightHeel = -100.0f;
	f32 HeightToe0 = -100.0f;
	Vec3 normalHeel = Vec3(0, 0, 1);
	Vec3 normalToe0 = Vec3(0, 0, 1);

	Ray RayHeel = Ray(Final_Heel + Vec3(0.0f, 0.0f, 5.5f), Vec3(0, 0, -1.0f));
	Ray RayToe0 = Ray(Final_Toe0 + Vec3(0.0f, 0.0f, 5.5f), Vec3(0, 0, -1.0f));
	//pAuxGeom->DrawLine(RayToeN.origin,RGBA8(0xff,0xff,0xff,0x00),RayToeN.origin+RayToeN.direction*10.0f,RGBA8(0x00,0x0,0xff,0x00));
	//pAuxGeom->DrawLine(RayHeel.origin,RGBA8(0xff,0xff,0xff,0x00),RayHeel.origin+RayHeel.direction*10.0f,RGBA8(0xff,0x0,0x00,0x00));

	uint32 numBoxes = m_arrBBoxes.size();

	for (uint32 i = 0; i < numBoxes; i++)
	{
		OBB obb = m_arrBBoxes[i].obb;
		Vec3 pos = m_arrBBoxes[i].pos;
		ColorB col = m_arrBBoxes[i].col;
		pAuxGeom->DrawOBB(obb, pos, 1, col, eBBD_Extremes_Color_Encoded);

		ival = Intersect::Ray_OBB(RayHeel, pos, obb, out);
		if (ival == 0x01)
			if (HeightHeel < out.z)
			{
				HeightHeel = out.z;
				normalHeel = obb.m33.GetColumn2(); // * m_PhysicalLocation.q;
			}
		;

		ival = Intersect::Ray_OBB(RayToe0, pos, obb, out);
		if (ival == 0x01)
			if (HeightToe0 < out.z)
			{
				HeightToe0 = out.z;
				normalToe0 = obb.m33.GetColumn2(); // * m_PhysicalLocation.q;
			}
		;
	}

	IVec retval;
	retval.nheel = normalHeel.GetNormalized();
	retval.ntoe = normalToe0.GetNormalized();
	retval.heel = Vec3(Final_Heel.x, Final_Heel.y, HeightHeel);
	retval.toe = Vec3(Final_Toe0.x, Final_Toe0.y, HeightToe0);
	return retval;
}

