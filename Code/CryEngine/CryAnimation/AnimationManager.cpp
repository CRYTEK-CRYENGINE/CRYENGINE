// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AnimationManager.h"

#include "LoaderTCB.h"
#include "ModelAnimationSet.h"
#include "LoaderCGA.h"
#include "LoaderDBA.h"
#include <CryCore/StlUtils.h>
#include "ControllerPQ.h"
#include "ControllerOpt.h"
#include "ParametricSampler.h"
#include "CharacterManager.h"
#include "Model.h"
#include <CryString/StringUtils.h>
#include <CryString/CryPath.h>

int CAnimationManager::GetGlobalIDbyFilePath_CAF(const char* szAnimFileName) const
{
	const int id = m_AnimationMapCAF.GetValue(szAnimFileName);
	if (id >= 0)
	{
		assert(size_t(id) < m_arrGlobalCAF.size());
		assert(!stricmp(m_arrGlobalCAF[id].GetFilePath(), szAnimFileName));
		return id;
	}
	return -1;
}

int CAnimationManager::GetGlobalIDbyFilePath_AIM(const char* szAnimFilePath) const
{
	const uint32 crc32 = CCrc32::ComputeLowercase(szAnimFilePath);

	const uint32 numAIM = m_arrGlobalAIM.size();
	for (uint32 id = 0; id < numAIM; ++id)
	{
		if (m_arrGlobalAIM[id].GetFilePathCRC32() == crc32)
		{
			assert(!stricmp(m_arrGlobalAIM[id].GetFilePath(), szAnimFilePath));
			return id;
		}
	}

	return -1;
}

int CAnimationManager::GetGlobalIDbyFilePath_LMG(const char* szAnimFilePath) const
{
	const int id = GetGlobalIDbyFilePathCRC_LMG(CCrc32::ComputeLowercase(szAnimFilePath));
	assert(id == -1 || stricmp(m_arrGlobalLMG[id].GetFilePath(), szAnimFilePath) == 0);

	return id;
}

int CAnimationManager::GetGlobalIDbyFilePathCRC_LMG(uint32 crc32) const
{
	const uint32 numLMG = m_arrGlobalLMG.size();
	for (uint32 id = 0; id < numLMG; ++id)
	{
		if (m_arrGlobalLMG[id].GetFilePathCRC32() == crc32)
		{
			return id;
		}
	}

	return -1;
}

////////////////////////////////////////////////////////////////////////////////////
// loads the animation with the specified name; if the animation is already loaded,
// then just returns its id
// The caller MUST TAKE CARE to bind the animation if it's already loaded before it has registered itself within this manager
// RETURNS:
//   The global animation id.
//   -1 if the animation couldn't be loaded
// SIDE EFFECT NOTES:
//   This function does not put up a warning in the case the file was not found.
//   The caller must do so.
int CAnimationManager::CreateGAH_CAF(const string& strFilePath)
{
	int nGlobalAnimId = GetGlobalIDbyFilePath_CAF(strFilePath);
	if (nGlobalAnimId < 0)
	{
		nGlobalAnimId = int32(m_arrGlobalCAF.size());
		// add a new animation structure that will hold the info about the new animation.
		// the new animation id is the index of this structure in the array

		m_arrGlobalCAF.push_back();
		m_arrGlobalCAF[nGlobalAnimId] = GlobalAnimationHeaderCAF();

		m_arrGlobalCAF[nGlobalAnimId].SetFilePath(strFilePath);
		m_animSearchHelper.AddAnimation(strFilePath, nGlobalAnimId);
		m_AnimationMapCAF.InsertValue(m_arrGlobalCAF[nGlobalAnimId].GetFilePathCRC32(), nGlobalAnimId);
	}
	return nGlobalAnimId;
}

int CAnimationManager::CreateGAH_AIM(const string& strFilePath)
{
	int nGlobalAnimId = GetGlobalIDbyFilePath_AIM(strFilePath);
	if (nGlobalAnimId < 0)
	{
		nGlobalAnimId = int32(m_arrGlobalAIM.size());
		// add a new animation structure that will hold the info about the new animation.
		// the new animation id is the index of this structure in the array
		m_arrGlobalAIM.push_back(GlobalAnimationHeaderAIM());
		m_arrGlobalAIM[nGlobalAnimId].SetFilePath(strFilePath);
		//	m_AnimationMapAIM.InsertValue(m_arrGlobalAIM[nGlobalAnimId].GetFilePathCRC32(), nGlobalAnimId);
	}
	return nGlobalAnimId;
}

int CAnimationManager::CreateGAH_LMG(const string& strFilePath)
{
	int nGlobalAnimId = GetGlobalIDbyFilePath_LMG(strFilePath);
	if (nGlobalAnimId < 0)
	{
		nGlobalAnimId = int32(m_arrGlobalLMG.size());
		// add a new animation structure that will hold the info about the new animation.
		// the new animation id is the index of this structure in the array
		m_arrGlobalLMG.push_back(GlobalAnimationHeaderLMG());
		m_arrGlobalLMG[nGlobalAnimId].SetFilePath(strFilePath);
		//	m_AnimationMapLMG.InsertValue(m_arrGlobalLMG[nGlobalAnimId].GetFilePathCRC32(), nGlobalAnimId);
	}
	return nGlobalAnimId;
}

CGlobalHeaderDBA* CAnimationManager::FindGlobalHeaderByCRC_DBA(uint32 crc)
{
	for (size_t i = 0, c = m_arrGlobalHeaderDBA.size(); i != c; ++i)
	{
		if (m_arrGlobalHeaderDBA[i].m_FilePathDBACRC32 == crc)
			return &m_arrGlobalHeaderDBA[i];
	}

	return NULL;
}

