// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "FileUtil.h"
#include "AnimationLoader.h"
#include "CompressionController.h"
#include "AnimationManager.h"
#include "TrackStorage.h"
#include "AnimationCompiler.h"
#include "../CryEngine/Cry3DEngine/CGF/ChunkData.h"
#include "FileUtil.h"
#include "SkeletonHelpers.h"


CAnimationCompressor::CAnimationCompressor(const CSkeletonInfo& skeleton)
	: m_skeleton(skeleton)  // copy skeleton, we are going to change root
{	
	CSkinningInfo& info = m_skeleton.m_SkinningInfo;
	if (info.m_arrBonesDesc.size() >= 2)
	{
		info.m_arrBonesDesc[0].m_DefaultB2W.SetIdentity();
		info.m_arrBonesDesc[0].m_DefaultW2B.SetIdentity();
		info.m_arrBonesDesc[1].m_DefaultW2B = info.m_arrBonesDesc[0].m_DefaultB2W.GetInverted();
	}
	else
	{
		RCLogWarning("Suspicious number of bones in skeleton: %d", int(info.m_arrBonesDesc.size()));
	}

	m_bAlignTracks = false;
	m_rootMotionJointName = "Locator_Locomotion";
}


bool CAnimationCompressor::LoadCAF(const char* name, const SPlatformAnimationSetup& platform, const SAnimationDesc& animDesc, ECAFLoadMode loadMode)
{
	if (m_GlobalAnimationHeader.LoadCAF(name, loadMode, &m_ChunkFile))
	{
		m_filename = name;
		m_platformSetup = platform;
		m_AnimDesc = animDesc;

		if (loadMode == eCAFLoadUncompressedOnly)
		{
			DeleteCompressedChunks();
		}
		return true;
	}
	return false;
}

uint32 CAnimationCompressor::AddCompressedChunksToFile(const char* name, bool bigEndianOutput)
{
	FILETIME oldTimeStamp = FileUtil::GetLastWriteFileTime(name);
	CSaverCGF cgfSaver(m_ChunkFile);

	//save information here

	SaveMotionParameters(&m_ChunkFile, bigEndianOutput);
	SaveControllers(cgfSaver, bigEndianOutput);
	SetFileAttributes( name,FILE_ATTRIBUTE_ARCHIVE );
	m_ChunkFile.Write( name );

	FileUtil::SetFileTimes(name, oldTimeStamp);

	const __int64 fileSize = FileUtil::GetFileSize(name);
	if (fileSize < 0)
	{
		RCLogError("Failed to get file size of '%s'", name);
		return ~0;
	}

	if (fileSize > 0xffFFffFFU)
	{
		RCLogError( "Unexpected huge file '%s' found", name);
		return ~0;
	}

	return (uint32)fileSize;
}


uint32 CAnimationCompressor::SaveOnlyCompressedChunksInFile( const char * name, FILETIME timeStamp, GlobalAnimationHeaderAIM* gaim, bool saveCAFHeader, bool bigEndianFormat)
{
	CChunkFile chunkFile;
	CSaverCGF cgfSaver(chunkFile);

	//save information here

	SaveMotionParameters(&chunkFile, bigEndianFormat);
	const uint32 expectedControlledCount = m_GlobalAnimationHeader.m_nCompressedControllerCount;
	m_GlobalAnimationHeader.m_nCompressedControllerCount = SaveControllers(cgfSaver, bigEndianFormat);
	if (expectedControlledCount != m_GlobalAnimationHeader.m_nCompressedControllerCount)
	{
		if (expectedControlledCount > m_GlobalAnimationHeader.m_nCompressedControllerCount)
		{
			RCLogError("Unexpected empty controllers (%u -> %u)", expectedControlledCount, m_GlobalAnimationHeader.m_nCompressedControllerCount);
		}
		else
		{
			RCLogError("Weird failure: controllers %u -> %u", expectedControlledCount, m_GlobalAnimationHeader.m_nCompressedControllerCount);
		}
	}
	if (gaim)
	{
		gaim->SaveToChunkFile(&chunkFile, bigEndianFormat);
	}
	if (saveCAFHeader)
	{
		m_GlobalAnimationHeader.SaveToChunkFile(&chunkFile, bigEndianFormat);
	}
	SetFileAttributes( name,FILE_ATTRIBUTE_ARCHIVE );

	chunkFile.Write( name );

	FileUtil::SetFileTimes(name, timeStamp);

	const __int64 fileSize = FileUtil::GetFileSize(name);
	if (fileSize < 0)
	{
		RCLogError("Failed to get file size of '%s'", name);
		return ~0;
	}

	if (fileSize > 0xffFFffFFU)
	{
		RCLogError( "Unexpected huge file '%s' found", name);
		return ~0;
	}

	return (uint32)fileSize;
}


// Modified version of AnalyseAndModifyAnimations function
bool CAnimationCompressor::ProcessAnimation(bool debugCompression)
{
	EvaluateStartLocation();

	const float minRootPosEpsilon = m_AnimDesc.m_bNewFormat ? FLT_MAX : m_AnimDesc.oldFmt.m_fPOS_EPSILON;
	const float minRootRotEpsilon = m_AnimDesc.m_bNewFormat ? FLT_MAX : m_AnimDesc.oldFmt.m_fROT_EPSILON;
	const float minRootSclEpsilon = m_AnimDesc.m_bNewFormat ? FLT_MAX : m_AnimDesc.oldFmt.m_fSCL_EPSILON;

	ReplaceRootByLocator(true, minRootPosEpsilon, minRootRotEpsilon, minRootSclEpsilon);   //replace root by locator, apply simple compression to the root-joint and re-transform all children
	ReplaceRootByLocator(false, minRootPosEpsilon, minRootRotEpsilon, minRootSclEpsilon);  //apply simple compression to the root-joint and re-transform all children

	DetectCycleAnimations();
	EvaluateSpeed();              // obsolete and not used in C3, but still used in Warface, so we can't erase it

	uint32 numJoints = m_skeleton.m_SkinningInfo.m_arrBonesDesc.size();
	uint32 numController = m_GlobalAnimationHeader.m_arrController.size();
	if (numController > numJoints)
	{
		numJoints = numController;
	}

	CCompressonator compressonator;
	m_GlobalAnimationHeader.m_nCompression = m_AnimDesc.oldFmt.m_CompressionQuality;  // FIXME: Sokov: it seems that it's not used. remove this line?
	compressonator.CompressAnimation(m_GlobalAnimationHeader, m_GlobalAnimationHeader, &m_skeleton, m_platformSetup, m_AnimDesc, debugCompression);

	return true;
}


