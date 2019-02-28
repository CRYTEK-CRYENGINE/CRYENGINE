// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "FacialModel.h"

#include "FaceAnimation.h"
#include "FaceEffectorLibrary.h"
#include <Cry3DEngine/I3DEngine.h>
#include "../CharacterInstance.h"
#include "../ModelMesh.h"
#include "../Model.h"
#include "../CharacterManager.h"
#include "CryModEffMorph.h" //embedded

#undef PI
#define PI 3.14159265358979323f

//////////////////////////////////////////////////////////////////////////
CFacialModel::CFacialModel(CDefaultSkeleton* pDefaultSkeleton)
{
	m_pDefaultSkeleton = pDefaultSkeleton;
	InitMorphTargets();
	m_bAttachmentChanged = false;
}

//////////////////////////////////////////////////////////////////////////
CFaceState* CFacialModel::CreateState()
{
	CFaceState* pState = new CFaceState(this);
	if (m_pEffectorsLibrary)
		pState->SetNumWeights(m_pEffectorsLibrary->GetLastIndex());
	return pState;
}

//////////////////////////////////////////////////////////////////////////
void CFacialModel::InitMorphTargets()
{
#if 0 // No longer supported with morph refactoring
	CModelMesh* pModelMesh = m_pDefaultSkeleton->GetModelMesh();
	int nMorphTargetsCount = (int)pModelMesh->m_morphTargets.size();
	m_effectors.reserve(nMorphTargetsCount);
	m_morphTargets.reserve(nMorphTargetsCount);
	for (int i = 0; i < nMorphTargetsCount; i++)
	{
		VertexFrame* pMorphTarget = pModelMesh->m_morphTargets[i];
		if (!pMorphTarget)
			continue;

		CFacialMorphTarget* pMorphEffector = new CFacialMorphTarget(i);
		string morphname = pMorphTarget->m_name;
		if (!morphname.empty() && morphname[0] == '#') // skip # character at the begining of the morph target.
			morphname = morphname.substr(1);

		CFacialAnimation* pFaceAnim = g_pCharacterManager->GetFacialAnimation();
		if (!pFaceAnim)
			return;
		pMorphEffector->SetIdentifier(pFaceAnim->CreateIdentifierHandle(morphname));
		pMorphEffector->SetIndexInState(i);

		SEffectorInfo ef;
		ef.pEffector = pMorphEffector;
		ef.nMorphTargetId = i;
		m_effectors.push_back(ef);

		SMorphTargetInfo mrphTrg;
		morphname.MakeLower();
		mrphTrg.name = pFaceAnim->CreateIdentifierHandle(morphname);
		mrphTrg.nMorphTargetId = i;
		m_morphTargets.push_back(mrphTrg);
	}

	std::sort(m_morphTargets.begin(), m_morphTargets.end());
#endif
	//m_libToFaceState
}

