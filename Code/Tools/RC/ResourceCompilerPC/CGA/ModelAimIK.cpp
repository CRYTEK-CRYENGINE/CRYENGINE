// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//	Crytek Character Animation source code
//	
//	
//  Contains:
//	This file contains just prototype-code for testing new features
//	As soon as this stuff is mature enough we will move it into production-tools (in this case the resource-compiler)
//	
//	
//
/////////////////////////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <algorithm>

//#include "CharacterManager.h"
//#include "Model.h"
//#include "ModelAnimationSet.h"
//#include "ModelMesh.h"
#include "AnimationLoader.h"
#include "AnimationManager.h"
#include "StringHelpers.h"


#define PRECISION (0.001)

//////////////////////////////////////////////////////////////////////////
// Implementation of ICryAnimationSet, holding the information about animations
// and bones for a single model. Animations also include the subclass of morph targets
//////////////////////////////////////////////////////////////////////////


struct VExampleInit
{
	struct VirtualExampleInit1
	{
		f32 m_fSmalest;
		uint8 i0,i1,i2,i3;
		f64  w0,w1,w2,w3;
	};

	f64 m_fSmallest;
	uint32 m_nIterations;
	Vec4d m_Weight;

	int32  m_arrParentIdx[MAX_JOINT_AMOUNT];
	QuatT  m_arrDefaultRelPose[MAX_JOINT_AMOUNT];
	uint32 m_arrJointCRC32[MAX_JOINT_AMOUNT];

	QuatTd m_arrRelPose[MAX_JOINT_AMOUNT];
	QuatTd m_arrAbsPose[MAX_JOINT_AMOUNT];
	QuatTd m_arrRelPose0[MAX_JOINT_AMOUNT];
	QuatTd m_arrAbsPose0[MAX_JOINT_AMOUNT];
	QuatTd m_arrRelPose1[MAX_JOINT_AMOUNT];
	QuatTd m_arrAbsPose1[MAX_JOINT_AMOUNT];
	QuatTd m_arrRelPose2[MAX_JOINT_AMOUNT];
	QuatTd m_arrAbsPose2[MAX_JOINT_AMOUNT];
	QuatTd m_arrRelPose3[MAX_JOINT_AMOUNT];
	QuatTd m_arrAbsPose3[MAX_JOINT_AMOUNT];
	VirtualExampleInit1 PolarGrid[CHUNK_GAHAIM_INFO::XGRID*CHUNK_GAHAIM_INFO::YGRID];


	void Init(const CSkinningInfo* pModelSkeleton,DynArray<SJointsAimIK_Rot>& rRot,DynArray<SJointsAimIK_Pos>& rPos,GlobalAnimationHeaderAIM& rAIM,int nWeaponBoneIdx,uint32 numAimPoses);
	void CopyPoses2(DynArray<SJointsAimIK_Rot>& rRot,DynArray<SJointsAimIK_Pos>& rPos, GlobalAnimationHeaderAIM& rAIM, uint32 numPoses, uint32 skey );
	void RecursiveTest( const Vec2d& ControlPoint, GlobalAnimationHeaderAIM& rAIM,DynArray<SJointsAimIK_Rot>& rRot,DynArray<SJointsAimIK_Pos>& rPos,int nWBone, int i0,int i1,int i2,int i3, const Vec4d& w0,const Vec4d& w1,const Vec4d& w2,const Vec4d& w3 );
	uint32 PointInQuat( const Vec2d& ControlPoint, GlobalAnimationHeaderAIM& rAIM,DynArray<SJointsAimIK_Rot>& rRot,DynArray<SJointsAimIK_Pos>& rPos,int nWBone, int i0,int i1,int i2,int i3, const Vec4d& w0,const Vec4d& w1,const Vec4d& w2,const Vec4d& w3 );

	uint32 LinesegOverlap2D(const Planer& plane0, const Vec2d& ls0,const Vec2d& le0,   const Vec2d& tp0,const Vec2d& tp1 );

	void ComputeAimPose( GlobalAnimationHeaderAIM& rAIM, DynArray<SJointsAimIK_Rot>& rRot,DynArray<SJointsAimIK_Pos>& rPos, QuatTd* arrAbsPose, uint32 nAimPose );
	void Blend4AimPose( GlobalAnimationHeaderAIM& rAIM, DynArray<SJointsAimIK_Rot>& rRot,DynArray<SJointsAimIK_Pos>& rPos, int8 i0,int8 i1,int8 i2,int8 i3, const Vec4d& w, QuatTd* arrRelPose,QuatTd* arrAbsPose );
	void NLerp2AimPose( DynArray<SJointsAimIK_Rot>& rRot,DynArray<SJointsAimIK_Pos>& rPos, QuatTd* arrRelPose0,QuatTd* arrRelPose1,f64 t, QuatTd* arrAbsPose );
	uint32 AnnotateExamples(uint32 numPoses, QuadIndices* arrQuat);
	Vec3d PolarCoordinate(const Quatd& q);


};





bool CAnimationCompressor::ProcessAimPoses(bool isLook, GlobalAnimationHeaderAIM& rAIM) 
{
	uint32 nAnimTokenCRC32 = 0;
	int32 nDirectionIdx = -1;

	CSkinningInfo& info = m_skeleton.m_SkinningInfo;
	DynArray<DirectionalBlends>& rDirBlends = isLook ? info.m_LookDirBlends : info.m_AimDirBlends;
	DynArray<SJointsAimIK_Rot>& rRot = isLook ? info.m_LookIK_Rot : info.m_AimIK_Rot;
	DynArray<SJointsAimIK_Pos>& rPos = isLook ? info.m_LookIK_Pos : info.m_AimIK_Pos;

	const char* strDirectionJointName = "";
	uint32 numAimDB = rDirBlends.size();
	for (uint32 d=0; d<numAimDB; d++)
	{
		const bool isAimIK = StringHelpers::ContainsIgnoreCase(rAIM.m_FilePath, rDirBlends[d].m_AnimToken);
		if (isAimIK)
		{
			nAnimTokenCRC32 = rDirBlends[d].m_AnimTokenCRC32;
			nDirectionIdx   = rDirBlends[d].m_nParaJointIdx;
			strDirectionJointName = rDirBlends[d].m_strParaJointName ? rDirBlends[d].m_strParaJointName : "";
			break;
		}
	}


	rAIM.m_AnimTokenCRC32=0xDeadBeef;  //not initialized
	if (rAIM.IsAssetCreated()==0)
		return false;
	if (rAIM.IsAssetLoaded()==0)
		return false;

	//if all joints in the aim-pose are valid 
	uint32 numRot = rRot.size();
	for (uint32 i=0; i<numRot; i++)
	{
		int32 idx = rRot[i].m_nJointIdx;
		if (idx<0)
		{
			RCLogError("CryAnimation: Aim/Look-pose '%s' disabled. Model doesn't have Jointname '%s'",rAIM.m_FilePath.c_str(),rRot[i].m_strJointName );
			return false;
		}
	}

	uint32 numPos = rPos.size();
	for (uint32 i=0; i<numPos; i++)
	{
		int32 idx = rPos[i].m_nJointIdx;
		if (idx<0) 
		{
			RCLogError("CryAnimation: Aim/Look-pose '%s' disabled. Model doesn't have Jointname '%s'",rAIM.m_FilePath.c_str(),rRot[i].m_strJointName );
			return false;
		}
	}

	//----------------------------------------------------------------------------------------------------------------------------------------

	uint32 numPoses = uint32(rAIM.m_fTotalDuration/SECONDS_PER_TICK+1.1f);
	assert(numPoses==9 || numPoses==15 || numPoses==21);
	if (numPoses!=9 && numPoses!=15 && numPoses!=21)
	{
		RCLogError("CryAnimation: Aim/Look-pose '%s' disabled. Invalid number of poses: %i (expected 9, 15 or 21)",rAIM.m_FilePath.c_str(),int(numPoses));
		return false;
	}

	uint32 found=0;
	uint32 numRotJoints = rRot.size();
	for (uint32 r=0; r<numRotJoints; r++)
	{
		int32 j	= rRot[r].m_nJointIdx;
		if (j != nDirectionIdx)
			continue;
		found=1;
		if (rRot[r].m_nPreEvaluate==0)
		{
			RCLogError("CryAnimation: Aim/Look-pose '%s' disabled. ParameterJoint '%s' not marked as primary",rAIM.m_FilePath.c_str(),strDirectionJointName);
			return false;
		}
	}

	if (found==0)
	{
		RCLogError("CryAnimation: Aim/Look-pose '%s' disabled. ParameterJoint '%s' not defined in rotation list",rAIM.m_FilePath.c_str(),strDirectionJointName);
		return false;
	}

	rAIM.m_AnimTokenCRC32 = nAnimTokenCRC32;

	VExampleInit vei;
	vei.Init(&m_skeleton.m_SkinningInfo, rRot, rPos, rAIM, nDirectionIdx, numPoses);

	return true;
}