bool CAnimationManager::LoadAnimationTCB(int nAnimId, DynArray<CControllerTCB>& arrNodeAnims, CryCGALoader* pCGA, const IDefaultSkeleton* pIDefaultSkeleton)
{
	if (pIDefaultSkeleton == 0)
		return 0;
	const CDefaultSkeleton* pDefaultSkeleton = (const CDefaultSkeleton*)pIDefaultSkeleton;

	LOADING_TIME_PROFILE_SECTION(g_pISystem);

	GlobalAnimationHeaderCAF& rGlobalAnim = m_arrGlobalCAF[nAnimId];
	int32 nStartKey = pCGA->m_start;
	int32 nEndKey = pCGA->m_end;

	rGlobalAnim.m_fStartSec = nStartKey / ANIMATION_30Hz;
	rGlobalAnim.m_fEndSec = nEndKey / ANIMATION_30Hz;
	if (rGlobalAnim.m_fEndSec <= rGlobalAnim.m_fStartSec)
		rGlobalAnim.m_fEndSec = rGlobalAnim.m_fStartSec;

	rGlobalAnim.m_fTotalDuration = rGlobalAnim.m_fEndSec - rGlobalAnim.m_fStartSec;

	uint32 numCtrl = rGlobalAnim.m_nControllers;
	assert(numCtrl == 0);
	if (numCtrl)
		return true;

	uint32 numController = 0;
	uint32 numAnimSize = arrNodeAnims.size();
	for (uint32 i = 0; i < numAnimSize; i++)
	{
		uint32 numPos = arrNodeAnims[i].m_posTrack.num_keys();
		uint32 numRot = arrNodeAnims[i].m_rotTrack.num_keys();
		uint32 numScl = arrNodeAnims[i].m_sclTrack.num_keys();
		if (numPos + numRot + numScl)
			numController++;
	}
	rGlobalAnim.m_arrController.resize(numController);
	rGlobalAnim.m_nControllers = numController;
	rGlobalAnim.m_nControllers2 = numController;

	for (uint32 i = 0, j = 0; i < numAnimSize; i++)
	{
		uint32 numPos = arrNodeAnims[i].m_posTrack.num_keys();
		uint32 numRot = arrNodeAnims[i].m_rotTrack.num_keys();
		uint32 numScl = arrNodeAnims[i].m_sclTrack.num_keys();
		if (numPos + numRot + numScl)
		{
			CControllerTCB* pControllerTCB = new CControllerTCB();
			*pControllerTCB = arrNodeAnims[i];

			pControllerTCB->m_nControllerId = pDefaultSkeleton->m_arrModelJoints[i].m_nJointCRC32;

			IController* pController = (IController*)pControllerTCB;
			PREFAST_SUPPRESS_WARNING(6386)
			rGlobalAnim.m_arrController[j] = static_cast<IController*>(pController);
			++j;
		}
	}

	std::sort(rGlobalAnim.m_arrController.begin(), rGlobalAnim.m_arrController.end(), AnimCtrlSortPred());

	rGlobalAnim.InitControllerLookup(numController);
	rGlobalAnim.OnAssetCreated();
	rGlobalAnim.m_nFlags |= CA_ASSET_TCB;
	return true;
}

// notifies the controller manager that this client doesn't use the given animation any more.
// these calls must be balanced with AnimationAddRef() calls
void CAnimationManager::AnimationReleaseCAF(GlobalAnimationHeaderCAF& rCAF)
{
	int32 RefCount = rCAF.m_nRef_by_Model;
	if (RefCount == 0)
		rCAF.Release();
}
void CAnimationManager::AnimationReleaseAIM(GlobalAnimationHeaderAIM& rAIM)
{
	int32 RefCount = rAIM.m_nRef_by_Model;
	if (RefCount == 0)
		rAIM.Release();
}
void CAnimationManager::AnimationReleaseLMG(GlobalAnimationHeaderLMG& rLMG)
{
	int32 RefCount = rLMG.m_nRef_by_Model;
	if (RefCount == 0)
		rLMG.Release();
}

//////////////////////////////////////////////////////////////////////////
IAnimEventList* CAnimationManager::GetAnimEventList(const char* animationFilePath)
{
	const CAnimationManager* cThis = this;
	const IAnimEventList* pAnimEventArray = cThis->GetAnimEventList(animationFilePath);
	return const_cast<IAnimEventList*>(pAnimEventArray);
}

const IAnimEventList* CAnimationManager::GetAnimEventList(const char* animationFilePath) const
{
	const int32 globalCafId = GetGlobalIDbyFilePath_CAF(animationFilePath);
	if (0 <= globalCafId)
	{
		assert(globalCafId < (int32)(m_arrGlobalCAF.size()));
		return &m_arrGlobalCAF[globalCafId].m_AnimEventsCAF;
	}

	const int32 globalLmgId = GetGlobalIDbyFilePath_LMG(animationFilePath);
	if (0 <= globalLmgId)
	{
		assert(globalLmgId < (int32)(m_arrGlobalLMG.size()));
		return &m_arrGlobalLMG[globalLmgId].m_AnimEventsLMG;
	}

	return NULL;
}

bool CAnimationManager::SaveAnimEventToXml(const CAnimEventData& dataIn, XmlNodeRef& dataOut)
{
	if (!dataOut)
		return false;

	dataOut->setTag("event");
	dataOut->setAttr("name", dataIn.GetName());
	dataOut->setAttr("time", dataIn.GetNormalizedTime());
	dataOut->setAttr("endTime", dataIn.GetNormalizedEndTime());
	dataOut->setAttr("parameter", dataIn.GetCustomParameter());
	dataOut->setAttr("bone", dataIn.GetBoneName());
	dataOut->setAttr("offset", dataIn.GetOffset());
	dataOut->setAttr("dir", dataIn.GetDirection());
	dataOut->setAttr("model", dataIn.GetModelName());

	return true;
}

bool CAnimationManager::LoadAnimEventFromXml(const XmlNodeRef& dataIn, CAnimEventData& dataOut)
{
	if (!dataIn)
		return false;

	if (stack_string("event") != dataIn->getTag())
		return false;

	XmlString sEventName;
	if (!(sEventName = dataIn->getAttr("name")))
		sEventName = "__unnamed__";

	float fTime = 0.f;
	dataIn->getAttr("time", fTime);

	float endTime = fTime;
	dataIn->getAttr("endTime", endTime);

	XmlString sParameter;
	sParameter = dataIn->getAttr("parameter");

	XmlString sBoneName;
	sBoneName = dataIn->getAttr("bone");

	Vec3 vOffset(0, 0, 0);
	dataIn->getAttr("offset", vOffset);

	Vec3 vDir(0, 0, 0);
	dataIn->getAttr("dir", vDir);

	XmlString sModel;
	sModel = dataIn->getAttr("model");

	dataOut.SetName(sEventName);
	dataOut.SetNormalizedTime(fTime);
	dataOut.SetNormalizedEndTime(endTime);
	dataOut.SetCustomParameter(sParameter);
	dataOut.SetBoneName(sBoneName);
	dataOut.SetOffset(vOffset);
	dataOut.SetDirection(vDir);
	dataOut.SetModelName(sModel);

	return true;
}

