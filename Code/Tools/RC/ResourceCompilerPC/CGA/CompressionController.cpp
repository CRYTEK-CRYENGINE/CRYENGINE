// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <algorithm>
#include <functional>

#include "AnimationLoader.h"
#include "CompressionController.h"
#include "ControllerPQ.h"


// Returns true if angular difference between two quaternions is bigger than
// the one specified by cosOfHalfMaxAngle.
// cosOfHalfMaxAngle == cosf(maxAngleDifferenceInRadians * 0.5f)
static inline bool error_quatCos(const Quat& q1, const Quat& q2, const float cosOfHalfMaxAngle)
{
	//	const float cosOfHalfAngle = fabsf(q1.v.x * q2.v.x + q1.v.y * q2.v.y + q1.v.z * q2.v.z + q1.v.w * q1.v.w);
	// Note about "1 - fabsf(" and "- 1)": q1 | q1 might return values > 1, so we map them to values < 1.
	const float cosOfHalfAngle = 1 - fabsf(fabsf(q1 | q2) - 1);
	return cosOfHalfAngle < cosOfHalfMaxAngle;
}

static inline bool error_vec3(const Vec3& v1, const Vec3& v2, const float maxAllowedDistanceSquared)
{
	const float x = v1.x - v2.x;
	const float y = v1.y - v2.y;
	const float z = v1.z - v2.z;

	const float distanceSquared = x * x + y * y + z * z;

	return distanceSquared > maxAllowedDistanceSquared;
}


static Vec3 Diag33ToVec3(const Diag33& input) 
{ 
	Vec3 output;
	output.x = input.x;
	output.y = input.y;
	output.z = input.z;
	return output;
};


static bool ObtainTrackData(
	const GlobalAnimationHeaderCAF &header,
	uint32 c,
	uint32 &curControllerId,
	std::vector<Quat>& rotations,
	std::vector<Vec3>& positions,
	std::vector<Diag33>& scales,
	std::vector<int>& rotTimes,
	std::vector<int>& posTimes,
	std::vector<int>& sclTimes
	)
{
	rotations.clear();
	positions.clear();
	scales.clear();
	rotTimes.clear();
	posTimes.clear();
	sclTimes.clear();

	const CControllerPQLog* const pController = dynamic_cast<const CControllerPQLog *>(header.m_arrController[c].get());

	if (pController)
	{
		uint32 numTimes = pController->m_arrTimes.size();
		rotations.reserve(numTimes);
		positions.reserve(numTimes);
		posTimes.reserve(numTimes);
		rotTimes.reserve(numTimes);

		curControllerId = pController->m_nControllerId;

		int oldtime = INT_MIN;

		for (uint32 i=0; i<numTimes; ++i)
		{
			if (oldtime != pController->m_arrTimes[i])
			{
				oldtime = pController->m_arrTimes[i];

				Vec3 vRot = pController->m_arrKeys[i].rotationLog;

				if (vRot.x<-gf_PI || vRot.x>gf_PI)
					vRot.x=0;	
				if (vRot.y<-gf_PI || vRot.y>gf_PI)
					vRot.y=0;	
				if (vRot.z<-gf_PI || vRot.z>gf_PI)
					vRot.z=0;	

				Vec3 vPos = pController->m_arrKeys[i].position;
				Diag33 vScl = pController->m_arrKeys[i].scale;

				rotations.push_back(!Quat::exp(vRot));
				positions.push_back(vPos);
				scales.push_back(vScl);

				rotTimes.push_back(pController->m_arrTimes[i]);
				posTimes.push_back(pController->m_arrTimes[i]);
				sclTimes.push_back(pController->m_arrTimes[i]);
			}
		}
	}
	else
	{
		const CController* const pCController = dynamic_cast<const CController*>(header.m_arrController[c].get());

		if (!pCController)
		{
			assert(0);
			RCLogError("Unknown controller type");
			return false;
		}

		curControllerId = pCController->GetID();

		const IRotationController* const rotationController = pCController->GetRotationController().get();
		uint32 numRotKeys=rotationController->GetNumKeys();

		rotations.resize(numRotKeys, Quat(IDENTITY));
		rotTimes.resize(numRotKeys);
		for (uint32 i=0; i<numRotKeys; ++i)
		{
			rotations[i] = rotationController->GetValueFromKey(i);
			rotTimes[i] = int(rotationController->GetTimeFromKey(i));
		}

		const IPositionController* const positionController = pCController->GetPositionController().get();
		uint32 numPosKeys=positionController->GetNumKeys();
		positions.resize(numPosKeys, Vec3(ZERO));
		posTimes.resize(numPosKeys);
		for (uint32 i=0; i<numPosKeys; ++i)
		{
			positions[i] = positionController->GetValueFromKey(i);
			posTimes[i] = int(positionController->GetTimeFromKey(i));
		}

		const IPositionController* const scaleController = pCController->GetScaleController().get();
		const uint32 numSclKeys = scaleController->GetNumKeys();
		scales.resize(numSclKeys, Diag33(IDENTITY));
		sclTimes.resize(numSclKeys);
		for (uint32 i = 0; i < numSclKeys; ++i)
		{
			scales[i] = scaleController->GetValueFromKey(i);
			sclTimes[i] = int(scaleController->GetTimeFromKey(i));
		}
	}

	if (rotations.empty() || positions.empty() || scales.empty())
	{
		assert(0);
		RCLogError("Unexpected empty positions and/or rotations");
		return false;
	}

	return true;
}


static bool GetControllerId(
	const GlobalAnimationHeaderCAF &header,
	uint32 controllerIndex,
	uint32& controllerId)
{
	const CControllerPQLog* const pController = dynamic_cast<const CControllerPQLog *>(header.m_arrController[controllerIndex].get());

	if (pController)
	{
		controllerId = pController->m_nControllerId;
	}
	else 
	{
		const CController* const pCController = dynamic_cast<const CController*>(header.m_arrController[controllerIndex].get());

		if (!pCController)
		{
			assert(0);
			RCLogError("Unknown controller type");
			return false;
		}

		controllerId = pCController->GetID();
	}

	return false;
}


