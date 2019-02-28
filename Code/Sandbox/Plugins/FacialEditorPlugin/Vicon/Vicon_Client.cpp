// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "Vicon_ClientCodes.h"

#ifndef DISABLE_VICON

//#include <iostream>
	#include <cassert>
	#include <vector>
	#include <algorithm>
	#include <functional>
	#include <limits>

	#include <math.h>

	#include <winsock2.h>

//#include "GameCVars.h"
//#include "GameRules.h"
	#include <CryRenderer/IRenderAuxGeom.h>

	#include <CryAnimation/ICryAnimation.h>
	#include "IEditorImpl.h"

extern tNetError tNetErrors[];

struct SBipedJoint
{
	int32       m_idxParent;
	const char* m_strJointName;
	QuatT       m_DefRelativePose;
	QuatT       m_DefAbsolutePose;

	QuatT       m_RelativePose;
	QuatT       m_AbsolutePose;

	ILINE SBipedJoint()
	{
		m_idxParent = -1;
		m_strJointName = 0;
		m_DefRelativePose.SetIdentity();
		m_DefAbsolutePose.SetIdentity();

		m_RelativePose.SetIdentity();
		m_AbsolutePose.SetIdentity();
	};

	ILINE SBipedJoint(const char* bname)
	{
		m_strJointName = bname;
	};
};

SBipedJoint BipedSkeleton[] =
{

	//--------------> BoneNr.0000
	SBipedJoint("Bip01"),
	//--------------> BoneNr.0001
	SBipedJoint("Bip01 Pelvis"),
	//--------------> BoneNr.0002
	SBipedJoint("Bip01 Spine"),
	//--------------> BoneNr.0003
	SBipedJoint("Bip01 Spine1"),
	//--------------> BoneNr.0004
	SBipedJoint("Bip01 Spine2"),
	//--------------> BoneNr.0005
	SBipedJoint("Bip01 Spine3"),
	//--------------> BoneNr.0006
	SBipedJoint("Bip01 Neck"),
	//--------------> BoneNr.0007
	SBipedJoint("Bip01 Neck1"),
	//--------------> BoneNr.0008
	SBipedJoint("Bip01 Head"),

	//--------------> BoneNr.0009
	SBipedJoint("Bip01 L Clavicle"),
	//--------------> BoneNr.0010
	SBipedJoint("Bip01 L UpperArm"),
	//--------------> BoneNr.0011
	SBipedJoint("Bip01 L Forearm"),
	//--------------> BoneNr.0012
	SBipedJoint("Bip01 L Hand"),

	//--------------> BoneNr.0013
	SBipedJoint("Bip01 R Clavicle"),
	//--------------> BoneNr.0014
	SBipedJoint("Bip01 R UpperArm"),
	//--------------> BoneNr.0015
	SBipedJoint("Bip01 R Forearm"),
	//--------------> BoneNr.0016
	SBipedJoint("Bip01 R Hand"),

	//--------------> BoneNr.0017
	SBipedJoint("Bip01 L Thigh"),
	//--------------> BoneNr.0018
	SBipedJoint("Bip01 L Calf"),
	//--------------> BoneNr.0019
	SBipedJoint("Bip01 L Foot"),
	//--------------> BoneNr.0020
	SBipedJoint("Bip01 L Toe0"),

	//--------------> BoneNr.0021
	SBipedJoint("Bip01 R Thigh"),
	//--------------> BoneNr.0022
	SBipedJoint("Bip01 R Calf"),
	//--------------> BoneNr.0023
	SBipedJoint("Bip01 R Foot"),
	//--------------> BoneNr.0024
	SBipedJoint("Bip01 R Toe0"),

	//--------------> BoneNr.0025
	SBipedJoint("Bip01 L Toe1"),
	//--------------> BoneNr.0026
	SBipedJoint("Bip01 R Toe1"),

};

uint32 numJoints = sizeof(BipedSkeleton) / sizeof(SBipedJoint);

//////////////////////////////////////////////////////////////////////////
struct SViconJoint
{
	SViconJoint(const char* szVicon, const char* szBip, f32 six, f32 siy, f32 siz, int32 nParent)
	{
		if (szVicon && szBip)
		{
			sViconName = string(szVicon);
			sBipName = string(szBip);
		}
		si_x = six;
		si_y = siy;
		si_z = siz;
		m_ParentIdx = nParent;
	}
	string sViconName;
	string sBipName;
	f32    si_x;
	f32    si_y;
	f32    si_z;
	int32  m_ParentIdx;
	Vec3   m_vBonePos;
};

SViconJoint ViconSkeleton[] =
{
	SViconJoint("Pelvis",    "Bip01 Pelvis",     3,  1,  2,  -1), // 0
	SViconJoint("Thorax",    "Bip01 Spine1",     3,  1,  2,  0),  // 1
	SViconJoint("Spine",     "Bip01 Spine3",     3,  1,  2,  2),  // 2
	SViconJoint("Head",      "Bip01 Head",       3,  1,  2,  3),  // 3

	SViconJoint("Lclavicle", "Bip01 L Clavicle", 2,  -1, 3,  0),  // 4
	SViconJoint("Lhumerus",  "Bip01 L UpperArm", -3, -1, 2,  4),  // 5
	SViconJoint("Lradius",   "Bip01 L Forearm",  -3, -1, 2,  5),  // 6
	SViconJoint("Lhand",     "Bip01 L Hand",     -3, -2, -1, 6),  // 7

	SViconJoint("Rclavicle", "Bip01 R Clavicle", -2, -1, -3, 0),  // 8
	SViconJoint("Rhumerus",  "Bip01 R UpperArm", -3, -1, 2,  8),  // 9
	SViconJoint("Rradius",   "Bip01 R Forearm",  -3, -1, 2,  9),  //10
	SViconJoint("Rhand",     "Bip01 R Hand",     -3, 2,  1,  10), //11

	SViconJoint("Lfemur",    "Bip01 L Thigh",    -3, 1,  -2, 0),  //12
	SViconJoint("Ltibia",    "Bip01 L Calf",     -3, 1,  -2, 12), //13
	SViconJoint("Lfoot",     "Bip01 L Foot",     -3, 1,  -2, 13), //14
	SViconJoint("Ltoes",     "Bip01 L Toe0",     1,  3,  -2, 14), //15

	SViconJoint("Rfemur",    "Bip01 R Thigh",    -3, 1,  -2, 0),  //16
	SViconJoint("Rtibia",    "Bip01 R Calf",     -3, 1,  -2, 16), //17
	SViconJoint("Rfoot",     "Bip01 R Foot",     -3, 1,  -2, 17), //18
	SViconJoint("Rtoes",     "Bip01 R Toe0",     1,  3,  -2, 18), //19

	SViconJoint(NULL,        NULL,               0,  0,  0,  -1),
};

//////////////////////////////////////////////////////////////////////////
CViconClient::CViconClient()
{
	m_bConnected = false;
	m_SocketHandle = INVALID_SOCKET;
}

//////////////////////////////////////////////////////////////////////////
CViconClient::~CViconClient()
{
	Disconnect();
}

//-----------------------------------------------------------------------------
//	The recv call may return with a half-full buffer.
//	revieve keeps going until the buffer is actually full.

bool recieve(SOCKET Socket, char* pBuffer, int BufferSize)
{
	char* p = pBuffer;
	char* e = pBuffer + BufferSize;

	int result;

	while (p != e)
	{
		result = recv(Socket, p, e - p, 0);

		if (result == SOCKET_ERROR)
			return false;

		p += result;
	}

	return true;
}

//	There are also some helpers to make the code a little less ugly.

bool recieve(SOCKET Socket, long int& Val)
{
	return recieve(Socket, (char*) &Val, sizeof(Val));
}

bool recieve(SOCKET Socket, unsigned long int& Val)
{
	return recieve(Socket, (char*) &Val, sizeof(Val));
}

bool recieve(SOCKET Socket, double& Val)
{
	return recieve(Socket, (char*) &Val, sizeof(Val));
}

//////////////////////////////////////////////////////////////////////////
std::vector<string>::const_iterator iFind(const std::vector<string>& vi, const string& sElem)
{
	for (std::vector<string>::const_iterator i = vi.begin(); i != vi.end(); i++)
	{
		const string& sTest = (*i);
		if (_stricmp(sElem.c_str(), sTest.c_str()) == 0)
			return (i);
	}
	return (vi.end());
} // iFind