void CAnimationManager::InitializeSegmentationDataFromAnimEvents(const char* animationFilePath)
{
	const int32 globalCafId = GetGlobalIDbyFilePath_CAF(animationFilePath);
	if (globalCafId < 0)
		return;

	static const uint32 s_crc32_segment1 = CCrc32::ComputeLowercase("segment1");
	static const uint32 s_crc32_segment2 = CCrc32::ComputeLowercase("segment2");
	static const uint32 s_crc32_segment3 = CCrc32::ComputeLowercase("segment3");

	GlobalAnimationHeaderCAF& cafHeader = m_arrGlobalCAF[globalCafId];
	cafHeader.m_Segments = 1;

	const IAnimEventList& animEvents = cafHeader.m_AnimEventsCAF;
	const uint32 animEventCount = animEvents.GetCount();
	for (uint32 i = 0; i < animEventCount; ++i)
	{
		const CAnimEventData& animEvent = animEvents.GetByIndex(i);
		const uint32 eventNameCRC32 = animEvent.GetNameLowercaseCRC32();
		float normalizedTime = animEvent.GetNormalizedTime();

		uint32 ind = -1;
		if (eventNameCRC32 == s_crc32_segment1)
		{
			ind = 1;
		}
		else if (eventNameCRC32 == s_crc32_segment2)
		{
			ind = 2;
		}
		else if (eventNameCRC32 == s_crc32_segment3)
		{
			ind = 3;
		}

		if (ind != -1)
		{
			cafHeader.m_SegmentsTime[ind] = normalizedTime;
			++cafHeader.m_Segments;
		}
	}
}

//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
bool CAnimationManager::CreateGlobalHeaderDBA(DynArray<string>& arrFilePathDBA)
{
	uint32 numDBAs = arrFilePathDBA.size();
	for (size_t d = 0; d < numDBAs; d++)
	{
		const char* pFilePathDBA = arrFilePathDBA[d];

		uint32 numHeaders = m_arrGlobalHeaderDBA.size();
		uint32 exits = 0;
		for (uint32 dba = 0; dba < numHeaders; dba++)
		{
			const char* pname = m_arrGlobalHeaderDBA[dba].m_strFilePathDBA;
			int32 same = stricmp(pname, pFilePathDBA);
			if (same == 0)
			{
				exits = 1;
				break;
			}
		}

		if (exits == 0)
		{
			//create GloablHeaderDBA and load data
			CGlobalHeaderDBA HeaderDBA;
			HeaderDBA.CreateDatabaseDBA(pFilePathDBA);
			m_arrGlobalHeaderDBA.push_back(HeaderDBA);
		}
	}
	return 0;
}

//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
bool CAnimationManager::DBA_PreLoad(const char* pFilePathDBA, bool highPriority)
{
	stack_string strPath = pFilePathDBA;
	PathUtil::UnifyFilePath(strPath);

	uint32 numHeaders = m_arrGlobalHeaderDBA.size();
	for (uint32 dba = 0; dba < numHeaders; dba++)
	{
		const char* pname = m_arrGlobalHeaderDBA[dba].m_strFilePathDBA;
		int32 same = stricmp(pname, strPath.c_str());
		if (same == 0)
		{
			if (Console::GetInst().ca_UseIMG_CAF)
				m_arrGlobalHeaderDBA[dba].StartStreamingDBA(highPriority);
			else
				m_arrGlobalHeaderDBA[dba].LoadDatabaseDBA("");
			return 1;
		}
	}

	//create GloablHeaderDBA and load data
	CGlobalHeaderDBA HeaderDBA;
	HeaderDBA.CreateDatabaseDBA(strPath.c_str());
	m_arrGlobalHeaderDBA.push_back(HeaderDBA);
	uint32 lastDBA = m_arrGlobalHeaderDBA.size() - 1;

	if (Console::GetInst().ca_UseIMG_CAF)
		m_arrGlobalHeaderDBA[lastDBA].StartStreamingDBA(highPriority);
	else
		m_arrGlobalHeaderDBA[lastDBA].LoadDatabaseDBA("");

	return 0;
}

bool CAnimationManager::DBA_LockStatus(const char* pFilePathDBA, uint32 status, bool highPriority)
{
	stack_string strPath = pFilePathDBA;
	PathUtil::UnifyFilePath(strPath);

	uint32 numHeaders = m_arrGlobalHeaderDBA.size();
	for (uint32 dba = 0; dba < numHeaders; dba++)
	{
		const char* pname = m_arrGlobalHeaderDBA[dba].m_strFilePathDBA;
		int32 same = stricmp(pname, strPath.c_str());
		if (same == 0)
		{
			m_arrGlobalHeaderDBA[dba].m_bDBALock = status;
			if (status)
			{
				if (Console::GetInst().ca_UseIMG_CAF)
					m_arrGlobalHeaderDBA[dba].StartStreamingDBA(highPriority);
				else
					m_arrGlobalHeaderDBA[dba].LoadDatabaseDBA("");
			}
			return 1;
		}
	}
	return 0;
}

bool CAnimationManager::DBA_Unload(const char* pFilePathDBA)
{
	stack_string strPath = pFilePathDBA;
	PathUtil::UnifyFilePath(strPath);

	uint32 numHeadersDBA = m_arrGlobalHeaderDBA.size();
	if (numHeadersDBA == 0)
		return 1;

	CGlobalHeaderDBA* parrGlobalDBA = &m_arrGlobalHeaderDBA[0];
	for (uint32 d = 0; d < numHeadersDBA; d++)
		parrGlobalDBA[d].m_nUsedAnimations = 0;

	uint32 numHeadersCAF = m_arrGlobalCAF.size();
	if (numHeadersCAF)
	{
		GlobalAnimationHeaderCAF* parrGlobalCAF = &m_arrGlobalCAF[0];
		for (uint32 i = 0; i < numHeadersCAF; i++)
		{
			if (parrGlobalCAF[i].m_nRef_at_Runtime == 0)
				continue;
			uint32 nCRC32 = parrGlobalCAF[i].m_FilePathDBACRC32;
			if (nCRC32)
			{
				for (uint32 d = 0; d < numHeadersDBA; d++)
				{
					if (nCRC32 == parrGlobalDBA[d].m_FilePathDBACRC32)
					{
						parrGlobalDBA[d].m_nUsedAnimations++;
						parrGlobalDBA[d].m_nLastUsedTimeDelta = 0;
					}
				}
			}
		}
	}

	for (uint32 dba = 0; dba < numHeadersDBA; dba++)
	{
		const char* pname = m_arrGlobalHeaderDBA[dba].m_strFilePathDBA;
		int32 same = stricmp(pname, strPath.c_str());
		if (same == 0)
		{
			m_arrGlobalHeaderDBA[dba].m_bDBALock = 0;

			if (m_arrGlobalHeaderDBA[dba].m_pDatabaseInfo == 0)
				return 1;
			if (m_arrGlobalHeaderDBA[dba].m_nUsedAnimations)
			{
				//CryFatalError("CryAnimation: Animations still in use. Can't delete DBA: %s",m_arrGlobalHeaderDBA[dba].m_strFilePathDBA.c_str() );
				continue;
			}
			m_arrGlobalHeaderDBA[dba].DeleteDatabaseDBA();
			return 1;
		}
	}

	return 0;
}