static int FindBoneByControllerId(
	const CSkeletonInfo* pSkeleton,
	const uint32 controllerId,
	const char*& pBoneName)
{
	pBoneName = 0;

	const int jointCount = (int)pSkeleton->m_SkinningInfo.m_arrBonesDesc.size();
	for (int b = 0; b < jointCount; ++b)
	{
		if (pSkeleton->m_SkinningInfo.m_arrBonesDesc[b].m_nControllerID == controllerId)
		{
			pBoneName = pSkeleton->m_SkinningInfo.m_arrBonesDesc[b].m_arrBoneName;
			return b;
		}
	}
	
	return -1;
}


static void PrintControllerInfo(
	const GlobalAnimationHeaderCAF& header,
	const CSkeletonInfo* pSkeleton,
	uint32 controllerIndex,
	const SPlatformAnimationSetup& platform,
	const SAnimationDesc& desc)
{
	if (!pSkeleton || controllerIndex >= header.m_arrController.size())
	{
		assert(0);
		return;
	}

	uint32 controllerId;
	if (!GetControllerId(header, controllerIndex, controllerId))
	{
		return;
	}

	const char* pBoneName = 0;

	const SBoneCompressionValues bc = desc.GetBoneCompressionValues(pBoneName, platform.m_compressionMultiplier);

	{
		const char* const pName = (pBoneName ? pBoneName : "<no name>");
		const char* pPos = 
			(bc.m_eAutodeletePos == SBoneCompressionValues::eDelete_Yes ? "Delete    " : (bc.m_eAutodeletePos == SBoneCompressionValues::eDelete_No ? "DontDelete" : "AutoDelete"));
		const char* pRot = 
			(bc.m_eAutodeleteRot == SBoneCompressionValues::eDelete_Yes ? "Delete    " : (bc.m_eAutodeleteRot == SBoneCompressionValues::eDelete_No ? "DontDelete" : "AutoDelete"));
		RCLog("[%3i] %s %s : %s", controllerIndex, pPos, pRot, pBoneName);
		RCLog(" pos:%20gx rot:%20gx", bc.m_compressPosTolerance, bc.m_compressRotToleranceInDegrees);
	}
}


static ICompressedQuat* CreateCompressedQuat(ECompressionFormat format, int count)
{ 
	switch(format)
	{
	case eNoCompress:
	case eNoCompressQuat:
		return new TNoCompressedQuat[count];

	case eShotInt3Quat:
		return new TShortInt3CompressedQuat[count];

	case eSmallTreeDWORDQuat:
		return new TSmallTreeDWORDCompressedQuat[count];

	case eSmallTree48BitQuat:
		return new TSmallTree48BitCompressedQuat[count];

	case eSmallTree64BitQuat:
		return new TSmallTree64BitCompressedQuat[count];

	case eSmallTree64BitExtQuat:
		return new TSmallTree64BitExtCompressedQuat[count];

	case ePolarQuat:
		return new TPolarQuatQuat[count];

	default:
		assert(0);
		RCLogError("Construction of an unsupported ICompressedQuat implementation has been requested.");
		return nullptr;
	}
}

static ICompressedVec3* CreateCompressedVec3(ECompressionFormat format, int count)
{
	switch(format)
	{
	case eNoCompressVec3:
	default:
		return new TBaseCompressedVec3[count];
	}

	return nullptr;
}


void CCompressonator::CompressAnimation(
	const GlobalAnimationHeaderCAF& header,
	GlobalAnimationHeaderCAF& newheader,
	const CSkeletonInfo* pSkeleton,
	const SPlatformAnimationSetup& platform,
	const SAnimationDesc& desc,
	bool bDebugCompression)
{
	if (bDebugCompression)
	{
		RCLog("Bone compression (%s) for %s:", (desc.m_bAdditiveAnimation ? "additive" : "override"), header.GetFilePath());
	}

	const uint32 controllerCount = header.m_arrController.size();

	std::vector<Operations> ops;
	{
		Operations o;
		if (!desc.m_bNewFormat && desc.oldFmt.m_CompressionQuality == 0)
		{
			o.m_ePosOperation = eOperation_DontCompress;
			o.m_eRotOperation = eOperation_DontCompress;
			o.m_eSclOperation = eOperation_DontCompress;
		}
		else
		{
			o.m_ePosOperation = eOperation_Compress;
			o.m_eRotOperation = eOperation_Compress;
			o.m_eSclOperation = eOperation_Compress;
		}

		ops.resize(controllerCount, o);
	}

	// Find deletable channels and modify ops[] accordingly
	for (uint32 c = 0; c < controllerCount; ++c)
	{
		if (c == 0 && !desc.m_bAdditiveAnimation)
		{
			// FIXME: why do we skip controller 0?
			continue;
		}

		const bool bOk = desc.m_bAdditiveAnimation // TODO: These calls only update the ops[c] flag, the code should be more explicit about it.
			? ComputeControllerDelete_Additive(ops[c], header, pSkeleton, c, platform, desc, bDebugCompression)
			: ComputeControllerDelete_Override(ops[c], header, pSkeleton, c, platform, desc, bDebugCompression);

		if (!bOk)
		{
			RCLogError("Pre-compression failed");
			return;
		}
	}

	// Compress/delete
	for (uint32 c = 0; c < controllerCount; ++c)
	{
		if (c == 0 && !desc.m_bAdditiveAnimation)
		{
			// FIXME: why do we skip controller 0?
			continue;
		}

		const bool bOk = desc.m_bAdditiveAnimation
			? CompressController_Additive(ops[c], header, newheader, pSkeleton, c, platform, desc, bDebugCompression)
			: CompressController_Override(ops[c], header, newheader, pSkeleton, c, platform, desc, bDebugCompression);

		if (!bOk)
		{
			RCLogError("Compression failed");
			return;
		}
	}
}