//////////////////////////////////////////////////////////////////////////
void CViconClient::Disconnect()
{
	if (m_bConnected)
	{
		if (closesocket(m_SocketHandle) == SOCKET_ERROR)
		{
			gEnv->pLog->LogError("<Vicon Client> Failed to close Socket");
			PrintErrorDescription(WSAGetLastError());
		}
	}

	int k = 0;
	for (std::vector<EntityId>::iterator i = m_lstEntities.begin(); i != m_lstEntities.end(); i++, k++)
	{
		IEntity* pEnt = gEnv->pEntitySystem->GetEntity(*i);
		if (!pEnt)
			continue;

		ICharacterInstance* pCharacter = pEnt->GetCharacter(0);
		if (pCharacter && pCharacter->GetISkeletonPose())
		{
			// TODO: This call was removed since now we can use the common
			// PoseModifier interface. Implement a PoseModifier if the
			// functionality is needed back.
			//			pCharacter->GetISkeletonPose()->SetPostProcessQuat(0, IDENTITY);
			pCharacter->GetISkeletonPose()->SetPostProcessCallback(NULL, NULL);
		}

		pEnt->SetRotation(m_lstEntitiesOriginRot[k]);
		pEnt->SetPos(m_lstEntitiesOriginPos[k]);
	} //i

	m_lstEntities.clear();
	m_lstEntityNames.clear();
	m_lstEntitiesOriginPos.clear();
	m_lstEntitiesOriginRot.clear();

	m_BodyChannels.clear();
	m_MarkerChannels.clear();

	m_markerPositions.clear();
	m_bodyPositions.clear();

	m_data.clear();
	m_info.clear();

	m_bConnected = false;
}

//////////////////////////////////////////////////////////////////////////
int Vicon_PostProcessCallback(ICharacterInstance* pInstance, void* pPlayer)
{
	//process bones specific stuff (IK, torso rotation, etc)
	gEditor->m_ViconClient->ExternalPostProcessing(pInstance);
	return 1;
}

//////////////////////////////////////////////////////////////////////////
int32 CViconClient::GetIDbyName(const char* strJointName)
{
	int32 idx = -1;
	for (uint32 i = 0; i < numJoints; i++)
	{
		uint32 res = stricmp(BipedSkeleton[i].m_strJointName, strJointName);
		if (res == 0)
		{
			idx = i;
			break;
		}
	}
	return idx;
}

