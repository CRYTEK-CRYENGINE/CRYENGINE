// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Contains an agent's target tracks and handles updating them

   -------------------------------------------------------------------------
   History:
   - 02:01:2010: Created by Kevin Kirst

*************************************************************************/

#include "StdAfx.h"
#include "TargetTrackGroup.h"
#include "TargetTrackManager.h"
#include "TargetTrack.h"
#include "ObjectContainer.h"

#ifdef TARGET_TRACK_DOTARGETTHREAT
	#include "Puppet.h"
#endif //TARGET_TRACK_DOTARGETTHREAT

#ifdef TARGET_TRACK_DEBUG
	#include "DebugDrawContext.h"
	#include <CryAction/IDebugHistory.h>
	#include <CryGame/IGame.h>
	#include <CryGame/IGameFramework.h>
#endif //TARGET_TRACK_DEBUG

//////////////////////////////////////////////////////////////////////////
CTargetTrackGroup::CTargetTrackGroup(TargetTrackHelpers::ITargetTrackPoolProxy* pTrackPoolProxy, tAIObjectID aiObjectId, uint32 uConfigHash, int nTargetLimit)
	: m_pTrackPoolProxy(pTrackPoolProxy)
	, m_aiObjectId(aiObjectId)
	, m_aiLastBestTargetId(0)
	, m_uConfigHash(uConfigHash)
	, m_nTargetLimit(max(nTargetLimit, 0))
	, m_bNeedSort(false)
	, m_bEnabled(true)
{
	assert(m_pTrackPoolProxy);
	assert(m_aiObjectId > 0);
	assert(m_uConfigHash > 0);

	CAISystem* pAISystem = GetAISystem();
	assert(pAISystem);

	CWeakRef<CAIObject> refAIObject = gAIEnv.pObjectContainer->GetWeakRef(aiObjectId);
	const bool bAIObjectValid = refAIObject.IsValid();

	// Create dummy representation for the dummy potential target
	//	NB: during serialization, dummy objects are created after loading to
	//	ensure we keep the same AI object id
	if (!gEnv->pSystem->IsSerializingFile())
	{
		gAIEnv.pAIObjectManager->CreateDummyObject(m_dummyPotentialTarget.refDummyRepresentation, "Target Track Dummy");
		if (bAIObjectValid)
			InitDummy();
	}

#ifdef TARGET_TRACK_DEBUG
	m_fLastGraphUpdate = 0.0f;
	m_pDebugHistoryManager = gEnv->pGameFramework->CreateDebugHistoryManager();
	assert(m_pDebugHistoryManager);

	memset(m_bDebugGraphOccupied, 0, sizeof(m_bDebugGraphOccupied));
#endif //TARGET_TRACK_DEBUG
}