bool CCompressonator::ComputeControllerDelete_Override(
	Operations& operations,
	const GlobalAnimationHeaderCAF& header,
	const CSkeletonInfo* pSkeleton,
	uint32 controllerIndex,
	const SPlatformAnimationSetup& platform,
	const SAnimationDesc& desc,
	bool bDebugCompression)
{
	if (!pSkeleton || controllerIndex >= header.m_arrController.size())
	{
		assert(0);
		return false;
	}

	if (bDebugCompression)
	{
		PrintControllerInfo(header, pSkeleton, controllerIndex, platform, desc);
	}

	if (operations.m_ePosOperation == eOperation_Delete &&
		operations.m_eRotOperation == eOperation_Delete &&
		operations.m_eSclOperation == eOperation_Delete)
	{
		// TODO: This whole check is probably irrelevant (all operations are initialized to eOperation_Compress at this point).
		return true;
	}

	uint32 controllerId;
	if (!ObtainTrackData(header, controllerIndex, controllerId, m_rotations, m_positions, m_scales, m_rotTimes, m_posTimes, m_sclTimes))
	{
		return false;
	}

	const char* pBoneName = 0;
	const int boneNumber = FindBoneByControllerId(pSkeleton, controllerId, pBoneName);

	SBoneCompressionValues bc = desc.GetBoneCompressionValues(pBoneName, platform.m_compressionMultiplier);

	if (!pBoneName)
	{
		bc.m_eAutodeletePos = SBoneCompressionValues::eDelete_Yes;
		bc.m_eAutodeleteRot = SBoneCompressionValues::eDelete_Yes;
		bc.m_eAutodeleteScl = SBoneCompressionValues::eDelete_Yes;
	}

	if (boneNumber == 0)
	{
		if (bc.m_eAutodeletePos == SBoneCompressionValues::eDelete_Auto)
		{
			bc.m_eAutodeletePos = SBoneCompressionValues::eDelete_No;
		}
		if (bc.m_eAutodeleteRot == SBoneCompressionValues::eDelete_Auto)
		{
			bc.m_eAutodeleteRot = SBoneCompressionValues::eDelete_No;
		}
		if (bc.m_eAutodeleteScl == SBoneCompressionValues::eDelete_Auto)
		{
			bc.m_eAutodeleteScl = SBoneCompressionValues::eDelete_No;
		}
	}

	// TODO: None of these conditions are probably ever met (all operations are initialized to eOperation_Compress at this point).
	if (operations.m_ePosOperation == eOperation_Delete)
	{
		bc.m_eAutodeletePos = SBoneCompressionValues::eDelete_Yes;
	}
	if (operations.m_eRotOperation == eOperation_Delete)
	{
		bc.m_eAutodeleteRot = SBoneCompressionValues::eDelete_Yes;
	}
	if (operations.m_eSclOperation == eOperation_Delete)
	{
		bc.m_eAutodeleteScl = SBoneCompressionValues::eDelete_Yes;
	}

	if (desc.m_bNewFormat)
	{
		if (bc.m_eAutodeletePos == SBoneCompressionValues::eDelete_Auto || bc.m_eAutodeleteRot == SBoneCompressionValues::eDelete_Auto)
		{
			QuatTNS relRigValue;
			{
				const int32 parentIndex = boneNumber + pSkeleton->m_SkinningInfo.m_arrBonesDesc[boneNumber].m_nOffsetParent;
				assert(parentIndex >= 0);
				const Matrix34 p34 = pSkeleton->m_SkinningInfo.m_arrBonesDesc[parentIndex].m_DefaultB2W;
				const Matrix34 c34 = pSkeleton->m_SkinningInfo.m_arrBonesDesc[boneNumber].m_DefaultB2W;
				relRigValue = QuatTNS(p34.GetInverted() * c34);
				// TODO: Should we actually validate the scaling value at some point? It makes sense to keep it at 1.0 in the 'rig'.
			}

			if (bc.m_eAutodeletePos == SBoneCompressionValues::eDelete_Auto)
			{
				const float maxDistSquared = bc.m_autodeletePosEps * bc.m_autodeletePosEps;

				auto sameAsRigValue = [&](const Vec3& x) { return !error_vec3(relRigValue.t, x, maxDistSquared); };

				if (std::all_of(m_positions.begin(), m_positions.end(), sameAsRigValue))
				{
					bc.m_eAutodeletePos = SBoneCompressionValues::eDelete_Yes;
				}
				else
				{
					bc.m_eAutodeletePos = SBoneCompressionValues::eDelete_No;
				}
			}

			if (bc.m_eAutodeleteRot == SBoneCompressionValues::eDelete_Auto)
			{
				const float maxAngleInRadians = Util::getClamped(DEG2RAD(bc.m_autodeleteRotEps), 0.0f, (float)gf_PI);
				const float cosOfHalfMaxAngle = cosf(maxAngleInRadians * 0.5f);

				auto sameAsRigValue = [&](const Quat& x) { return !error_quatCos(relRigValue.q, x, cosOfHalfMaxAngle); };

				if (std::all_of(m_rotations.begin(), m_rotations.end(), sameAsRigValue))
				{
					bc.m_eAutodeleteRot = SBoneCompressionValues::eDelete_Yes;
				}
				else
				{
					bc.m_eAutodeleteRot = SBoneCompressionValues::eDelete_No;
				}
			}

			if (bc.m_eAutodeleteScl == SBoneCompressionValues::eDelete_Auto)
			{
				auto sameAsRigValue = [&](const Diag33& x) { return IsEquivalent(x, relRigValue.s, bc.m_autodeleteSclEps); };

				if (std::all_of(m_scales.begin(), m_scales.end(), sameAsRigValue))
				{
					bc.m_eAutodeleteScl = SBoneCompressionValues::eDelete_Yes;
				}
				else
				{
					bc.m_eAutodeleteScl = SBoneCompressionValues::eDelete_No;
				}
			}
		}
	}
	else
	{
		const Quat firstKeyRot = m_rotations[0];
		const Vec3 firstKeyPos = m_positions[0];
		const Diag33 firstKeyScl = m_scales[0];

		if (bc.m_eAutodeleteRot == SBoneCompressionValues::eDelete_Auto)
		{
			auto differentFromFirstKey = [&](const Quat& x) { return !IsQuatEquivalent_dot(firstKeyRot, x, bc.m_autodeleteRotEps); };

			if (std::any_of(m_rotations.begin() + 1, m_rotations.end(), differentFromFirstKey))
			{
				bc.m_eAutodeleteRot = SBoneCompressionValues::eDelete_No;
			}
		}

		if (bc.m_eAutodeletePos == SBoneCompressionValues::eDelete_Auto)
		{
			f32 pos_epsilon;
			{
				// TODO: Move the epsilon computation to a separate function.
				const int32 pidx = boneNumber + pSkeleton->m_SkinningInfo.m_arrBonesDesc[boneNumber].m_nOffsetParent;
				assert(pidx >= 0);
				const Vec3 p34 = pSkeleton->m_SkinningInfo.m_arrBonesDesc[    pidx    ].m_DefaultB2W.GetTranslation();
				const Vec3 c34 = pSkeleton->m_SkinningInfo.m_arrBonesDesc[ boneNumber ].m_DefaultB2W.GetTranslation();
				const Vec3 BoneSegment = p34 - c34;
				pos_epsilon = (std::min)((BoneSegment * bc.m_autodeletePosEps).GetLength(), 0.02f); 
			}

			auto differentFromFirstKey = [&](const Vec3& x) { return !firstKeyPos.IsEquivalent(x, pos_epsilon); };

			if (std::any_of(m_positions.begin() + 1, m_positions.end(), differentFromFirstKey))
			{
				bc.m_eAutodeletePos = SBoneCompressionValues::eDelete_No;
			}
		}

		if (bc.m_eAutodeleteScl == SBoneCompressionValues::eDelete_Auto)
		{
			auto differentFromFirstKey = [&](const Diag33& x) { return !IsEquivalent(x, firstKeyScl, bc.m_autodeleteSclEps); };

			if (std::any_of(m_scales.begin() + 1, m_scales.end(), differentFromFirstKey))
			{
				bc.m_eAutodeleteScl = SBoneCompressionValues::eDelete_No;
			}
		}

		if (bc.m_eAutodeletePos == SBoneCompressionValues::eDelete_Auto || bc.m_eAutodeleteRot == SBoneCompressionValues::eDelete_Auto || bc.m_eAutodeleteScl == SBoneCompressionValues::eDelete_Auto)
		{
			// compare with reference model (rig)

			f32 pos_epsilon;
			QuatTNS relRigValue;
			{
				const int32 offset = pSkeleton->m_SkinningInfo.m_arrBonesDesc[boneNumber].m_nOffsetParent;
				const int32 pidx = boneNumber+offset;
				assert(pidx>-1);
				const Matrix34 p34 = pSkeleton->m_SkinningInfo.m_arrBonesDesc[ pidx ].m_DefaultB2W;
				const Matrix34 c34 = pSkeleton->m_SkinningInfo.m_arrBonesDesc[ boneNumber ].m_DefaultB2W;
				relRigValue = QuatTNS(p34.GetInverted() * c34);
				pos_epsilon = (std::min)((relRigValue.t * bc.m_autodeletePosEps).GetLength(), 0.02f);
				// TODO: Should we actually validate the scaling value at some point? It makes sense to keep it at 1.0 in the 'rig'.
				// TODO: What about non-uniform scaling?
			}
	
			if (bc.m_eAutodeleteRot == SBoneCompressionValues::eDelete_Auto)
			{
				if (IsQuatEquivalent_dot(firstKeyRot, relRigValue.q, bc.m_autodeleteRotEps))
				{
					bc.m_eAutodeleteRot = SBoneCompressionValues::eDelete_Yes;
				}
				else
				{
					bc.m_eAutodeleteRot = SBoneCompressionValues::eDelete_No;
				}
			}

			if (bc.m_eAutodeletePos == SBoneCompressionValues::eDelete_Auto)
			{
				if (firstKeyPos.IsEquivalent(relRigValue.t, (std::max)(pos_epsilon, 0.0001f)))
				{
					bc.m_eAutodeletePos = SBoneCompressionValues::eDelete_Yes;
				}
				else
				{
					bc.m_eAutodeletePos = SBoneCompressionValues::eDelete_No;
				}
			}

			if (bc.m_eAutodeleteScl == SBoneCompressionValues::eDelete_Auto)
			{
				if (IsEquivalent(firstKeyScl, relRigValue.s, bc.m_autodeleteSclEps))
				{
					bc.m_eAutodeleteScl = SBoneCompressionValues::eDelete_Yes;
				}
				else
				{
					bc.m_eAutodeleteScl = SBoneCompressionValues::eDelete_No;
				}
			}
		}
	}

	if (bc.m_eAutodeletePos == SBoneCompressionValues::eDelete_Yes)
	{
		operations.m_ePosOperation = eOperation_Delete;
	}

	if (bc.m_eAutodeleteRot == SBoneCompressionValues::eDelete_Yes)
	{
		operations.m_eRotOperation = eOperation_Delete;
	}

	if (bc.m_eAutodeleteScl == SBoneCompressionValues::eDelete_Yes)
	{
		operations.m_eSclOperation = eOperation_Delete;
	}

	return true;
}