void CAnimationManager::InitialiseRunTimePools()
{
	const int numParametrics = Console::GetInst().ca_ParametricPoolSize;

	if (numParametrics != g_totalParametrics)
	{
		delete[] g_parametricPool;
		delete[] g_usedParametrics;
		g_parametricPool = new SParametricSamplerInternal[numParametrics];
		g_usedParametrics = new bool[numParametrics];
		g_totalParametrics = numParametrics;
	}
	memset(g_usedParametrics, 0, numParametrics);
}

void CAnimationManager::DestroyRunTimePools()
{
	delete[] g_parametricPool;
	delete[] g_usedParametrics;
	g_parametricPool = NULL;
	g_usedParametrics = NULL;
	g_totalParametrics = 0;
}

bool CAnimationManager::Unload_All_Animation()
{
	uint32 numHeadersCAF = m_arrGlobalCAF.size();
	if (numHeadersCAF)
	{
		//remove all CAFs
		GlobalAnimationHeaderCAF* parrGlobalCAF = &m_arrGlobalCAF[0];
		for (uint32 i = 0; i < numHeadersCAF; i++)
		{
			parrGlobalCAF[i].m_nRef_at_Runtime = 0;
			uint32 nCRC32 = parrGlobalCAF[i].m_FilePathDBACRC32;
			if (nCRC32 == 0 || nCRC32 == -1)
				parrGlobalCAF[i].ClearControllers();
		}
	}

	uint32 numHeadersDBA = m_arrGlobalHeaderDBA.size();
	for (uint32 dba = 0; dba < numHeadersDBA; dba++)
	{
		m_arrGlobalHeaderDBA[dba].m_nUsedAnimations = 0;
		if (m_arrGlobalHeaderDBA[dba].m_pDatabaseInfo == 0)
			continue;
		m_arrGlobalHeaderDBA[dba].DeleteDatabaseDBA();
	}

	InitialiseRunTimePools();

	return 1;
}

bool CAnimationManager::DBA_Unload_All()
{
	uint32 numHeadersDBA = m_arrGlobalHeaderDBA.size();
	if (numHeadersDBA == 0)
		return 1;

	CGlobalHeaderDBA* parrGlobalDBA = &m_arrGlobalHeaderDBA[0];
	for (uint32 d = 0; d < numHeadersDBA; d++)
		parrGlobalDBA[d].m_nUsedAnimations = 0;

	for (uint32 d = 0; d < numHeadersDBA; d++)  //unlock all
		parrGlobalDBA[d].m_bDBALock = 0;

	uint32 numHeadersCAF = m_arrGlobalCAF.size();
	if (numHeadersCAF)
	{
		GlobalAnimationHeaderCAF* parrGlobalCAF = &m_arrGlobalCAF[0];
		for (uint32 i = 0; i < numHeadersCAF; i++)
		{
			//	parrGlobalCAF[i].m_nRef_at_Runtime=0;

			uint32 nCRC32 = parrGlobalCAF[i].m_FilePathDBACRC32;
			if (nCRC32 == 0 || nCRC32 == -1)
				parrGlobalCAF[i].ClearControllers();

			if (parrGlobalCAF[i].m_nRef_at_Runtime == 0)
				continue;
			if (nCRC32)
			{
				for (uint32 d = 0; d < numHeadersDBA; d++)
				{
					if (nCRC32 == parrGlobalDBA[d].m_FilePathDBACRC32)
					{
						parrGlobalDBA[d].m_nUsedAnimations++;
						parrGlobalDBA[d].m_nLastUsedTimeDelta = 0;
					}
				}
			}
		}
	}

	for (uint32 dba = 0; dba < numHeadersDBA; dba++)
	{
		if (m_arrGlobalHeaderDBA[dba].m_pDatabaseInfo == 0)
			continue;
		if (m_arrGlobalHeaderDBA[dba].m_nUsedAnimations)
		{
			//CryFatalError("CryAnimation: Animations still in use. Can't delete DBA : %s  used: %d",m_arrGlobalHeaderDBA[dba].m_strFilePathDBA.c_str(), m_arrGlobalHeaderDBA[dba].m_nUsedAnimations );
			continue;
		}
		m_arrGlobalHeaderDBA[dba].DeleteDatabaseDBA();
	}

	return 1;
}