//////////////////////////////////////////////////////////////////////////
void CViconClient::ExternalPostProcessing(ICharacterInstance* pInstance)
{
	if (!m_bConnected)
		return;

	IRenderAuxGeom* g_pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	g_pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);

	string sActorName;
	Vec3 vOrigin(0, 0, 0);
	IPhysicalEntity* pPhys = pInstance->GetISkeletonPose()->GetCharacterPhysics();
	if (pPhys->GetForeignData(PHYS_FOREIGN_ID_ENTITY))
	{
		IEntity* pEntity = (IEntity*)pPhys->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
		vOrigin = pEntity->GetPos();
		sActorName = string(pEntity->GetName());
	}
	int nNameLen = strlen(sActorName);

	//g_pAuxGeom->DrawSphere(vOrigin,0.15f,ColorB(255,0,0));

	//////////////////////////////////////////////////////////////////////////
	if (pInstance)
	{

		Vec3 varrBPos[0x100];
		//ISkeletonPose* pISkeletonPose = pInstance->GetISkeletonPose();
		IDefaultSkeleton& rIDefaultSkeleton = pInstance->GetIDefaultSkeleton();

		// TODO: This call was removed since now we can use the common
		// PoseModifier interface. Implement a PoseModifier if the
		// functionality is needed back.
		//		pISkeletonPose->SetPostProcessQuat(0, IDENTITY);

		std::vector<BodyChannel>::iterator iBody;
		std::vector<BodyData>::iterator iBodyData;

		//initialize the Biped Skeleton
		uint32 IsInitialized = 0;
		for (iBody = m_BodyChannels.begin(), iBodyData = m_bodyPositions.begin(); iBody != m_BodyChannels.end(); iBody++, iBodyData++)
		{
			IsInitialized = 0;
			if (iBodyData->m_IsInitialized == 0)
				break;  //not initialized

			IsInitialized = 1;
			if (strncmp(sActorName.c_str(), iBody->Name.c_str(), nNameLen) != 0)
				continue;

			const char* joint_name = iBody->m_BipName.c_str();
			int32 idx = GetIDbyName(joint_name);
			assert(idx > -1); //Bad stuff. Must likely it will crash now!
			varrBPos[idx] = iBodyData->Joint.t;

			//get the absolute joint-quaternion from Vicon
			Quat q = iBodyData->Joint.q;  //Quat(iBodyData->QW,iBodyData->QX,iBodyData->QY,iBodyData->QZ);
			//..and now convert it into the Biped-format
			Vec3 axisX = q.GetColumn(fabsf(iBody->m_x) - 1) * sgn(iBody->m_x);
			Vec3 axisY = q.GetColumn(fabsf(iBody->m_y) - 1) * sgn(iBody->m_y);
			Vec3 axisZ = q.GetColumn(fabsf(iBody->m_z) - 1) * sgn(iBody->m_z);
			Matrix33 m33;
			m33.SetFromVectors(axisX, axisY, axisZ);

			int32 ortho = m33.IsOrthonormalRH(0.01f);
			assert(ortho);   //Vicon sends bad values the first 2 frames. MarcoC, please investigate this

			if (ortho)
				BipedSkeleton[idx].m_AbsolutePose.q = Quat(m33);

			//additive skeleton
			//the rest is just for debugging
			Vec3 vBPos = varrBPos[idx] + vOrigin;
			int32 idxRFoot = GetIDbyName("Bip01 R Foot");
			int32 idxLFoot = GetIDbyName("Bip01 L Foot");
			if (idx == idxRFoot || idx == idxLFoot)
			{
				Vec3 axis = BipedSkeleton[idx].m_AbsolutePose.q.GetColumn2();
				BipedSkeleton[idx].m_AbsolutePose.q = Quat::CreateRotationAA(0.6f, axis) * BipedSkeleton[idx].m_AbsolutePose.q;
				/*
				        Vec3 zaxis0 = BipedSkeleton[idx].m_AbsolutePose.q.GetColumn2();
				        Vec3 zaxis1 = Vec3(zaxis0.x,zaxis0.y,0); zaxis1.Normalize();
				        Quat rel=Quat::CreateRotationV0V1(zaxis0,zaxis1);
				        BipedSkeleton[idx].m_AbsolutePose.q=rel*BipedSkeleton[idx].m_AbsolutePose.q;
				 */

				Vec3 axisX = BipedSkeleton[idx].m_AbsolutePose.q.GetColumn0();
				Vec3 axisY = BipedSkeleton[idx].m_AbsolutePose.q.GetColumn1();
				Vec3 axisZ = BipedSkeleton[idx].m_AbsolutePose.q.GetColumn2();
				g_pAuxGeom->DrawLine(vBPos, RGBA8(0xff, 0x00, 0x00, 0x00), vBPos + axisX, RGBA8(0xff, 0x00, 0x00, 0x00));
				g_pAuxGeom->DrawLine(vBPos, RGBA8(0x00, 0xff, 0x00, 0x00), vBPos + axisY, RGBA8(0x00, 0xff, 0x00, 0x00));
				g_pAuxGeom->DrawLine(vBPos, RGBA8(0x00, 0x00, 0xff, 0x00), vBPos + axisZ, RGBA8(0x00, 0x00, 0xff, 0x00));
				IRenderAuxText::DrawLabel(vBPos, 1.0f, iBody->m_BipName.c_str());
			}
			g_pAuxGeom->DrawSphere(vBPos, 0.01f, ColorB(255, 0, 255));
			IRenderAuxText::DrawLabel(varrBPos[idx], 1.0f, iBody->Name.c_str());

			//if (ortho==0)
			//	IRenderAuxText::DrawLabel(vBPos,1.0f,iBody->m_BipName.c_str());

			//---------------------------------------------------

			uint32 numViconJoints = sizeof(ViconSkeleton) / sizeof(SViconJoint) - 1;
			const char* VicName1 = iBody->Name.c_str();
			VicName1 = strstr(VicName1, ":") + 1; // skip useless stuff
			for (uint32 i = 0; i < numViconJoints; i++)
			{
				const char* VicName2 = ViconSkeleton[i].sViconName.c_str();
				int32 eg = stricmp(VicName1, VicName2);
				if (eg)
					continue;
				ViconSkeleton[i].m_vBonePos = varrBPos[idx];
			}

		}

		//----------------------------------------------------------------------------------------

		if (IsInitialized)
		{
			//not all primary joints are in the Vicon-skeleton. I will initialize them by interpolating the nearest joints
			int32 a_idxSrc1 = GetIDbyName("Bip01 Pelvis");
			int32 a_idxSrc2 = GetIDbyName("Bip01 Spine1");

			int32 b_idxSrc1 = GetIDbyName("Bip01 Spine1");
			int32 b_idxSrc2 = GetIDbyName("Bip01 Spine3");

			int32 c_idxSrc1 = GetIDbyName("Bip01 Spine3");
			int32 c_idxSrc2 = GetIDbyName("Bip01 Head");

			int32 idxSpine0 = GetIDbyName("Bip01 Spine");
			int32 idxSpine2 = GetIDbyName("Bip01 Spine2");
			int32 idxSpine3 = GetIDbyName("Bip01 Spine3");
			int32 idxNeck = GetIDbyName("Bip01 Neck");
			int32 idxNeck1 = GetIDbyName("Bip01 Neck1");

			int32 vidxNeck1 = rIDefaultSkeleton.GetJointIDByName("Bip01 Neck1");

			BipedSkeleton[idxSpine0].m_AbsolutePose.q.SetNlerp(BipedSkeleton[a_idxSrc1].m_AbsolutePose.q, BipedSkeleton[a_idxSrc2].m_AbsolutePose.q, 0.50f);

			BipedSkeleton[idxSpine2].m_AbsolutePose.q.SetNlerp(BipedSkeleton[b_idxSrc1].m_AbsolutePose.q, BipedSkeleton[b_idxSrc2].m_AbsolutePose.q, 0.50f);

			if (vidxNeck1 > 0)
			{
				BipedSkeleton[idxNeck].m_AbsolutePose.q.SetNlerp(BipedSkeleton[c_idxSrc1].m_AbsolutePose.q, BipedSkeleton[c_idxSrc2].m_AbsolutePose.q, 0.66f);
				BipedSkeleton[idxNeck1].m_AbsolutePose.q.SetNlerp(BipedSkeleton[c_idxSrc1].m_AbsolutePose.q, BipedSkeleton[c_idxSrc2].m_AbsolutePose.q, 0.33f);
			}
			else
			{
				BipedSkeleton[idxNeck].m_AbsolutePose.q.SetNlerp(BipedSkeleton[c_idxSrc1].m_AbsolutePose.q, BipedSkeleton[c_idxSrc2].m_AbsolutePose.q, 0.50f);
			}

			int32 idxLFoot = GetIDbyName("Bip01 L Foot");
			assert(idxLFoot > -1);
			int32 idxLToe0 = GetIDbyName("Bip01 L Toe0");
			assert(idxLToe0 > -1);
			int32 idxLToe1 = GetIDbyName("Bip01 L Toe1");
			assert(idxLToe1 > -1);
			BipedSkeleton[idxLToe1].m_RelativePose = BipedSkeleton[idxLToe1].m_DefRelativePose;
			//	BipedSkeleton[idxLToe1].m_AbsolutePose = BipedSkeleton[idxLFoot].m_AbsolutePose*BipedSkeleton[idxLToe1].m_RelativePose;

			int32 idxRFoot = GetIDbyName("Bip01 R Foot");
			assert(idxRFoot > -1);
			int32 idxRToe0 = GetIDbyName("Bip01 R Toe0");
			assert(idxRToe0 > -1);
			int32 idxRToe1 = GetIDbyName("Bip01 R Toe1");
			assert(idxRToe1 > -1);
			BipedSkeleton[idxRToe1].m_RelativePose = BipedSkeleton[idxRToe1].m_DefRelativePose;
			//	BipedSkeleton[idxRToe1].m_AbsolutePose = BipedSkeleton[idxRFoot].m_AbsolutePose*BipedSkeleton[idxRToe1].m_RelativePose;

			//calculate the relative quaternion
			BipedSkeleton[0].m_RelativePose.q = BipedSkeleton[0].m_AbsolutePose.q;
			for (uint32 i = 1; i < numJoints; i++)
			{
				int32 p = BipedSkeleton[i].m_idxParent;
				if (p > -1) //maybe some bones like Neck1 do not exist at all
					BipedSkeleton[i].m_RelativePose.q = !BipedSkeleton[p].m_AbsolutePose.q * BipedSkeleton[i].m_AbsolutePose.q;
			}

			//in the Pelvis we have the ground movement
			Vec3 vlt0 = ViconSkeleton[12].m_vBonePos;
			Vec3 vlt1 = ViconSkeleton[13].m_vBonePos;
			Vec3 vlt2 = ViconSkeleton[14].m_vBonePos;
			f32 LViconLength = (vlt0 - vlt1).GetLength() + (vlt1 - vlt2).GetLength();
			Vec3 vrt0 = ViconSkeleton[16].m_vBonePos;
			Vec3 vrt1 = ViconSkeleton[17].m_vBonePos;
			Vec3 vrt2 = ViconSkeleton[18].m_vBonePos;
			f32 RViconLength = (vrt0 - vrt1).GetLength() + (vrt1 - vrt2).GetLength();
			//pRenderer->D8Print("LViconLength: %f",LViconLength);
			//pRenderer->D8Print("RViconLength: %f",RViconLength);

			Vec3 blt0 = BipedSkeleton[17].m_DefAbsolutePose.t;
			Vec3 blt1 = BipedSkeleton[18].m_DefAbsolutePose.t;
			Vec3 blt2 = BipedSkeleton[19].m_DefAbsolutePose.t;
			f32 LBipedLength = (blt0 - blt1).GetLength() + (blt1 - blt2).GetLength();
			Vec3 brt0 = BipedSkeleton[21].m_DefAbsolutePose.t;
			Vec3 brt1 = BipedSkeleton[22].m_DefAbsolutePose.t;
			Vec3 brt2 = BipedSkeleton[23].m_DefAbsolutePose.t;
			f32 RBipedLength = (brt0 - brt1).GetLength() + (brt1 - brt2).GetLength();
			//pRenderer->D8Print("LBipedLength: %f",LBipedLength);
			//pRenderer->D8Print("RBipedLength: %f",RBipedLength);
			f32 scale = LBipedLength / LViconLength;
			//pRenderer->D8Print("scale: %f",scale);

			BipedSkeleton[1].m_RelativePose.t = varrBPos[1] * scale - Vec3(0, 0, 0.010f); //translation in the worlds is controlled by pelvis

			//------------------------------------------------------------------------------

			for (uint32 i = 1; i < numJoints; i++)
			{
				int32 p = BipedSkeleton[i].m_idxParent;
				if (p > -1) //maybe some bones like Neck1 do not exist at all
					BipedSkeleton[i].m_AbsolutePose = BipedSkeleton[p].m_AbsolutePose * BipedSkeleton[i].m_RelativePose;
			}

			//-------------------------------------------------------------------------------
			//---         make sure we have no Hyper Extension or Inverse Bending         ---
			//-------------------------------------------------------------------------------
			if (1)
			{
				int32 Pelvis = GetIDbyName("Bip01 Pelvis");
				assert(Pelvis > -1);

				int32 rThigh = GetIDbyName("Bip01 R Thigh");
				assert(rThigh > -1);
				int32 rCalf = GetIDbyName("Bip01 R Calf");
				assert(rCalf > -1);
				int32 rFoot = GetIDbyName("Bip01 R Foot");
				assert(rFoot > -1);
				int32 rToe0 = GetIDbyName("Bip01 R Toe0");
				assert(rToe0 > -1);
				FixInverseBending(Pelvis, rThigh, rCalf, rFoot, rToe0);

				int32 lThigh = GetIDbyName("Bip01 L Thigh");
				assert(lThigh > -1);
				int32 lCalf = GetIDbyName("Bip01 L Calf");
				assert(lCalf > -1);
				int32 lFoot = GetIDbyName("Bip01 L Foot");
				assert(lFoot > -1);
				int32 lToe0 = GetIDbyName("Bip01 L Toe0");
				assert(lToe0 > -1);
				FixInverseBending(Pelvis, lThigh, lCalf, lFoot, lToe0);
			}

			if (0)
			{
				int32 Pelvis = GetIDbyName("Bip01 Pelvis");
				assert(Pelvis > -1);

				int32 rThigh = GetIDbyName("Bip01 R Thigh");
				assert(rThigh > -1);
				int32 rCalf = GetIDbyName("Bip01 R Calf");
				assert(rCalf > -1);
				int32 rFoot = GetIDbyName("Bip01 R Foot");
				assert(rFoot > -1);
				int32 rToe0 = GetIDbyName("Bip01 R Toe0");
				assert(rToe0 > -1);
				int32 rToe1 = GetIDbyName("Bip01 R Toe1");
				assert(rToe1 > -1);

				int32 lThigh = GetIDbyName("Bip01 L Thigh");
				assert(lThigh > -1);
				int32 lCalf = GetIDbyName("Bip01 L Calf");
				assert(lCalf > -1);
				int32 lFoot = GetIDbyName("Bip01 L Foot");
				assert(lFoot > -1);
				int32 lToe0 = GetIDbyName("Bip01 L Toe0");
				assert(lToe0 > -1);
				int32 lToe1 = GetIDbyName("Bip01 L Toe1");
				assert(lToe1 > -1);

				Vec3d rtoe1 = BipedSkeleton[rToe1].m_AbsolutePose.t;
				if (rtoe1.z < 0)
				{
					Vec3d goal = BipedSkeleton[rFoot].m_AbsolutePose.t;
					goal.z -= rtoe1.z;
					TwoBoneSolver(goal, Pelvis, rThigh, rCalf, rFoot, rToe0, rToe1);
				}

				Vec3d ltoe1 = BipedSkeleton[lToe1].m_AbsolutePose.t;
				if (ltoe1.z < 0)
				{
					Vec3d goal = BipedSkeleton[lFoot].m_AbsolutePose.t;
					goal.z -= ltoe1.z;
					TwoBoneSolver(goal, Pelvis, lThigh, lCalf, lFoot, lToe0, lToe1);
				}
			}

			float fC1[4] = { 1, 0, 0, 1 };

			{
				int32 RCalf = GetIDbyName("Bip01 R Calf");
				assert(RCalf > -1);
				Vec3 axisZ = BipedSkeleton[RCalf].m_AbsolutePose.q.GetColumn2();
				Vec3 v0 = BipedSkeleton[RCalf - 1].m_AbsolutePose.t - BipedSkeleton[RCalf].m_AbsolutePose.t;
				Vec3 v1 = BipedSkeleton[RCalf + 1].m_AbsolutePose.t - BipedSkeleton[RCalf].m_AbsolutePose.t;
				Vec3 cross = v0 % v1;
				f32 dot = cross | axisZ;
				if (dot <= 0)
					IRenderAuxText::Draw2dLabel(1, 200, 1.6f, fC1, false, "Inverse Bending on right leg");
			}

			{
				int32 LCalf = GetIDbyName("Bip01 L Calf");
				assert(LCalf > -1);
				Vec3 axisZ = BipedSkeleton[LCalf].m_AbsolutePose.q.GetColumn2();
				Vec3 v0 = BipedSkeleton[LCalf - 1].m_AbsolutePose.t - BipedSkeleton[LCalf].m_AbsolutePose.t;
				Vec3 v1 = BipedSkeleton[LCalf + 1].m_AbsolutePose.t - BipedSkeleton[LCalf].m_AbsolutePose.t;
				Vec3 cross = v0 % v1;
				f32 dot = cross | axisZ;
				if (dot <= 0)
					IRenderAuxText::Draw2dLabel(1, 220, 1.6f, fC1, false, "Inverse Bending on left leg");
			}

			//-------------------------------------------------------------------------------
			//---     draw the optimized Biped Skeleton                                   ---
			//-------------------------------------------------------------------------------
			for (uint32 i = 1; i < numJoints; i++)
			{
				int32 p = BipedSkeleton[i].m_idxParent;
				if (p < 0)
					continue;
				Vec3 vChild = BipedSkeleton[i].m_AbsolutePose.t + vOrigin;
				Vec3 vParent = BipedSkeleton[p].m_AbsolutePose.t + vOrigin;
				g_pAuxGeom->DrawLine(vChild, RGBA8(0x00, 0x00, 0x00, 0x00), vParent, RGBA8(0xff, 0xff, 0xff, 0x00));
				g_pAuxGeom->DrawSphere(vChild, 0.01f, ColorB(0, 255, 0));
			}

			Vec3 ltoe1 = BipedSkeleton[idxLToe1].m_AbsolutePose.t;
			Vec3 rtoe1 = BipedSkeleton[idxRToe1].m_AbsolutePose.t;
			IRenderAuxText::Draw2dLabel(1, 300, 1.6f, fC1, false, "LToe1: %f %f %f", ltoe1.x, ltoe1.y, ltoe1.z);
			IRenderAuxText::Draw2dLabel(1, 320, 1.6f, fC1, false, "RToe1: %f %f %f", rtoe1.x, rtoe1.y, rtoe1.z);

			// TODO: This call was removed since now we can use the common
			// PoseModifier interface. Implement a PoseModifier if the
			// functionality is needed back.
			//			pISkeletonPose->SetPostProcessQuat(0, IDENTITY);
			for (int i = 0; i < numJoints; i++)
			{
				int idx = rIDefaultSkeleton.GetJointIDByName(BipedSkeleton[i].m_strJointName);
				if (idx < 0)
					continue;
				//	if (i>1)
				//		BipedSkeleton[i].m_RelativePose.t=pISkeletonPose->GetRelJointByID(idx).t; //we want to keep the joint-translation of the original model
				//				pISkeletonPose->SetPostProcessQuat(idx, BipedSkeleton[i].m_RelativePose);
			}

			//-------------------------------------------------------------------------------
			//---                    Debugging: draw the Vicon-skeleton                   ---
			//-------------------------------------------------------------------------------
			g_pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);
			uint32 numViconJoints = sizeof(ViconSkeleton) / sizeof(SViconJoint) - 1;
			for (uint32 i = 1; i < numViconJoints; i++)
			{
				int32 p = ViconSkeleton[i].m_ParentIdx;
				if (p < 0)
					continue;
				Vec3 vChild = ViconSkeleton[i].m_vBonePos + vOrigin;
				Vec3 vParent = ViconSkeleton[p].m_vBonePos + vOrigin;
				g_pAuxGeom->DrawLine(vChild, RGBA8(0x00, 0x00, 0x00, 0x00), vParent, RGBA8(0xff, 0xff, 0xff, 0x00));
			}
		}

	}
}