void CAnimationCompressor::ReplaceRootByLocator(bool bCheckLocator, float minRootPosEpsilon, float minRootRotEpsilon, float minRootSclEpsilon)
{
	const uint32 numJoints = m_skeleton.m_SkinningInfo.m_arrBonesDesc.size();
	if (numJoints==0)
	{
		return; 
	}

	const char* const szLocatorName = m_rootMotionJointName.c_str();
	IController* pLocatorController = m_GlobalAnimationHeader.GetController(SkeletonHelpers::ComputeControllerId(szLocatorName));
	if (bCheckLocator) 
	{
		if (!pLocatorController)
		{
			ConvertRemainingControllers(m_GlobalAnimationHeader.m_arrController);
			return;
		}
	}

	const char* const pRootName = m_skeleton.m_SkinningInfo.m_arrBonesDesc[0].m_arrBoneName;
	const uint32 RootControllerID = SkeletonHelpers::ComputeControllerId(pRootName);
	IController* pRootController = m_GlobalAnimationHeader.GetController(RootControllerID);
	if (!pRootController)
	{
		RCLog("Can't find root-controller with name: %s", pRootName);

		ConvertRemainingControllers(m_GlobalAnimationHeader.m_arrController);
		return;
	}

	if (!bCheckLocator)
	{
		pLocatorController = pRootController;
	}


	// Collect controllers of direct children of the root bone (excluding controllers of the locator bone).
	std::vector<IController*> arrpChildController;
	for (uint32 i = 0; i < numJoints; i++)
	{
		const char* szChildName = m_skeleton.m_SkinningInfo.m_arrBonesDesc[i].m_arrBoneName;

		const bool bLocator = strcmp(szChildName, szLocatorName) == 0;
		if (bLocator)
			continue;

		const bool bRoot = strcmp(szChildName, pRootName) == 0;
		if (bRoot)
			continue;

		const uint32 parentIndex = i + m_skeleton.m_SkinningInfo.m_arrBonesDesc[i].m_nOffsetParent;
		const bool bParentIsRoot = (0 == m_skeleton.m_SkinningInfo.m_arrBonesDesc[parentIndex].m_nOffsetParent);
		if (bParentIsRoot)
		{
			IController* pChildController = m_GlobalAnimationHeader.GetController(SkeletonHelpers::ComputeControllerId(szChildName));
			if (pChildController)
			{
				arrpChildController.push_back(pChildController);
			}
		}
	}


	// The CInfo descriptor is retrieved to access the realkeys member _only_.
	// * realkeys is the duration of the animation (difference between start and end times) expressed in controller 
	//   time units ('keytime', 1/30 s) plus one. It basically translates directly to the total number of keys loaded 
	//   from an i_caf file, assuming a constant 30Hz sampling rate.
	// * realkeys will be used to resample motion data from certain controllers (root, its direct children and the 
	//   locator) to retrieve uniformly sized arrays of values that are used during the computation (to make the 
	//   computation independent from the controller implementation, e.g. at point we might actually be working on 
	//   compressed CControllers, with varying numbers of keys).
	const CInfo ERot0 = pRootController->GetControllerInfo();
	if (ERot0.realkeys == 0) 
	{
		return;
	}
	if (ERot0.realkeys < 0)
	{
		RCLogError("Corrupted keys in %s", m_filename.c_str());
		return;
	}


	// NOTE: Motion data of the locator controller exported from the DCC (stored in the i_caf file) is always expressed in absolute space (same as the root controller).

	// The absFirstKey value is used to offset motion of the whole skeleton from the initial root starting point to the first key of the locator 
	// controller. The locator controller is then used as the new skeleton root (which will effectively make the root motion alway start with an 
	// identity transform).
	//TODO: This name is misleading, should be changed.
	QuatTNS absFirstKey;
	{
		pLocatorController->GetOPS(m_GlobalAnimationHeader.NTime2KTime(0), absFirstKey.q, absFirstKey.t, absFirstKey.s);
		// for some reason there is some noise in the original animation. 
		if (1.0f - fabsf(absFirstKey.q.w) < 0.00001f && fabsf(absFirstKey.q.v.x) < 0.00001f && fabsf(absFirstKey.q.v.y) < 0.00001f && fabsf(absFirstKey.q.v.z) < 0.00001f) // TODO: Can we replace it with Quat::IsEquivalent() somehow?
		{
			absFirstKey.q.SetIdentity();
		}
	}


	const size_t numChildController = arrpChildController.size();

	std::vector<int> arrTimes(ERot0.realkeys);
	std::vector<std::vector<QuatTNS> > arrChildAbsKeys(numChildController, std::vector<QuatTNS>(ERot0.realkeys, QuatTNS(IDENTITY)));

	const int32 startkey = int32(m_GlobalAnimationHeader.m_fStartSec * TICKS_PER_SECOND);
	int32 stime	= startkey;
	for (int32 t = 0; t < ERot0.realkeys; t++) // TODO: This loop is repeated exactly three times. We might want to unify them.
	{
		f32 normalized_time = 0.0f;
		if (stime != startkey) // TODO: Simplify. It's basically a very roundabout way to check if ((ERot0.realkeys - 1) != 0).
		{
			normalized_time = f32(stime - startkey) / (TICKS_PER_FRAME * (ERot0.realkeys - 1));
		}

		QuatTNS absRootKey;
		pRootController->GetOPS(m_GlobalAnimationHeader.NTime2KTime(normalized_time), absRootKey.q, absRootKey.t, absRootKey.s);

		for (uint32 c = 0; c < numChildController; c++)
		{
			QuatTNS relChildKey;
			arrpChildController[c]->GetOPS(m_GlobalAnimationHeader.NTime2KTime(normalized_time), relChildKey.q, relChildKey.t, relChildKey.s);
			arrChildAbsKeys[c][t] = absFirstKey.GetInverted() * absRootKey * relChildKey; // Move child keys from the space of the root to absolute space and offset them by absFirstKey.
		}

		arrTimes[t] = stime;
		stime += TICKS_PER_FRAME;
	}


	std::vector<PQLogS> arrRootPQKeys;

	stime = startkey;
	for (int32 key = 0; key < ERot0.realkeys; key++)
	{
		f32 normalized_time=0.0f;
		if (stime != startkey)
		{
			normalized_time = f32(stime - startkey) / (TICKS_PER_FRAME * (ERot0.realkeys - 1));
		}

		QuatTNS relLocatorKey;
		pLocatorController->GetOPS(m_GlobalAnimationHeader.NTime2KTime(normalized_time), relLocatorKey.q, relLocatorKey.t, relLocatorKey.s);	
		relLocatorKey = absFirstKey.GetInverted() * relLocatorKey; // Offset locator keys by absFirstKey. They will be later used as the new root controller.

		PQLogS pqlog;
		pqlog.rotationLog = Quat::log(!relLocatorKey.q);
		pqlog.position = relLocatorKey.t;
		pqlog.scale = relLocatorKey.s;
		arrRootPQKeys.push_back(pqlog);

		stime += TICKS_PER_FRAME;
	}


	//--------------------------------------------------------------------------------------
	//---               calculate the slope, turn and turn speed values                 ----
	//--------------------------------------------------------------------------------------
	// TODO: Move to a separate function
	{
		QuatT fkey, mkey, lkey; 
		pLocatorController->GetOP(m_GlobalAnimationHeader.NTime2KTime(0.0f), fkey.q, fkey.t);
		pLocatorController->GetOP(m_GlobalAnimationHeader.NTime2KTime(0.5f), mkey.q, mkey.t);
		pLocatorController->GetOP(m_GlobalAnimationHeader.NTime2KTime(1.0f), lkey.q, lkey.t);

		m_GlobalAnimationHeader.m_fSlope = 0;
		Vec3 p0 = fkey.t;
		Vec3 p1 = lkey.t;
		Vec3 vdir = Vec3(ZERO);

		const f32 fLength = (p1 - p0).GetLength();
		if (fLength > 0.01f) //only if there is enough movement
			vdir = ((p1 - p0) * fkey.q).GetNormalized();
		const f64 l = sqrt(vdir.x * vdir.x + vdir.y * vdir.y);
		if (l > 0.0001)	
		{
			const f32 rad = f32(atan2(-vdir.z * (vdir.y / l), l));
			m_GlobalAnimationHeader.m_fSlope = rad;
		}

		const Vec3 startDir	= fkey.q.GetColumn1();
		const Vec3 middleDir = mkey.q.GetColumn1();
		const Vec3 endDir = lkey.q.GetColumn1();
		const f32 leftAngle = Ang3::CreateRadZ(startDir, middleDir);
		const f32 rightAngle = Ang3::CreateRadZ(middleDir, endDir);
		f32 angle = leftAngle + rightAngle;
		const f32 duration = m_GlobalAnimationHeader.m_fEndSec - m_GlobalAnimationHeader.m_fStartSec;
		m_GlobalAnimationHeader.m_fAssetTurn = angle;
		m_GlobalAnimationHeader.m_fTurnSpeed = angle / duration;
	}
	//--------------------------------------------------------------------------------


	const float rootPositionTolerance = Util::getMin(0.0005f, minRootPosEpsilon); // 0.5mm. previously it was 1cm
	const float rootRotationToleranceInDegrees = Util::getMin(0.1f, minRootRotEpsilon); // previously it was 1.145915590262f;
	const float rootScaleTolerance = Util::getMin(1.0e-5f, minRootSclEpsilon);
	IController_AutoPtr newController(CreateNewController(pRootController->GetID(), arrTimes, arrRootPQKeys, rootPositionTolerance, rootRotationToleranceInDegrees, rootScaleTolerance)); // TODO: Do we need shared pointer here?
	m_GlobalAnimationHeader.ReplaceController(pRootController, newController);
	pRootController = m_GlobalAnimationHeader.GetController(RootControllerID);


	std::vector<std::vector<PQLogS> > arrChildPQKeys(numChildController, std::vector<PQLogS>(ERot0.realkeys));	

	stime = startkey;
	for (int32 t = 0; t < ERot0.realkeys; t++)
	{
		f32 normalized_time = 0.0f;
		if (stime != startkey)
		{
			normalized_time = f32(stime - startkey) / (TICKS_PER_FRAME * (ERot0.realkeys - 1));
		}

		QuatTNS absRootKey;
		pRootController->GetOPS(m_GlobalAnimationHeader.NTime2KTime(normalized_time), absRootKey.q, absRootKey.t, absRootKey.s);

		for (uint32 c = 0; c < numChildController; c++)
		{
			const QuatTNS relChildKeys = absRootKey.GetInverted() * arrChildAbsKeys[c][t]; // Move child keys to the space of the new root (locator).
			arrChildPQKeys[c][t].rotationLog = Quat::log(!relChildKeys.q);
			arrChildPQKeys[c][t].position = relChildKeys.t;
			arrChildPQKeys[c][t].scale = relChildKeys.s;
		}

		stime += TICKS_PER_FRAME;
	}


	const float positionTolerance = Util::getMin(0.000031622777f, minRootPosEpsilon);
	const float rotationToleranceInDegrees = Util::getMin(0.003623703272f, minRootRotEpsilon);
	const float scaleTolerance = Util::getMin(0.000031622777f, minRootSclEpsilon);

	for (uint32 c = 0; c < numChildController; c++)
	{
		IController_AutoPtr oldController = arrpChildController[c];
		IController_AutoPtr newController(CreateNewController(oldController->GetID(), arrTimes, arrChildPQKeys[c], positionTolerance, rotationToleranceInDegrees, scaleTolerance));
		m_GlobalAnimationHeader.ReplaceController(oldController, newController);
	}
}