//-----------------------------------------------------------------------------------------------------

void VExampleInit::Init(const CSkinningInfo* pModelSkeleton,DynArray<SJointsAimIK_Rot>& rRot,DynArray<SJointsAimIK_Pos>& rPos, GlobalAnimationHeaderAIM& rAIM,int nWeaponBoneIdx,uint32 numAimPoses)
{
	uint32 numJoints = pModelSkeleton->m_arrBonesDesc.size();

	if(numJoints >= uint32(MAX_JOINT_AMOUNT))
	{
		RCLogError(" Too many joints defined for Aim/Look-IK... requested:%d max:%i", numJoints, MAX_JOINT_AMOUNT);
	}

	numJoints = min(numJoints, uint32(MAX_JOINT_AMOUNT));

	const char* pJointName = pModelSkeleton->m_arrBonesDesc[0].m_arrBoneName;
	m_arrJointCRC32[0] = CCrc32::Compute(pJointName);
	m_arrRelPose[0] = QuatT(pModelSkeleton->m_arrBonesDesc[0].m_DefaultB2W);
	m_arrDefaultRelPose[0] = m_arrRelPose[0];
	m_arrParentIdx[0]=-1;
	for (int32 i=1; i<numJoints; i++)
	{
		int32 offset=pModelSkeleton->m_arrBonesDesc[i].m_nOffsetParent;
		int32 p	= i+offset;
		m_arrParentIdx[i]=p;

		const char* pJointName = pModelSkeleton->m_arrBonesDesc[i].m_arrBoneName;
		QuatT pquat=QuatT(pModelSkeleton->m_arrBonesDesc[p].m_DefaultB2W);
		QuatT cquat=QuatT(pModelSkeleton->m_arrBonesDesc[i].m_DefaultB2W);
		m_arrRelPose[i] =	pquat.GetInverted() *	cquat;
		m_arrRelPose[i].q.Normalize();
		m_arrDefaultRelPose[i] = m_arrRelPose[i];
		m_arrJointCRC32[i] = CCrc32::Compute(pJointName);
	}

	//-------------------------------------------------------------------------------------------------
	//from here on the computation of aim-poses in RC and Engine is supposed to be identical
	//-------------------------------------------------------------------------------------------------

	rAIM.OnAimpose();

	rAIM.m_arrAimIKPosesAIM.resize(numAimPoses);
	uint32 numRot = rRot.size();
	uint32 numPos = rPos.size();
	for (uint32 a=0; a<numAimPoses; a++)
	{
		rAIM.m_arrAimIKPosesAIM[a].m_arrRotation.resize(numRot);
		rAIM.m_arrAimIKPosesAIM[a].m_arrPosition.resize(numPos);
	}

	m_arrAbsPose[0] = m_arrRelPose[0];
	for (uint32 i=1; i<numJoints; i++)
		m_arrAbsPose[i] = m_arrAbsPose[m_arrParentIdx[i]] * m_arrRelPose[i];

	for (uint32 p=0; p<numAimPoses; p++)
		CopyPoses2(rRot,rPos,rAIM,numAimPoses,p );

	//-----------------------------------------------------------------------------


	uint32 nMidPoseIdx = uint32(numAimPoses/2);
	uint32 nUpPoseIdx  = uint32(numAimPoses/3/2);
	ComputeAimPose( rAIM, rRot,rPos, m_arrAbsPose, nMidPoseIdx );

	Quatd qDefaultMidAimPose= !m_arrAbsPose[nWeaponBoneIdx].q;
	Quatd arrOriginalAimPoses[27]; 
	uint32 numRotJoints =rRot.size();
	for (uint32 ap=0; ap<numAimPoses; ap++)
	{
		ComputeAimPose( rAIM, rRot,rPos, m_arrAbsPose, ap );
		Quat q = m_arrAbsPose[nWeaponBoneIdx].q;
		arrOriginalAimPoses[ap] = qDefaultMidAimPose*m_arrAbsPose[nWeaponBoneIdx].q;
	}

	Vec3 v0=arrOriginalAimPoses[nUpPoseIdx].GetColumn1();
	Vec3 v1=arrOriginalAimPoses[nMidPoseIdx].GetColumn1();
	Vec3 upvector = v0-v1;
	upvector.y=0;
	upvector.Normalize();
	rAIM.m_MiddleAimPoseRot.SetRotationV0V1(Vec3(0,0,1),upvector);
	rAIM.m_MiddleAimPose=!rAIM.m_MiddleAimPoseRot*Quat(qDefaultMidAimPose);
	for (uint32 ap=0; ap<numAimPoses; ap++)
	{
		ComputeAimPose( rAIM, rRot,rPos, m_arrAbsPose, ap );
		arrOriginalAimPoses[ap] = qDefaultMidAimPose*m_arrAbsPose[nWeaponBoneIdx].q;
	}

	for (uint32 i=0; i<(CHUNK_GAHAIM_INFO::YGRID*CHUNK_GAHAIM_INFO::XGRID); i++)
	{
		PolarGrid[i].m_fSmalest=9999.0f;
		PolarGrid[i].i0=0xff;
		PolarGrid[i].i1=0xff;
		PolarGrid[i].i2=0xff;
		PolarGrid[i].i3=0xff;
		PolarGrid[i].w0=99.0f;
		PolarGrid[i].w1=99.0f;
		PolarGrid[i].w2=99.0f;
		PolarGrid[i].w3=99.0f;
	}


	QuadIndices arrQuat[64];
	uint32 numQuats = AnnotateExamples(numAimPoses, arrQuat);

	for (uint32 i=0; i<numJoints; i++)	m_arrRelPose0[i] = m_arrDefaultRelPose[i];
	for (uint32 i=0; i<numJoints; i++)	m_arrRelPose1[i] = m_arrDefaultRelPose[i];
	for (uint32 i=0; i<numJoints; i++)	m_arrRelPose2[i] = m_arrDefaultRelPose[i];
	for (uint32 i=0; i<numJoints; i++)	m_arrRelPose3[i] = m_arrDefaultRelPose[i];
	m_arrAbsPose0[0] = m_arrRelPose0[0];
	m_arrAbsPose1[0] = m_arrRelPose1[0];
	m_arrAbsPose2[0] = m_arrRelPose2[0];
	m_arrAbsPose3[0] = m_arrRelPose3[0];
	for (uint32 i=1; i<numJoints; i++)
	{	
		uint32 p=m_arrParentIdx[i];
		m_arrAbsPose0[i] = m_arrAbsPose0[p] * m_arrRelPose0[i];
		m_arrAbsPose1[i] = m_arrAbsPose1[p] * m_arrRelPose1[i];
		m_arrAbsPose2[i] = m_arrAbsPose2[p] * m_arrRelPose2[i];
		m_arrAbsPose3[i] = m_arrAbsPose3[p] * m_arrRelPose3[i];
	}


	for (int32 y=0; y<CHUNK_GAHAIM_INFO::YGRID; y++)
	{
		for (int32 x=0; x<CHUNK_GAHAIM_INFO::XGRID; x++)
		{
			f32 fx =x*(gf_PI/8)-gf_PI;
			f32 fy =y*(gf_PI/8)-gf_PI*0.5f;
			for (uint32 i=0; i<numQuats; i++)
			{
				int8 i0=arrQuat[i].i0;
				int8 i1=arrQuat[i].i1;
				int8 i2=arrQuat[i].i2;
				int8 i3=arrQuat[i].i3;
				Vec4 w0=arrQuat[i].w0;
				Vec4 w1=arrQuat[i].w1;
				Vec4 w2=arrQuat[i].w2;
				Vec4 w3=arrQuat[i].w3;

				m_nIterations=0;
				m_fSmallest = 9999.0f;
				RecursiveTest(Vec2d(fx,fy), rAIM,rRot,rPos,nWeaponBoneIdx, i0,i1,i2,i3, w0,w1,w2,w3 );

				if (m_fSmallest<10.0)
				{
					PolarGrid[y*CHUNK_GAHAIM_INFO::XGRID+x].m_fSmalest = f32(m_fSmallest);
					PolarGrid[y*CHUNK_GAHAIM_INFO::XGRID+x].i0 =i0;
					PolarGrid[y*CHUNK_GAHAIM_INFO::XGRID+x].i1 =i1;
					PolarGrid[y*CHUNK_GAHAIM_INFO::XGRID+x].i2 =i2;
					PolarGrid[y*CHUNK_GAHAIM_INFO::XGRID+x].i3 =i3;
					PolarGrid[y*CHUNK_GAHAIM_INFO::XGRID+x].w0 =m_Weight.x;
					PolarGrid[y*CHUNK_GAHAIM_INFO::XGRID+x].w1 =m_Weight.y;
					PolarGrid[y*CHUNK_GAHAIM_INFO::XGRID+x].w2 =m_Weight.z;
					PolarGrid[y*CHUNK_GAHAIM_INFO::XGRID+x].w3 =m_Weight.w;
					break;
				}
			}
		}
	}

	//----------------------------------------------------------------------------------------

	if (1)
	{
		if ( rAIM.VE2.capacity()==0 )
			rAIM.VE2.reserve(1024);

		for (uint32 i=0; i<numQuats; i++)
			//		for (uint32 i=0; i<1; i++)
		{
			int8 i0=arrQuat[i].i0;
			int8 i1=arrQuat[i].i1;
			int8 i2=arrQuat[i].i2;
			int8 i3=arrQuat[i].i3;

			Vec4d w0=arrQuat[i].w0;
			Vec4d w1=arrQuat[i].w1;
			Vec4d w2=arrQuat[i].w2;
			Vec4d w3=arrQuat[i].w3;

			Quatd mid;
			mid.w		= f32(rAIM.m_MiddleAimPose.w);
			mid.v.x = f32(rAIM.m_MiddleAimPose.v.x);
			mid.v.y = f32(rAIM.m_MiddleAimPose.v.y);
			mid.v.z = f32(rAIM.m_MiddleAimPose.v.z);



			Vec4d x0 = w0;
			Blend4AimPose( rAIM, rRot,rPos, i0,i1,i2,i3, w0,  m_arrRelPose0,m_arrAbsPose0 );
			Quatd q0 = mid*m_arrAbsPose0[nWeaponBoneIdx].q;
			for (f64 t=0; t<1.001; t+=0.05)
			{
				Vec4d x1; x1.SetLerp(w0,w1,t);
				Blend4AimPose( rAIM, rRot,rPos, i0,i1,i2,i3, x1,  m_arrRelPose0,m_arrAbsPose0 );
				Quatd q1 = mid*m_arrAbsPose0[nWeaponBoneIdx].q;

				Vec2d p0 = Vec2d( PolarCoordinate(q0) );
				Vec2d p1 = Vec2d( PolarCoordinate(q1) );
				for (int32 gx=-8; gx<+8; gx++)
				{
					f64 e=gx*(g_PI/8);
					if ((p0.x<e && p1.x>e) || (p0.x>e && p1.x<e))
					{
						f64 d0=e-p0.x;	f64 d1=p1.x-e;
						f64 d=d0+d1;
						f64 b0=1-d0/d;	f64 b1=1-d1/d;
						Vec2d polcor = p0*b0 + p1*b1;
						Vec4d weight = x0*b0 + x1*b1;
						CHUNK_GAHAIM_INFO::VirtualExampleInit2 ve2;
						ve2.polar.x=f32(polcor.x);	ve2.polar.y=f32(polcor.y);
						ve2.i0=i0; ve2.i1=i1; ve2.i2=i2; ve2.i3=i3;
						ve2.w0=f32(weight.x); ve2.w1=f32(weight.y); ve2.w2=f32(weight.z);	ve2.w3=f32(weight.w);
						rAIM.VE2.push_back(ve2);
					}
				}
				for (int32 gy=-5; gy<+5; gy++)
				{
					f64 e=gy*(g_PI/8);
					if ((p0.y<e && p1.y>e) || (p0.y>e && p1.y<e))
					{
						f64 d0=e-p0.y;	f64 d1=p1.y-e;
						f64 d=d0+d1;
						f64 b0=1-d0/d;	f64 b1=1-d1/d;
						Vec2d polcor = p0*b0 + p1*b1;
						Vec4d weight = x0*b0 + x1*b1;
						CHUNK_GAHAIM_INFO::VirtualExampleInit2 ve2;
						ve2.polar.x=f32(polcor.x);	ve2.polar.y=f32(polcor.y);
						ve2.i0=i0; ve2.i1=i1; ve2.i2=i2; ve2.i3=i3;
						ve2.w0=f32(weight.x); ve2.w1=f32(weight.y); ve2.w2=f32(weight.z);	ve2.w3=f32(weight.w);
						rAIM.VE2.push_back(ve2);
					}
				}
				q0=q1;
				x0=x1;
			}

			x0 = w1;
			Blend4AimPose( rAIM, rRot,rPos, i0,i1,i2,i3, w1,  m_arrRelPose0,m_arrAbsPose0 );
			q0 = mid*m_arrAbsPose0[nWeaponBoneIdx].q;
			for (f64 t=0; t<1.0001; t+=0.02)
			{
				Vec4d x1; x1.SetLerp(w1,w2,t);
				Blend4AimPose( rAIM, rRot,rPos, i0,i1,i2,i3, x1,  m_arrRelPose0,m_arrAbsPose0 );
				Quatd q1 = mid*m_arrAbsPose0[nWeaponBoneIdx].q;
				Vec2d p0 = Vec2d( PolarCoordinate(q0) );
				Vec2d p1 = Vec2d( PolarCoordinate(q1) );
				for (int32 gx=-8; gx<+8; gx++)
				{
					f64 e=gx*(g_PI/8);
					if ((p0.x<e && p1.x>e) || (p0.x>e && p1.x<e))
					{
						f64 d0=e-p0.x;	f64 d1=p1.x-e;
						f64 d=d0+d1;
						f64 b0=1-d0/d;	f64 b1=1-d1/d;
						Vec2d polcor = p0*b0 + p1*b1;
						Vec4d weight = x0*b0 + x1*b1;
						CHUNK_GAHAIM_INFO::VirtualExampleInit2 ve2;
						ve2.polar.x=f32(polcor.x);	ve2.polar.y=f32(polcor.y);
						ve2.i0=i0; ve2.i1=i1; ve2.i2=i2; ve2.i3=i3;
						ve2.w0=f32(weight.x); ve2.w1=f32(weight.y); ve2.w2=f32(weight.z);	ve2.w3=f32(weight.w);
						rAIM.VE2.push_back(ve2);
					}
				}
				for (int32 gy=-5; gy<+5; gy++)
				{
					f64 e=gy*(g_PI/8);
					if ((p0.y<e && p1.y>e) || (p0.y>e && p1.y<e))
					{
						f64 d0=e-p0.y;	f64 d1=p1.y-e;
						f64 d=d0+d1;
						f64 b0=1-d0/d;	f64 b1=1-d1/d;
						Vec2d polcor = p0*b0 + p1*b1;
						Vec4d weight = x0*b0 + x1*b1;
						CHUNK_GAHAIM_INFO::VirtualExampleInit2 ve2;
						ve2.polar.x=f32(polcor.x);	ve2.polar.y=f32(polcor.y);
						ve2.i0=i0; ve2.i1=i1; ve2.i2=i2; ve2.i3=i3;
						ve2.w0=f32(weight.x); ve2.w1=f32(weight.y); ve2.w2=f32(weight.z);	ve2.w3=f32(weight.w);
						rAIM.VE2.push_back(ve2);
					}
				}
				q0=q1;
				x0=x1;
			}

			x0 = w2;
			Blend4AimPose( rAIM, rRot,rPos, i0,i1,i2,i3, w2,  m_arrRelPose0,m_arrAbsPose0 );
			q0 = mid*m_arrAbsPose0[nWeaponBoneIdx].q;
			for (f64 t=0; t<1.0001; t+=0.05)
			{
				Vec4d x1; x1.SetLerp(w2,w3,t);
				Blend4AimPose( rAIM, rRot,rPos, i0,i1,i2,i3, x1,  m_arrRelPose0,m_arrAbsPose0 );
				Quatd q1 = mid*m_arrAbsPose0[nWeaponBoneIdx].q;
				Vec2d p0 = Vec2d( PolarCoordinate(q0) );
				Vec2d p1 = Vec2d( PolarCoordinate(q1) );
				for (int32 gx=-8; gx<+8; gx++)
				{
					f64 e=gx*(g_PI/8);
					if ((p0.x<e && p1.x>e) || (p0.x>e && p1.x<e))
					{
						f64 d0=e-p0.x;	f64 d1=p1.x-e;
						f64 d=d0+d1;
						f64 b0=1-d0/d;	f64 b1=1-d1/d;
						Vec2d polcor = p0*b0 + p1*b1;
						Vec4d weight = x0*b0 + x1*b1;
						CHUNK_GAHAIM_INFO::VirtualExampleInit2 ve2;
						ve2.polar.x=f32(polcor.x);	ve2.polar.y=f32(polcor.y);
						ve2.i0=i0; ve2.i1=i1; ve2.i2=i2; ve2.i3=i3;
						ve2.w0=f32(weight.x); ve2.w1=f32(weight.y); ve2.w2=f32(weight.z);	ve2.w3=f32(weight.w);
						rAIM.VE2.push_back(ve2);
					}
				}
				for (int32 gy=-5; gy<+5; gy++)
				{
					f64 e=gy*(g_PI/8);
					if ((p0.y<e && p1.y>e) || (p0.y>e && p1.y<e))
					{
						f64 d0=e-p0.y;	f64 d1=p1.y-e;
						f64 d=d0+d1;
						f64 b0=1-d0/d;	f64 b1=1-d1/d;
						Vec2d polcor = p0*b0 + p1*b1;
						Vec4d weight = x0*b0 + x1*b1;
						CHUNK_GAHAIM_INFO::VirtualExampleInit2 ve2;
						ve2.polar.x=f32(polcor.x);	ve2.polar.y=f32(polcor.y);
						ve2.i0=i0; ve2.i1=i1; ve2.i2=i2; ve2.i3=i3;
						ve2.w0=f32(weight.x); ve2.w1=f32(weight.y); ve2.w2=f32(weight.z);	ve2.w3=f32(weight.w);
						rAIM.VE2.push_back(ve2);
					}
				}
				q0=q1;
				x0=x1;
			}


			x0 = w3;
			Blend4AimPose( rAIM, rRot,rPos, i0,i1,i2,i3, w3,  m_arrRelPose0,m_arrAbsPose0 );
			q0 = mid*m_arrAbsPose0[nWeaponBoneIdx].q;
			for (f64 t=0; t<1.0001; t+=0.02)
			{
				Vec4d x1; x1.SetLerp(w3,w0,t);
				Blend4AimPose( rAIM, rRot,rPos, i0,i1,i2,i3, x1,  m_arrRelPose0,m_arrAbsPose0 );
				Quatd q1 = mid*m_arrAbsPose0[nWeaponBoneIdx].q;
				Vec2d p0 = Vec2d( PolarCoordinate(q0) );
				Vec2d p1 = Vec2d( PolarCoordinate(q1) );
				for (int32 gx=-8; gx<+8; gx++)
				{
					f64 e=gx*(g_PI/8);
					if ((p0.x<e && p1.x>e) || (p0.x>e && p1.x<e))
					{
						f64 d0=e-p0.x;	f64 d1=p1.x-e;
						f64 d=d0+d1;
						f64 b0=1-d0/d;	f64 b1=1-d1/d;
						Vec2d polcor = p0*b0 + p1*b1;
						Vec4d weight = x0*b0 + x1*b1;
						CHUNK_GAHAIM_INFO::VirtualExampleInit2 ve2;
						ve2.polar.x=f32(polcor.x);	ve2.polar.y=f32(polcor.y);
						ve2.i0=i0; ve2.i1=i1; ve2.i2=i2; ve2.i3=i3;
						ve2.w0=f32(weight.x); ve2.w1=f32(weight.y); ve2.w2=f32(weight.z);	ve2.w3=f32(weight.w);
						rAIM.VE2.push_back(ve2);
					}
				}
				for (int32 gy=-5; gy<+5; gy++)
				{
					f64 e=gy*(g_PI/8);
					if ((p0.y<e && p1.y>e) || (p0.y>e && p1.y<e))
					{
						f64 d0=e-p0.y;	f64 d1=p1.y-e;
						f64 d=d0+d1;
						f64 b0=1-d0/d;	f64 b1=1-d1/d;
						Vec2d polcor = p0*b0 + p1*b1;
						Vec4d weight = x0*b0 + x1*b1;
						CHUNK_GAHAIM_INFO::VirtualExampleInit2 ve2;
						ve2.polar.x=f32(polcor.x);	ve2.polar.y=f32(polcor.y);
						ve2.i0=i0; ve2.i1=i1; ve2.i2=i2; ve2.i3=i3;
						ve2.w0=f32(weight.x); ve2.w1=f32(weight.y); ve2.w2=f32(weight.z);	ve2.w3=f32(weight.w);
						rAIM.VE2.push_back(ve2);
					}
				}
				q0=q1;
				x0=x1;
			}

			uint32 ddd=0;
		}

		CHUNK_GAHAIM_INFO::VirtualExampleInit2 ve;
		for (int32 gy=-4; gy<5; gy++)
		{
			for (int32 gx=-8; gx<9; gx++)
			{
				f32 ex=gx*(gf_PI/8);
				f32 ey=gy*(gf_PI/8);

				if (PolarGrid[(4+gy)*CHUNK_GAHAIM_INFO::XGRID+(gx+8)].m_fSmalest>1.00f)
				{
					f32 dist=9999.0f;
					size_t nHowManyVE = rAIM.VE2.size();
					if (nHowManyVE==0)
						break;
					for (uint32 i=0; i<nHowManyVE; i++)
					{
						f32 fDistance=(Vec2(ex,ey)-rAIM.VE2[i].polar).GetLength();
						if (dist>fDistance)
						{
							dist=fDistance;
							ve=rAIM.VE2[i];
						}
					}				

					PolarGrid[(4+gy)*CHUNK_GAHAIM_INFO::XGRID+(gx+8)].m_fSmalest = 0;

					PolarGrid[(4+gy)*CHUNK_GAHAIM_INFO::XGRID+(gx+8)].i0 =ve.i0;
					PolarGrid[(4+gy)*CHUNK_GAHAIM_INFO::XGRID+(gx+8)].i1 =ve.i1;
					PolarGrid[(4+gy)*CHUNK_GAHAIM_INFO::XGRID+(gx+8)].i2 =ve.i2;
					PolarGrid[(4+gy)*CHUNK_GAHAIM_INFO::XGRID+(gx+8)].i3 =ve.i3;

					PolarGrid[(4+gy)*CHUNK_GAHAIM_INFO::XGRID+(gx+8)].w0 =ve.w0;
					PolarGrid[(4+gy)*CHUNK_GAHAIM_INFO::XGRID+(gx+8)].w1 =ve.w1;
					PolarGrid[(4+gy)*CHUNK_GAHAIM_INFO::XGRID+(gx+8)].w2 =ve.w2;
					PolarGrid[(4+gy)*CHUNK_GAHAIM_INFO::XGRID+(gx+8)].w3 =ve.w3;
				}
			}
		}
		rAIM.VE2.clear();	
	}

	for (int32 i=0; i<(CHUNK_GAHAIM_INFO::XGRID*CHUNK_GAHAIM_INFO::YGRID); i++)
	{
		rAIM.m_PolarGrid[i].i0 =	PolarGrid[i].i0;
		rAIM.m_PolarGrid[i].i1 =	PolarGrid[i].i1;
		rAIM.m_PolarGrid[i].i2 =	PolarGrid[i].i2;
		rAIM.m_PolarGrid[i].i3 =	PolarGrid[i].i3;
		rAIM.m_PolarGrid[i].v0 =	int16(PolarGrid[i].w0*f64(0x2000));
		rAIM.m_PolarGrid[i].v1 =	int16(PolarGrid[i].w1*f64(0x2000));
		rAIM.m_PolarGrid[i].v2 =	int16(PolarGrid[i].w2*f64(0x2000));
		rAIM.m_PolarGrid[i].v3 =	int16(PolarGrid[i].w3*f64(0x2000));
	}
}