EReloadCAFResult CAnimationManager::ReloadCAF(const char* szFilePathCAF)
{
	uint32 nCRC32 = CCrc32::ComputeLowercase(szFilePathCAF);

	uint32 numAIM = m_arrGlobalAIM.size();
	for (uint32 id = 0; id < numAIM; id++)
	{
		GlobalAnimationHeaderAIM& rAIM = m_arrGlobalAIM[id];
		if (rAIM.m_FilePathCRC32 == nCRC32)
		{
			assert(!strcmp(rAIM.GetFilePath(), szFilePathCAF));
			rAIM.m_nControllers = 0;
			int status = rAIM.LoadAIM();
			if (status)
			{
				CryLog("Hot loaded animation CAF-file for an Directional-Pose: %s", rAIM.m_FilePath.c_str());

				CDefaultSkeleton* pDefaultSkeleton = g_pCharacterManager->GetModelByAimPoseID(id);
				if (pDefaultSkeleton)
				{
					uint32 numDB = pDefaultSkeleton->m_poseBlenderAimDesc.m_blends.size();
					for (uint32 d = 0; d < numDB; d++)
					{
						const char* strAnimToken = pDefaultSkeleton->m_poseBlenderAimDesc.m_blends[d].m_AnimToken;
						uint32 IsAIM = (CryStringUtils::stristr(rAIM.m_FilePath.c_str(), strAnimToken) != 0);
						if (IsAIM)
						{
							rAIM.ProcessDirectionalPoses(pDefaultSkeleton, pDefaultSkeleton->m_poseBlenderAimDesc.m_blends, pDefaultSkeleton->m_poseBlenderAimDesc.m_rotations, pDefaultSkeleton->m_poseBlenderAimDesc.m_positions);
							CryLog("Re-Processed Aim-Pose: %s", rAIM.m_FilePath.c_str());
							break;
						}
					}

					uint32 numLB = pDefaultSkeleton->m_poseBlenderLookDesc.m_blends.size();
					for (uint32 d = 0; d < numLB; d++)
					{
						const char* strAnimToken = pDefaultSkeleton->m_poseBlenderLookDesc.m_blends[d].m_AnimToken;
						uint32 IsLOOK = (CryStringUtils::stristr(rAIM.m_FilePath.c_str(), strAnimToken) != 0);
						if (IsLOOK)
						{
							rAIM.ProcessDirectionalPoses(pDefaultSkeleton, pDefaultSkeleton->m_poseBlenderLookDesc.m_blends, pDefaultSkeleton->m_poseBlenderLookDesc.m_rotations, pDefaultSkeleton->m_poseBlenderLookDesc.m_positions);
							CryLog("Re-Processed Look-Pose: %s", rAIM.m_FilePath.c_str());
							break;
						}
					}
				}
				else
				{
					CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "Could not process aim poses from '%s' because could not find a model associated to it.", rAIM.m_FilePath.c_str());
				}
			}

			if (g_pCharacterManager->m_pStreamingListener)
			{
				g_pCharacterManager->m_pStreamingListener->NotifyAnimReloaded(id);
			}

			return status ? CR_RELOAD_SUCCEED : CR_RELOAD_FAILED;
		}
	}

	//asset not found in the AIM-array
	//now check the CAF array
	uint32 numCAF = m_arrGlobalCAF.size();
	for (uint32 id = 0; id < numCAF; id++)
	{
		GlobalAnimationHeaderCAF& rCAF = m_arrGlobalCAF[id];
		if (rCAF.m_FilePathCRC32 == nCRC32)
		{
			assert(!strcmp(rCAF.GetFilePath(), szFilePathCAF));
			rCAF.m_nControllers = 0;
			int status = rCAF.LoadCAF();
			if (status)
			{
				uint32 numBSpaces = m_arrGlobalLMG.size();
				for (uint32 bs = 0; bs < numBSpaces; bs++)
				{
					m_arrGlobalLMG[bs].m_DimPara[0].m_nInitialized = 0;
					m_arrGlobalLMG[bs].m_DimPara[1].m_nInitialized = 0;
					m_arrGlobalLMG[bs].m_DimPara[2].m_nInitialized = 0;
					m_arrGlobalLMG[bs].m_DimPara[3].m_nInitialized = 0;
				}
				CryLog("Hot loaded animation CAF-file: %s", rCAF.GetFilePath());
			}
			else
			{
				gEnv->pLog->LogError("CAF Reloading failed: %s", szFilePathCAF);
			}

			if (g_pCharacterManager->m_pStreamingListener)
			{
				g_pCharacterManager->m_pStreamingListener->NotifyAnimReloaded(id);
			}

			return status != 0 ? CR_RELOAD_SUCCEED : CR_RELOAD_FAILED;
		}
	}

	return CR_RELOAD_GAH_NOT_IN_ARRAY;
}

int CAnimationManager::ReloadLMG(const char* szFilePath)
{
	CAnimationSet* pAnimationSet = g_pCharacterManager->GetAnimationSetUsedInCharEdit();
	if (pAnimationSet == 0)
	{
		gEnv->pLog->LogError("BlendSpace reloading failed: %s", szFilePath);
		return -1;
	}

	uint32 nCRC32 = CCrc32::ComputeLowercase(szFilePath);

	uint32 numBlendSpaces = m_arrGlobalLMG.size();
	for (uint32 id = 0; id < numBlendSpaces; id++)
	{
		GlobalAnimationHeaderLMG& rBlendSpace = m_arrGlobalLMG[id];
		if (rBlendSpace.m_FilePathCRC32 == nCRC32)
		{
			assert(!strcmp(rBlendSpace.GetFilePath(), szFilePath));
			bool status = false;
#ifdef EDITOR_PCDEBUGCODE
			stack_string path = szFilePath;
			path.MakeLower();
			CachedBSPACES::iterator it = m_cachedBSPACES.find(path);

			if (it != m_cachedBSPACES.end())
			{
				XmlNodeRef root = gEnv->pSystem->LoadXmlFromBuffer(it->second.data(), it->second.size());
				if (root)
				{
					status = rBlendSpace.LoadFromXML(pAnimationSet, root);
					if (!status)
					{
						gEnv->pLog->LogError("BlendSpace reloading failed: %s", szFilePath);
					}
				}
				else
				{
					gEnv->pLog->LogError("Failed to parse cached XML: %s", szFilePath);
				}
			}
			else
#endif
			{
				status = rBlendSpace.LoadAndParseXML(pAnimationSet, 1);
				if (status)
					CryLog("Hot loaded animation BlendSpace-file: %s", szFilePath);
				else
					gEnv->pLog->LogError("BlendSpace reloading failed: %s", szFilePath);
			}

			if (g_pCharacterManager->m_pStreamingListener)
			{
				g_pCharacterManager->m_pStreamingListener->NotifyAnimReloaded(id);
			}
			return status;
		}
	}

	gEnv->pLog->LogError("BlendSpace reloading failed: %s", szFilePath);
	return -1;
}

uint32 CAnimationManager::GetDBACRC32fromFilePath(const char* szFilePathCAF)
{
	uint32 nCRC32 = CCrc32::ComputeLowercase(szFilePathCAF);

	uint32 numCAF = m_arrGlobalCAF.size();
	for (uint32 id = 0; id < numCAF; id++)
	{
		GlobalAnimationHeaderCAF& rCAF = m_arrGlobalCAF[id];
		if (rCAF.m_FilePathCRC32 == nCRC32)
			return rCAF.m_FilePathDBACRC32;
	}
	return 0;
}

bool CAnimationManager::IsDatabaseInMemory(uint32 nDBACRC32)
{
	size_t numDBA_Files = m_arrGlobalHeaderDBA.size();
	for (uint32 d = 0; d < numDBA_Files; d++)
	{
		if (nDBACRC32 == m_arrGlobalHeaderDBA[d].m_FilePathDBACRC32)
			return m_arrGlobalHeaderDBA[d].InMemory();
	}
	return 0;
}