void CAnimationCompressor::SetIdentityLocator(GlobalAnimationHeaderAIM& gaim)
{
	uint32 numJoints = m_skeleton.m_SkinningInfo.m_arrBonesDesc.size();
	if (numJoints==0)
		return; 

	uint32 locator_locoman01 = SkeletonHelpers::ComputeControllerId(m_rootMotionJointName.c_str());
	IController* pLocomotionController = gaim.GetController(locator_locoman01);

	const char* pRootName = m_skeleton.m_SkinningInfo.m_arrBonesDesc[0].m_arrBoneName;
	int32 RootParent = m_skeleton.m_SkinningInfo.m_arrBonesDesc[0].m_nOffsetParent;
	const uint32 RootControllerID = SkeletonHelpers::ComputeControllerId(pRootName);
	IController_AutoPtr pRootController = gaim.GetController(RootControllerID);
	if (!pRootController)
	{
		RCLog("Can't find root-joint with name: %s",pRootName);
		return;
	}

	std::vector<IController*> arrpChildController;

	uint32 nDirectChildren=0;
	for (uint32 i=0; i<numJoints; i++)
	{
		int32 nParent = m_skeleton.m_SkinningInfo.m_arrBonesDesc[ i].m_nOffsetParent;
		const char* pChildName  = m_skeleton.m_SkinningInfo.m_arrBonesDesc[ i].m_arrBoneName;

		int32 IsLocator = strcmp(pChildName, m_rootMotionJointName.c_str()) == 0;
		if (IsLocator)
			continue;
		int32 IsRoot = strcmp(pChildName,pRootName)==0;
		if (IsRoot)
			continue;

		int32 idx = m_skeleton.m_SkinningInfo.m_arrBonesDesc[ i+nParent].m_nOffsetParent;
		if (idx==0)
		{
			const uint32 ChildControllerID = SkeletonHelpers::ComputeControllerId(pChildName);
			IController* pChildController = gaim.GetController(ChildControllerID);
			if (pChildController)
			{
				arrpChildController.push_back(pChildController);
				nDirectChildren++;
			}
		}
	}


	//evaluate the distance and the duration of this animation (we use it to calculate the speed)
	CInfo ERot0	=	pRootController->GetControllerInfo();
	if (ERot0.realkeys==0)
		return;

	QuatT absFirstKey; absFirstKey.SetIdentity();
	if (pLocomotionController)
		pLocomotionController->GetOP(gaim.NTime2KTime(0),absFirstKey.q,absFirstKey.t);	
	//for some reason there is some noise in the original animation. 
	if ( 1.0f-fabsf(absFirstKey.q.w)<0.00001f && fabsf(absFirstKey.q.v.x)<0.00001f && fabsf(absFirstKey.q.v.y)<0.00001f && fabsf(absFirstKey.q.v.z)<0.00001f) // TODO: Can we somehow replace this with Quat::IsEquivalent()?
		absFirstKey.q.SetIdentity();


	PQLogS zeroPQLS;
	zeroPQLS.position = Vec3(ZERO);
	zeroPQLS.rotationLog = Vec3(ZERO);
	zeroPQLS.scale = Diag33(IDENTITY);
	
	std::vector<int> arrTimes;					arrTimes.resize(ERot0.realkeys);
	std::vector<QuatTNS> arrRootKeys;			arrRootKeys.resize(ERot0.realkeys, QuatTNS(IDENTITY));

	std::vector< std::vector<QuatTNS> > arrChildAbsKeys;	
	std::vector< std::vector<PQLogS> > arrChildPQKeys;	

	const size_t numChildController = arrpChildController.size();
	arrChildAbsKeys.resize(numChildController);
	arrChildPQKeys.resize(numChildController);
	for (size_t c = 0; c < numChildController; c++)
	{
		arrChildAbsKeys[c].resize(ERot0.realkeys, QuatTNS(IDENTITY));
		arrChildPQKeys[c].resize(ERot0.realkeys, zeroPQLS);
	}


	int32 startkey	=	int32(gaim.m_fStartSec*TICKS_PER_SECOND);
	int32 stime	=	startkey;
	for (int32 t=0; t<ERot0.realkeys; t++)
	{
		f32 normalized_time=0.0f;
		if (stime!=startkey)
			normalized_time=f32(stime-startkey)/(TICKS_PER_FRAME*(ERot0.realkeys-1));

		QuatTNS absRootKey;
		pRootController->GetOPS(gaim.NTime2KTime(normalized_time),absRootKey.q,absRootKey.t, absRootKey.s);	
		arrRootKeys[t] = absFirstKey.GetInverted() * absRootKey;
		for (uint32 c=0; c<numChildController; c++)
		{
			QuatTNS relChildKey; relChildKey.SetIdentity();
			if (arrpChildController[c])
			{
				arrpChildController[c]->GetOPS(gaim.NTime2KTime(normalized_time), relChildKey.q, relChildKey.t, relChildKey.s);
				arrChildAbsKeys[c][t] = arrRootKeys[t] * relChildKey; //child of root
			}
		}
		arrTimes[t]=stime;
		stime += TICKS_PER_FRAME;
	}


	stime	=	startkey;
	std::vector<PQLogS> arrRootPQKeys;
	for (int32 key=0; key<ERot0.realkeys; key++)
	{
		f32 normalized_time=0.0f;
		if (stime!=startkey)
			normalized_time=f32(stime-startkey)/(TICKS_PER_FRAME*(ERot0.realkeys-1));

		QuatT relLocatorKey; relLocatorKey.SetIdentity();
		if (pLocomotionController)
			pLocomotionController->GetOP(gaim.NTime2KTime(normalized_time), relLocatorKey.q, relLocatorKey.t);
		relLocatorKey = absFirstKey.GetInverted() * relLocatorKey;

		//convert into the old FarCry format
		PQLogS pqlog; 
		pqlog.rotationLog = Quat::log(!relLocatorKey.q);
		pqlog.position = relLocatorKey.t;
		pqlog.scale = Diag33(IDENTITY);
		arrRootPQKeys.push_back(pqlog);

		stime += TICKS_PER_FRAME;
	}

	gaim.ReplaceController(pRootController.get(), CreateNewControllerAIM(pRootController->GetID(), arrTimes, arrRootPQKeys));

	//--------------------------------------------------------------------------------
	pRootController = gaim.GetController(RootControllerID);

	stime=startkey;
	for (int32 t=0; t<ERot0.realkeys; t++)
	{
		QuatTNS absRootKey;	   

		f32 normalized_time=0.0f;
		if (stime!=startkey)
			normalized_time=f32(stime-startkey)/(TICKS_PER_FRAME*(ERot0.realkeys-1));

		pRootController->GetOPS(gaim.NTime2KTime(normalized_time), absRootKey.q, absRootKey.t, absRootKey.s);	
		for (uint32 c=0; c<numChildController; c++)
		{
			QuatTNS relChildKeys = absRootKey.GetInverted() * arrChildAbsKeys[c][t]; //relative Pivot
			arrChildPQKeys[c][t].rotationLog = Quat::log(!relChildKeys.q);
			arrChildPQKeys[c][t].position = relChildKeys.t;
			arrChildPQKeys[c][t].scale = relChildKeys.s;
		}
		stime += TICKS_PER_FRAME;
	}


	for (size_t c = 0 ; c < numChildController; ++c)
	{
		gaim.ReplaceController(arrpChildController[c], CreateNewControllerAIM(arrpChildController[c]->GetID(), arrTimes, arrChildPQKeys[c]));
	}

}