//----------------------------------------------------------------------------------------------

void VExampleInit::CopyPoses2(DynArray<SJointsAimIK_Rot>& rRot,DynArray<SJointsAimIK_Pos>& rPos, GlobalAnimationHeaderAIM& rAIM, uint32 numPoses, uint32 skey )
{

	f32 fRealTime = rAIM.NTime2KTime(f32(skey)/(numPoses-1));

	uint32 numRot =rRot.size();
	for (uint32 r=0; r<numRot; r++)
	{
		const char* const pJointName = rRot[r].m_strJointName;
		(void)pJointName;
		int32 j	=rRot[r].m_nJointIdx;
		assert(j>0);
		rAIM.m_arrAimIKPosesAIM[skey].m_arrRotation[r]=m_arrDefaultRelPose[j].q;
		IController* pFAimController = rAIM.GetController(m_arrJointCRC32[j]);
		if (pFAimController)
		{
			int32 numKeyRot = pFAimController->GetO_numKey();
			int32 numKeyPos = pFAimController->GetP_numKey();
			if (numKeyRot>0 || numKeyPos>0) //do we have a rotation channel
			{
				Quat q=m_arrDefaultRelPose[j].q;
				if (numKeyRot>0)
					pFAimController->GetO(fRealTime,q);

				f32 dot=m_arrDefaultRelPose[j].q|q;
				if (dot<0)
					q=-q;

				rAIM.m_arrAimIKPosesAIM[skey].m_arrRotation[r]=q;
				rAIM.m_nExist |= uint64(1)<<r;
			}
		}
	}


	uint32 numPos = rPos.size();
	for (uint32 r=0; r<numPos; r++)
	{
		const char* const pJointName = rPos[r].m_strJointName;
		(void)pJointName;
		int32 j	= rPos[r].m_nJointIdx;
		assert(j>0);   
		rAIM.m_arrAimIKPosesAIM[skey].m_arrPosition[r]=m_arrDefaultRelPose[j].t;
		IController* pFAimController = rAIM.GetController(m_arrJointCRC32[j]);
		if (pFAimController)
		{
			int32 numKeys = pFAimController->GetP_numKey();
			if (numKeys>0)  //do we have a position channel
			{
				Vec3 p;	pFAimController->GetP(fRealTime,p);
				rAIM.m_arrAimIKPosesAIM[skey].m_arrPosition[r]=p;
			}
		}
	}
}