//------------------------------------------------------------------------------
// Unloads animation from memory and remove
//------------------------------------------------------------------------------
void CAnimationManager::UnloadAnimationCAF(GlobalAnimationHeaderCAF& rCAF)
{
	if (Console::GetInst().ca_UnloadAnimationCAF == 0)
		return;
	uint32 requested = rCAF.IsAssetRequested();
	if (requested)
		return;
	if (rCAF.m_nControllers2 == 0)
		return;

	assert(rCAF.GetControllersCount());

	rCAF.ClearControllers();

	if (g_pCharacterManager->m_pStreamingListener)
	{
		int32 globalID = GetGlobalIDbyFilePath_CAF(rCAF.GetFilePath());
		g_pCharacterManager->m_pStreamingListener->NotifyAnimUnloaded(globalID);
	}

	//	g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,0,	"UnloadingAsset: Name: %s",rCAF.GetFilePath() );
}

void CAnimationManager::UnloadAnimationAIM(int nGLobalAnimID)
{
	assert(m_arrGlobalAIM[nGLobalAnimID].GetControllersCount());
	m_arrGlobalAIM[nGLobalAnimID].ClearControllers();
}

//struct AnimSearchHelper {
//
//	typedef std::vector<int> TIndexVector;
//	typedef std::map<uint32 /*crc*/, TIndexVector*/*vector of indexes*/> TFoldersVector;
//
//	void AddAnimation(uint32 crc, uint32 gahIndex);
//	void AddAnimation(const string& path, uint32 gahIndex);
//
//	TIndexVector * GetAnimationsVector(uint32 crc);
//	TIndexVector * GetAnimationsVector(const string& path);
//
//};

bool AnimSearchHelper::AddAnimation(uint32 crc, uint32 gahIndex)
{
	bool newPath = false;
	TIndexVector* vect = GetAnimationsVector(crc);
	if (!vect)
	{
		vect = new TIndexVector;
		m_AnimationsMap[crc] = vect;
		newPath = true;
	}

	vect->push_back(gahIndex);
	return newPath;
}

void AnimSearchHelper::AddAnimation(const string& path, uint32 gahIndex)
{
	stack_string pathDir = PathUtil::GetPathWithoutFilename(path);
	PathUtil::ToUnixPath(pathDir);
	if (strcmp(pathDir, "animations/human/male/behavior/fear/") == 0)
		int A = 0;
	if (strcmp(pathDir, "animations\\human\\male\\behavior\\fear\\") == 0)
		int A = 0;

	uint32 crc = CCrc32::ComputeLowercase(pathDir);
	if (AddAnimation(crc, gahIndex))
	{
		for (int32 lastSlashPos = pathDir.rfind('/'); lastSlashPos >= 0; lastSlashPos = pathDir.rfind('/', lastSlashPos - 1))
		{
			uint32 parentDirCRC = CCrc32::ComputeLowercase(pathDir, size_t(lastSlashPos + 1), 0);
			TSubFolderCrCVector* pSubFolderVec = NULL;
			TSubFoldersMap::iterator parentFolderIter = m_SubFoldersMap.find(parentDirCRC);
			if (parentFolderIter == m_SubFoldersMap.end())
			{
				pSubFolderVec = new TSubFolderCrCVector;
				m_SubFoldersMap[parentDirCRC] = pSubFolderVec;
			}
			else
			{
				pSubFolderVec = parentFolderIter->second;
			}

			pSubFolderVec->push_back(crc);
		}
	}
}

AnimSearchHelper::TSubFolderCrCVector* AnimSearchHelper::GetSubFoldersVector(uint32 crc)
{
	TSubFoldersMap::iterator parentFolderIter = m_SubFoldersMap.find(crc);
	if (parentFolderIter != m_SubFoldersMap.end())
	{
		return parentFolderIter->second;
	}
	else
	{
		return NULL;
	}
}

AnimSearchHelper::TIndexVector* AnimSearchHelper::GetAnimationsVector(uint32 crc)
{
	TFoldersVector::iterator it = m_AnimationsMap.find(crc);

	if (it != m_AnimationsMap.end())
		return it->second;

	return 0;
}

AnimSearchHelper::TIndexVector* AnimSearchHelper::GetAnimationsVector(const string& path)
{
	uint32 crc = CCrc32::ComputeLowercase(path);
	return GetAnimationsVector(crc);
}

void AnimSearchHelper::Clear()
{
	for (TFoldersVector::iterator it = m_AnimationsMap.begin(), end = m_AnimationsMap.end(); it != end; ++it)
		delete it->second;
	for (TSubFoldersMap::iterator it = m_SubFoldersMap.begin(), end = m_SubFoldersMap.end(); it != end; ++it)
		delete it->second;
	m_AnimationsMap.clear();
	m_SubFoldersMap.clear();
}

size_t CAnimationManager::GetSizeOfDBA()
{
	size_t nSize = 0;
	uint32 numDBAs = m_arrGlobalHeaderDBA.size();
	for (uint32 i = 0; i < numDBAs; i++)
		nSize += m_arrGlobalHeaderDBA[i].SizeOf_DBA();
	return nSize;
}

void CAnimationManager::GetMemoryUsage(class ICrySizer* pSizer) const
{
	SIZER_SUBCOMPONENT_NAME(pSizer, "AnimationKeys");
	pSizer->AddObject(m_AnimationMapCAF);
	pSizer->AddObject(m_arrGlobalCAF);
	pSizer->AddObject(m_arrGlobalAIM);
	pSizer->AddObject(m_arrGlobalLMG);
	pSizer->AddObject(m_arrGlobalHeaderDBA);
}

