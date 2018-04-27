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

//------------------------------------------------------------------------------
//---              simple path-following test-application                    ---
//------------------------------------------------------------------------------

uint32 rcr_key_Z = 0, flip_Z = 2;
uint32 rcr_key_H = 0, flip_H = 2;
uint32 rcr_key_J = 0, flip_J = 1;
uint32 rcr_key_T = 0, flip_T = 0;

void CModelViewportCE::PathFollowing_UnitTest(ICharacterInstance* pInstance, const SRendParams& rRP, const SRenderingPassInfo& passInfo)
{
}

#define STATE_STAND           (0)
#define STATE_WALK            (1)
#define STATE_WALKSTRAFELEFT  (2)
#define STATE_WALKSTRAFERIGHT (3)
#define STATE_RUN             (4)

#define RELAXED               (0)
#define COMBAT                (1)
#define STEALTH               (2)
#define CROUCH                (3)
#define PRONE                 (4)

extern int32 g_ShiftTarget;
extern int32 g_ShiftSource;
extern char* g_NewName;

const uint32 LIMIT = 8 * 5;
Vec3 g_Path[LIMIT];

SPlayerControlMotionParams CModelViewportCE::EntityFollowing(uint32& ypos)
{
	SPlayerControlMotionParams mp;
	mp.m_fCatchUpSpeed = 0.0f;
	return mp;
}

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
f32 CModelViewportCE::PathCreation(f32 fDesiredSpeed)
{
	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);

	AABB aabb;
	f32 div = 0.60f;
	f32 radiants[LIMIT];

	g_Path[0] = Vec3(ZERO);   //these are the points we calculate in real-time based on the travel-speed and path-corner
	g_Path[1] = Vec3(ZERO);
	g_Path[2] = Vec3(15 * div, 15 * div, 0.003f);
	g_Path[3] = Vec3(ZERO);   //these are the points we calculate in real-time based on the travel-speed and path-corner
	g_Path[4] = Vec3(ZERO);

	g_Path[5] = Vec3(ZERO);
	g_Path[6] = Vec3(ZERO);
	g_Path[7] = Vec3(0 * div, 10 * div, 0.003f);
	g_Path[8] = Vec3(ZERO);
	g_Path[9] = Vec3(ZERO);

	g_Path[10] = Vec3(ZERO);
	g_Path[11] = Vec3(ZERO);
	g_Path[12] = Vec3(-25 * div, 25 * div, 0.003f); //peak
	g_Path[13] = Vec3(ZERO);
	g_Path[14] = Vec3(ZERO);

	g_Path[15] = Vec3(ZERO);
	g_Path[16] = Vec3(ZERO);
	g_Path[17] = Vec3(-5 * div, 0 * div, 0.003f);
	g_Path[18] = Vec3(ZERO);
	g_Path[19] = Vec3(ZERO);

	g_Path[20] = Vec3(ZERO);
	g_Path[21] = Vec3(ZERO);
	g_Path[22] = Vec3(-15 * div, -15 * div, 0.003f);
	g_Path[23] = Vec3(ZERO);
	g_Path[24] = Vec3(ZERO);

	g_Path[25] = Vec3(ZERO);
	g_Path[26] = Vec3(ZERO);
	g_Path[27] = Vec3(0 * div, -20 * div, 0.003f);
	g_Path[28] = Vec3(ZERO);
	g_Path[29] = Vec3(ZERO);

	g_Path[30] = Vec3(ZERO);
	g_Path[31] = Vec3(ZERO);
	g_Path[32] = Vec3(15 * div, -15 * div, 0.003f);
	g_Path[33] = Vec3(ZERO);
	g_Path[34] = Vec3(ZERO);

	g_Path[35] = Vec3(ZERO);
	g_Path[36] = Vec3(ZERO);
	g_Path[37] = Vec3(19 * div, 0 * div, 0.003f);
	g_Path[38] = Vec3(ZERO);
	g_Path[39] = Vec3(ZERO);

	aabb = AABB(Vec3(-0.05f, -0.05f, 0.0f) + g_Path[2], Vec3(+0.05f, +0.05f, +0.5f) + g_Path[2]);
	//	pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(0xff,0x00,0x00,0xff), eBBD_Extremes_Color_Encoded);

	//calculate the empty path-points
	for (uint32 i = 0; i < LIMIT; i = i + 5)
	{
		uint32 i2 = i + 2;
		if (i2 >= LIMIT) i2 -= LIMIT;            //this is the first point on line-segment
		uint32 i3 = i + 3;
		if (i3 >= LIMIT) i3 -= LIMIT;            //dynamic point-1
		uint32 i4 = i + 4;
		if (i4 >= LIMIT) i4 -= LIMIT;            //dynamic point-2

		uint32 i5 = i + 5;
		if (i5 >= LIMIT) i5 -= LIMIT;            //dynamic point-3
		uint32 i6 = i + 6;
		if (i6 >= LIMIT) i6 -= LIMIT;            //dynamic point-4
		uint32 i7 = i + 7;
		if (i7 >= LIMIT) i7 -= LIMIT;            //this is the second point on line-segment
		pAuxGeom->DrawLine(g_Path[i2], RGBA8(0x00, 0xff, 0x00, 0x00), g_Path[i7], RGBA8(0xff, 0xff, 0xff, 0xff));

		Vec3 vDistance = g_Path[i7] - g_Path[i2];
		f32 fLength = vDistance.GetLength();
		f32 fHLength = fLength * 0.4f;
		Vec3 n = vDistance / fLength; //DANGEROUS: can be a DivByZero

		f32 ds = fDesiredSpeed;
		Vec3 vSwing = n * (ds * 0.60f);
		f32 fLenSwing = vSwing.GetLength();
		if (fHLength < fLenSwing)
			vSwing = vSwing.GetNormalized() * fHLength;

		g_Path[i3] = g_Path[i2] + vSwing * 0.5f; //swing-out point
		g_Path[i4] = g_Path[i2] + vSwing;
		g_Path[i5] = g_Path[i7] - vSwing;
		g_Path[i6] = g_Path[i7] - vSwing * 0.5f; //swing-out point
	}

	//calculate the radiants between line-segments
	for (int32 i = 0; i < LIMIT; i++)
	{
		int32 p0 = i - 1;
		if (p0 < 0) p0 += LIMIT;
		int32 p1 = i;
		int32 p2 = i + 1;
		if (p2 >= LIMIT) p2 -= LIMIT;
		Vec3 v0 = g_Path[p1] - g_Path[p0];
		Vec3 v1 = g_Path[p2] - g_Path[p1];
		radiants[i] = Ang3::CreateRadZ(v0, v1) / gf_PI; //range [-1 ... +1]
	}

	//swing-out when running along curves
	for (uint32 i = 0; i < LIMIT; i = i + 5)
	{
		uint32 i2 = i + 2;
		if (i2 >= LIMIT) i2 -= LIMIT;             //this is the first point on line-segment
		uint32 i3 = i + 3;
		if (i3 >= LIMIT) i3 -= LIMIT;
		uint32 i4 = i + 4;
		if (i4 >= LIMIT) i4 -= LIMIT;

		uint32 i5 = i + 5;
		if (i5 >= LIMIT) i5 -= LIMIT;
		uint32 i6 = i + 6;
		if (i6 >= LIMIT) i6 -= LIMIT;
		uint32 i7 = i + 7;
		if (i7 >= LIMIT) i7 -= LIMIT;            //this is the second point on line-segment
		pAuxGeom->DrawLine(g_Path[i2], RGBA8(0x00, 0xff, 0x00, 0x00), g_Path[i7], RGBA8(0xff, 0xff, 0xff, 0xff));

		//before the corner
		Vec3 linedir1 = (g_Path[i7] - g_Path[i5]) * 0.35f;
		Vec3 DispDirection1 = Vec3(linedir1.y, -linedir1.x, 0);                          //clock-wise 90-degree rotation
		g_Path[i6] = g_Path[i6] + (DispDirection1 * radiants[i7] * (fDesiredSpeed / 3)); //another nice banana-formula to control the path-deviation

		//after the corner
		Vec3 linedir0 = (g_Path[i4] - g_Path[i2]) * 0.25f;
		Vec3 DispDirection0 = Vec3(linedir0.y, -linedir0.x, 0);                          //clock-wise 90-degree rotation
		g_Path[i3] = g_Path[i3] + (DispDirection0 * radiants[i2] * (fDesiredSpeed / 3)); //another nice banana-formula to control the path-deviation

	}

	//visualisation
	/*	for (uint32 i=0; i<LIMIT; i=i+5)
	   {

	   uint32 i0=i+0; if (i0>=LIMIT) i0-=LIMIT;
	   uint32 i1=i+1; if (i1>=LIMIT) i1-=LIMIT;
	   uint32 i2=i+2; if (i2>=LIMIT) i2-=LIMIT;//this is the first point on line-segment
	   uint32 i3=i+3; if (i3>=LIMIT) i3-=LIMIT;
	   uint32 i4=i+4; if (i4>=LIMIT) i4-=LIMIT;


	   aabb = AABB(Vec3(-0.01f,-0.01f, 0.0f)+g_Path[i0],Vec3(+0.01f,+0.01f,+0.3f)+g_Path[i0]);
	   pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(0xff,0x00,0x00,0xff), eBBD_Extremes_Color_Encoded);
	   aabb = AABB(Vec3(-0.01f,-0.01f, 0.0f)+g_Path[i1],Vec3(+0.01f,+0.01f,+0.3f)+g_Path[i1]);
	   pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(0xff,0x00,0x00,0xff), eBBD_Extremes_Color_Encoded);

	   uint32 t = uint32(fabsf(radiants[i2])*0xff);
	   if (t>0xff)	t=0xff;
	   uint8 c=0xff-t;
	   aabb = AABB(Vec3(-0.05f,-0.05f, 0.0f)+g_Path[i2],Vec3(+0.05f,+0.05f,+0.5f)+g_Path[i2]);
	   pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(c,c,c,0xff), eBBD_Extremes_Color_Encoded);

	   aabb = AABB(Vec3(-0.01f,-0.01f, 0.0f)+g_Path[i3],Vec3(+0.01f,+0.01f,+0.3f)+g_Path[i3]);
	   pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(0xff,0x00,0x00,0xff), eBBD_Extremes_Color_Encoded);
	   aabb = AABB(Vec3(-0.01f,-0.01f, 0.0f)+g_Path[i4],Vec3(+0.01f,+0.01f,+0.3f)+g_Path[i4]);
	   pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(0xff,0x00,0x00,0xff), eBBD_Extremes_Color_Encoded);

	   }*/

	return 0;
}

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
void CModelViewportCE::SetLocomotionValuesIvo(const SPlayerControlMotionParams& mp)
{
	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	Vec3 absEntityPos = m_PhysicalLocation.t;
	f32 FrameTime = GetISystem()->GetITimer()->GetFrameTime();
	f32 TimeScale = GetISystem()->GetITimer()->GetTimeScale();

	f32 FramesPerSecond = TimeScale / m_AverageFrameTime;

	static f32 LastTurnRadianSec4 = 0;
	static f32 LastTurnRadianSec3 = 0;
	static f32 LastTurnRadianSec2 = 0;
	static f32 LastTurnRadianSec1 = 0;
	static f32 LastTurnRadianSec0 = 0;

	LastTurnRadianSec4 = LastTurnRadianSec3;
	LastTurnRadianSec3 = LastTurnRadianSec2;
	LastTurnRadianSec2 = LastTurnRadianSec1;
	LastTurnRadianSec1 = LastTurnRadianSec0;

	Vec2 vCurrentBodyDir = Vec2(m_PhysicalLocation.GetColumn1());
	LastTurnRadianSec0 = Ang3::CreateRadZ(vCurrentBodyDir, m_vWorldDesiredBodyDirectionSmooth) * FramesPerSecond;
	//to avoid wild&crazy direction changes, we scale-down the body-turn based on the framerate
	LastTurnRadianSec0 /= (FramesPerSecond / 12.0f);
	//a very cheesy way to do the smoothing because of all the stalls we have
	f32 TurnRadianSecAvrg = (LastTurnRadianSec0 + LastTurnRadianSec1 + LastTurnRadianSec2 + LastTurnRadianSec3 + LastTurnRadianSec4) * 0.20f;

	TurnRadianSecAvrg = LastTurnRadianSec0;

	ISkeletonAnim* pISkeletonAnim = GetCharacterBase()->GetISkeletonAnim();

	//disable the overrides
	f32 rad = -atan2f(m_vLocalDesiredMoveDirectionSmooth.x, m_vLocalDesiredMoveDirectionSmooth.y);
	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelAngle, rad, FrameTime);
	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TurnSpeed, TurnRadianSecAvrg, FrameTime);
	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelSpeed, mp.m_fCatchUpSpeed, FrameTime);

	float color1[4] = { 1, 1, 1, 1 };
	m_renderer->Draw2dLabel(12, g_ypos, 1.8f, color1, false, "TurnRadianSecAvrg: %f", TurnRadianSecAvrg);
	g_ypos += 20;
	//m_renderer->Draw2dLabel(12,g_ypos,1.8f,color1,false,"localtraveldir: %f %f",localtraveldir.x,localtraveldir.y);
	//g_ypos+=20;

}

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
void CModelViewportCE::PathFollowing(const SPlayerControlMotionParams& mp, f32 fDesiredSpeed)
{
	uint32 ypos = 512;
	float color1[4] = { 1, 1, 1, 1 };
	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	Vec3 absEntityPos = m_PhysicalLocation.t;

	ISkeletonAnim* pISkeletonAnim = GetCharacterBase()->GetISkeletonAnim();
	ISkeletonPose* pISkeletonPose = GetCharacterBase()->GetISkeletonPose();
	const IDefaultSkeleton& rIDefaultSkeleton = GetCharacterBase()->GetIDefaultSkeleton();

	f32 FrameTime = GetISystem()->GetITimer()->GetFrameTime();
	f32 TimeScale = GetISystem()->GetITimer()->GetTimeScale();
	f32 FramesPerSecond = TimeScale / m_AverageFrameTime;

	CryCharAnimationParams AParams;

	SetLocomotionValuesIvo(mp);

	/*
	   m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"TurnRadianSec: %f",TurnRadianSec);
	   ypos+=10;
	   m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"m_FrameTime: %f   FramesPerSecond: %f",FrameTime,FramesPerSecond );
	   ypos+=10;
	 */

	//------------------------------------------------------------------------
	//---  camera control
	//------------------------------------------------------------------------
	uint32 AttachedCamera = m_pCharPanel_Animation->GetAttachedCamera();
	if (AttachedCamera)
	{

		m_absCameraHigh -= m_relCameraRotX * 0.002f;
		if (m_absCameraHigh > 3)
			m_absCameraHigh = 3;
		if (m_absCameraHigh < 0)
			m_absCameraHigh = 0;

		SmoothCD(m_LookAt, m_LookAtRate, FrameTime, Vec3(absEntityPos.x, absEntityPos.y, 0.7f), 0.05f);

		//	Vec2 vWorldCurrentBodyDirection = Vec2(m_PhysicalLocation.GetColumn1());
		Vec2 vWorldCurrentBodyDirection = Vec2(0, 1);

		Vec3 absCameraPos = vWorldCurrentBodyDirection * 6 + m_LookAt + Vec3(0, 0, m_absCameraHigh);
		//absCameraPos.x=absEntityPos.x;

		SmoothCD(m_vCamPos, m_vCamPosRate, FrameTime, absCameraPos, 0.25f);
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

			//	m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"m_absCameraPos: %f %f %f",m_absCameraPos.x,m_absCameraPos.y,m_absCameraPos.z );	ypos+=10;
		}

	}

}