//----------------------------------------------------------------------------------------------

void VExampleInit::RecursiveTest( const Vec2d& ControlPoint, GlobalAnimationHeaderAIM& rAIM,DynArray<SJointsAimIK_Rot>& rRot,DynArray<SJointsAimIK_Pos>& rPos,int nWBone, int i0,int i1,int i2,int i3, const Vec4d& w0,const Vec4d& w1,const Vec4d& w2,const Vec4d& w3)
{
	uint32 nOverlap = PointInQuat( ControlPoint,   rAIM,rRot,rPos,nWBone,  i0,i1,i2,i3, w0,w1,w2,w3);
	if (nOverlap)
	{

		Vec4d mid=(w0+w1+w2+w3)*0.25;

		//w0 w1
		//w3 w2
		Vec4d x0a=w0;						Vec4d x1a=(w0+w1)*0.5;						Vec4d x0b=(w0+w1)*0.5;			Vec4d x1b=w1;
		Vec4d x3a=(w0+w3)*0.5;	Vec4d x2a=mid;										Vec4d x3b=mid;							Vec4d x2b=(w1+w2)*0.5;
		Vec4d x0d=(w0+w3)*0.5;	Vec4d x1d=mid;										Vec4d x0c=mid;							Vec4d x1c=(w1+w2)*0.5;
		Vec4d x3d=w3;						Vec4d x2d=(w3+w2)*0.5;						Vec4d x3c=(w3+w2)*0.5;			Vec4d x2c=w2;

		uint32 o0=0;
		uint32 o1=0;
		uint32 o2=0;
		uint32 o3=0;
		Vec4d w0a=x0a;	Vec4d w1a=x1a;				Vec4d w0b=x0b;		Vec4d w1b=x1b;
		Vec4d w3a=x3a;	Vec4d w2a=x2a;				Vec4d w3b=x3b;		Vec4d w2b=x2b;
		Vec4d w0d=x0d;	Vec4d w1d=x1d;				Vec4d w0c=x0c;		Vec4d w1c=x1c;
		Vec4d w3d=x3d;	Vec4d w2d=x2d;				Vec4d w3c=x3c;		Vec4d w2c=x2c;
		for (f32 s=1.005f; s<1.1f; s+=0.001f )
		{
			f64 a=s;
			f64 b=1.0-s;

			w0a=x0a*a+x2a*b; w1a=x1a*a+x3a*b;		w0b=x0b*a+x2b*b;	w1b=x1b*a+x3b*b;
			w3a=x3a*a+x1a*b; w2a=x2a*a+x0a*b;		w3b=x3b*a+x1b*b;	w2b=x2b*a+x0b*b;
			w0d=x0d*a+x2d*b; w1d=x1d*a+x3d*b;		w0c=x0c*a+x2c*b;	w1c=x1c*a+x3c*b;
			w3d=x3d*a+x1d*b; w2d=x2d*a+x0d*b;		w3c=x3c*a+x1c*b;	w2c=x2c*a+x0c*b;

			o0 = PointInQuat( ControlPoint,  rAIM,rRot,rPos,nWBone,  i0,i1,i2,i3,w0a,w1a,w2a,w3a);
			o1 = PointInQuat( ControlPoint,  rAIM,rRot,rPos,nWBone,  i0,i1,i2,i3,w0b,w1b,w2b,w3b);
			o2 = PointInQuat( ControlPoint,  rAIM,rRot,rPos,nWBone,  i0,i1,i2,i3,w0c,w1c,w2c,w3c);
			o3 = PointInQuat( ControlPoint,  rAIM,rRot,rPos,nWBone,  i0,i1,i2,i3,w0d,w1d,w2d,w3d);
			uint32 sum=o0+o1+o2+o3;
			if (sum)
				break;
		}

		uint32 sum=o0+o1+o2+o3;
//		assert(sum);

		m_nIterations++;
		if (m_nIterations>50)
			return;
		if (m_fSmallest<PRECISION)
			return;

		if (o0)
			RecursiveTest( ControlPoint, rAIM,rRot,rPos,nWBone, i0,i1,i2,i3, w0a,w1a,w2a,w3a);
		if (o1)
			RecursiveTest( ControlPoint, rAIM,rRot,rPos,nWBone, i0,i1,i2,i3, w0b,w1b,w2b,w3b);
		if (o2)
			RecursiveTest( ControlPoint, rAIM,rRot,rPos,nWBone, i0,i1,i2,i3, w0c,w1c,w2c,w3c);
		if (o3)
			RecursiveTest( ControlPoint, rAIM,rRot,rPos,nWBone, i0,i1,i2,i3, w0d,w1d,w2d,w3d);
	}
}