void CAnimationCompressor::EvaluateSpeed()
{
	uint32 numJoints = m_skeleton.m_SkinningInfo.m_arrBonesDesc.size();
	if (numJoints==0)
		return;

	uint32 TicksPerSecond = TICKS_PER_SECOND;
	f32 fStart		=	m_GlobalAnimationHeader.m_fStartSec;
	f32 fDuration	=	m_GlobalAnimationHeader.m_fEndSec - m_GlobalAnimationHeader.m_fStartSec;
	f32 fDistance	=	0.0f;

	m_GlobalAnimationHeader.m_fDistance = 0.0f;
	m_GlobalAnimationHeader.m_fSpeed    = 0.0f;


	IController * pController = m_GlobalAnimationHeader.GetController(m_skeleton.m_SkinningInfo.m_arrBonesDesc[0].m_nControllerID);
	if (pController==0)
		return;
	CInfo ERot0	=	pController->GetControllerInfo();
	if (ERot0.realkeys<2)
		return;

	GlobalAnimationHeaderCAF& rCAF = m_GlobalAnimationHeader;

	Vec3 SPos0,SPos1;	
	f32 newtime = 0; //fStart*TicksPerSecond;
	pController->GetP( rCAF.NTime2KTime(newtime), SPos0 );
	for (f32 t=0.01f; t<=1.0f; t=t+0.01f)
	{
		//newtime = fStart*TicksPerSecond + t*TicksPerSecond*fDuration;
		newtime = t;
		pController->GetP( rCAF.NTime2KTime(newtime), SPos1 );
		if (SPos0 != SPos1 )
		{
			fDistance += (SPos0-SPos1).GetLength();
			SPos0=SPos1;
		}
	}

	m_GlobalAnimationHeader.m_fDistance			=	fDistance;
	if (fDuration)
		m_GlobalAnimationHeader.m_fSpeed      = fDistance/fDuration;

	if (fDuration<0.001f)	
	{
		RCLogWarning ("CryAnimation: Animation-asset '%s' has just one keyframe. Impossible to detect Duration", m_GlobalAnimationHeader.GetFilePath());
	}
}