bool CCompressonator::CompressController_Override(
	const Operations& operations,
	const GlobalAnimationHeaderCAF& header,
	GlobalAnimationHeaderCAF& newheader,
	const CSkeletonInfo* pSkeleton,
	uint32 controllerIndex,
	const SPlatformAnimationSetup& platform,
	const SAnimationDesc& desc,
	bool bDebugCompression)
{
	if (!pSkeleton || controllerIndex >= header.m_arrController.size())
	{
		assert(0);
		return false;
	}

	if (operations.m_ePosOperation == eOperation_Delete &&
		operations.m_eRotOperation == eOperation_Delete &&
		operations.m_eSclOperation == eOperation_Delete)
	{
		newheader.m_arrController[controllerIndex] = 0;
		return true;
	}

	uint32 controllerId;
	if (!ObtainTrackData(header, controllerIndex, controllerId, m_rotations, m_positions, m_scales, m_rotTimes, m_posTimes, m_sclTimes))
	{
		return false;
	}

	const char* pBoneName = 0;
	const int boneNumber = FindBoneByControllerId(pSkeleton, controllerId, pBoneName);
	if (boneNumber < 0 || pBoneName == 0)
	{
		assert(0);
		return false;
	}

	const SBoneCompressionValues bc = desc.GetBoneCompressionValues(pBoneName, platform.m_compressionMultiplier);

	if (header.m_arrController[controllerIndex]->GetID() != controllerId)
	{
		assert(0);
		RCLogError("controller id mismatch");
		return false;
	}

	CompressController(
		bc.m_compressPosTolerance, bc.m_compressRotToleranceInDegrees, bc.m_compressSclTolerance,
		operations,
		newheader,
		controllerIndex,
		header.m_arrController[controllerIndex]->GetID());
	return true;
}