#pragma warning(push)
#pragma warning(disable : 6262) // this is only run in RC which has enough stack space
uint32 VExampleInit::PointInQuat( const Vec2d& ControlPoint, GlobalAnimationHeaderAIM& rAIM, DynArray<SJointsAimIK_Rot>& rRot,DynArray<SJointsAimIK_Pos>& rPos,int nWBone,	int i0,int i1,int i2,int i3, const Vec4d& w0,const Vec4d& w1,const Vec4d& w2,const Vec4d& w3)
{
	f64 sum0 = w0.x + w0.y + w0.z + w0.w;
	assert( fabs(sum0-1.0f)<0.00001 );
	f64 sum1 = w1.x + w1.y + w1.z + w1.w;
	assert( fabs(sum1-1.0f)<0.00001 );
	f64 sum2 = w2.x + w2.y + w2.z + w2.w;
	assert( fabs(sum2-1.0f)<0.00001 );
	f64 sum3 = w3.x + w3.y + w3.z + w3.w;
	assert( fabs(sum3-1.0f)<0.00001 );

	Quatd mid;
	mid.w		= f32(rAIM.m_MiddleAimPose.w);
	mid.v.x = f32(rAIM.m_MiddleAimPose.v.x);
	mid.v.y = f32(rAIM.m_MiddleAimPose.v.y);
	mid.v.z = f32(rAIM.m_MiddleAimPose.v.z);

	Blend4AimPose( rAIM, rRot,rPos, i0,i1,i2,i3, w0,  m_arrRelPose0,m_arrAbsPose0 );
	Quatd tq0 = mid*m_arrAbsPose0[nWBone].q;
	Blend4AimPose( rAIM, rRot,rPos, i0,i1,i2,i3, w1,  m_arrRelPose1,m_arrAbsPose1 );
	Quatd tq1 = mid*m_arrAbsPose1[nWBone].q;
	Blend4AimPose( rAIM, rRot,rPos, i0,i1,i2,i3, w2,  m_arrRelPose2,m_arrAbsPose2 );
	Quatd tq2 = mid*m_arrAbsPose2[nWBone].q;
	Blend4AimPose( rAIM, rRot,rPos, i0,i1,i2,i3, w3,  m_arrRelPose3,m_arrAbsPose3 );
	Quatd tq3 = mid*m_arrAbsPose3[nWBone].q;


	Vec2d	tp0 = Vec2d(PolarCoordinate(tq0));
	assert(fabs(tp0.x)<3.1416f);
	assert(fabs(tp0.y)<2.0f);

	Vec2d	tp1=Vec2d(PolarCoordinate(tq1));
	assert(fabs(tp1.x)<3.1416f);
	assert(fabs(tp1.y)<2.0f);

	Vec2d	tp2=Vec2d(PolarCoordinate(tq2));
	assert(fabs(tp2.x)<3.1416f);
	assert(fabs(tp2.y)<2.0f);

	Vec2d	tp3=Vec2d(PolarCoordinate(tq3));
	assert(fabs(tp3.x)<3.1416f);
	assert(fabs(tp3.y)<2.0f);

	f64 t;
	uint32 c=0;
	Vec2d polar[2000];
	Vec4d weight[2000];
	polar[0]	= Vec2d(PolarCoordinate(tq0));
	weight[0]	=	w0;
	f64 maxstep=0.250f;

	f64 angle0	=	std::max(acos_tpl(std::min(tq1|tq2,1.0)),0.01);	assert(angle0>=0.009);
	f64 step0		=	std::min((1.0/(angle0*angle0*30.0)),maxstep);
	for (f64 i=step0; i<3.0; i+=step0)
	{
		c++;	t=i; if (i>0.999) t=1.0;
		NLerp2AimPose( rRot,rPos, m_arrRelPose0,m_arrRelPose1,t,m_arrAbsPose);
		Quatd qt = mid*m_arrAbsPose[nWBone].q;
		polar[c] = Vec2d(PolarCoordinate(qt));
		weight[c].SetLerp(w0,w1,t);;
		if (t==1.0)	break;
	}

	f64 angle1	=	std::max(acos_tpl(std::min(tq1|tq2,1.0)),0.01);	assert(angle1>=0.009);
	f64 step1		=	std::min((1.0/(angle1*angle1*30.0)),maxstep);
	for (f64 i=step1; i<3.0; i+=step1)
	{
		c++; t=i; if (i>0.999) t=1.0;
		NLerp2AimPose( rRot,rPos, m_arrRelPose1,m_arrRelPose2,t,m_arrAbsPose);
		Quatd qt = mid*m_arrAbsPose[nWBone].q;
		polar[c] = Vec2d(PolarCoordinate(qt));
		weight[c].SetLerp(w1,w2,t);;
		if (t==1.0) break;
	}

	f64 angle2	=	std::max(acos_tpl(std::min(tq2|tq3,1.0)),0.01); assert(angle2>=0.009);
	f64 step2		=	std::min((1.0/(angle2*angle2*30.0)),maxstep);
	for (f64 i=step2; i<3.0; i+=step2)
	{
		c++; t=i; if (i>0.999) t=1.0;
		NLerp2AimPose( rRot,rPos, m_arrRelPose2,m_arrRelPose3,t,m_arrAbsPose);
		Quatd qt = mid*m_arrAbsPose[nWBone].q;
		polar[c] = Vec2d(PolarCoordinate(qt));
		weight[c].SetLerp(w2,w3,t);;
		if (t==1.0) break;
	}

	f64 angle3	=	std::max(acos_tpl(std::min(tq3|tq0,1.0)),0.01);	assert(angle3>=0.009);
	f64 step3		=	std::min((1.0/(angle3*angle3*30.0)),maxstep);
	for (f64 i=step3; i<3.0; i+=step3)
	{
		c++; t=i; if (i>0.999) t=1.0;
		NLerp2AimPose( rRot,rPos, m_arrRelPose3,m_arrRelPose0,t,m_arrAbsPose);
		Quat qt = mid*m_arrAbsPose[nWBone].q;
		polar[c] = Vec2d(PolarCoordinate(qt));
		weight[c].SetLerp(w3,w0,t);;
		if (t==1.0) break;
	}
	assert(c);
	assert(c<1000);
	f64 length = (polar[c]-polar[0]).GetLength();
	// we should either add a warning here or remove this assertion, 
	// it makes debugging impossible
	// assert(length<0.0001);
	polar[c]=polar[0];



	Vec2d ControlPointEnd = ControlPoint+Vec2d(1,0)*10;
	Planer plane; plane.SetPlane(Vec2d(0,-1),ControlPoint);
	//pRenderer->Draw_Lineseg( Vec3(ControlPoint), RGBA8(0xff,0xff,0xff,0xff),Vec3(ControlPointEnd), RGBA8(0xff,0xff,0xff,0xff)  );

	int32 nOverlap=0;
	Vec2d p0=polar[0];
	for (uint32 l=1; l<(c+1); l++)
	{
		Vec2d p1=polar[l];
		nOverlap += LinesegOverlap2D(plane, ControlPoint,ControlPointEnd,  p0,p1);
		p0=p1;
	}


	if (nOverlap&1)
	{
		for (uint32 l=0; l<(c+1); l++)
		{
			f64 dif = (ControlPoint-polar[l]).GetLength();
			if (m_fSmallest>dif)
				m_fSmallest=dif,m_Weight=weight[l];
		}

	}

	return nOverlap&1;
}
#pragma warning(pop)