//////////////////////////////////////////////////////////////////////////
CTargetTrackGroup::~CTargetTrackGroup()
{
	Reset();

#ifdef TARGET_TRACK_DEBUG
	SAFE_RELEASE(m_pDebugHistoryManager);
#endif //TARGET_TRACK_DEBUG
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackGroup::Reset()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	// Add all active tracks back to the pool
	TTargetTrackContainer::iterator itTrack = m_TargetTracks.begin();
	TTargetTrackContainer::iterator itTrackEnd = m_TargetTracks.end();
	for (; itTrack != itTrackEnd; ++itTrack)
	{
		CTargetTrack* pTrack = itTrack->second;
		assert(pTrack);

		m_pTrackPoolProxy->AddTargetTrackToPool(pTrack);
	}

	m_TargetTracks.clear();
	m_SortedTracks.clear();
	m_bNeedSort = false;

#ifdef TARGET_TRACK_DEBUG
	m_pDebugHistoryManager->Clear();
#endif //TARGET_TRACK_DEBUG
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackGroup::Serialize_Write(TSerialize ser)
{
	assert(ser.IsWriting());

	int iTrackCount = m_TargetTracks.size();
	ser.Value("iTrackCount", iTrackCount);

	TTargetTrackContainer::iterator itTrack = m_TargetTracks.begin();
	TTargetTrackContainer::iterator itTrackEnd = m_TargetTracks.end();
	for (; itTrack != itTrackEnd; ++itTrack)
	{
		tAIObjectID targetId = itTrack->first;
		CTargetTrack* pTrack = itTrack->second;
		assert(targetId > 0 && pTrack);

		ser.BeginGroup("Track");
		{
			ser.Value("targetId", targetId);

			pTrack->Serialize(ser);
		}
		ser.EndGroup();
	}

	m_dummyPotentialTarget.Serialize(ser);
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackGroup::Serialize_Read(TSerialize ser)
{
	assert(ser.IsReading());

	assert(m_TargetTracks.empty());

	int iTrackCount = 0;
	ser.Value("iTrackCount", iTrackCount);

	for (int iTrack = 0; iTrack < iTrackCount; ++iTrack)
	{
		tAIObjectID targetId;

		ser.BeginGroup("Track");
		{
			ser.Value("targetId", targetId);
			assert(targetId != INVALID_AIOBJECTID);

			CTargetTrack* pTrack = GetTargetTrack(targetId);
			assert(pTrack);
			if (pTrack)
			{
				pTrack->Serialize(ser);
			}
		}
		ser.EndGroup();
	}

	m_dummyPotentialTarget.Serialize(ser);

	InitDummy();
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackGroup::InitDummy()
{
	CAIObject* pDummyRep = m_dummyPotentialTarget.refDummyRepresentation.GetAIObject();
	assert(pDummyRep);
	if (pDummyRep)
	{
		CWeakRef<CAIObject> refAIObject = gAIEnv.pObjectContainer->GetWeakRef(m_aiObjectId);
		pDummyRep->SetAssociation(refAIObject);

		pDummyRep->SetShouldSerialize(false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackGroup::Update(TargetTrackHelpers::ITargetTrackConfigProxy* pConfigProxy)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	const float fCurrTime = GetAISystem()->GetFrameStartTimeSeconds();

	m_SortedTracks.clear();
	m_SortedTracks.reserve(m_TargetTracks.size());

	// Update the tracks
	TTargetTrackContainer::iterator itTrack = m_TargetTracks.begin();
	TTargetTrackContainer::iterator itTrackEnd = m_TargetTracks.end();
	for (; itTrack != itTrackEnd; ++itTrack)
	{
		CTargetTrack* pTrack = itTrack->second;
		assert(pTrack);

		if (pTrack->Update(fCurrTime, pConfigProxy))
		{
			m_SortedTracks.push_back(pTrack);
		}
	}

	m_bNeedSort = (!m_SortedTracks.empty());
}

struct TargetTrackSorter
{
	bool operator()(const CTargetTrack* left, const CTargetTrack* right) const
	{
		if (left && right)
			return *left < *right;

		assert(false);
		return left < right;
	}
};

//////////////////////////////////////////////////////////////////////////
void CTargetTrackGroup::UpdateSortedTracks(bool bForced)
{
	if (m_bNeedSort || bForced)
	{
		m_bNeedSort = false;
		std::sort(m_SortedTracks.begin(), m_SortedTracks.end(), TargetTrackSorter());
	}
}

//////////////////////////////////////////////////////////////////////////
CTargetTrack* CTargetTrackGroup::GetTargetTrack(tAIObjectID aiTargetId)
{
	CTargetTrack* pTrack = NULL;

	TTargetTrackContainer::iterator itTrack = m_TargetTracks.find(aiTargetId);
	if (itTrack != m_TargetTracks.end())
	{
		pTrack = itTrack->second;
	}
	else
	{
		// Request a new track to use from the pool
		pTrack = m_pTrackPoolProxy->GetUnusedTargetTrackFromPool();
		assert(pTrack);

		pTrack->Init(m_aiObjectId, aiTargetId, m_uConfigHash);
		m_TargetTracks[aiTargetId] = pTrack;

#ifdef TARGET_TRACK_DEBUG
		// Create debug history for it
		uint32 uDebugGraphIndex;
		if (FindFreeGraphSlot(uDebugGraphIndex))
		{
			CWeakRef<CAIObject> refTarget = gAIEnv.pObjectContainer->GetWeakRef(aiTargetId);
			CAIObject* pTarget = refTarget.GetAIObject();
			if (pTarget) // might be null in the case of bookmarked entities
			{
				m_bDebugGraphOccupied[uDebugGraphIndex] = true;
				pTrack->SetDebugGraphIndex(uDebugGraphIndex);

				IDebugHistory* pDebugHistory = m_pDebugHistoryManager->CreateHistory(pTarget->GetName());
				if (pDebugHistory)
				{
					pDebugHistory->SetVisibility(false);
					pDebugHistory->SetupScopeExtent(-360.0f, 360.0f, 0.0f, 200.0f);
				}
			}
		}
#endif //TARGET_TRACK_DEBUG

	}

	assert(pTrack);
	return pTrack;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackGroup::HandleStimulusEvent(const TargetTrackHelpers::STargetTrackStimulusEvent& stimulusEvent, uint32 uStimulusNameHash)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert(!stimulusEvent.m_sStimulusName.empty());

	CTargetTrack* pTrack = GetTargetTrack(stimulusEvent.m_targetId);
	return HandleStimulusEvent_Target(stimulusEvent, uStimulusNameHash, pTrack);
}

bool CTargetTrackGroup::TriggerPulse(tAIObjectID targetID, uint32 uStimulusNameHash, uint32 uPulseNameHash)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	bool bResult = false;

	if (CTargetTrack* pTrack = GetTargetTrack(targetID))
		bResult = pTrack->TriggerPulse(uStimulusNameHash, uPulseNameHash);

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackGroup::TriggerPulse(uint32 uStimulusNameHash, uint32 uPulseNameHash)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	bool bResult = true;

	TTargetTrackContainer::iterator itTrack = m_TargetTracks.begin();
	TTargetTrackContainer::iterator itTrackEnd = m_TargetTracks.end();
	for (; itTrack != itTrackEnd; ++itTrack)
	{
		CTargetTrack* pTrack = itTrack->second;
		assert(pTrack);

		bResult &= pTrack->TriggerPulse(uStimulusNameHash, uPulseNameHash);
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackGroup::HandleStimulusEvent_All(const TargetTrackHelpers::STargetTrackStimulusEvent& stimulusEvent, uint32 uStimulusNameHash)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	bool bResult = true;

	TTargetTrackContainer::iterator itTrack = m_TargetTracks.begin();
	TTargetTrackContainer::iterator itTrackEnd = m_TargetTracks.end();
	for (; itTrack != itTrackEnd; ++itTrack)
	{
		bResult &= HandleStimulusEvent_Target(stimulusEvent, uStimulusNameHash, itTrack->second);
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackGroup::HandleStimulusEvent_Target(const TargetTrackHelpers::STargetTrackStimulusEvent& stimulusEvent, uint32 uStimulusNameHash, CTargetTrack* pTrack)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert(pTrack);

	return (pTrack && pTrack->InvokeStimulus(stimulusEvent, uStimulusNameHash));
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackGroup::IsPotentialTarget(tAIObjectID aiTargetId) const
{
	assert(aiTargetId > 0);

	TTargetTrackContainer::const_iterator itTrack = m_TargetTracks.find(aiTargetId);
	return (itTrack != m_TargetTracks.end());
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackGroup::IsDesiredTarget(tAIObjectID aiTargetId) const
{
	assert(aiTargetId > 0);

	return (aiTargetId == m_aiLastBestTargetId);
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackGroup::GetDesiredTarget(TargetTrackHelpers::EDesiredTargetMethod eMethod, CWeakRef<CAIObject>& outTarget, SAIPotentialTarget*& pOutTargetInfo)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	bool bResult = false;

	CTargetTrack* pBestTrack;
	uint32 count = GetBestTrack(eMethod, &pBestTrack, 1);

	if (count)
	{
		UpdateTargetRepresentation(pBestTrack, outTarget, pOutTargetInfo);
		bResult = true;
	}

	m_aiLastBestTargetId = outTarget.GetObjectID();

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
uint32 CTargetTrackGroup::GetBestTrack(TargetTrackHelpers::EDesiredTargetMethod eMethod,
                                       CTargetTrack** tracks, uint32 maxCount)
{
	UpdateSortedTracks();

	uint32 count = 0;
	switch (eMethod & TargetTrackHelpers::eDTM_SELECTION_MASK)
	{
	case TargetTrackHelpers::eDTM_Select_Highest:
		{
			TSortedTracks::iterator itBestTrack = m_SortedTracks.begin();
			TSortedTracks::iterator itTrackEnd = m_SortedTracks.end();

			for (; itBestTrack != itTrackEnd; ++itBestTrack)
			{
				CTargetTrack* track(*itBestTrack);

				if (track && TestTrackAgainstFilters(track, eMethod))
				{
					tracks[count++] = track;

					if (count >= maxCount)
						break;
				}
			}
		}
		break;

	case TargetTrackHelpers::eDTM_Select_Lowest:
		{
			TSortedTracks::reverse_iterator itBestTrack = m_SortedTracks.rbegin();
			TSortedTracks::reverse_iterator itTrackEnd = m_SortedTracks.rend();

			for (; itBestTrack != itTrackEnd; ++itBestTrack)
			{
				CTargetTrack* track(*itBestTrack);

				if (track && TestTrackAgainstFilters(track, eMethod))
				{
					tracks[count++] = track;

					if (count >= maxCount)
						break;
				}
			}
		}
		break;

	default:
		CRY_ASSERT_MESSAGE(false, "CTargetTrackGroup::GetBestTrack Unhandled desired target method");
		break;
	}

	return count;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackGroup::TestTrackAgainstFilters(CTargetTrack* pTrack, TargetTrackHelpers::EDesiredTargetMethod eMethod) const
{
	assert(pTrack);

	bool bResult = true;

	// [Kevin:26.02.2010] Need a better method than using the track manager here...
	CTargetTrackManager* pManager = gAIEnv.pTargetTrackManager;
	assert(pManager);

	const uint32 uFilterBitmask = eMethod & TargetTrackHelpers::eDTM_FILTER_MASK;

	if (uFilterBitmask & TargetTrackHelpers::eDTM_Filter_CanAquireTarget)
	{
		CAIActor* pOwner = CastToCAIActorSafe(gAIEnv.pObjectContainer->GetAIObject(m_aiObjectId));
		IAIObject* pTargetAI = pTrack->GetAIObject().GetIAIObject();

		if ((pOwner != NULL) && pTargetAI)
			bResult = pOwner->CanAcquireTarget(pTargetAI);
	}

	if (uFilterBitmask & TargetTrackHelpers::eDTM_Filter_LimitDesired)
	{
		/*const int iTargetLimit = pManager->GetTargetLimit(aiTargetId);
		   bResult &= (iTargetLimit <= 0 || pManager->GetDesiredTargetCount(aiTargetId, m_aiObjectId) < iTargetLimit);*/
	}

	if (uFilterBitmask & TargetTrackHelpers::eDTM_Filter_LimitPotential)
	{
		/*const int iTargetLimit = pManager->GetTargetLimit(aiTargetId);
		   bResult &= (iTargetLimit <= 0 || pManager->GetPotentialTargetCount(aiTargetId, m_aiObjectId) < iTargetLimit);*/
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackGroup::UpdateTargetRepresentation(const CTargetTrack* pBestTrack, CWeakRef<CAIObject>& outTarget, SAIPotentialTarget*& pOutTargetInfo)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	CWeakRef<CAIObject> refTarget = pBestTrack->GetAITarget();

#ifdef CRYAISYSTEM_DEBUG
	CAIObject* pTarget = refTarget.GetAIObject();
#endif

	CAIObject* pDummyRep = m_dummyPotentialTarget.refDummyRepresentation.GetAIObject();
	assert(pDummyRep);
	if (!pDummyRep)
		return;

	pDummyRep->SetPos(pBestTrack->GetTargetPos());
	pDummyRep->SetEntityDir(pBestTrack->GetTargetDir());
	pDummyRep->SetBodyDir(pBestTrack->GetTargetDir());
	pDummyRep->SetViewDir(pBestTrack->GetTargetDir());
	pDummyRep->SetFireDir(pBestTrack->GetTargetDir());
	pDummyRep->SetAssociation(refTarget);

#ifdef CRYAISYSTEM_DEBUG
	stack_string sDummyRepName;
#endif

	const EAITargetType eTargetType = pBestTrack->GetTargetType();
	const EAITargetContextType eTargetContextType = pBestTrack->GetTargetContext();
	const EAITargetThreat eTargetThreat = pBestTrack->GetTargetThreat();

	switch (eTargetType)
	{
	case AITARGET_VISUAL:
		{
#ifdef CRYAISYSTEM_DEBUG
			sDummyRepName.Format("Target Track Dummy - Visual \'%s\'", pTarget ? pTarget->GetName() : "(Null)");
#endif
			pDummyRep->SetSubType(IAIObject::STP_NONE);
			outTarget = refTarget;
		}
		break;

	case AITARGET_MEMORY:
		{
#ifdef CRYAISYSTEM_DEBUG
			sDummyRepName.Format("Target Track Dummy - Memory \'%s\'", pTarget ? pTarget->GetName() : "(Null)");
#endif
			pDummyRep->SetSubType(IAIObject::STP_MEMORY);
			outTarget = pDummyRep->GetSelfReference();
		}
		break;

	case AITARGET_SOUND:
		{
#ifdef CRYAISYSTEM_DEBUG
			sDummyRepName.Format("Target Track Dummy - Sound \'%s\'", pTarget ? pTarget->GetName() : "(Null)");
#endif
			switch (eTargetContextType)
			{
			case AITARGET_CONTEXT_GUNFIRE:
				pDummyRep->SetSubType(IAIObject::STP_GUNFIRE);
				break;
			default:
				pDummyRep->SetSubType(IAIObject::STP_SOUND);
				break;
			}
			outTarget = pDummyRep->GetSelfReference();
		}
		break;

	case AITARGET_NONE:
		{
#ifdef CRYAISYSTEM_DEBUG
			sDummyRepName.Format("Target Track Dummy - No Type \'%s\'", pTarget ? pTarget->GetName() : "(Null)");
#endif
			pDummyRep->SetSubType(IAIObject::STP_NONE);
			outTarget = pDummyRep->GetSelfReference();
		}
		break;

	default:
		CRY_ASSERT_MESSAGE(0, "CTargetTrackGroup::UpdateDummyTargetRep Unhandled AI target type");
		break;
	}

#ifdef CRYAISYSTEM_DEBUG
	pDummyRep->SetName(sDummyRepName.c_str());
#endif

	// Update the dummy target
	m_dummyPotentialTarget.bNeedsUpdating = false;
	m_dummyPotentialTarget.type = eTargetType;
	m_dummyPotentialTarget.threat = eTargetThreat;
	m_dummyPotentialTarget.exposureThreat = eTargetThreat;

	pOutTargetInfo = &m_dummyPotentialTarget;
}

#ifdef TARGET_TRACK_DEBUG
//////////////////////////////////////////////////////////////////////////
bool CTargetTrackGroup::FindFreeGraphSlot(uint32& outIndex) const
{
	outIndex = UINT_MAX;

	for (uint32 i = 0; i < DEBUG_GRAPH_OCCUPIED_SIZE; ++i)
	{
		if (!m_bDebugGraphOccupied[i])
		{
			outIndex = i;
			break;
		}
	}

	return (outIndex < UINT_MAX);
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackGroup::DebugDrawTracks(TargetTrackHelpers::ITargetTrackConfigProxy* pConfigProxy, bool bLastDraw)
{
	CDebugDrawContext dc;
	float fColumnX = 1.0f;
	float fColumnY = 11.0f;
	float fColumnGraphX = fColumnX + 475.0f;
	float fColumnGraphY = fColumnY;
	const float fGraphWidth = 150.0f;
	const float fGraphHeight = 150.0f;
	const float fGraphMargin = 5.0f;

	static float s_fGraphUpdateInterval = 0.125f;
	bool bUpdateGraph = false;
	const float fCurrTime = GetAISystem()->GetFrameStartTimeSeconds();
	if (fCurrTime - m_fLastGraphUpdate >= s_fGraphUpdateInterval)
	{
		m_fLastGraphUpdate = fCurrTime;
		bUpdateGraph = true;
	}

	CWeakRef<CAIObject> refObject = gAIEnv.pObjectContainer->GetWeakRef(m_aiObjectId);
	CAIObject* pObject = refObject.GetAIObject();
	assert(pObject);
	if (!pObject || !pObject->IsEnabled())
		return;

	const ColorB textCol(255, 255, 255, 255);
	dc->Draw2dLabel(fColumnX, fColumnY, 1.5f, textCol, false, "Target Tracks for Agent \'%s\':", pObject->GetName());
	fColumnY += 20.0f;

	UpdateSortedTracks();

	TTargetTrackContainer::const_iterator itTrack = m_TargetTracks.begin();
	TTargetTrackContainer::const_iterator itTrackEnd = m_TargetTracks.end();
	for (; itTrack != itTrackEnd; ++itTrack)
	{
		const CTargetTrack* pTrack = itTrack->second;
		assert(pTrack);

		// TODO Not returning right distance?
		const int iIndex = (int)std::distance(m_SortedTracks.begin(), std::find(m_SortedTracks.begin(), m_SortedTracks.end(), pTrack));
		pTrack->DebugDraw(dc, iIndex, fColumnX, fColumnY, pConfigProxy);
		fColumnY += 20.0f;

		if (bUpdateGraph)
		{
			CWeakRef<CAIObject> refTarget = pTrack->GetAIObject();
			if (CAIObject* pTarget = refTarget.GetAIObject())
			{
				const uint32 uDebugGraphIndex = pTrack->GetDebugGraphIndex();
				const uint32 uGraphX = uDebugGraphIndex % 3;
				const uint32 uGraphY = uDebugGraphIndex / 4;

				// Debug graph update
				IDebugHistory* pDebugHistory = m_pDebugHistoryManager->GetHistory(pTarget->GetName());
				if (pDebugHistory)
				{
					pDebugHistory->SetupLayoutAbs(fColumnGraphX + (fGraphWidth + fGraphMargin) * uGraphX, fColumnGraphY + (fGraphHeight + fGraphMargin) * uGraphY, fGraphWidth, fGraphHeight, fGraphMargin);
					pDebugHistory->AddValue(pTrack->GetTrackValue());
					pDebugHistory->SetVisibility(!bLastDraw);
				}
			}
		}

		pTrack->SetLastDebugDrawTime(fCurrTime);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackGroup::DebugDrawTargets(int nMode, int nTargetedCount, bool bExtraInfo)
{
	assert(nMode > 0);

	CDebugDrawContext dc;
	const ColorB visualColor(255, 0, 0, 255);
	const ColorB memoryColor(255, 255, 0, 255);
	const ColorB soundColor(0, 255, 0, 255);
	const ColorB invalidColor(120, 120, 120, 255);
	const float fProbableRatio = 0.45f;

	const float fCurrTime = GetAISystem()->GetFrameStartTimeSeconds();

	CWeakRef<CAIObject> refObject = gAIEnv.pObjectContainer->GetWeakRef(m_aiObjectId);
	CAIObject* pObject = refObject.GetAIObject();
	assert(pObject);
	if (!pObject || !pObject->IsEnabled())
		return;

	const Vec3& vPos = pObject->GetPos();

	UpdateSortedTracks();

	TSortedTracks::const_iterator itTrack = m_SortedTracks.begin();
	TSortedTracks::const_iterator itTrackEnd = m_SortedTracks.end();
	bool bFirst = true;
	for (; itTrack != itTrackEnd; ++itTrack)
	{
		const CTargetTrack* pTrack = *itTrack;
		assert(pTrack);

		ColorB drawColor;
		EAITargetType eType = pTrack->GetTargetType();

		CWeakRef<CAIObject> refTarget = pTrack->GetAIObject();
		assert(refTarget.IsValid());
		CAIObject* pTarget = refTarget.GetAIObject();

		switch (eType)
		{
		case AITARGET_VISUAL:
			{
				drawColor = visualColor;
			}
			break;

		case AITARGET_MEMORY:
			{
				drawColor = memoryColor;
			}
			break;

		case AITARGET_SOUND:
			{
				drawColor = soundColor;
			}
			break;

		case AITARGET_NONE:
			{
				drawColor = invalidColor;
			}
			break;

		default:
			CRY_ASSERT_MESSAGE(0, "CTargetTrackGroup::DebugDrawTargets Unhandled target type");
			break;
		}
		if (!bFirst)
		{
			drawColor.r = uint8((float)drawColor.r * fProbableRatio);
			drawColor.g = uint8((float)drawColor.g * fProbableRatio);
			drawColor.b = uint8((float)drawColor.b * fProbableRatio);
			drawColor.a = uint8((float)drawColor.a * fProbableRatio);
		}

		const Vec3& vTargetPos = pTrack->GetTargetPos();
		const Vec3 vTargetToPos = (vPos - vTargetPos).GetNormalizedSafe();
		const Vec3 vTargetToPosRight = Vec3Constants<float>::fVec3_OneZ.Cross(vTargetToPos).GetNormalizedSafe();

		dc->DrawLine(vPos, drawColor, vTargetPos, drawColor, 2.0f);
		dc->DrawLine(vTargetPos, drawColor, vTargetPos + vTargetToPos + vTargetToPosRight * 0.5f, drawColor, 2.0f);
		dc->DrawLine(vTargetPos, drawColor, vTargetPos + vTargetToPos - vTargetToPosRight * 0.5f, drawColor, 2.0f);

		if (bExtraInfo)
		{
			string sExtraInfoText;
			sExtraInfoText.Format("%.3f", pTrack->GetTrackValue());

			dc->Draw3dLabel(vTargetPos + (vPos - vTargetPos) * 0.5f, 1.25f, "%s", sExtraInfoText.c_str());
		}

		pTrack->SetLastDebugDrawTime(fCurrTime);

		if (bFirst && nMode == 1)
			break;
		bFirst = false;
	}

	dc->Draw3dLabel(vPos + Vec3(0.0f, 0.0f, 1.0f), 1.5f, "%d", nTargetedCount);
}
#endif //TARGET_TRACK_DEBUG