bool CCompressonator::ComputeControllerDelete_Additive(
	Operations& operations,
	const GlobalAnimationHeaderCAF& header,
	const CSkeletonInfo* pSkeleton,
	uint32 controllerIndex,
	const SPlatformAnimationSetup& platform,
	const SAnimationDesc& desc,
	bool bDebugCompression)
{
	if (!pSkeleton || controllerIndex >= header.m_arrController.size())
	{
		assert(0);
		return false;
	}

	if (bDebugCompression)
	{
		PrintControllerInfo(header, pSkeleton, controllerIndex, platform, desc);
	}

	if (operations.m_ePosOperation == eOperation_Delete &&
		operations.m_eRotOperation == eOperation_Delete &&
		operations.m_eSclOperation == eOperation_Delete)
	{
		// TODO: This whole check is probably irrelevant (all operations are initialized to eOperation_Compress at this point).
		return true;
	}

	uint32 controllerId;
	if (!ObtainTrackData(header, controllerIndex, controllerId, m_rotations, m_positions, m_scales, m_rotTimes, m_posTimes, m_sclTimes))
	{
		return false;
	}

	const char* pBoneName = 0;
	const int boneNumber = FindBoneByControllerId(pSkeleton, controllerId, pBoneName);

	SBoneCompressionValues bc;
	if (controllerIndex > 0 && boneNumber >= 0 && pBoneName)
	{
		bc = desc.GetBoneCompressionValues(pBoneName, platform.m_compressionMultiplier);
	}
	else
	{
		bc.m_eAutodeletePos = SBoneCompressionValues::eDelete_Yes;
		bc.m_eAutodeleteRot = SBoneCompressionValues::eDelete_Yes;
		bc.m_eAutodeleteScl = SBoneCompressionValues::eDelete_Yes;
	}

	const QuatTNS additiveBase = ComputeAdditiveBaseFrame(header, controllerId);

	if (bc.m_eAutodeleteRot != SBoneCompressionValues::eDelete_Yes && m_rotations.size() <= 1)
	{
		assert(0);
		RCLogError("Found a single-frame additive animation (rotation). This is not supported, as it would mean the additive animation is empty.");
		bc.m_eAutodeleteRot = SBoneCompressionValues::eDelete_Yes;
	}

	if (bc.m_eAutodeleteRot == SBoneCompressionValues::eDelete_Auto)
	{
		std::function<bool(const Quat&)> sameAsAdditiveBase;
		if (desc.m_bNewFormat)
		{
			const Quat identity(IDENTITY);
			const float maxAngleInRadians = Util::getClamped(DEG2RAD(bc.m_autodeleteRotEps), 0.0f, (float)gf_PI);
			const float cosOfHalfMaxAngle = cosf(maxAngleInRadians * 0.5f);
			sameAsAdditiveBase = [=](const Quat& x) { return !error_quatCos((x * !additiveBase.q), identity, cosOfHalfMaxAngle); };
		}
		else
		{
			const Quat identity(IDENTITY);
			sameAsAdditiveBase = [=](const Quat& x) { return fabsf(((x * !additiveBase.q) | identity) - 1.0f) <= bc.m_autodeleteRotEps; };
		}

		bc.m_eAutodeleteRot = 
			std::all_of(m_rotations.begin(), m_rotations.end(), sameAsAdditiveBase)
			? SBoneCompressionValues::eDelete_Yes
			: SBoneCompressionValues::eDelete_No;
	}

	if (bc.m_eAutodeletePos != SBoneCompressionValues::eDelete_Yes && m_positions.size() <= 1)
	{
		assert(0);
		RCLogError("Found a single-frame additive animation (position). This is not supported, as it would mean the additive animation is empty.");
		bc.m_eAutodeletePos = SBoneCompressionValues::eDelete_Yes;
	}

	if (bc.m_eAutodeletePos == SBoneCompressionValues::eDelete_Auto)
	{
		std::function<bool(const Vec3&)> sameAsAdditiveBase;
		if (desc.m_bNewFormat)
		{
			const float maxDistSquared = bc.m_autodeletePosEps * bc.m_autodeletePosEps;
			sameAsAdditiveBase = [=](const Vec3& x) { return !error_vec3(additiveBase.t, x, maxDistSquared); };
		}
		else
		{
			const float length = additiveBase.t.GetLength();
			sameAsAdditiveBase = [=](const Vec3& x) { return (x - additiveBase.t).GetLength() <= (length * bc.m_autodeletePosEps); };
		}

		bc.m_eAutodeletePos = 
			std::all_of(m_positions.begin(), m_positions.end(), sameAsAdditiveBase)
			? SBoneCompressionValues::eDelete_Yes
			: SBoneCompressionValues::eDelete_No;
	}

	if (bc.m_eAutodeleteScl != SBoneCompressionValues::eDelete_Yes && m_scales.size() <= 1)
	{
		assert(0);
		RCLogError("Found a single-frame additive animation (scaling). This is not supported, as it would mean the additive animation is empty.");
		bc.m_eAutodeleteScl = SBoneCompressionValues::eDelete_Yes;
	}

	if (bc.m_eAutodeleteScl == SBoneCompressionValues::eDelete_Auto)
	{
		const Diag33 identity(IDENTITY);
		auto sameAsAdditiveBase = [=](const Diag33& x) { return IsEquivalent((x / additiveBase.s), identity, bc.m_autodeleteSclEps); };
	
		bc.m_eAutodeleteScl = 
			std::all_of(m_scales.begin(), m_scales.end(), sameAsAdditiveBase)
			? SBoneCompressionValues::eDelete_Yes
			: SBoneCompressionValues::eDelete_No;
	}

	if (bc.m_eAutodeletePos == SBoneCompressionValues::eDelete_Yes)
	{
		operations.m_ePosOperation = eOperation_Delete;
	}
	if (bc.m_eAutodeleteRot == SBoneCompressionValues::eDelete_Yes)
	{
		operations.m_eRotOperation = eOperation_Delete;
	}
	if (bc.m_eAutodeleteScl == SBoneCompressionValues::eDelete_Yes)
	{
		operations.m_eSclOperation = eOperation_Delete;
	}

	return true;
}