void CAnimationManager::DebugAnimUsage(uint32 printtxt)
{
	DEFINE_PROFILER_FUNCTION();

#ifndef CONSOLE_CONST_CVAR_MODE
	//	if (Console::GetInst().ca_DebugAnimMemTracking==0)
	//		return;
	if (m_shuttingDown)
		return;
	float fRed[4] = { 1, 0, 0, 1 };
	float fGreen[4] = { 0, 1, 0, 1 };
	float fBlue[4] = { 0, 0, 1, 1 };

	size_t numDBA_Files = m_arrGlobalHeaderDBA.size();

	for (uint32 d = 0; d < numDBA_Files; d++)
		m_arrGlobalHeaderDBA[d].m_nEmpty = 1;

	for (uint32 x = 0; x < numDBA_Files; x++)
	{
		uint32 biggest = 0;
		int32 b = -1;
		for (uint32 d = 0; d < numDBA_Files; d++)
		{
			uint32 nUsed = m_arrGlobalHeaderDBA[d].m_nEmpty;
			size_t nSizeOfDBA = m_arrGlobalHeaderDBA[d].SizeOf_DBA();
			if (biggest < nSizeOfDBA && nUsed)
			{
				biggest = nSizeOfDBA;
				b = d;
			}
		}

		if (b > -1)
		{
			m_arrGlobalHeaderDBA[b].m_nEmpty = 0;
			const char* pName = m_arrGlobalHeaderDBA[b].m_strFilePathDBA;
			uint32 nDBACRC32 = m_arrGlobalHeaderDBA[b].m_FilePathDBACRC32;
			uint32 nUsedAssets = m_arrGlobalHeaderDBA[b].m_nUsedAnimations;
			size_t nSizeOfDBA = m_arrGlobalHeaderDBA[b].SizeOf_DBA();
			uint32 nLastUsedTimeSec = m_arrGlobalHeaderDBA[b].m_nLastUsedTimeDelta / 1000;
			uint32 nLock = m_arrGlobalHeaderDBA[b].m_bDBALock;

			if (m_arrGlobalHeaderDBA[b].m_pDatabaseInfo == 0)
			{
				//	fColorGreen[3]=0.4f;
				//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 0.9f, fColorGreen, false,"UsedAssets: %04d  nTCount: %08x  Size: %08d  FilePathDBA: %s",nUsedAssets,nTCount,nSizeOfDBA,pName );
				//	g_YLine+=9.0f;
			}
			else
			{
				float fColor[4] = { 0, 1, 0, 1 };
				if (nLock)
				{
					fColor[0] = 1.0f;
					fColor[1] = 0.0f;
				}
				if (printtxt)
				{
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.1f, fColor, false, "UsedAssets: %04d  nTCount: %u  Size: %" PRISIZE_T "  FilePathDBA: %s", nUsedAssets, nLastUsedTimeSec, nSizeOfDBA, pName);
					g_YLine += 11.0f;
				}
			}
		}
	}
	g_YLine += 16.0f;

	size_t nAnimationManager = sizeof(CAnimationManager);

	size_t nMap = m_AnimationMapCAF.GetAllocMemSize();

	size_t nFilePathSize = 0;

	{
		uint32 numCAF = m_arrGlobalCAF.size();
		for (uint32 i = 0; i < numCAF; i++)
			nFilePathSize += m_arrGlobalCAF[i].m_FilePath.capacity();
		uint32 numAIM = m_arrGlobalAIM.size();
		for (uint32 i = 0; i < numAIM; i++)
			nFilePathSize += m_arrGlobalAIM[i].m_FilePath.capacity();
		uint32 numLMG = m_arrGlobalLMG.size();
		for (uint32 i = 0; i < numLMG; i++)
			nFilePathSize += m_arrGlobalLMG[i].m_FilePath.capacity();

		numCAF = m_arrGlobalCAF.capacity();
		numAIM = m_arrGlobalAIM.capacity();
		numLMG = m_arrGlobalLMG.capacity();

		size_t nEmptyHeaderSize = 0;
		nEmptyHeaderSize += numCAF * sizeof(GlobalAnimationHeaderCAF);
		nEmptyHeaderSize += numAIM * sizeof(GlobalAnimationHeaderAIM);
		nEmptyHeaderSize += numLMG * sizeof(GlobalAnimationHeaderLMG);
		if (printtxt)
		{
			g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.0f, fRed, false, "size of path names: %" PRISIZE_T " bytes     Empty Headers: %" PRISIZE_T " bytes", nFilePathSize, nEmptyHeaderSize);
			g_YLine += 16.0f;
		}
	}

	//calculate size of DBAs
	size_t nDBAalloc = GetSizeOfDBA();
	nAnimationManager += nDBAalloc;
	if (printtxt)
	{
		g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.0f, fRed, false, "DBA: %" PRISIZE_T " KBytes   map: %" PRISIZE_T " KBytes", nDBAalloc / 1024, nMap / 1024);
		g_YLine += 16.0f;
	}

	constexpr uint32 kDisplayCafUsageMask    = BIT(1);
	constexpr uint32 kDumpCafUsageMask       = BIT(2);
	constexpr uint32 kDisplayBSpaceUsageMask = BIT(3);
	constexpr uint32 kDumpBSpaceUsageMask    = BIT(4);

	uint32 nUsedCAFs = 0;
	{
		uint32 numCAF = m_arrGlobalCAF.size();
		size_t nSize = m_arrGlobalCAF.get_alloc_size() + nMap;
		for (uint32 i = 0; i < numCAF; i++)
			nSize += m_arrGlobalCAF[i].SizeOfCAF();
		uint32 nNumHeaders = m_arrGlobalCAF.size();
		uint32 nLoaded = 0;
		uint32 nSizeOfLoadedKeys = 0;
		for (uint32 i = 0; i < nNumHeaders; i++)
		{
			uint32 IsAssetLoaded = m_arrGlobalCAF[i].IsAssetLoaded();
			uint32 IsDBAFile = m_arrGlobalCAF[i].m_FilePathDBACRC32;
			if (m_arrGlobalCAF[i].m_FilePathDBACRC32 == -1)
				IsDBAFile = 0;

			if (IsAssetLoaded && IsDBAFile == 0)
			{
				nLoaded++;
				uint32 nSizeOfCAF = m_arrGlobalCAF[i].SizeOfCAF();
				nSizeOfLoadedKeys += nSizeOfCAF;

				uint32 nRef_at_Runtime = m_arrGlobalCAF[i].m_nRef_at_Runtime;

				if (printtxt && Console::GetInst().ca_DebugAnimUsage & kDisplayCafUsageMask)
				{
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.1f, fGreen, false, "CafInMemory: %7d ref: %5d FilePath: %s", nSizeOfCAF, nRef_at_Runtime, m_arrGlobalCAF[i].GetFilePath());
					g_YLine += 11.0f;
				}

				if (printtxt && Console::GetInst().ca_DebugAnimUsage & kDumpCafUsageMask)
				{
					CryLogAlways("CafInMemory: %07u FilePath: %s", nSizeOfCAF, m_arrGlobalCAF[i].GetFilePath());
				}
			}
			nUsedCAFs += (m_arrGlobalCAF[i].m_nTouchedCounter != 0);
		}

		if (printtxt && Console::GetInst().ca_DebugAnimUsage & kDumpCafUsageMask)
		{
			CryLogAlways("nSizeOfLoadedKeys: %07u", nSizeOfLoadedKeys);
			Console::GetInst().ca_DebugAnimUsage &= ~kDumpCafUsageMask;
		}
		if (printtxt)
		{
			g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.0f, fRed, false, "CAF: %04d    Loaded: %04d    Used: %04d   Memory: %05" PRISIZE_T " / %05d KBytes", nNumHeaders, nLoaded, nUsedCAFs, nSize / 1024, nSizeOfLoadedKeys / 1024);
			g_YLine += 16.0f;
		}
		nAnimationManager += nSize;
	}

	{
		uint32 numAIM = m_arrGlobalAIM.size();
		size_t nSize = m_arrGlobalAIM.get_alloc_size();
		for (uint32 i = 0; i < numAIM; i++)
			nSize += m_arrGlobalAIM[i].SizeOfAIM();

		uint32 nNumHeaders = m_arrGlobalAIM.size();
		uint32 nLoaded = 0;
		uint32 nUsed = 0;
		for (uint32 i = 0; i < nNumHeaders; i++)
		{
			nLoaded += (m_arrGlobalAIM[i].IsAssetLoaded() != 0);
			nUsed += (m_arrGlobalAIM[i].m_nTouchedCounter != 0);
		}
		if (printtxt)
		{
			g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.0f, fRed, false, "AIM: %04d    Loaded: %04d    Used: %04d   Memory: %05" PRISIZE_T " KBytes", nNumHeaders, nLoaded, nUsed, nSize / 1024);
			g_YLine += 16.0f;
		}
		nAnimationManager += nSize;
	}

	{
		uint32 numLMG = m_arrGlobalLMG.size();
		size_t nSize = m_arrGlobalLMG.get_alloc_size();
		for (uint32 i = 0; i < numLMG; i++)
			nSize += m_arrGlobalLMG[i].SizeOfLMG();

		uint32 nNumHeaders = m_arrGlobalLMG.size();
		uint32 nLoaded = 0;
		uint32 nUsed = 0;
		for (uint32 i = 0; i < nNumHeaders; i++)
		{
			nLoaded += (m_arrGlobalLMG[i].IsAssetLoaded() != 0);
			nUsed += (m_arrGlobalLMG[i].m_nTouchedCounter != 0);
		}
		if (printtxt)
		{
			g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.0f, fRed, false, "Blendspace: %04d    Loaded: %04d    Used: %04d  Memory: %05" PRISIZE_T "KBytes", nNumHeaders, nLoaded, nUsed, nSize / 1024);
			g_YLine += 16.0f;

			if (Console::GetInst().ca_DebugAnimUsage & kDisplayBSpaceUsageMask || Console::GetInst().ca_DebugAnimUsage & kDumpBSpaceUsageMask)
			{
				for (uint32 i = 0; i < nNumHeaders; i++)
				{
					if (m_arrGlobalLMG[i].m_nTouchedCounter != 0)
					{
						g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.1f, fGreen, false, "FilePath: %s  ref: %5d  Memory: %05 KBytes", m_arrGlobalLMG[i].GetFilePath(), m_arrGlobalLMG[i].m_nTouchedCounter, m_arrGlobalLMG[i].SizeOfLMG() / 1024);
						g_YLine += 11.0f;

						if (Console::GetInst().ca_DebugAnimUsage & kDumpBSpaceUsageMask)
						{
							CryLogAlways("FilePath: %s  ref: %5d  Memory: %05 KBytes", m_arrGlobalLMG[i].GetFilePath(), m_arrGlobalLMG[i].m_nTouchedCounter, m_arrGlobalLMG[i].SizeOfLMG() / 1024);
						}
					}
				}

				if (Console::GetInst().ca_DebugAnimUsage & kDumpBSpaceUsageMask)
				{
					Console::GetInst().ca_DebugAnimUsage &= ~kDumpBSpaceUsageMask;
				}
			}
		}
		nAnimationManager += nSize;
	}

	if (printtxt)
	{
		CControllerDefragHeap::Stats stats = g_controllerHeap.GetStats();
		size_t moveableFree = stats.defragStats.nCapacity - stats.defragStats.nInUseSize;
		g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.0f, fRed, false, "Defrag heap: %" PRISIZE_T " KB moveable %" PRISIZE_T "KB fixed %.2f%% frag", stats.defragStats.nInUseSize / 1024, stats.bytesInFixedAllocs / 1024,
		                        100.0f * ((moveableFree - stats.defragStats.nLargestFreeBlockSize) / (float)stats.defragStats.nCapacity));
		g_YLine += 16.0f;
	}

	m_AnimMemoryTracker.m_nAnimsCurrent = nAnimationManager;

	m_AnimMemoryTracker.m_nGlobalCAFs = m_arrGlobalCAF.size();
	m_AnimMemoryTracker.m_nUsedGlobalCAFs = nUsedCAFs;

	if (m_AnimMemoryTracker.m_nAnimsMax < nAnimationManager)
		m_AnimMemoryTracker.m_nAnimsMax = nAnimationManager;

	uint64 average = 0;
	if (m_AnimMemoryTracker.m_nAnimsCounter || m_AnimMemoryTracker.m_nAnimsCurrent > 7211000)
	{
		m_AnimMemoryTracker.m_nAnimsCounter++;
		m_AnimMemoryTracker.m_nAnimsAdd += nAnimationManager;
		average = m_AnimMemoryTracker.m_nAnimsAdd / m_AnimMemoryTracker.m_nAnimsCounter;
	}

	static uint32 mupdate = 0x1f;
	mupdate++;
	mupdate &= 0x7f;
	if (mupdate == 0)
		g_pCharacterManager->TrackMemoryOfModels();

	if (printtxt)
	{
		g_YLine += 10.0f;
		g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.2f, fBlue, false, "nAnimsPerFrame: %4d  nAnimsMax: %4d nAnimsAvrg: %4d", m_AnimMemoryTracker.m_nAnimsCurrent / 1024, m_AnimMemoryTracker.m_nAnimsMax / 1024, (uint32)(average / 1024));
		g_YLine += 25.0f;
		g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.2f, fRed, false, "CharInstances:%3d (Mem: %4dKB) SkinInstances:%3d (Mem: %4dKB)  Models:%3d (Mem: %4dKB)", m_AnimMemoryTracker.m_numTCharInstances, m_AnimMemoryTracker.m_nTotalCharMemory / 1024, m_AnimMemoryTracker.m_numTSkinInstances, m_AnimMemoryTracker.m_nTotalSkinMemory / 1024, m_AnimMemoryTracker.m_numModels, m_AnimMemoryTracker.m_nTotalMMemory / 1024);
		g_YLine += 25.0f;
		g_pAuxGeom->Draw2dLabel(1, g_YLine, 3.0f, fRed, false, "Total: %4dKB", (m_AnimMemoryTracker.m_nTotalCharMemory + m_AnimMemoryTracker.m_nTotalSkinMemory + m_AnimMemoryTracker.m_nTotalMMemory + m_AnimMemoryTracker.m_nAnimsCurrent) / 1024);
	}
#endif
}