//////////////////////////////////////////////////////////////////////////
void CFacialModel::AssignLibrary(IFacialEffectorsLibrary* pLibrary)
{
	assert(pLibrary);
	if (pLibrary == m_pEffectorsLibrary)
		return;

	m_pEffectorsLibrary = (CFacialEffectorsLibrary*)pLibrary;

	// Adds missing morph targets to the Facial Library.
	uint32 numEffectors = m_effectors.size();
	for (uint32 i = 0; i < numEffectors; i++)
	{
		CFacialEffector* pModelEffector = m_effectors[i].pEffector;

		CFacialEffector* pEffector = (CFacialEffector*)m_pEffectorsLibrary->Find(pModelEffector->GetIdentifier());
		if (!pEffector)
		{
			// Library does not contain this effector, so add it into the library.
			if (m_effectors[i].nMorphTargetId >= 0)
			{
				IFacialEffector* pNewEffector = m_pEffectorsLibrary->CreateEffector(EFE_TYPE_MORPH_TARGET, pModelEffector->GetIdentifier());
				IFacialEffector* pRoot = m_pEffectorsLibrary->GetRoot();
				if (pRoot && pNewEffector)
					pRoot->AddSubEffector(pNewEffector);
			}
		}
	}

	// Creates a mapping table between Face state index to the index in MorphTargetsInfo array.
	// When face state is applied to the model, this mapping table is used to find what morph target is associated with state index.
	int nIndices = m_pEffectorsLibrary->GetLastIndex();
	m_faceStateIndexToMorphTargetId.resize(nIndices);
	for (int i = 0; i < nIndices; i++)
	{
		// Initialize all face state mappings.
		m_faceStateIndexToMorphTargetId[i] = -1;
	}
	SMorphTargetInfo tempMT;
	for (CFacialEffectorsLibrary::CrcToEffectorMap::iterator it = m_pEffectorsLibrary->m_crcToEffectorMap.begin(); it != m_pEffectorsLibrary->m_crcToEffectorMap.end(); ++it)
	{
		CFacialEffector* pEffector = it->second;

		int nStateIndex = pEffector->GetIndexInState();
		if (nStateIndex >= 0 && nStateIndex < nIndices)
		{
			// Find morph target for this state index.
			// Morph targets must be sorted by name for binary_find to work correctly.
			tempMT.name = pEffector->GetIdentifier();

			// i think this can be taken out because comparisons are now done over lowercase crc32 anyway
			// tempMT.name.MakeLower();
			MorphTargetsInfo::iterator it2 = stl::binary_find(m_morphTargets.begin(), m_morphTargets.end(), tempMT);
			if (it2 != m_morphTargets.end())
			{
				m_faceStateIndexToMorphTargetId[nStateIndex] = (int)std::distance(m_morphTargets.begin(), it2);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IFacialEffectorsLibrary* CFacialModel::GetLibrary()
{
	return m_pEffectorsLibrary;
}

//////////////////////////////////////////////////////////////////////////
int CFacialModel::GetMorphTargetCount() const
{
	return m_morphTargets.size();
}

//////////////////////////////////////////////////////////////////////////
const char* CFacialModel::GetMorphTargetName(int morphTargetIndex) const
{
#ifdef FACE_STORE_ASSET_VALUES
	return (morphTargetIndex >= 0 && morphTargetIndex < int(m_morphTargets.size()) ? m_morphTargets[morphTargetIndex].name.GetString().c_str() : 0);
#else
	CryFatalError("Not Supported on this platform.");
	return NULL;
#endif
}

void CFacialModel::PushMorphEffectors(int i, const MorphTargetsInfo& morphTargets, f32 fWeight, f32 fBalance, DynArray<CryModEffMorph>& arrMorphEffectors)
{
	// This is a morph target.
	int nMorphIndex = m_faceStateIndexToMorphTargetId[i];
	if (nMorphIndex >= 0 && nMorphIndex < (int)m_morphTargets.size())
	{
		int nMorphTargetId = m_morphTargets[nMorphIndex].nMorphTargetId;

		// Add morph target to character.
		CryModEffMorph morph;
		morph.m_fTime = 0;
		morph.m_nFlags = CryCharMorphParams::FLAGS_FREEZE | CryCharMorphParams::FLAGS_NO_BLENDOUT | CryCharMorphParams::FLAGS_INSTANTANEOUS;
		morph.m_nMorphTargetId = nMorphTargetId;
		morph.m_Params.m_fAmplitude = fWeight;
		morph.m_Params.m_fBlendIn = 0;
		morph.m_Params.m_fBlendOut = 0;
		morph.m_Params.m_fBalance = fBalance;
		arrMorphEffectors.push_back(morph);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialModel::FillInfoMapFromFaceState(CFacialDisplaceInfo& info, CFaceState* const pFaceState, CCharInstance* const pInstance, int numForcedRotations, const CFacialAnimForcedRotationEntry* const forcedRotations, float blendBoneRotations)
{
	DEFINE_PROFILER_FUNCTION();

	if (!m_pEffectorsLibrary)
		return;

	// Update Face State with the new number of indices.
	if (pFaceState->GetNumWeights() != m_pEffectorsLibrary->GetLastIndex())
	{
		pFaceState->SetNumWeights(m_pEffectorsLibrary->GetLastIndex());
	}

	//SetEffectorCoordinateSystems(pSkeletonPose);

	const bool blendBones = (blendBoneRotations > 0.0f);

	const size_t infoDisplaceInfoMapSize = info.GetCount();
	s_boneInfoBlending.Initialize(infoDisplaceInfoMapSize);
	if (blendBones)
	{
		s_boneInfoBlending.CopyUsed(info);
	}
	else
	{
		info.Clear();
	}

	const int32 nNumIndices = pFaceState->GetNumWeights();
	const int32 faceStateIndexToMorphTargetSize = (int32)m_faceStateIndexToMorphTargetId.size();
	for (int32 i = 0; i < nNumIndices; i++)
	{
		f32 fWeight = pFaceState->GetWeight(i);
		f32 fWeightAbs = (f32)__fsel(fWeight, fWeight, -fWeight);

		if (fWeightAbs < EFFECTOR_MIN_WEIGHT_EPSILON)
			continue;

		const bool isMorph = (i < faceStateIndexToMorphTargetSize && m_faceStateIndexToMorphTargetId[i] >= 0);

		if (!isMorph)
		{
			CFacialEffector* pEffector = m_pEffectorsLibrary->GetEffectorFromIndex(i);
			if (pEffector)
			{
				// Not a morph target.
				switch (pEffector->GetType())
				{
				case EFE_TYPE_BONE:
					{
						QuatT qt;
						const int32 boneID = ApplyBoneEffector(pInstance, pEffector, fWeight, qt);
						if (boneID >= 0)
						{
							QuatT boneInfo = s_boneInfoBlending.GetDisplaceInfo(boneID);
							{
								boneInfo.q = boneInfo.q * qt.q;
								boneInfo.t += qt.t;
								s_boneInfoBlending.SetDisplaceInfo(boneID, boneInfo);
							}
						}
					}
					break;

				case EFE_TYPE_ATTACHMENT:
					//ApplyAttachmentEffector( pMasterCharacter,pEffector,fWeight );
					CryFatalError("Attachment Effectors not longer supported!");
					break;
				}
			}
		}
		else
		{
			//    f32 fBalance = pFaceState->GetBalance(i);
			//    PushMorphEffectors(i, m_morphTargets, fWeight, fBalance, arrMorphEffectors);
			CryFatalError("Morphs not longer supported!");
		}
	}

	{
		const float blendWeight = (1.f - blendBoneRotations);
		for (size_t i = 0; i < infoDisplaceInfoMapSize; ++i)
		{
			const bool isUsed = s_boneInfoBlending.IsUsed(i);
			if (isUsed)
			{
				const QuatT& blendBoneInfo = s_boneInfoBlending.GetDisplaceInfo(i);
				QuatT boneInfo = info.GetDisplaceInfo(i);

				boneInfo.SetNLerp(boneInfo, blendBoneInfo, blendWeight);
				boneInfo.q.Normalize();

				info.SetDisplaceInfo(i, boneInfo);
			}
		}
	}

	/*
	   uint32 numFaceJoints = pSkeletonPose->m_FaceAnimPosSmooth.size();
	   if (numFaceJoints!=numJoints)
	   {
	   pSkeletonPose->m_FaceAnimPosSmooth.resize(numJoints);
	   pSkeletonPose->m_FaceAnimPosSmoothRate.resize(numJoints);
	   for (uint32 i=0; i<numJoints; i++)
	   {
	    pSkeletonPose->m_FaceAnimPosSmooth[i]	= Vec3(ZERO);
	    pSkeletonPose->m_FaceAnimPosSmoothRate[i] = Vec3(ZERO);
	   }
	   }


	   {
	   InfoMap::iterator it;
	   for (it = info.begin(); it != info.end(); ++it)
	   {
	    uint32 i = it->first;
	    SFacialDisplaceInfo& entry = it->second;
	    SmoothCD(pSkeletonPose->m_FaceAnimPosSmooth[i], pSkeletonPose->m_FaceAnimPosSmoothRate[i], pMasterCharacter->m_fOriginalDeltaTime, entry.rotPos.t, 0.05f);
	   }
	   }*/

	//	float fColor[4] = {0,1,0,1};
	//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.4f, fColor, false,"Facial Animation" );
	//	g_YLine+=16.0f;

	/*
	   if (blendBoneRotations)
	   {
	   g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.4f, fColor, false,"blendBoneRotations" );	g_YLine+=16.0f;
	   // Blend the additional rotations - this is necessary when we are changing sequences or Look IK settings to avoid snapping.
	   const float interpolationRate = 5.0f;
	   float interpolateFraction = min(1.0f, max(0.0f, pMasterCharacter->m_fOriginalDeltaTime * interpolationRate));
	   for (uint32 boneIndex=0; boneIndex<numJoints; ++boneIndex)
	   {
	    additionalRotPos[boneIndex].q.SetNlerp(additionalRotPos[boneIndex].q, additionalRotPos[boneIndex].q, interpolateFraction);
	    additionalRotPos[boneIndex].t.SetLerp(additionalRotPos[boneIndex].t, additionalRotPos[boneIndex].t, interpolateFraction);
	   }
	   }
	   else
	   {
	   // Usually we don't want to blend, we want the exact settings as they are in the fsq to preserve the subtleties of the animation.
	   for (uint32 boneIndex=0; boneIndex<numJoints; ++boneIndex)
	    additionalRotPos[boneIndex] = additionalRotPos[boneIndex];
	   }*/

	// Apply forced rotations if any. This is currently used only in the facial editor preview mode to
	// support the code interface to force the neck and eye orientations.
	{
		// Make sure we use the master skeleton, in case we are an attachment.
		IF_UNLIKELY (forcedRotations)
		{
			for (int rotIndex = 0; rotIndex < numForcedRotations; ++rotIndex)
			{
				//CSkeletonPose* pSkeletonPose = &pMasterCharacter->m_SkeletonPose;
				int32 id = forcedRotations[rotIndex].jointId;
				if (id >= 0 && id < MAX_JOINT_AMOUNT)
				{
					QuatT qt = info.GetDisplaceInfo(id);
					qt = qt * forcedRotations[rotIndex].rotation;
					qt.q.Normalize();

					info.SetDisplaceInfo(id, qt);
				}
			}
		}
	}

	TransformTranslations(info, pInstance);
}

void CFacialModel::TransformTranslations(CFacialDisplaceInfo& info, const CCharInstance* const pInstance)
{
	const size_t infoCount = info.GetCount();
	for (size_t i = 0; i < infoCount; ++i)
	{
		const bool isUsed = info.IsUsed(i);
		if (isUsed)
		{
			const int32 p = pInstance->m_pDefaultSkeleton->m_arrModelJoints[i].m_idxParent;
			QuatT q = info.GetDisplaceInfo(i);
			q.t = !pInstance->m_SkeletonPose.GetPoseDataDefault().GetJointAbsolute(p).q * q.t;
			info.SetDisplaceInfo(i, q);
		}
	}
}
/*
   void CFacialModel::HandleLookIK(FacialDisplaceInfoMap& info, CLookAt& lookIK, bool physicsRelinquished, int32 head, int32 leye, int32 reye)
   {
   f32 lookIKFade = 1.0f - lookIK.m_LookIKBlend;

   if (lookIKFade == 1.0f)
    return;

   QuatT temp;

   if (head>0 && head < MAX_JOINT_AMOUNT)
   {
    //	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.4f, fColor, false,"Head: %f (%f %f %f)",additionalRotPos[head].q.w,additionalRotPos[head].q.v.x,additionalRotPos[head].q.v.y,additionalRotPos[head].q.v.z );	g_YLine+=16.0f;
    SFacialDisplaceInfo& iHead = info[head];
    lookIK.m_addHead=Quat::CreateNlerp(iHead.quatT.q,IDENTITY,lookIKFade);
    iHead.quatT.q.SetNlerp(IDENTITY,iHead.quatT.q,lookIKFade);

    if (physicsRelinquished)
      iHead.quatT.q.SetIdentity(); //if this is a rag-doll, then don't rotate the head.
   }

   if (leye>0 && leye < MAX_JOINT_AMOUNT)
   {
    //	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.4f, fColor, false,"LEye: %f (%f %f %f)",additionalRotPos[leye].q.w,additionalRotPos[leye].q.v.x,additionalRotPos[leye].q.v.y,additionalRotPos[leye].q.v.z);	g_YLine+=16.0f;
    SFacialDisplaceInfo& iLeye = info[leye];
    lookIK.m_addLEye = Quat::CreateNlerp(iLeye.quatT.q,IDENTITY,lookIKFade);
    iLeye.quatT.q.SetNlerp(IDENTITY,iLeye.quatT.q,lookIKFade);
   }

   if (reye>0 && reye < MAX_JOINT_AMOUNT)
   {
    //	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.4f, fColor, false,"REye: %f (%f %f %f)",additionalRotPos[reye].q.w,additionalRotPos[reye].q.v.x,additionalRotPos[reye].q.v.y,additionalRotPos[reye].q.v.z  );	g_YLine+=16.0f;
    //	int32 p = pSkeletonPose->m_parrModelJoints[reye].m_idxParent;
    SFacialDisplaceInfo& iReye = info[reye];
    lookIK.m_addREye = Quat::CreateNlerp(iReye.quatT.q,IDENTITY,lookIKFade);
    iReye.quatT.q.SetNlerp(IDENTITY,iReye.quatT.q,lookIKFade);
   }

   }
 */

void CFacialModel::ApplyDisplaceInfoToJoints(const CFacialDisplaceInfo& info, const CDefaultSkeleton* const pSkeleton, Skeleton::CPoseData* const pPoseData, const QuatT* const pJointsRelativeDefault)
{
	DEFINE_PROFILER_FUNCTION();
	uint32 numJoints = pPoseData->GetJointCount();

	// for dirty joints, the absolute pose has to be recalculated -- see below
	uint32 dirty[MAX_JOINT_AMOUNT];
	uint32 firstDirty = MAX_JOINT_AMOUNT;
	memset(dirty, 0x00, sizeof(uint32) * numJoints);

	const uint32 infoCount = static_cast<uint32>(info.GetCount());
	for (uint32 i = 1; i < infoCount; ++i)
	{
		const bool isUsed = info.IsUsed(i);
		if (isUsed)
		{
			const QuatT& jointFacialRelative = info.GetDisplaceInfo(i);
			const QuatT& jointDefaultRelative = pJointsRelativeDefault[i];
			pPoseData->SetJointRelative(i, jointDefaultRelative * jointFacialRelative);

			PREFAST_ASSUME(i < numJoints && i < MAX_JOINT_AMOUNT);
			dirty[i] = true;
			firstDirty = min(firstDirty, i);
		}
	}

	assert(firstDirty >= 1 && firstDirty < MAX_JOINT_AMOUNT);
	PREFAST_ASSUME(firstDirty < numJoints && firstDirty < MAX_JOINT_AMOUNT);

	for (uint32 i = firstDirty; i < numJoints; ++i)
	{

		int p = pSkeleton->m_arrModelJoints[i].m_idxParent;
		assert(p >= 0 && p < MAX_JOINT_AMOUNT);
		PREFAST_ASSUME(p >= 0 && p < MAX_JOINT_AMOUNT);
		if (dirty[p])
		{
			PREFAST_ASSUME(i < numJoints && i < MAX_JOINT_AMOUNT);
			dirty[i] = dirty[p];
			pPoseData->GetJointsAbsolute()[i] = pPoseData->GetJointsAbsolute()[p] * pPoseData->GetJointsRelative()[i];
		}
	}
}

//////////////////////////////////////////////////////////////////////////
/* // Attachment Effectors do not exist anymore

   void CFacialModel::ApplyAttachmentEffector( CCharInstance *pSkinInstance,CFacialEffector *pEffector,float fWeight )
   {
   const char *sAttachmentName = pEffector->GetParamString( EFE_PARAM_BONE_NAME );

   IAttachment *pIAttachment = pSkinInstance->GetIAttachmentManager()->GetInterfaceByName( sAttachmentName );
   if (pIAttachment)
   {
    Ang3 vRot = Ang3(pEffector->GetParamVec3( EFE_PARAM_BONE_ROT_AXIS ));
    vRot = vRot * fWeight;

    CAttachment *pCAttachment = (CAttachment*)pIAttachment;
    Quat q;
    q.SetRotationXYZ( vRot );
    pCAttachment->m_additionalRotation = pCAttachment->m_additionalRotation * q;
    m_bAttachmentChanged = true;
   }
   else
   {
    // No such attachment, look into the parent.
    CAttachment* pMasterAttachment = pSkinInstance->m_pSkinAttachment;
    if (pMasterAttachment)
    {
      uint32 type = pMasterAttachment->GetType();
      if (pMasterAttachment->GetType() == CA_SKIN)
      {
        // Apply on parent bones.
        CCharInstance* pCharacter = pMasterAttachment->m_pAttachmentManager->m_pSkelInstance;
        IAttachment *pAttachment = pCharacter->GetIAttachmentManager()->GetInterfaceByName( sAttachmentName );
        if (pAttachment)
        {
          Ang3 vRot = Ang3(pEffector->GetParamVec3( EFE_PARAM_BONE_ROT_AXIS ));
          vRot = vRot * fWeight;

          CAttachment *pCAttachment = (CAttachment*)pAttachment;
          Quat q;
          q.SetRotationXYZ( vRot );
          pCAttachment->m_additionalRotation = pCAttachment->m_additionalRotation * q;
          m_bAttachmentChanged = true;
        }
      }
    }
   }
   }
 */

//////////////////////////////////////////////////////////////////////////
int32 CFacialModel::ApplyBoneEffector(CCharInstance* pInstance, CFacialEffector* pEffector, float fWeight, QuatT& quatT)
{
	const uint32 crc32 = pEffector->GetAttachmentCRC();
	const int nBoneId = pInstance->m_pDefaultSkeleton->GetJointIDByCRC32(crc32);

	if (nBoneId >= 0)
	{
		QuatT qt = pEffector->GetQuatT();
		qt.t *= fWeight;

		qt.q = (0 <= fWeight) ? qt.q : !qt.q;
		qt.q.SetNlerp(Quat(IDENTITY), qt.q, fabsf(fWeight));

		quatT = qt;
		return nBoneId;
	}
	return -1;
}

size_t CFacialModel::SizeOfThis()
{
	size_t nSize(sizeof(CFacialModel));
	nSize += sizeofVector(m_effectors);
	nSize += sizeofVector(m_faceStateIndexToMorphTargetId);
	nSize += sizeofVector(m_morphTargets);

	return nSize;
}

void CFacialModel::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_effectors);
	pSizer->AddObject(m_pEffectorsLibrary);
	pSizer->AddObject(m_faceStateIndexToMorphTargetId);
	pSizer->AddObject(m_morphTargets);
}

void CFacialModel::ClearResources()
{
	stl::free_container(s_boneInfoBlending);
}

CFacialDisplaceInfo CFacialModel::s_boneInfoBlending;