bool CCompressonator::CompressController_Additive(
	const Operations& operations,
	const GlobalAnimationHeaderCAF& header,
	GlobalAnimationHeaderCAF& newheader,
	const CSkeletonInfo* pSkeleton,
	uint32 controllerIndex,
	const SPlatformAnimationSetup& platform,
	const SAnimationDesc& desc,
	bool bDebugCompression)
{
	if (!pSkeleton || controllerIndex >= header.m_arrController.size())
	{
		assert(0);
		return false;
	}

	if (operations.m_ePosOperation == eOperation_Delete &&
		operations.m_eRotOperation == eOperation_Delete &&
		operations.m_eSclOperation == eOperation_Delete)
	{
		newheader.m_arrController[controllerIndex] = 0;
		return true;
	}

	uint32 controllerId;
	if (!ObtainTrackData(header, controllerIndex, controllerId, m_rotations, m_positions, m_scales, m_rotTimes, m_posTimes, m_sclTimes))
	{
		return false;
	}

	const char* pBoneName = 0;
	const int boneNumber = FindBoneByControllerId(pSkeleton, controllerId, pBoneName);
	if (boneNumber < 0 || pBoneName == 0)
	{
		assert(0);
		return false;
	}

	const QuatTNS additiveBase = ComputeAdditiveBaseFrame(header, controllerId);

	if (operations.m_eRotOperation != eOperation_Delete)
	{
		const uint32 numRotKeys = m_rotations.size();
		for (uint32 k = 1; k < numRotKeys; k++)
		{
			m_rotations[k] = m_rotations[k] * !additiveBase.q;
		}
		m_rotations.erase(m_rotations.begin());
		m_rotTimes.erase(m_rotTimes.begin());
	}
	if (operations.m_ePosOperation != eOperation_Delete)
	{
		const uint32 numPosKeys = m_positions.size();
		for (uint32 k = 1; k < numPosKeys; k++)
		{
			m_positions[k] = m_positions[k] - additiveBase.t;
		}
		m_positions.erase(m_positions.begin());
		m_posTimes.erase(m_posTimes.begin());
	}
	if (operations.m_eSclOperation != eOperation_Delete)
	{
		const uint32 numSclKeys = m_scales.size();
		for (uint32 k = 1; k < numSclKeys; k++)
		{
			m_scales[k] = m_scales[k] / additiveBase.s;
		}
		m_scales.erase(m_scales.begin());
		m_sclTimes.erase(m_sclTimes.begin());
	}

	const SBoneCompressionValues bc = desc.GetBoneCompressionValues(pBoneName, platform.m_compressionMultiplier);

	if (header.m_arrController[controllerIndex]->GetID() != controllerId)
	{
		assert(0);
		RCLogError("controller id mismatch");
		return false;
	}

	CompressController(
		bc.m_compressPosTolerance, bc.m_compressRotToleranceInDegrees, bc.m_compressSclTolerance,
		operations, 
		newheader, 
		controllerIndex, 
		header.m_arrController[controllerIndex]->GetID());
	return true;
}