//----------------------------------------------------------------------------------------------
//---              make sure we have no Hyper Extension or Inverse Bending                   ---
//----------------------------------------------------------------------------------------------
void CViconClient::FixInverseBending(int32 Pelvis, int32 Thigh, int32 Calf, int32 Foot, int32 Toe0)
{
	QuatT wThigh = BipedSkeleton[Thigh].m_AbsolutePose;
	QuatT wCalf = BipedSkeleton[Calf].m_AbsolutePose;
	QuatT wFoot = BipedSkeleton[Foot].m_AbsolutePose;
	QuatT wToe0 = BipedSkeleton[Toe0].m_AbsolutePose;

	Vec3 axisY = BipedSkeleton[Calf].m_AbsolutePose.q.GetColumn1();
	Vec3 axisZ = BipedSkeleton[Calf].m_AbsolutePose.q.GetColumn2();
	for (uint32 t = 0; t < 200; t++)
	{
		Vec3 v0 = wThigh.t - wCalf.t;
		Vec3 v1 = wFoot.t - wCalf.t;
		Vec3 cross = v0 % v1;
		f32 dot = cross | axisZ;
		if (dot > 0.01f)
			break;
		wCalf.t += axisY * 0.001f;
		//pRenderer->Draw_Lineseg(t,RGBA8(0x00,0x00,0x00,0x00), t+cross*10,RGBA8(0xff,0xff,0xff,0x00) );
	}

	Vec3 v0 = (BipedSkeleton[Thigh].m_AbsolutePose.t - BipedSkeleton[Calf].m_AbsolutePose.t).GetNormalized();
	Vec3 v1 = (wThigh.t - wCalf.t).GetNormalized();
	Quat rel = Quat::CreateRotationV0V1(v0, v1);

	BipedSkeleton[Thigh].m_AbsolutePose.q = rel * BipedSkeleton[Thigh].m_AbsolutePose.q;
	BipedSkeleton[Thigh].m_RelativePose = BipedSkeleton[Pelvis].m_AbsolutePose.GetInverted() * BipedSkeleton[Thigh].m_AbsolutePose;

	BipedSkeleton[Calf].m_AbsolutePose = BipedSkeleton[Thigh].m_AbsolutePose * BipedSkeleton[Calf].m_RelativePose;
	BipedSkeleton[Foot].m_AbsolutePose = BipedSkeleton[Calf].m_AbsolutePose * BipedSkeleton[Foot].m_RelativePose;
	BipedSkeleton[Toe0].m_AbsolutePose = BipedSkeleton[Foot].m_AbsolutePose * BipedSkeleton[Toe0].m_RelativePose;

	Vec3 x1 = (wCalf.t - wFoot.t).GetNormalized();
	Vec3 x0 = (BipedSkeleton[Calf].m_AbsolutePose.t - BipedSkeleton[Foot].m_AbsolutePose.t).GetNormalized();
	Quat xrel = Quat::CreateRotationV0V1(x0, x1);

	BipedSkeleton[Calf].m_AbsolutePose.q = xrel * BipedSkeleton[Calf].m_AbsolutePose.q;
	BipedSkeleton[Calf].m_RelativePose = BipedSkeleton[Thigh].m_AbsolutePose.GetInverted() * BipedSkeleton[Calf].m_AbsolutePose;

	BipedSkeleton[Foot].m_AbsolutePose = BipedSkeleton[Calf].m_AbsolutePose * BipedSkeleton[Foot].m_RelativePose;
	BipedSkeleton[Toe0].m_AbsolutePose = BipedSkeleton[Foot].m_AbsolutePose * BipedSkeleton[Toe0].m_RelativePose;
}