// 1=	v	{x=-0.49029412865638733 y=-0.49029466509819031  z=-0.50952035188674927 }	w	0.50952118635177612	double
// 9=	v	{x=0.078175172209739685 y=-0.033533055335283279 z=0.0030255538877099752 }	w 0.99637091159820557	double
//22= v	{x=-0.022930311039090157 y=0.038566075265407562 z=0.10730034112930298 }	  w	0.99321371316909790	double
//23	v	{x=-0.0096390629187226295 y=0.027362542226910591 z=0.12028977274894714 }	w	0.99231481552124023	double
//30	v	{x=-0.028650492429733276 y=0.00049196928739547729 z=0.069963596761226654 }w	0.99713790416717529	double
//32	v	{x=0.20867864787578583 y=-0.066090457141399384 z=-0.078888267278671265 }  w	0.97255432605743408	double
//33	v	{x=0.81940579414367676 y=0.067025691270828247 z=-0.56066983938217163 }	  w	-0.098646357655525208	double
//76	v	{x=0.085588440299034119 y=-0.43747770786285400 z=-0.43326830863952637 }	  w	0.78330481052398682	double
//80	v	{x=5.3992001980418536e-009 y=6.6407221943620698e-009 z=-0.88179987668991089 }	w	0.47162374854087830	double
//83	v	{x=-0.68800711631774902 y=-0.45051130652427673 z=-0.00087945495033636689 }	  w	-0.56893330812454224	double
//94	v	{x=0.028186293318867683 y=0.069935172796249390 z=-0.59418272972106934 }	     w	0.80078804492950439	double
void VExampleInit::ComputeAimPose( GlobalAnimationHeaderAIM& rAIM, DynArray<SJointsAimIK_Rot>& rRot,DynArray<SJointsAimIK_Pos>& rPos, QuatTd* arrAbsPose, uint32 nAimPoseMid )
{
	uint32 numRotJoints =rRot.size();
	for (uint32 r=0; r<numRotJoints; r++)
	{
		if (rRot[r].m_nPreEvaluate)
		{
			int32 j	=rRot[r].m_nJointIdx;
			const char* const pName = rRot[r].m_strJointName;
			(void)pName;
			QuatTd qtemp;
			qtemp.q = rAIM.m_arrAimIKPosesAIM[nAimPoseMid].m_arrRotation[r];
			qtemp.t = m_arrDefaultRelPose[j].t;
			int32 p	=rRot[r].m_nPosIndex;
			if (p>-1)
				qtemp.t = rAIM.m_arrAimIKPosesAIM[nAimPoseMid].m_arrPosition[p];

			uint32 pidx = m_arrParentIdx[j];
			arrAbsPose[j]	= arrAbsPose[pidx] * qtemp;
		}
	}
}