void CAnimationCompressor::DetectCycleAnimations()
{
	GlobalAnimationHeaderCAF& rCAF = m_GlobalAnimationHeader;

	uint32 numJoints = m_skeleton.m_SkinningInfo.m_arrBonesDesc.size();
	bool equal=1;
	for (uint32 j=0; j<numJoints; j++)
	{
		if (m_skeleton.m_SkinningInfo.m_arrBonesDesc[j].m_nOffsetParent == 0)
			continue;

		//int AnimID = 0;
		IController* pController = m_GlobalAnimationHeader.GetController(m_skeleton.m_SkinningInfo.m_arrBonesDesc[j].m_nControllerID);;//m_GlobalAnimationHeader.m_arrController[AnimID];//pModelJoint[j].m_arrControllersMJoint[AnimID];
		if (pController==0)
			continue;

		Quat SRot;	pController->GetO(rCAF.NTime2KTime(0), SRot );
		Quat ERot	=	pController->GetControllerInfo().quat;

		Ang3 sang=Ang3(SRot);
		Ang3 eang=Ang3(ERot);

		CInfo info = pController->GetControllerInfo();
		bool status = IsQuatEquivalent_dot(SRot, ERot, 0.1f);
		equal &= status;
	}

	if (equal)
		m_GlobalAnimationHeader.m_nFlags |= CA_ASSET_CYCLE;
	else
		m_GlobalAnimationHeader.m_nFlags &= ~CA_ASSET_CYCLE;

	if (m_AnimDesc.m_bAdditiveAnimation)
		m_GlobalAnimationHeader.OnAssetAdditive();
}


void CAnimationCompressor::EnableTrackAligning(bool bEnable)
{
	m_bAlignTracks = bEnable;
}


void CAnimationCompressor::EnableRootMotionExtraction(const char* szRootMotionJointName)
{
	m_rootMotionJointName = szRootMotionJointName;
}


CController* CAnimationCompressor::CreateNewController(int controllerID, const std::vector<int>& arrFullTimes, const std::vector<PQLogS>& arrFullQTKeys, float positionTolerance, float rotationToleranceInDegrees, float scaleTolerance) 
{
	CCompressonator compressonator;
	{
		int32 oldtime =  -1;
		for (size_t i = 0; i < arrFullTimes.size(); ++i)
		{
			if (oldtime != arrFullTimes[i])
			{
				oldtime = arrFullTimes[i];

				compressonator.m_rotTimes.push_back(arrFullTimes[i]);
				compressonator.m_posTimes.push_back(arrFullTimes[i]);
				compressonator.m_sclTimes.push_back(arrFullTimes[i]);

				Quat q = !Quat::exp(arrFullQTKeys[i].rotationLog);
				compressonator.m_rotations.push_back(q);
				compressonator.m_positions.push_back(arrFullQTKeys[i].position);
				compressonator.m_scales.push_back(arrFullQTKeys[i].scale);
			}
		}
	}

	CController* pNewController = new CController();

	pNewController->SetID(controllerID);
	pNewController->SetPositionController(compressonator.CompressPositions(eNoCompressVec3, (positionTolerance != 0.0f), positionTolerance));
	pNewController->SetRotationController(compressonator.CompressRotations(eSmallTree64BitExtQuat, (rotationToleranceInDegrees != 0.0f), rotationToleranceInDegrees));
	pNewController->SetScaleController(compressonator.CompressScales(eNoCompressVec3, (scaleTolerance != 0.0f), scaleTolerance));

	return pNewController;
}

CController* CAnimationCompressor::CreateNewControllerAIM( int controllerID, const std::vector<int>& arrFullTimes, const std::vector<PQLogS>& arrFullQTKeys) 
{
	return CreateNewController(controllerID, arrFullTimes, arrFullQTKeys, 0.0f, 0.0f, 0.0f);
}

void CAnimationCompressor::EvaluateStartLocation()
{
	uint32 numJoints = m_skeleton.m_SkinningInfo.m_arrBonesDesc.size();
	if (numJoints==0)
		return;

	const uint32 locator_locoman01 = SkeletonHelpers::ComputeControllerId(m_rootMotionJointName.c_str());
	IController* pLocomotionController = m_GlobalAnimationHeader.GetController(locator_locoman01);
	GlobalAnimationHeaderCAF& rCAF = m_GlobalAnimationHeader;

	const char* pRootName = m_skeleton.m_SkinningInfo.m_arrBonesDesc[0].m_arrBoneName;
	const uint32 RootControllerID = SkeletonHelpers::ComputeControllerId(pRootName);
	IController* pRootController = m_GlobalAnimationHeader.GetController(RootControllerID);

	GlobalAnimationHeaderCAF& rGlobalAnimHeader = m_GlobalAnimationHeader;

	Diag33 Scale; 
	QuatT absFirstKey;	absFirstKey.SetIdentity();
	QuatT absLastKey;	  absLastKey.SetIdentity();
	if (pLocomotionController)
	{
		pLocomotionController->GetOPS(rCAF.NTime2KTime(0),absFirstKey.q,absFirstKey.t,Scale);	
		pLocomotionController->GetOPS(rCAF.NTime2KTime(1),absLastKey.q,absLastKey.t,Scale);	
		//for some reason there is some noise in the original animation. 
		if ( 1.0f-fabsf(absFirstKey.q.w)<0.00001f && fabsf(absFirstKey.q.v.x)<0.00001f && fabsf(absFirstKey.q.v.y)<0.00001f && fabsf(absFirstKey.q.v.z)<0.00001f)
			absFirstKey.q.SetIdentity();
	}
	else if (pRootController)
	{
		pRootController->GetOPS(rCAF.NTime2KTime(0),absFirstKey.q,absFirstKey.t,Scale);	
		pRootController->GetOPS(rCAF.NTime2KTime(1),absLastKey.q,absLastKey.t,Scale);	
		//for some reason there is some noise in the original animation. 
		if ( 1.0f-fabsf(absFirstKey.q.w)<0.00001f && fabsf(absFirstKey.q.v.x)<0.00001f && fabsf(absFirstKey.q.v.y)<0.00001f && fabsf(absFirstKey.q.v.z)<0.00001f)
			absFirstKey.q.SetIdentity();
	}
	rGlobalAnimHeader.m_StartLocation2  = absFirstKey;
	rGlobalAnimHeader.m_LastLocatorKey2 = absLastKey;
}