//----------------------------------------------------------------------------------------------
//---              make sure we have no Hyper Extension or Inverse Bending                   ---
//----------------------------------------------------------------------------------------------
void CViconClient::TwoBoneSolver(Vec3d goal, int32 Pelvis, int32 Thigh, int32 Calf, int32 Foot, int32 Toe0, int32 Toe1)
{
	Vec3d t0 = BipedSkeleton[Thigh].m_AbsolutePose.t;
	Vec3d t1 = BipedSkeleton[Calf].m_AbsolutePose.t;
	Vec3d t2 = BipedSkeleton[Foot].m_AbsolutePose.t;

	Vec3d dif = goal - t2;
	if (dif.GetLength() < 0.00001f)
		return; //end-effector and new goal are very close

	Vec3d a = t1 - t0;
	Vec3d b = t2 - t1;
	Vec3d c = goal - t0;
	Vec3 cross = a % b;
	if ((cross | cross) < 0.00001) return;

	f64 alen = a.GetLength();
	f64 blen = b.GetLength();
	f64 clen = c.GetLength();
	if (alen * clen < 0.0001)
		return;

	a /= alen;
	c /= clen;

	f64 cos0 = clamp_tpl((alen * alen + clen * clen - blen * blen) / (2 * alen * clen), -1.0, +1.0);
	f64 sin0 = sqrt(max(1.0 - cos0 * cos0, 0.0));
	a = a.GetRotated((a % b).GetNormalized(), cos0, sin0);
	assert((fabs_tpl(1 - (a | a))) < 0.001);   //check if unit-vector

	{
		Vec3d h = (a + c).GetNormalized();
		Vec3d axis = a % c;
		f64 sine = axis.GetLength();
		axis /= sine;
		Vec3d trans = t0 - t0.GetRotated(axis, a | c, sine);
		QuatT qt(Quat(f32(a | h), axis * (a % h).GetLength()), trans);

		BipedSkeleton[Thigh].m_AbsolutePose = qt * BipedSkeleton[Thigh].m_AbsolutePose;
		BipedSkeleton[Calf].m_AbsolutePose = qt * BipedSkeleton[Calf].m_AbsolutePose;
		BipedSkeleton[Foot].m_AbsolutePose = qt * BipedSkeleton[Foot].m_AbsolutePose;
	}

	//---------------------------------------------------

	{
		t1 = BipedSkeleton[Calf].m_AbsolutePose.t;
		t2 = BipedSkeleton[Foot].m_AbsolutePose.t;
		c = (goal - t1).GetNormalized();
		b = (t2 - t1) / blen;

		Vec3d h = (b + c).GetNormalized();
		Vec3d axis = b % c;
		f64 sine = axis.GetLength();
		axis /= sine;
		Vec3d trans = t1 - t1.GetRotated(axis, b | c, sine);
		QuatT qt2(Quat(f32(b | h), axis * (b % h).GetLength()), trans);
		BipedSkeleton[Calf].m_AbsolutePose = qt2 * BipedSkeleton[Calf].m_AbsolutePose;
	}

	BipedSkeleton[Thigh].m_RelativePose.q = !BipedSkeleton[Pelvis].m_AbsolutePose.q * BipedSkeleton[Thigh].m_AbsolutePose.q;
	BipedSkeleton[Calf].m_RelativePose.q = !BipedSkeleton[Thigh].m_AbsolutePose.q * BipedSkeleton[Calf].m_AbsolutePose.q;
	//BipedSkeleton[Foot ].m_RelativePose.q = !BipedSkeleton[Calf ].m_AbsolutePose.q  * BipedSkeleton[Foot ].m_AbsolutePose.q;
	//BipedSkeleton[Toe0 ].m_RelativePose.q = !BipedSkeleton[Foot ].m_AbsolutePose.q  * BipedSkeleton[Toe0 ].m_AbsolutePose.q;

	BipedSkeleton[Calf].m_AbsolutePose = BipedSkeleton[Thigh].m_AbsolutePose * BipedSkeleton[Calf].m_RelativePose;
	BipedSkeleton[Foot].m_AbsolutePose = BipedSkeleton[Calf].m_AbsolutePose * BipedSkeleton[Foot].m_RelativePose;
	BipedSkeleton[Toe0].m_AbsolutePose = BipedSkeleton[Foot].m_AbsolutePose * BipedSkeleton[Toe0].m_RelativePose;
	BipedSkeleton[Toe1].m_AbsolutePose = BipedSkeleton[Foot].m_AbsolutePose * BipedSkeleton[Toe1].m_RelativePose;
	uint32 sds = 0;
}

//////////////////////////////////////////////////////////////////////////
MarkerData* CViconClient::FindMarker(const char* szName)
{
	std::vector<MarkerChannel>::const_iterator iMarker;
	int nPos = 0;
	//std::vector< MarkerData >::iterator iMarkerData;
	for (iMarker = m_MarkerChannels.begin();
	     iMarker != m_MarkerChannels.end(); ++iMarker, nPos++)
	{
		const char* szSrc = iMarker->Name.c_str();
		if (stricmp(szSrc, szName) == 0)
		{
			MarkerData* pMarker = &m_markerPositions[nPos];
			return (pMarker);
		}
	}

	return (NULL);
}

//////////////////////////////////////////////////////////////////////////
Vec3 CViconClient::GetTransformedMarkerPos(const char* szName)
{
	MarkerData* pMarker = FindMarker(szName);
	if (!pMarker)
		return (Vec3(0, 0, 0));
	return (pMarker->TransformData());
}