void VExampleInit::Blend4AimPose( GlobalAnimationHeaderAIM& rAIM, DynArray<SJointsAimIK_Rot>& rRot,DynArray<SJointsAimIK_Pos>& rPos, int8 i0,int8 i1,int8 i2,int8 i3, const Vec4d& w, QuatTd* arrRelPose,QuatTd* arrAbsPose  )
{
	uint32 numRotJoints =rRot.size();
	for (uint32 r=0; r<numRotJoints; r++)
	{
		if (rRot[r].m_nPreEvaluate)
		{
			int32 j	=rRot[r].m_nJointIdx;
			arrRelPose[j].q.w		= 0.0f;
			arrRelPose[j].q.v.x = 0.0f;
			arrRelPose[j].q.v.y = 0.0f;
			arrRelPose[j].q.v.z = 0.0f;

			arrRelPose[j].t.x		= 0.0f;
			arrRelPose[j].t.y		= 0.0f;
			arrRelPose[j].t.z		= 0.0f;
		}
	}

	if (w.x)
	{
		for (uint32 r=0; r<numRotJoints; r++)
		{
			if (rRot[r].m_nPreEvaluate)
			{
				int32 j	=rRot[r].m_nJointIdx;
				Quatd qRot = rAIM.m_arrAimIKPosesAIM[i0].m_arrRotation[r];
				Vec3d vPos = m_arrDefaultRelPose[j].t;
				int32 p	=rRot[r].m_nPosIndex;
				if (p>-1)
					vPos = rAIM.m_arrAimIKPosesAIM[i0].m_arrPosition[p];

				arrRelPose[j].q += qRot*w.x;
				arrRelPose[j].t += vPos*w.x;
			}
		}
	}

	if (w.y)
	{
		for (uint32 r=0; r<numRotJoints; r++)
		{
			if (rRot[r].m_nPreEvaluate)
			{
				int32 j	=rRot[r].m_nJointIdx;
				Quatd qRot = rAIM.m_arrAimIKPosesAIM[i1].m_arrRotation[r];
				Vec3d vPos = m_arrDefaultRelPose[j].t;
				int32 p	=rRot[r].m_nPosIndex;
				if (p>-1)
					vPos = rAIM.m_arrAimIKPosesAIM[i1].m_arrPosition[p];

				arrRelPose[j].q += qRot*w.y;
				arrRelPose[j].t += vPos*w.y;
			}
		}
	}

	if (w.z)
	{
		for (uint32 r=0; r<numRotJoints; r++)
		{
			if (rRot[r].m_nPreEvaluate)
			{
				int32 j	=rRot[r].m_nJointIdx;
				Quatd qRot = rAIM.m_arrAimIKPosesAIM[i2].m_arrRotation[r];
				Vec3d vPos = m_arrDefaultRelPose[j].t;
				int32 p	=rRot[r].m_nPosIndex;
				if (p>-1)
					vPos = rAIM.m_arrAimIKPosesAIM[i2].m_arrPosition[p];

				arrRelPose[j].q += qRot*w.z;
				arrRelPose[j].t += vPos*w.z;
			}
		}
	}

	if (w.w)
	{
		for (uint32 r=0; r<numRotJoints; r++)
		{
			if (rRot[r].m_nPreEvaluate)
			{
				int32 j	=rRot[r].m_nJointIdx;
				Quatd qRot = rAIM.m_arrAimIKPosesAIM[i3].m_arrRotation[r];
				Vec3d vPos = m_arrDefaultRelPose[j].t;
				int32 p	=rRot[r].m_nPosIndex;
				if (p>-1)
					vPos = rAIM.m_arrAimIKPosesAIM[i3].m_arrPosition[p];

				arrRelPose[j].q += qRot*w.w;
				arrRelPose[j].t += vPos*w.w;
			}
		}
	}

	for (uint32 r=0; r<numRotJoints; r++)
	{
		if (rRot[r].m_nPreEvaluate)
		{
			int32 j	=rRot[r].m_nJointIdx;
			int32 p = m_arrParentIdx[j];
			arrRelPose[j].q.Normalize();
				arrAbsPose[j] = arrAbsPose[p] * arrRelPose[j];
		}
	}
}




//---------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------
void VExampleInit::NLerp2AimPose( DynArray<SJointsAimIK_Rot>& rRot,DynArray<SJointsAimIK_Pos>& rPos, QuatTd* arrRelPose0,QuatTd* arrRelPose1,f64 t, QuatTd* arrAbsPose )
{
	uint32 numRotJoints =rRot.size();
	f64 t0=1.0-t;
	f64 t1=t;
	QuatTd qt;
	for (uint32 r=0; r<numRotJoints; r++)
	{
		if (rRot[r].m_nPreEvaluate)
		{
			int32 j	=rRot[r].m_nJointIdx;
			int32 p = m_arrParentIdx[j];
				qt.q = arrRelPose0[j].q*t0 + arrRelPose1[j].q*t1;
				qt.t = arrRelPose0[j].t*t0 + arrRelPose1[j].t*t1;
			qt.q.Normalize();
				arrAbsPose[j] = arrAbsPose[p]*qt;
		}
	}
}


uint32 VExampleInit::LinesegOverlap2D(const Planer& plane, const Vec2d& ls0,const Vec2d& le0,   const Vec2d& tp0,const Vec2d& tp1 )
{
	f64 d0 = plane|tp0;
	f64 d1 = plane|tp1;
	if (d0<0 && d1<0) return 0;
	if (d0>=0 && d1>=0)	return 0;

	Vec2d n = tp1-tp0; 
	if ((n|n)<0.0000000001) 
		return 0;
	n.Normalize();
	f64 d =  -tp0.y*n.x+tp0.x*n.y;	
	f64 d2 = n.y*ls0.x-n.x*ls0.y-d;
	f64 d3 = n.y*le0.x-n.x*le0.y-d;
	if (d2<=0 && d3<=0)	return 0;
	if (d2>0 && d3>0)	return 0;
	return 1;
}

Vec3d VExampleInit::PolarCoordinate(const Quatd& q)
{
	Matrix33r m=Matrix33r(q);
	real l = sqrt(m.m01*m.m01+m.m11*m.m11); 
	if (l > static_cast<real>(0.0001) ) 
		return Vec3d(atan2(-m.m01/l,m.m11/l),-atan2(m.m21,l),atan2(-m.m20/l,m.m22/l)); 
	else 
		return Vec3d(0,atan2(m.m21,l),0); 
}