void CCompressonator::CompressController(
	float positionTolerance,
	float rotationToleranceInDegrees,
	float scaleTolerance,
	const Operations& operations,
	GlobalAnimationHeaderCAF& newheader,
	uint32 controllerIndex,
	uint32 controllerId)
{
	if (operations.m_ePosOperation == eOperation_Delete &&
		operations.m_eRotOperation == eOperation_Delete && 
		operations.m_eSclOperation == eOperation_Delete)
	{
		// TODO: We should probably move this piece of logic one level above (will also remove the dependency on GlobalAnimationHeaderCAF).
		newheader.m_arrController[controllerIndex] = 0;
		return;
	}

	const ECompressionFormat positionFormat = eNoCompressVec3;
	const ECompressionFormat rotationFormat = (rotationToleranceInDegrees == 0) ? eSmallTree64BitExtQuat : eAutomaticQuat;
	const ECompressionFormat scaleFormat = eNoCompressVec3;

	// We will try few rotation formats and choose one that produces smallest size
	ECompressionFormat formats[] = 
	{
		eNoCompressQuat,
		eSmallTree48BitQuat,
		eSmallTree64BitExtQuat
	}; 

	uint32 formatCount = sizeof(formats) / sizeof(formats[0]);
	if (rotationFormat != eAutomaticQuat)
	{
		formatCount = 1;
		formats[0] = rotationFormat;
	}

	std::vector<_smart_ptr<CController>> newControllers(formatCount);

	uint32 minSize = -1;
	uint32 bestIndex;

	// TODO: Remove scaling and translation controllers from the optimization loop.
	for (uint32 formatIndex = 0; formatIndex < formatCount; ++formatIndex)
	{
		newControllers[formatIndex] = new CController;
		newControllers[formatIndex]->SetID(controllerId);

		if (operations.m_eRotOperation != eOperation_Delete)
		{
			newControllers[formatIndex]->SetRotationController(CompressRotations(formats[formatIndex], (operations.m_eRotOperation == eOperation_Compress), rotationToleranceInDegrees));
		}

		if (operations.m_ePosOperation != eOperation_Delete)
		{
			// Note: position format is the same for each loop iteration, so we need to compress it only once
			if (formatIndex == 0)
			{
				newControllers[formatIndex]->SetPositionController(CompressPositions(positionFormat, (operations.m_ePosOperation == eOperation_Compress), positionTolerance));
			}
			else
			{
				newControllers[formatIndex]->SetPositionController(newControllers[0]->GetPositionController());
			}
		}

		if (operations.m_eSclOperation != eOperation_Delete)
		{
			// Note: scale format is the same for each loop iteration, so we need to compress it only once
			if (formatIndex == 0)
			{
				newControllers[formatIndex]->SetScaleController(CompressScales(scaleFormat, (operations.m_eSclOperation == eOperation_Compress), scaleTolerance));
			}
			else
			{
				newControllers[formatIndex]->SetScaleController(newControllers[0]->GetScaleController());
			}
		}

		const uint32 currentSize = newControllers[formatIndex]->SizeOfThis();
		if (currentSize < minSize)
		{
			minSize = currentSize;
			bestIndex = formatIndex;
		}
	}

	newheader.m_arrController[controllerIndex] = newControllers[bestIndex];
}

RotationControllerPtr CCompressonator::CompressRotations(ECompressionFormat rotationFormat, bool bRemoveKeys, float rotationToleranceInDegrees)
{
	const uint32 numKeys = m_rotations.size();
	assert(numKeys > 0);

	KeyTimesInformationPtr pTimes;
	if (m_rotTimes[0] >= 0 && m_rotTimes[numKeys-1] < 256)
	{
		pTimes = new ByteKeyTimesInformation;
	}
	else if (m_rotTimes[0] >= 0 && m_rotTimes[numKeys-1] < 65536)
	{
		pTimes = new UINT16KeyTimesInformation;
	}
	else
	{
		pTimes = new F32KeyTimesInformation;
	}

	TrackRotationStoragePtr pStorage = ControllerHelper::ConstructRotationController(rotationFormat);

	unsigned int iFirstKey = 0;

	pTimes->AddKeyTime(f32(m_rotTimes[iFirstKey]));
	pStorage->AddValue(m_rotations[iFirstKey]);

	if (bRemoveKeys)
	{
		std::auto_ptr<ICompressedQuat> compressed(CreateCompressedQuat(rotationFormat, 1));
		Quat first, last, interpolated;

		const float maxAngleInRadians = Util::getClamped(DEG2RAD(rotationToleranceInDegrees), 0.0f, (float)gf_PI);
		const float cosOfHalfMaxAngle = cosf(maxAngleInRadians * 0.5f);

		while (iFirstKey + 2 < numKeys)
		{
			compressed->FromQuat(m_rotations[iFirstKey]);
			first = compressed->ToQuat();

			unsigned int iLastKey;
			for(iLastKey = iFirstKey + 2; iLastKey < numKeys; ++iLastKey)
			{
				compressed->FromQuat(m_rotations[iLastKey]);
				last = compressed->ToQuat();

				const unsigned int count = iLastKey - iFirstKey;
				for (unsigned int i = 1; i < count; ++i)
				{
					interpolated.SetNlerp(first, last, i / (float)count);

					if (error_quatCos(interpolated, m_rotations[iFirstKey + i], cosOfHalfMaxAngle))
					{
						goto addKey;
					}
				}				
			}
addKey:
			iFirstKey = iLastKey - 1;
			pTimes->AddKeyTime(f32(m_rotTimes[iFirstKey]));
			pStorage->AddValue(m_rotations[iFirstKey]);
		}
	}

	for (++iFirstKey; iFirstKey < numKeys; ++iFirstKey)
	{
		pTimes->AddKeyTime(f32(m_rotTimes[iFirstKey]));
		pStorage->AddValue(m_rotations[iFirstKey]);
	}

	RotationControllerPtr pNewTrack = RotationControllerPtr(new RotationTrackInformation);
	pNewTrack->SetRotationStorage(pStorage);
	pNewTrack->SetKeyTimesInformation(pTimes);
	return pNewTrack;
}