#define POSES (0x080)


bool IsOldChunk(CChunkFile::ChunkDesc * ch)
{
	return (ch->chunkVersion < CONTROLLER_CHUNK_DESC_0829::VERSION ) || (ch->chunkVersion ==  CONTROLLER_CHUNK_DESC_0831::VERSION);
}

bool IsNewChunk(CChunkFile::ChunkDesc * ch)
{
	return !IsOldChunk(ch);
}

void CAnimationCompressor::DeleteCompressedChunks()
{
	// remove new MotionParams from file
	m_ChunkFile.DeleteChunksByType(ChunkType_MotionParameters);
	
	// delete controllers in new format
	uint32 numChunck = m_ChunkFile.NumChunks();
	for (uint32 i=0; i<numChunck; i++)
	{

		CChunkFile::ChunkDesc *cd = m_ChunkFile.GetChunk(i);
		if (cd)
		{
			if (cd->chunkType != ChunkType_Controller)
				continue;

			if (IsNewChunk(cd))
			{
				m_ChunkFile.DeleteChunkById(cd->chunkId);
				--numChunck;
				i = 0;
			}
		}
	}
}

bool CAnimationCompressor::AreKeyTimesAscending(KeyTimesInformationPtr& keyTimes)
{
	const uint32 keysCount = keyTimes->GetNumKeys();

	float lastTime = -FLT_MAX;
	for (uint32 i = 0; i < keysCount; ++i)
	{
		const float t = keyTimes->GetKeyValueFloat(i);
		if (t <= lastTime)
		{
			return false;
		}
		lastTime = t;
	}
	return true;
};

bool CAnimationCompressor::CompareKeyTimes(KeyTimesInformationPtr& ptr1, KeyTimesInformationPtr& ptr2)
{
	if (ptr1->GetNumKeys() != ptr2->GetNumKeys())
	{
		return false;
	}

	for (uint32 i = 0; i < ptr1->GetNumKeys(); ++i)
	{
		if (ptr1->GetKeyValueFloat(i) != ptr2->GetKeyValueFloat(i))
		{
			return false;
		}
	}
	return true;
}


struct CAnimChunkData
{
	std::vector<char> m_data;

	void Align(size_t boundary)
	{
		m_data.resize(::Align(m_data.size(), boundary), (char)0);
	}

	template <class T>
	void Add( const T& object )
	{
		AddData( &object,sizeof(object) );
	}
	void AddData( const void *pSrcData,int nSrcDataSize )
	{
		const char* src = static_cast<const char*>(pSrcData);
		if(m_data.empty())
			m_data.assign( src, src + nSrcDataSize);
		else
			m_data.insert( m_data.end(), src, src + nSrcDataSize);
	}
	void* data()
	{
		return &m_data.front();
	}

	size_t size()const{return m_data.size();}
};


void CAnimationCompressor::SaveMotionParameters( CChunkFile* pChunkFile, bool bigEndianOutput )
{
	SEndiannessSwapper swap(bigEndianOutput);

	CHUNK_MOTION_PARAMETERS chunk;
	ZeroStruct(chunk);

	MotionParams905& mp = chunk.mp;
	uint32 assetFlags = 0;
	if (m_GlobalAnimationHeader.IsAssetCycle())
		assetFlags |= CA_ASSET_CYCLE;
	if (m_AnimDesc.m_bAdditiveAnimation)
		assetFlags |= CA_ASSET_ADDITIVE;
	if (bigEndianOutput)
		assetFlags |= CA_ASSET_BIG_ENDIAN;
	swap(mp.m_nAssetFlags = assetFlags);

	swap(mp.m_nCompression = m_AnimDesc.oldFmt.m_CompressionQuality);

	swap(mp.m_nTicksPerFrame = TICKS_PER_FRAME);
	swap(mp.m_fSecsPerTick = SECONDS_PER_TICK);
	swap(mp.m_nStart = m_GlobalAnimationHeader.m_nStartKey);
	swap(mp.m_nEnd = m_GlobalAnimationHeader.m_nEndKey);

	swap(mp.m_fMoveSpeed = m_GlobalAnimationHeader.m_fSpeed);
	swap(mp.m_fTurnSpeed = m_GlobalAnimationHeader.m_fTurnSpeed);
	swap(mp.m_fAssetTurn = m_GlobalAnimationHeader.m_fAssetTurn);
	swap(mp.m_fDistance = m_GlobalAnimationHeader.m_fDistance);
	swap(mp.m_fSlope = m_GlobalAnimationHeader.m_fSlope);

	mp.m_StartLocation = m_GlobalAnimationHeader.m_StartLocation2;
	swap(mp.m_StartLocation.q.w);
	swap(mp.m_StartLocation.q.v.x);
	swap(mp.m_StartLocation.q.v.y);
	swap(mp.m_StartLocation.q.v.z);
	swap(mp.m_StartLocation.t.x);
	swap(mp.m_StartLocation.t.y);
	swap(mp.m_StartLocation.t.z);

	mp.m_EndLocation		= m_GlobalAnimationHeader.m_LastLocatorKey2;
	swap(mp.m_EndLocation.q.w);
	swap(mp.m_EndLocation.q.v.x);
	swap(mp.m_EndLocation.q.v.y);
	swap(mp.m_EndLocation.q.v.z);
	swap(mp.m_EndLocation.t.x);
	swap(mp.m_EndLocation.t.y);
	swap(mp.m_EndLocation.t.z);


	swap(mp.m_LHeelStart = m_GlobalAnimationHeader.m_FootPlantVectors.m_LHeelStart);
	swap(mp.m_LHeelEnd = m_GlobalAnimationHeader.m_FootPlantVectors.m_LHeelEnd);
	swap(mp.m_LToe0Start = m_GlobalAnimationHeader.m_FootPlantVectors.m_LToe0Start);
	swap(mp.m_LToe0End = m_GlobalAnimationHeader.m_FootPlantVectors.m_LToe0End);

	swap(mp.m_RHeelStart = m_GlobalAnimationHeader.m_FootPlantVectors.m_RHeelStart);
	swap(mp.m_RHeelEnd = m_GlobalAnimationHeader.m_FootPlantVectors.m_RHeelEnd);
	swap(mp.m_RToe0Start = m_GlobalAnimationHeader.m_FootPlantVectors.m_RToe0Start);
	swap(mp.m_RToe0End = m_GlobalAnimationHeader.m_FootPlantVectors.m_RToe0End);

	CChunkData chunkData;
	chunkData.Add( chunk );

	pChunkFile->AddChunk(
		ChunkType_MotionParameters,
		CHUNK_MOTION_PARAMETERS::VERSION,
		(bigEndianOutput ? eEndianness_Big : eEndianness_Little),
		chunkData.GetData(), chunkData.GetSize());
}