//////////////////////////////////////////////////////////////////////////
bool CViconClient::Connect(IConsoleCmdArgs* pArgs)
{
	if (m_bConnected)
	{
		Disconnect();
	}

	m_fLastTimeStamp = -1;

	m_nFrameCounter = 0;
	vMid.Set(0, 0, 0);

	gEnv->pLog->Log("<Vicon Client> Connecting to Vicon server...");

	//- Initialisation - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	//  Windows-specific initialisation.

	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 0);
	if (WSAStartup(wVersionRequested, &wsaData) != 0)
	{
		gEnv->pLog->LogError("<Vicon Client> Socket Initialization Error");
		PrintErrorDescription(WSAGetLastError());
		return false;
	}

	// Create Socket

	m_SocketHandle = INVALID_SOCKET;

	struct protoent* pProtocolInfoEntry;
	char* protocol;
	long int type;

	protocol = "tcp";
	type = SOCK_STREAM;

	pProtocolInfoEntry = getprotobyname(protocol);
	assert(pProtocolInfoEntry);

	if (pProtocolInfoEntry)
		m_SocketHandle = socket(PF_INET, type, pProtocolInfoEntry->p_proto);

	if (m_SocketHandle == INVALID_SOCKET)
	{
		gEnv->pLog->LogError("<Vicon Client> Socket Creation Error");
		PrintErrorDescription(WSAGetLastError());
		return false;
	}

	//string sAddress=string("127.0.0.1");
	string sAddress = string("192.168.10.96");

	if (pArgs && pArgs->GetArgCount() > 1)
	{
		sAddress.clear();
		for (int i = 1; i < pArgs->GetArgCount(); ++i)
			//sAddress += string(" ") + pArgs->GetArg(i);
			sAddress += pArgs->GetArg(i);
	}

	gEnv->pLog->Log("<Vicon Client> Connecting to address: %s", sAddress.c_str());

	/*
	   //	Find Endpoint
	   if(argc != 2)
	   {
	   std::cout << "Usage: " << argv[0] << " <address>" << std::endl;
	   return 1;
	   }
	 */

	struct hostent* pHostInfoEntry;
	struct sockaddr_in Endpoint;

	static const int port = 800;

	memset(&Endpoint, 0, sizeof(Endpoint));
	Endpoint.sin_family = AF_INET;
	Endpoint.sin_port = htons(port);

	pHostInfoEntry = gethostbyname(sAddress.c_str());

	if (pHostInfoEntry)
		memcpy(&Endpoint.sin_addr, pHostInfoEntry->h_addr,
		       pHostInfoEntry->h_length);
	else
		Endpoint.sin_addr.s_addr = inet_addr(sAddress.c_str());

	if (Endpoint.sin_addr.s_addr == INADDR_NONE)
	{
		gEnv->pLog->LogError("<Vicon Client> Bad Address");
		PrintErrorDescription(WSAGetLastError());
		return false;
	}

	//	Create Socket

	int result = connect(m_SocketHandle, (struct sockaddr*) &Endpoint, sizeof(Endpoint));

	if (result == SOCKET_ERROR)
	{
		uint32 nError = WSAGetLastError();
		gEnv->pLog->LogError("<Vicon Client> Failed to create socket (%d)", nError);
		PrintErrorDescription(nError);
		return false;
	}

	//	A connection with the Vicon Realtime should now be open.
	gEnv->pLog->Log("<Vicon Client> connected to Vicon server...requesting m_info");
	m_bConnected = true;

	//////////////////////////////////////////////////////////////////////////
	//- Get Info - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	//	Request the channel information

	const int bufferSize = 2040;
	char buff[bufferSize];
	char* pBuff;

	pBuff = buff;

	*((long int*) pBuff) = ClientCodes::EInfo;
	pBuff += sizeof(long int);
	*((long int*) pBuff) = ClientCodes::ERequest;
	pBuff += sizeof(long int);

	if (send(m_SocketHandle, buff, pBuff - buff, 0) == SOCKET_ERROR)
	{
		gEnv->pLog->LogError("<Vicon Client> Error requesting");
		PrintErrorDescription(WSAGetLastError());
		Disconnect();
		return false;
	}

	long int packet;
	//long int type;

	if (!recieve(m_SocketHandle, packet))
	{
		gEnv->pLog->LogError("<Vicon Client> Error recieving packet");
		PrintErrorDescription(WSAGetLastError());
		Disconnect();
		return false;
	}

	if (!recieve(m_SocketHandle, type))
	{
		gEnv->pLog->LogError("<Vicon Client> Error recieving type");
		PrintErrorDescription(WSAGetLastError());
		Disconnect();
		return false;
	}

	if (type != ClientCodes::EReply)
	{
		gEnv->pLog->LogError("<Vicon Client> Bad packet");
		Disconnect();
		return false;
	}

	if (packet != ClientCodes::EInfo)
	{
		gEnv->pLog->LogError("<Vicon Client> Bad Reply type");
		Disconnect();
		return false;
	}

	long int size;

	if (!recieve(m_SocketHandle, size))
	{
		gEnv->pLog->LogError("<Vicon Client> Error recieving size");
		PrintErrorDescription(WSAGetLastError());
		Disconnect();
		return false;
	}

	m_info.resize(size);

	std::vector<string>::iterator iInfo;

	for (iInfo = m_info.begin(); iInfo != m_info.end(); iInfo++)
	{
		long int s;
		char c[255];
		char* p = c;

		if (!recieve(m_SocketHandle, s))
		{
			gEnv->pLog->LogError("<Vicon Client> Error recieving int s");
			PrintErrorDescription(WSAGetLastError());
			Disconnect();
			return false;
		}

		if (!recieve(m_SocketHandle, c, s))
		{
			gEnv->pLog->LogError("<Vicon Client> Error recieving c,s");
			PrintErrorDescription(WSAGetLastError());
			Disconnect();
			return false;
		}

		p += s;

		*p = 0;

		*iInfo = string(c);
	}

	//////////////////////////////////////////////////////////////////////////
	//- Parse Info - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	//	The m_info packets now contain the channel names.
	//	Identify the channels with the various dof's.

	for (iInfo = m_info.begin(); iInfo != m_info.end(); iInfo++)
	{
		//	Extract the channel type

		int openBrace = iInfo->find('<');

		if (openBrace == iInfo->npos)
		{
			gEnv->pLog->LogError("<Vicon Client> Bad Channel Id (<)");
			Disconnect();
			return false;
		}

		int closeBrace = iInfo->find('>');

		if (closeBrace == iInfo->npos)
		{
			gEnv->pLog->LogError("<Vicon Client> Bad Channel Id (>)");
			Disconnect();
			return false;
		}

		closeBrace++;

		string Type = iInfo->substr(openBrace, closeBrace - openBrace);

		//	Extract the Name

		string Name = iInfo->substr(0, openBrace);

		int space = Name.rfind(' ');

		if (space != Name.npos)
			Name.resize(space);

		std::vector<MarkerChannel>::iterator iMarker;
		std::vector<BodyChannel>::iterator iBody;
		std::vector<string>::const_iterator iTypes;

		iMarker = std::find(m_MarkerChannels.begin(),
		                    m_MarkerChannels.end(), Name);

		iBody = std::find(m_BodyChannels.begin(), m_BodyChannels.end(), Name);

		if (iMarker != m_MarkerChannels.end())
		{
			//	The channel is for a marker we already have.
			//iTypes = std::find(	ClientCodes::MarkerTokens.begin(), ClientCodes::MarkerTokens.end(), Type);
			iTypes = iFind(ClientCodes::MarkerTokens, Type);
			if (iTypes != ClientCodes::MarkerTokens.end())
				iMarker->operator[](iTypes - ClientCodes::MarkerTokens.begin()) = iInfo - m_info.begin();
		}
		else if (iBody != m_BodyChannels.end())
		{
			//	The channel is for a body we already have.
			//iTypes = std::find(ClientCodes::BodyTokens.begin(), ClientCodes::BodyTokens.end(), Type);
			iTypes = iFind(ClientCodes::BodyTokens, Type);
			if (iTypes != ClientCodes::BodyTokens.end())
				iBody->operator[](iTypes - ClientCodes::BodyTokens.begin()) = iInfo - m_info.begin();
		}
		else
		//if((iTypes = std::find(ClientCodes::MarkerTokens.begin(), ClientCodes::MarkerTokens.end(), Type))
		if ((iTypes = iFind(ClientCodes::MarkerTokens, Type))
		    != ClientCodes::MarkerTokens.end())
		{
			//	Its a new marker.
			m_MarkerChannels.push_back(MarkerChannel(Name));
			m_MarkerChannels.back()[iTypes - ClientCodes::MarkerTokens.begin()] = iInfo - m_info.begin();
		}
		else
		//if((iTypes = std::find(ClientCodes::BodyTokens.begin(), ClientCodes::BodyTokens.end(), Type))
		if ((iTypes = iFind(ClientCodes::BodyTokens, Type))
		    != ClientCodes::BodyTokens.end())
		{
			//	Its a new body.
			m_BodyChannels.push_back(BodyChannel(Name));
			m_BodyChannels.back()[iTypes - ClientCodes::BodyTokens.begin()] = iInfo - m_info.begin();
		}
		else if (Type == "<F>")
		{
			m_nFrameChannel = iInfo - m_info.begin();
		}
		else
		{
			//	It could be a new channel type.
		}
	}

	m_data.resize(m_info.size());

	m_markerPositions.resize(m_MarkerChannels.size());
	m_bodyPositions.resize(m_BodyChannels.size());

	std::vector<BodyChannel>::iterator iBody;

	int nCounter = 0;
	char szTemp[256];
	for (iBody = m_BodyChannels.begin(); iBody != m_BodyChannels.end(); iBody++, nCounter++)
	{
		//gEnv->pLog->Log("<Vicon Client> Body=%s T=(%f %f %f) R=(%f %f %f)",iBody->Name.c_str(),
		gEnv->pLog->Log("<Vicon Client> Body(%d)=%s", nCounter, iBody->Name.c_str());
		int i = 0;
		while (!ViconSkeleton[i].sViconName.empty())
		{
			const char* szSrc = iBody->Name.c_str();
			const char* szStart = strstr(szSrc, ":"); // skip useless stuff
			if (!szStart)
				szStart = szSrc;
			else
			{
				//gEnv->pLog->Log("<Vicon Client> %s %s %d",szSrc,szStart,szStart-szSrc);
				cry_strcpy(szTemp, szSrc, (size_t)(szStart - szSrc));
				szStart++; // skip ":"

				bool bFound = false;
				for (std::vector<string>::iterator it = m_lstEntityNames.begin(); it != m_lstEntityNames.end(); it++)
				{
					string sName = (*it);
					if (strcmp(sName.c_str(), szTemp) == 0)
					{
						bFound = true;
						break;
					}
				} //i

				if (!bFound)
				{
					IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName(szTemp);
					if (pEntity)
					{
						ICharacterInstance* pCharacter = pEntity->GetCharacter(0);
						if (pCharacter)
						{
							ISkeletonPose* pISkeletonPose = pCharacter->GetISkeletonPose();
							IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
							// animated character

							pISkeletonPose->SetPostProcessCallback(Vicon_PostProcessCallback, NULL);
							pISkeletonPose->SetForceSkeletonUpdate(1);

							//----------------------------------------------------------------------
							//----      get the skeleton directly from the model             -------
							//----------------------------------------------------------------------
							//initialize the default-skeleton
							BipedSkeleton[0].m_DefRelativePose.SetIdentity();
							BipedSkeleton[0].m_DefAbsolutePose.SetIdentity();
							for (uint32 i = 1; i < numJoints; i++)
							{
								BipedSkeleton[i].m_DefRelativePose.SetIdentity();
								BipedSkeleton[i].m_DefAbsolutePose.SetIdentity();
								BipedSkeleton[i].m_idxParent = -1;

								const char* strBipName = BipedSkeleton[i].m_strJointName;  //initialize this joint
								int32 idx = rIDefaultSkeleton.GetJointIDByName(strBipName);
								if (idx < 0)
									continue;

								BipedSkeleton[i].m_DefRelativePose = rIDefaultSkeleton.GetDefaultRelJointByID(idx);
								assert(BipedSkeleton[i].m_DefRelativePose.IsValid());
								BipedSkeleton[i].m_DefAbsolutePose = rIDefaultSkeleton.GetDefaultAbsJointByID(idx);
								assert(BipedSkeleton[i].m_DefAbsolutePose.IsValid());
								BipedSkeleton[i].m_idxParent = -1;
								int32 p = rIDefaultSkeleton.GetJointParentIDByID(idx);
								if (p > -1)
								{
									const char* pname = rIDefaultSkeleton.GetJointNameByID(p);
									int32 ParentID = GetIDbyName(pname);
									assert(ParentID > -1);
									if (p > -1)
										BipedSkeleton[i].m_idxParent = ParentID;
								}
							}

							int32 idxLFoot = GetIDbyName("Bip01 L Foot");
							assert(idxLFoot > -1);
							int32 idxLToe0 = GetIDbyName("Bip01 L Toe0");
							assert(idxLToe0 > -1);
							int32 idxLToe1 = GetIDbyName("Bip01 L Toe1");
							assert(idxLToe1 > -1);
							BipedSkeleton[idxLToe1].m_DefRelativePose = BipedSkeleton[idxLToe0].m_DefRelativePose;
							BipedSkeleton[idxLToe1].m_idxParent = BipedSkeleton[idxLToe0].m_idxParent;
							BipedSkeleton[idxLToe1].m_DefAbsolutePose = BipedSkeleton[idxLFoot].m_DefAbsolutePose * BipedSkeleton[idxLToe1].m_DefRelativePose;
							BipedSkeleton[idxLToe1].m_DefAbsolutePose.t.z = 0;
							BipedSkeleton[idxLToe1].m_DefRelativePose = BipedSkeleton[idxLFoot].m_DefAbsolutePose.GetInverted() * BipedSkeleton[idxLToe1].m_DefAbsolutePose;

							int32 idxRFoot = GetIDbyName("Bip01 R Foot");
							assert(idxRFoot > -1);
							int32 idxRToe0 = GetIDbyName("Bip01 R Toe0");
							assert(idxRToe0 > -1);
							int32 idxRToe1 = GetIDbyName("Bip01 R Toe1");
							assert(idxRToe1 > -1);
							BipedSkeleton[idxRToe1].m_DefRelativePose = BipedSkeleton[idxRToe0].m_DefRelativePose;
							BipedSkeleton[idxRToe1].m_idxParent = BipedSkeleton[idxRToe0].m_idxParent;
							BipedSkeleton[idxRToe1].m_DefAbsolutePose = BipedSkeleton[idxRFoot].m_DefAbsolutePose * BipedSkeleton[idxRToe1].m_DefRelativePose;
							BipedSkeleton[idxRToe1].m_DefAbsolutePose.t.z = 0;
							BipedSkeleton[idxRToe1].m_DefRelativePose = BipedSkeleton[idxRFoot].m_DefAbsolutePose.GetInverted() * BipedSkeleton[idxRToe1].m_DefAbsolutePose;

							//-----------------------------------------------------------------------
							//-----------------------------------------------------------------------
							//-----------------------------------------------------------------------
							BipedSkeleton[0].m_DefAbsolutePose = BipedSkeleton[0].m_DefRelativePose;
							for (uint32 i = 1; i < numJoints; i++)
							{
								int32 p = BipedSkeleton[i].m_idxParent;
								if (p > -1)
									BipedSkeleton[i].m_DefAbsolutePose = BipedSkeleton[p].m_DefAbsolutePose * BipedSkeleton[i].m_DefRelativePose;
							}

							BipedSkeleton[0].m_DefRelativePose.SetIdentity();
							BipedSkeleton[0].m_DefAbsolutePose.SetIdentity();
							for (uint32 i = 0; i < numJoints; i++)
							{
								BipedSkeleton[i].m_RelativePose = BipedSkeleton[i].m_DefRelativePose;
								BipedSkeleton[i].m_AbsolutePose = BipedSkeleton[i].m_DefAbsolutePose;
							}

							gEnv->pLog->Log("<Vicon Client> Adding animated character(%d)=%s", m_lstEntityNames.size(), szTemp);
						}
						else
						{
							// prop
							gEnv->pLog->Log("<Vicon Client> Adding prop(%d)=%s", m_lstEntityNames.size(), szTemp);
						}

						m_lstEntityNames.push_back(string(szTemp));
						m_lstEntities.push_back(pEntity->GetId());
						m_lstEntitiesOriginPos.push_back(pEntity->GetPos());
						m_lstEntitiesOriginRot.push_back(Quat(pEntity->GetRotation()));
					}
				}
			}

			if (stricmp(ViconSkeleton[i].sViconName.c_str(), szStart) == 0)
			{
				iBody->m_BipName = ViconSkeleton[i].sBipName;
				iBody->m_x = ViconSkeleton[i].si_x;
				iBody->m_y = ViconSkeleton[i].si_y;
				iBody->m_z = ViconSkeleton[i].si_z;
				break;
			}
			i++;
		}
	}

	gEnv->pLog->Log("Streaming %d markers, %d bodies, %d entities", m_MarkerChannels.size(), m_BodyChannels.size(), m_lstEntities.size());

	std::vector<MarkerChannel>::iterator iMarker;
	std::vector<MarkerData>::iterator iMarkerData;
	int nPosMarker = 0;
	for (iMarker = m_MarkerChannels.begin(),
	     iMarkerData = m_markerPositions.begin();
	     iMarker != m_MarkerChannels.end(); iMarker++, iMarkerData++, nPosMarker++)
	{
		gEnv->pLog->Log("<Vicon Client> Marker(%d)=%s", nPosMarker, iMarker->Name.c_str());

		const char* szSrc = iMarker->Name.c_str();
		const char* szStart = strstr(szSrc, ":"); // skip useless stuff
		if (szStart)
			szStart++; // skip ":"
		else
			szStart = szSrc;

		char szTemp[256];
		cry_strcpy(szTemp, szStart);
		iMarker->Name = string(szTemp);
	}

	// Set the socket I/O mode: In this case FIONBIO
	// enables or disables the blocking mode for the
	// socket based on the numerical value of iMode.
	// If iMode = 0, blocking is enabled;
	// If iMode != 0, non-blocking mode is enabled.
	u_long iMode = 1;
	ioctlsocket(m_SocketHandle, FIONBIO, &iMode);

	return (true);
}

	#include "Viewport.h"
	#include "ViewManager.h"