PositionControllerPtr CCompressonator::CompressPositions(ECompressionFormat positionFormat, bool bRemoveKeys, float positionTolerance)
{
	const uint32 numKeys = m_positions.size();
	assert(numKeys > 0);

	KeyTimesInformationPtr pTimes;
	if (m_posTimes[0] >= 0 && m_posTimes[numKeys - 1] < 256)
	{
		pTimes = new ByteKeyTimesInformation;
	}
	else if (m_posTimes[0] >= 0 && m_posTimes[numKeys - 1] < 65536)
	{
		pTimes = new UINT16KeyTimesInformation;
	}
	else
	{
		pTimes = new F32KeyTimesInformation;
	}

	TrackPositionStoragePtr pStorage = ControllerHelper::ConstructPositionController(positionFormat);

	unsigned int iFirstKey = 0;

	pTimes->AddKeyTime(f32(m_posTimes[iFirstKey]));
	pStorage->AddValue(m_positions[iFirstKey]);

	if (bRemoveKeys)
	{
		std::auto_ptr<ICompressedVec3> compressed(CreateCompressedVec3(positionFormat, 1));
		Vec3 first, last, interpolated;

		const float squaredPositionTolerance = positionTolerance * positionTolerance;

		while (iFirstKey + 2 < numKeys)
		{
			compressed->FromVec3(m_positions[iFirstKey]);
			first = compressed->ToVec3();

			unsigned int iLastKey;
			for(iLastKey = iFirstKey + 2; iLastKey < numKeys; ++iLastKey)
			{
				compressed->FromVec3(m_positions[iLastKey]);
				last = compressed->ToVec3();

				const unsigned int count = iLastKey - iFirstKey;
				for (unsigned int i = 1; i < count; ++i)
				{
					interpolated.SetLerp(first, last, i / (float)count);

					if (error_vec3(interpolated, m_positions[iFirstKey + i], squaredPositionTolerance))
					{
						goto addKey;
					}
				}
			}
addKey:
			iFirstKey = iLastKey - 1;
			pTimes->AddKeyTime(f32(m_posTimes[iFirstKey]));
			pStorage->AddValue(m_positions[iFirstKey]);
		}
	}

	for (++iFirstKey; iFirstKey < numKeys; ++iFirstKey)
	{
		pTimes->AddKeyTime(f32(m_posTimes[iFirstKey]));
		pStorage->AddValue(m_positions[iFirstKey]);
	}

	PositionControllerPtr pNewTrack = PositionControllerPtr(new PositionTrackInformation);
	pNewTrack->SetPositionStorage(pStorage);
	pNewTrack->SetKeyTimesInformation(pTimes);
	return pNewTrack;
}

PositionControllerPtr CCompressonator::CompressScales(ECompressionFormat scaleFormat, bool shouldRemoveKyes, float scaleTolerance)
{
	const uint32 numKeys = m_scales.size();
	assert(numKeys > 0);

	KeyTimesInformationPtr pTimes;
	if (m_sclTimes[0] >= 0 && m_sclTimes[numKeys - 1] < 256)
	{
		pTimes = new ByteKeyTimesInformation;
	}
	else if (m_sclTimes[0] >= 0 && m_sclTimes[numKeys - 1] < 65536)
	{
		pTimes = new UINT16KeyTimesInformation;
	}
	else
	{
		pTimes = new F32KeyTimesInformation;
	}

	TrackPositionStoragePtr pStorage = ControllerHelper::ConstructScaleController(scaleFormat);

	unsigned int iFirstKey = 0;

	pTimes->AddKeyTime(f32(m_sclTimes[iFirstKey]));
	pStorage->AddValue(Diag33ToVec3(m_scales[iFirstKey]));

	if (shouldRemoveKyes)
	{
		std::auto_ptr<ICompressedVec3> compressed(CreateCompressedVec3(scaleFormat, 1));
		Vec3 first, last, interpolated;

		while (iFirstKey + 2 < numKeys)
		{
			compressed->FromVec3(Diag33ToVec3(m_scales[iFirstKey]));
			first = compressed->ToVec3();

			unsigned int iLastKey;
			for(iLastKey = iFirstKey + 2; iLastKey < numKeys; ++iLastKey)
			{
				compressed->FromVec3(Diag33ToVec3(m_scales[iLastKey]));
				last = compressed->ToVec3();

				const unsigned int count = iLastKey - iFirstKey;
				for (unsigned int i = 1; i < count; ++i)
				{
					interpolated.SetLerp(first, last, i / (float)count);

					if (!interpolated.IsEquivalent(Diag33ToVec3(m_scales[iFirstKey + i]), scaleTolerance))
					{
						goto addKey;
					}
				}
			}
addKey:
			iFirstKey = iLastKey - 1;
			pTimes->AddKeyTime(f32(m_sclTimes[iFirstKey]));
			pStorage->AddValue(Diag33ToVec3(m_scales[iFirstKey]));
		}
	}

	for (++iFirstKey; iFirstKey < numKeys; ++iFirstKey)
	{
		pTimes->AddKeyTime(f32(m_sclTimes[iFirstKey]));
		pStorage->AddValue(Diag33ToVec3(m_scales[iFirstKey]));
	}

	PositionControllerPtr pNewTrack = PositionControllerPtr(new PositionTrackInformation);
	pNewTrack->SetPositionStorage(pStorage);
	pNewTrack->SetKeyTimesInformation(pTimes);
	return pNewTrack;
}

QuatTNS CCompressonator::ComputeAdditiveBaseFrame(const GlobalAnimationHeaderCAF& header, const uint32 controllerId) const
{
	// By a convention, we will compute additive animation as difference between
	// a key and the first key. Note that after such computation the first frame
	// will always contain zeros. So after computation we delete the first frame
	// (animators are expected to be aware of that).

	QuatTNS out;
	out.q = m_rotations[0];
	out.t = m_positions[0];
	out.s = m_scales[0];

	if (std::abs(out.s.x) <= std::numeric_limits<float>::epsilon())
	{
		out.s.x = 1.0f;
		RCLogWarning("Controller '%s' contains a scale.x value of 0.0 in its additive animation reference frame.", header.GetOriginalControllerName(controllerId));
	}

	if (std::abs(out.s.y) <= std::numeric_limits<float>::epsilon())
	{
		out.s.y = 1.0f;
		RCLogWarning("Controller '%s' contains a scale.y value of 0.0 in its additive animation reference frame.", header.GetOriginalControllerName(controllerId));
	}

	if (std::abs(out.s.z) <= std::numeric_limits<float>::epsilon())
	{
		out.s.z = 1.0f;
		RCLogWarning("Controller '%s' contains a scale.z value of 0.0 in its additive animation reference frame.", header.GetOriginalControllerName(controllerId));
	}

	return out;
}