void CAnimationCompressor::ExtractKeys(std::vector<int>& times, std::vector<PQLogS>& logKeys, const CControllerPQLog* controller) const
{
	const int32 numKeys = controller->m_arrKeys.size();
	times.resize(numKeys);
	logKeys.resize(numKeys);

	const int32 startkey = int32(m_GlobalAnimationHeader.m_fStartSec * TICKS_PER_SECOND);
	int32 stime = startkey;
	for (int32 t = 0; t < numKeys; t++)
	{
		logKeys[t] = controller->m_arrKeys[t];
		times[t] = stime;

		stime += TICKS_PER_FRAME;
	}
}


int CAnimationCompressor::ConvertRemainingControllers(TControllersVector& controllers) const
{
	int result = 0;
	std::vector<int> times;
	std::vector<PQLogS> logKeys;
	size_t count = controllers.size();
	for (size_t i = 0; i < count; ++i)
	{
		IController_AutoPtr& controller = controllers[i];
		if (CControllerPQLog* logController = dynamic_cast<CControllerPQLog *>(controller.get()))
		{
			ExtractKeys(times, logKeys, logController);
			controller = CreateNewController(controllers[i]->GetID(), times, logKeys, 0.0f, 0.0f, 0.0f);
			++result;
		}
	}
	return result;
}

uint32 CAnimationCompressor::SaveControllers(CSaverCGF& saver, bool bigEndianOutput)
{
	uint32 numSaved = 0;

	const auto& controllers = m_GlobalAnimationHeader.m_arrController;

	const uint32 numChunks = controllers.size();
	for (uint32 chunk = 0; chunk < numChunks; ++chunk)
	{
		if (CControllerPQLog* controller = dynamic_cast<CControllerPQLog *>(controllers[chunk].get()))
		{
			RCLogError("Saving of old (V0828) controllers is not supported anymore");
			continue;
		}

		// New format controller
		if (CController* controller = dynamic_cast<CController *>(controllers[chunk].get()))
		{
			if (SaveController832(saver, controller, bigEndianOutput)) // TODO: Switch to 832 once the runtime support for it has been implemented.
			{
				++numSaved;
			}
			continue;
		}
	}

	return numSaved;
}

bool CAnimationCompressor::SaveController829(CSaverCGF& saver, CController* pController, bool bigEndianOutput)
{
	const bool bSwapEndian = (bigEndianOutput ? (eEndianness_Big == eEndianness_NonNative) : (eEndianness_Big == eEndianness_Native));

	SEndiannessSwapper swap(bSwapEndian);

	CONTROLLER_CHUNK_DESC_0829 chunk;
	ZeroStruct(chunk);

	CAnimChunkData Data;

	chunk.nControllerId = pController->GetID();
	swap(chunk.nControllerId);

	chunk.TracksAligned = m_bAlignTracks ? 1 : 0;
	int nTrackAlignment = m_bAlignTracks ? 4 : 1;

	RotationControllerPtr pRotation = pController->GetRotationController();
	KeyTimesInformationPtr pRotTimes;
	if (pRotation)
	{
		pRotTimes = pRotation->GetKeyTimesInformation();
		chunk.numRotationKeys = pRotation->GetNumKeys();
		float lastTime = -FLT_MAX;
		for (int i = 0; i < chunk.numRotationKeys; ++i)
		{
			float t = pRotTimes->GetKeyValueFloat(i);
			if (t <= lastTime)
			{
				RCLogError("Trying to save controller with repeated or non-sorted time keys.");
				return false;
			}
			lastTime = t;
		}
		swap(chunk.numRotationKeys);

		chunk.RotationFormat = pRotation->GetFormat();
		swap(chunk.RotationFormat);


		chunk.RotationTimeFormat = pRotTimes->GetFormat();
		swap(chunk.RotationTimeFormat);

		//copy to chunk
		if (bSwapEndian) pRotation->GetRotationStorage()->SwapBytes();
		Data.Align(nTrackAlignment);
		Data.AddData(pRotation->GetRotationStorage()->GetData(), pRotation->GetRotationStorage()->GetDataRawSize());
		if (bSwapEndian) pRotation->GetRotationStorage()->SwapBytes();

		if (bSwapEndian) pRotTimes->SwapBytes();
		Data.Align(nTrackAlignment);
		Data.AddData(pRotTimes->GetData(), pRotTimes->GetDataRawSize());
		if (bSwapEndian) pRotTimes->SwapBytes();
	}

	PositionControllerPtr pPosition = pController->GetPositionController();
	KeyTimesInformationPtr pPosTimes;
	if (pPosition)
	{
		pPosTimes = pPosition->GetKeyTimesInformation();
		chunk.numPositionKeys = pPosition->GetNumKeys();
		float lastTime = -FLT_MAX;
		for (int i = 0; i < chunk.numPositionKeys; ++i)
		{
			float t = pPosTimes->GetKeyValueFloat(i);
			if (t <= lastTime)
			{
				RCLogError("Trying to save controller with repeated or non-sorted time keys.");
				return false;
			}
			lastTime = t;
		}
		swap(chunk.numPositionKeys);

		chunk.PositionFormat = pPosition->GetFormat();
		swap(chunk.PositionFormat);

		chunk.PositionKeysInfo = CONTROLLER_CHUNK_DESC_0829::eKeyTimePosition;

		if (bSwapEndian) pPosition->GetPositionStorage()->SwapBytes();
		Data.Align(nTrackAlignment);
		Data.AddData(pPosition->GetPositionStorage()->GetData(), pPosition->GetPositionStorage()->GetDataRawSize());
		if (bSwapEndian) pPosition->GetPositionStorage()->SwapBytes();

		if (pRotation && CompareKeyTimes(pPosTimes, pRotTimes))
		{
			chunk.PositionKeysInfo = CONTROLLER_CHUNK_DESC_0829::eKeyTimeRotation;
		}
		else
		{
			if (bSwapEndian) pPosTimes->SwapBytes();

			Data.Align(nTrackAlignment);
			Data.AddData(pPosTimes->GetData(), pPosTimes->GetDataRawSize());
			if (bSwapEndian) pPosTimes->SwapBytes();
			chunk.PositionTimeFormat = pPosTimes->GetFormat();
		}
	}

	uint32 numData = Data.size();
	if (numData)
	{
		saver.SaveController829(bSwapEndian, chunk, Data.data(), numData);
		return true;
	}
	return false;
}