//////////////////////////////////////////////////////////////////////////
void CViconClient::Update()
{
	if (!m_bConnected)
		return;

	//////////////////////////////////////////////////////////////////////////
	//- Get Data - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	//	Get the m_data using the request/reply protocol.

	const bool bLogging = false;

	const int bufferSize = 2040;
	char buff[bufferSize];
	char* pBuff;

	pBuff = buff;

	*((long int*) pBuff) = ClientCodes::EData;
	pBuff += sizeof(long int);
	*((long int*) pBuff) = ClientCodes::ERequest;
	pBuff += sizeof(long int);

	if (bLogging)
		gEnv->pLog->Log("STEP 1");
	if (send(m_SocketHandle, buff, pBuff - buff, 0) == SOCKET_ERROR)
	{
		if (bLogging)
		{
			gEnv->pLog->LogError("<Vicon Client> Error requesting");
			PrintErrorDescription(WSAGetLastError());
		}
		return;
	}

	long int packet;
	long int type;

	if (bLogging)
		gEnv->pLog->Log("STEP 2");
	if (!recieve(m_SocketHandle, packet))
	{
		if (bLogging)
		{
			gEnv->pLog->LogError("<Vicon Client> Error recieving packet");
			PrintErrorDescription(WSAGetLastError());
		}
		return;
	}

	if (bLogging)
		gEnv->pLog->Log("STEP 3");
	if (!recieve(m_SocketHandle, type))
	{
		if (bLogging)
		{
			gEnv->pLog->LogError("<Vicon Client> Error recieving type");
			PrintErrorDescription(WSAGetLastError());
		}
		return;
	}

	if (type != ClientCodes::EReply)
	{
		gEnv->pLog->LogError("<Vicon Client> Bad packet");
		Disconnect();
		return;
	}

	if (packet != ClientCodes::EData)
	{
		gEnv->pLog->LogError("<Vicon Client> Bad Reply type");
		Disconnect();
		return;
	}

	long int size;

	if (bLogging)
		gEnv->pLog->Log("STEP 4");
	if (!recieve(m_SocketHandle, size))
	{
		if (bLogging)
		{
			gEnv->pLog->LogError("<Vicon Client> Error recieving size");
			PrintErrorDescription(WSAGetLastError());
		}
		return;
	}

	if (size != m_info.size())
	{
		if (bLogging)
		{
			gEnv->pLog->LogError("<Vicon Client> Bad Data Packet");
			PrintErrorDescription(WSAGetLastError());
		}
		return;
	}

	//	Get the m_data.

	std::vector<double>::iterator iData;

	if (bLogging)
		gEnv->pLog->Log("STEP 5");
	for (iData = m_data.begin(); iData != m_data.end(); iData++)
	{
		if (!recieve(m_SocketHandle, *iData))
		{
			if (bLogging)
			{
				gEnv->pLog->LogError("<Vicon Client> Error recieving iData");
				PrintErrorDescription(WSAGetLastError());
			}
			return;
		}
	}

	if (bLogging)
		gEnv->pLog->Log("STEP 6");

	//- Look Up Channels - - - - - - - - - - - - - - - - - - - - - - -
	//  Get the TimeStamp

	m_fTimeStamp = (float)(m_data[m_nFrameChannel]);

	if (bLogging)
		gEnv->pLog->Log("<Vicon Client> timestamp=%f, frame channel=%d", m_fTimeStamp, m_nFrameChannel);

	//	Get the channels corresponding to the markers.
	//	Y is up
	//	The values are in millimeters

	std::vector<MarkerChannel>::iterator iMarker;
	std::vector<MarkerData>::iterator iMarkerData;

	for (iMarker = m_MarkerChannels.begin(),
	     iMarkerData = m_markerPositions.begin();
	     iMarker != m_MarkerChannels.end(); iMarker++, iMarkerData++)
	{
		iMarkerData->X = m_data[iMarker->X];
		iMarkerData->Y = m_data[iMarker->Y];
		iMarkerData->Z = m_data[iMarker->Z];
		if (m_data[iMarker->O] > 0.5)
			iMarkerData->Visible = false;
		else
			iMarkerData->Visible = true;
	}

	//	Get the channels corresponding to the bodies.
	//=================================================================
	//	The bodies are in global space
	//	The world is Z-up
	//	The translational values are in millimeters
	//	The rotational values are in radians
	//=================================================================

	std::vector<BodyChannel>::iterator iBody;
	std::vector<BodyData>::iterator iBodyData;

	for (iBody = m_BodyChannels.begin(), iBodyData = m_bodyPositions.begin(); iBody != m_BodyChannels.end(); iBody++, iBodyData++)
	{
		iBodyData->Joint.q = Quat::exp(Vec3d(m_data[iBody->RX], m_data[iBody->RY], m_data[iBody->RZ]) * 0.5);
		iBodyData->Joint.t = Vec3d(m_data[iBody->TX], m_data[iBody->TY], m_data[iBody->TZ]) / 1000.0;
		iBodyData->m_IsInitialized = 1;
		if (bLogging)
			gEnv->pLog->Log("<Vicon Client> Body=%s T=(%f %f %f)", iBody->Name.c_str(),
			                f32(iBodyData->Joint.t.x), f32(iBodyData->Joint.t.y), f32(iBodyData->Joint.t.z));
	}

	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	//  The marker and body data now resides in the arrays
	//	m_markerPositions & m_bodyPositions.

	m_nFrameCounter++;

	//if (!m_pEntity)
	//	Vicon_PostProcessCallback(NULL,NULL);

	//////////////////////////////////////////////////////////////////////////
	IRenderAuxGeom* g_pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	g_pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);

	int nSize = m_lstEntities.size();
	for (int k = 0; k < nSize; k++)
	{
		IEntity* pEnt = gEnv->pEntitySystem->GetEntity(m_lstEntities[k]);
		if (!pEnt)
			continue;

		if (pEnt->GetCharacter(0) && pEnt->GetCharacter(0)->GetISkeletonPose())
			continue; // its a character, this will be handled by the postprocess callback

		int nNameLen = strlen(m_lstEntityNames[k].c_str());

		bool bCam = false;
		if (strnicmp(pEnt->GetName(), "camera", 6) == 0)
			bCam = true;

		for (iBody = m_BodyChannels.begin(),
		     iBodyData = m_bodyPositions.begin();
		     iBody != m_BodyChannels.end(); iBody++, iBodyData++)
		{
			if (strncmp(m_lstEntityNames[k].c_str(), iBody->Name.c_str(), nNameLen) != 0)
				continue; // try next body

			Vec3 vPos = iBodyData->Joint.t;
			vPos += m_lstEntitiesOriginPos[k];
			Quat q = iBodyData->Joint.q;
			Vec3 v0 = q.GetColumn0();
			Vec3 v1 = q.GetColumn1();
			Vec3 v2 = q.GetColumn2();

			g_pAuxGeom->DrawLine(vPos, RGBA8(0xff, 0x00, 0x00, 0x00), vPos + v0, RGBA8(0xff, 0x00, 0x00, 0x00));
			g_pAuxGeom->DrawLine(vPos, RGBA8(0x00, 0xff, 0x00, 0x00), vPos + v1, RGBA8(0x00, 0xff, 0x00, 0x00));
			g_pAuxGeom->DrawLine(vPos, RGBA8(0x00, 0x00, 0xff, 0x00), vPos + v2, RGBA8(0x00, 0x00, 0xff, 0x00));

			if (bCam)
			{
				CViewport* pRendView = GetIEditor()->GetViewManager()->GetViewport(ET_ViewportCamera);
				if (pRendView)
				{
					Matrix34 tm;
					//Quat q2	=	Quat(iBodyData->QW,iBodyData->QX,iBodyData->QY,-iBodyData->QZ);
					//tm.Set( Vec3(1,1,1), q2, vPos);
					tm.SetFromVectors(v0, v1, v2, vPos);

					// X-Forward! Fix the kinematic fit in Vicon! >:(
					//Matrix34 rot2;
					//rot2.SetRotationZ(DEG2RAD(270));

					// This camera from MM double wrong
					//Matrix34 rot2;
					//rot2.SetRotationZ(DEG2RAD(90));
					//Matrix34 rot3;
					//rot3.SetRotationY(DEG2RAD(180));
					//pRendView->SetViewTM(tm*rot2*rot3);

					pRendView->SetViewTM(tm);
				}
			}
			else
			{
				pEnt->SetPos(vPos);
				pEnt->SetRotation(q);
			}

		} //ibody
	}   //k
}

#endif // disable vicon