uint32 VExampleInit::AnnotateExamples(uint32 numPoses, QuadIndices* arrQuat)
{
	uint32 q = numPoses >= 3 ? numPoses/3-1 : 0;
	for (uint32 i=0; i<q; i++)
	{
		arrQuat[i+0].i0=i+0;		arrQuat[i+0].w0=Vec4(1,0,0,0);
		arrQuat[i+0].i1=i+1;		arrQuat[i+0].w1=Vec4(0,1,0,0);
		arrQuat[i+0].i2=i+q+2;	arrQuat[i+0].w2=Vec4(0,0,1,0);
		arrQuat[i+0].i3=i+q+1;	arrQuat[i+0].w3=Vec4(0,0,0,1);
		arrQuat[i+0].col		=		RGBA8(0x00,0xff,0x00,0xff);
		arrQuat[i+0].height	=		Vec3(0,0,0.001f);

		arrQuat[i+q].i0=i+q+1;		arrQuat[i+q].w0=Vec4(1,0,0,0);
		arrQuat[i+q].i1=i+q+2;		arrQuat[i+q].w1=Vec4(0,1,0,0);
		arrQuat[i+q].i2=i+q+q+3;	arrQuat[i+q].w2=Vec4(0,0,1,0);
		arrQuat[i+q].i3=i+q+q+2;	arrQuat[i+q].w3=Vec4(0,0,0,1);
		arrQuat[i+q].col		= RGBA8(0x00,0xff,0x00,0xff);
		arrQuat[i+q].height	= Vec3(0,0,0.001f);
	}

	if (numPoses != 9)
		return q*2;

	f32 t=0;

	f32 diag=1.70f;

	f32 eup  =1.60f; f32 mup  =1.20f;

	f32 mside=2.00f;
	f32 eside=1.90f;

	f32 edown=1.6f;  f32 mdown=1.20f;

	uint32 i;

	i=4;
	arrQuat[i].i0=3;	t=eup;   arrQuat[i].w0=Vec4(1-t,  0,  0,  t); //3-mirrored 0-scaled
	arrQuat[i].i1=4;	t=mup;   arrQuat[i].w1=Vec4(  0,1-t,  t,  0); //4-mirrored 1-scaled
	arrQuat[i].i2=1;	         arrQuat[i].w2=Vec4(  0,  0,  1,  0); 
	arrQuat[i].i3=0;	         arrQuat[i].w3=Vec4(  0,  0,  0,  1);
	arrQuat[i].col		= RGBA8(0xff,0x00,0x00,0xff);
	arrQuat[i].height	= Vec3(0,0,0.001f);

	i=5;
	arrQuat[i].i0=3;	         arrQuat[i].w0=Vec4(  1,  0,  0,  0); 
	arrQuat[i].i1=4;	t=mside; arrQuat[i].w1=Vec4(  t,1-t,  0,  0); //4-mirrored 3-scaled
	arrQuat[i].i2=1;	t=eside; arrQuat[i].w2=Vec4(  0,  0,1-t,  t); //1-mirrored 0-scaled
	arrQuat[i].i3=0;	         arrQuat[i].w3=Vec4(  0,  0,  0,  1);
	arrQuat[i].col		= RGBA8(0xff,0x00,0x00,0xff);
	arrQuat[i].height	= Vec3(0,0,0.001f);

	i=6;
	arrQuat[i].i0=4;	t=diag;  arrQuat[i].w0=Vec4(1-t,  0,  t,  0); //4-mirrored 0-scaled
	arrQuat[i].i1=3;	t=eup;   arrQuat[i].w1=Vec4(  0,1-t,  t,  0); //3-mirrored 0-scaled
	arrQuat[i].i2=0;	         arrQuat[i].w2=Vec4(  0,  0,  1,  0); 
	arrQuat[i].i3=1;	t=eside; arrQuat[i].w3=Vec4(  0,  0,  t,1-t); //1-mirrored 0-scaled
	arrQuat[i].col		= RGBA8(0xff,0x00,0x00,0xff);
	arrQuat[i].height	= Vec3(0,0,0.001f);





	i=7;
	arrQuat[i].i0=2;	         arrQuat[i].w0=Vec4(  1,  0,  0,  0);
	arrQuat[i].i1=1;	t=eside; arrQuat[i].w1=Vec4(  t,1-t,  0,  0); //1-mirrored 2-scaled
	arrQuat[i].i2=4;	t=mside; arrQuat[i].w2=Vec4(  0,  0,1-t,  t); //4-mirrored 5-scaled
	arrQuat[i].i3=5;	         arrQuat[i].w3=Vec4(  0,  0,  0,  1);
	arrQuat[i].col		= RGBA8(0xff,0x00,0x00,0xff);
	arrQuat[i].height	= Vec3(0,0,0.001f);

	i=8;
	arrQuat[i].i0=4;	t=mup;   arrQuat[i].w0=Vec4(1-t,  0,  0,  t); //4-mirrored 1-scaled
	arrQuat[i].i1=5;	t=eup;   arrQuat[i].w1=Vec4(  0,1-t,  t,  0); //5-mirrored 2-scaled
	arrQuat[i].i2=2;	         arrQuat[i].w2=Vec4(  0,  0,  1,  0); 
	arrQuat[i].i3=1;	         arrQuat[i].w3=Vec4(  0,  0,  0,  1); 
	arrQuat[i].col		= RGBA8(0xff,0x00,0x00,0xff);
	arrQuat[i].height	= Vec3(0,0,0.001f);

	i=9;
	arrQuat[i].i0=5;	t=eup;   arrQuat[i].w0=Vec4(1-t,  0,  0,  t); //5-mirrored 2-scaled
	arrQuat[i].i1=4;	t=diag;  arrQuat[i].w1=Vec4(  0,1-t,  0,  t); //4-mirrored 2-scaled
	arrQuat[i].i2=1;	t=eside; arrQuat[i].w2=Vec4(  0,  0,1-t,  t); //1-mirrored 2-scaled
	arrQuat[i].i3=2;	         arrQuat[i].w3=Vec4(  0,  0,  0,  1);
	arrQuat[i].col		= RGBA8(0xff,0x00,0x00,0xff);
	arrQuat[i].height	= Vec3(0,0,0.001f);


	i=10;
	arrQuat[i].i0=4;	t=mside; arrQuat[i].w0=Vec4(1-t,  t,  0,  0); //4-mirrored 3-scaled
	arrQuat[i].i1=3;	         arrQuat[i].w1=Vec4(  0,  1,  0,  0); 
	arrQuat[i].i2=6;	         arrQuat[i].w2=Vec4(  0,  0,  1,  0); 
	arrQuat[i].i3=7;	t=eside; arrQuat[i].w3=Vec4(  0,  0,  t,1-t); //7-mirrored 6-scaled
	arrQuat[i].col			= RGBA8(0xff,0x00,0x00,0xff);
	arrQuat[i].height	= Vec3(0,0,0.001f);
	i=11;
	arrQuat[i].i0=6;	         arrQuat[i].w0=Vec4(  1,  0,  0,  0); 
	arrQuat[i].i1=7;	         arrQuat[i].w1=Vec4(  0,  1,  0,  0); 
	arrQuat[i].i2=4;	t=mdown; arrQuat[i].w2=Vec4(  0,  t,1-t,  0); //4-mirrored 7-scaled
	arrQuat[i].i3=3;	t=edown; arrQuat[i].w3=Vec4(  t,  0,  0,1-t); //3-mirrored 6-scaled
	arrQuat[i].col			= RGBA8(0xff,0x00,0x00,0xff);
	arrQuat[i].height	= Vec3(0,0,0.001f);
	i=12;
	arrQuat[i].i0=7;	t=eside; arrQuat[i].w0=Vec4(1-t,  t,  0,  0); //7-mirrored 6-scaled
	arrQuat[i].i1=6;	         arrQuat[i].w1=Vec4(  0,  1,  0,  0); 
	arrQuat[i].i2=3;	t=edown; arrQuat[i].w2=Vec4(  0,  t,1-t,  0); //3-mirrored 6-scaled
	arrQuat[i].i3=4;	t=diag;  arrQuat[i].w3=Vec4(  0,  t,  0,1-t); //4-mirrored 6-scaled
	arrQuat[i].col		= RGBA8(0xff,0x00,0x00,0xff);
	arrQuat[i].height	= Vec3(0,0,0.001f);


	i=13;
	arrQuat[i].i0=5;	         arrQuat[i].w0=Vec4(  1,  0,  0,  0);
	arrQuat[i].i1=4;	t=mside; arrQuat[i].w1=Vec4(  t,1-t,  0,  0);  //4 mirrored 5-scaled
	arrQuat[i].i2=7;	t=eside; arrQuat[i].w2=Vec4(  0,  0,1-t,  t);  //7 mirrored 8-scaled
	arrQuat[i].i3=8;	         arrQuat[i].w3=Vec4(  0,  0,  0,  1);
	arrQuat[i].col		= RGBA8(0xff,0x00,0x00,0xff);
	arrQuat[i].height	= Vec3(0,0,0.001f);
	i=14;
	arrQuat[i].i0=7;	         arrQuat[i].w0=Vec4(  1,  0,  0,  0);  
	arrQuat[i].i1=8;	         arrQuat[i].w1=Vec4(  0,  1,  0,  0); 
	arrQuat[i].i2=5;	t=edown; arrQuat[i].w2=Vec4(  0,  t, 1-t, 0); //5-mirrored 8-scaled
	arrQuat[i].i3=4;	t=mdown; arrQuat[i].w3=Vec4(  t,  0,  0,1-t); //4-mirrored 7-scaled
	arrQuat[i].col		= RGBA8(0xff,0x00,0x00,0xff);
	arrQuat[i].height	= Vec3(0,0,0.001f);
	i=15;
	arrQuat[i].i0=8;	         arrQuat[i].w0=Vec4(  1,  0,  0,  0);  
	arrQuat[i].i1=7;	t=eside; arrQuat[i].w1=Vec4(  t,1-t,  0,  0); //7-mirrored 8-scaled
	arrQuat[i].i2=4;	t=diag;  arrQuat[i].w2=Vec4(  t,  0,1-t,  0); //4-mirrored 8-scaled cross
	arrQuat[i].i3=5;	t=edown; arrQuat[i].w3=Vec4(  t,  0,  0,1-t); //5-mirrored 8-scaled
	arrQuat[i].col		= RGBA8(0xff,0x00,0x00,0xff);
	arrQuat[i].height	= Vec3(0,0,0.001f);

	return q*2 +3 +3 +3 +3;
}