bool CAnimationCompressor::SaveController832(CSaverCGF& saver, CController* pController, bool bigEndianOutput)
{
	const bool bSwapEndian = (bigEndianOutput ? (eEndianness_Big == eEndianness_NonNative) : (eEndianness_Big == eEndianness_Native));
	const SEndiannessSwapper swap(bSwapEndian);

	CONTROLLER_CHUNK_DESC_0832 chunkHeader = CONTROLLER_CHUNK_DESC_0832();
	CAnimChunkData chunkData;

	chunkHeader.nControllerId = pController->GetID();
	swap(chunkHeader.nControllerId);

	RotationControllerPtr pRotation = pController->GetRotationController();
	KeyTimesInformationPtr pRotTimes;
	if (pRotation)
	{
		pRotTimes = pRotation->GetKeyTimesInformation();
		if (!AreKeyTimesAscending(pRotTimes))
		{
			RCLogError("Trying to save controller with repeated or non-sorted time keys.");
			return false;
		}

		chunkHeader.numRotationKeys = pRotation->GetNumKeys();
		swap(chunkHeader.numRotationKeys);

		chunkHeader.rotationFormat = pRotation->GetFormat();
		swap(chunkHeader.rotationFormat);

		if (bSwapEndian) pRotation->GetRotationStorage()->SwapBytes();
		chunkData.Align(4);
		chunkData.AddData(pRotation->GetRotationStorage()->GetData(), pRotation->GetRotationStorage()->GetDataRawSize());
		if (bSwapEndian) pRotation->GetRotationStorage()->SwapBytes();

		chunkHeader.rotationTimeFormat = pRotTimes->GetFormat();
		swap(chunkHeader.rotationTimeFormat);

		if (bSwapEndian) pRotTimes->SwapBytes();
		chunkData.Align(4);
		chunkData.AddData(pRotTimes->GetData(), pRotTimes->GetDataRawSize());
		if (bSwapEndian) pRotTimes->SwapBytes();
	}


	PositionControllerPtr pPosition = pController->GetPositionController();
	KeyTimesInformationPtr pPosTimes;
	if (pPosition)
	{
		pPosTimes = pPosition->GetKeyTimesInformation();
		if (!AreKeyTimesAscending(pPosTimes))
		{
			RCLogError("Trying to save controller with repeated or non-sorted time keys.");
			return false;
		}

		chunkHeader.numPositionKeys = pPosition->GetNumKeys();
		swap(chunkHeader.numPositionKeys);

		chunkHeader.positionFormat = pPosition->GetFormat();
		swap(chunkHeader.positionFormat);

		if (bSwapEndian) pPosition->GetPositionStorage()->SwapBytes();
		chunkData.Align(4);
		chunkData.AddData(pPosition->GetPositionStorage()->GetData(), pPosition->GetPositionStorage()->GetDataRawSize());
		if (bSwapEndian) pPosition->GetPositionStorage()->SwapBytes();

		if (pRotation && CompareKeyTimes(pPosTimes, pRotTimes))
		{
			chunkHeader.positionKeysInfo = CONTROLLER_CHUNK_DESC_0832::eKeyTimeRotation;
			swap(chunkHeader.positionKeysInfo);
		}
		else
		{
			chunkHeader.positionKeysInfo = CONTROLLER_CHUNK_DESC_0832::eKeyTimePosition;
			swap(chunkHeader.positionKeysInfo);

			chunkHeader.positionTimeFormat = pPosTimes->GetFormat();
			swap(chunkHeader.positionTimeFormat);

			if (bSwapEndian) pPosTimes->SwapBytes();
			chunkData.Align(4);
			chunkData.AddData(pPosTimes->GetData(), pPosTimes->GetDataRawSize());
			if (bSwapEndian) pPosTimes->SwapBytes();
		}
	}

	PositionControllerPtr pScale = pController->GetScaleController();
	KeyTimesInformationPtr pSclTimes;
	if (pScale)
	{
		pSclTimes = pScale->GetKeyTimesInformation();
		if (!AreKeyTimesAscending(pSclTimes))
		{
			RCLogError("Trying to save controller with repeated or non-sorted time keys.");
			return false;
		}

		chunkHeader.numScaleKeys = pScale->GetNumKeys();
		swap(chunkHeader.numScaleKeys);

		chunkHeader.scaleFormat = pScale->GetFormat();
		swap(chunkHeader.scaleFormat);

		if (bSwapEndian) pScale->GetPositionStorage()->SwapBytes();
		chunkData.Align(4);
		chunkData.AddData(pScale->GetPositionStorage()->GetData(), pScale->GetPositionStorage()->GetDataRawSize());
		if (bSwapEndian) pScale->GetPositionStorage()->SwapBytes();

		if (pRotation && CompareKeyTimes(pSclTimes, pRotTimes))
		{
			chunkHeader.scaleKeysInfo = CONTROLLER_CHUNK_DESC_0832::eKeyTimeRotation;
			swap(chunkHeader.scaleKeysInfo);
		}
		else if (pPosition && CompareKeyTimes(pSclTimes, pPosTimes))
		{
			chunkHeader.scaleKeysInfo = CONTROLLER_CHUNK_DESC_0832::eKeyTimePosition;
			swap(chunkHeader.scaleKeysInfo);
		}
		else
		{
			chunkHeader.scaleKeysInfo = CONTROLLER_CHUNK_DESC_0832::eKeyTimeScale;
			swap(chunkHeader.scaleKeysInfo);

			chunkHeader.scaleTimeFormat = pSclTimes->GetFormat();
			swap(chunkHeader.scaleTimeFormat);

			if (bSwapEndian) pSclTimes->SwapBytes();
			chunkData.Align(4);
			chunkData.AddData(pSclTimes->GetData(), pSclTimes->GetDataRawSize());
			if (bSwapEndian) pSclTimes->SwapBytes();
		}
	}

	const uint32 numData = chunkData.size();
	if (numData == 0)
	{
		return false;
	}

	saver.SaveController832(bSwapEndian, chunkHeader, chunkData.data(), numData);
	return true;
}
