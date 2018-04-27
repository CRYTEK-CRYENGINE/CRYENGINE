// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#if defined(USE_GEOM_CACHES)
	#include "AnimGeomCacheNode.h"
	#include "TimeRangesTrack.h"

namespace AnimGeomCacheNode
{
bool s_geomCacheNodeParamsInitialized = false;
std::vector<CAnimNode::SParamInfo> s_geomCacheNodeParams;

void AddSupportedParams(const char* sName, int paramType, EAnimValue valueType)
{
	CAnimNode::SParamInfo param;
	param.name = sName;
	param.paramType = paramType;
	param.valueType = valueType;
	param.flags = IAnimNode::ESupportedParamFlags(0);
	s_geomCacheNodeParams.push_back(param);
}
};

CAnimGeomCacheNode::CAnimGeomCacheNode(const int id)
	: CAnimEntityNode(id), m_bActive(false)
{
	CAnimGeomCacheNode::Initialize();
}

CAnimGeomCacheNode::~CAnimGeomCacheNode()
{
}

void CAnimGeomCacheNode::Initialize()
{
	if (!AnimGeomCacheNode::s_geomCacheNodeParamsInitialized)
	{
		AnimGeomCacheNode::s_geomCacheNodeParamsInitialized = true;
		AnimGeomCacheNode::s_geomCacheNodeParams.reserve(1);
		AnimGeomCacheNode::AddSupportedParams("Animation", eAnimParamType_TimeRanges, eAnimValue_Unknown);
	}
}

void CAnimGeomCacheNode::Animate(SAnimContext& animContext)
{
	IGeomCacheRenderNode* pGeomCacheRenderNode = GetGeomCacheRenderNode();

	if (pGeomCacheRenderNode)
	{
		const unsigned int trackCount = NumTracks();

		for (unsigned int paramIndex = 0; paramIndex < trackCount; ++paramIndex)
		{
			CAnimParamType paramType = m_tracks[paramIndex]->GetParameterType();
			IAnimTrack* pTrack = m_tracks[paramIndex];

			if (pTrack && paramType == eAnimParamType_TimeRanges)
			{
				CTimeRangesTrack* pTimeRangesTrack = static_cast<CTimeRangesTrack*>(pTrack);

				if ((pTrack->GetFlags() & IAnimTrack::eAnimTrackFlags_Disabled) || pTrack->IsMasked(animContext.trackMask) || pTrack->GetNumKeys() == 0)
				{
					continue;
				}

				const int currentKeyIndex = pTimeRangesTrack->GetActiveKeyIndexForTime(animContext.time);

				if (currentKeyIndex != -1)
				{
					STimeRangeKey key;
					pTimeRangesTrack->GetKey(currentKeyIndex, &key);
					pGeomCacheRenderNode->SetLooping(key.m_bLoop);

					const float playbackTime = (animContext.time - key.m_time).ToFloat() * key.m_speed + key.m_startTime;
					pGeomCacheRenderNode->SetPlaybackTime(min(playbackTime, key.m_endTime));

					if (playbackTime > key.m_endTime)
					{
						pGeomCacheRenderNode->StopStreaming();
					}
				}
				else
				{
					pGeomCacheRenderNode->SetLooping(false);
					pGeomCacheRenderNode->SetPlaybackTime(0.0f);
					pGeomCacheRenderNode->StopStreaming();
				}
			}
		}
	}

	CAnimEntityNode::Animate(animContext);
}

void CAnimGeomCacheNode::CreateDefaultTracks()
{
	CreateTrack(eAnimParamType_Position);
	CreateTrack(eAnimParamType_Rotation);
	CreateTrack(eAnimParamType_TimeRanges);
}

void CAnimGeomCacheNode::OnReset()
{
	CAnimNode::OnReset();
	m_bActive = false;
}

void CAnimGeomCacheNode::Activate(bool bActivate)
{
	m_bActive = bActivate;

	IGeomCacheRenderNode* pGeomCacheRenderNode = GetGeomCacheRenderNode();

	if (pGeomCacheRenderNode)
	{
		if (!bActivate)
		{
			if (gEnv->IsEditor() && gEnv->IsEditorGameMode() == false)
			{
				pGeomCacheRenderNode->SetLooping(false);
				pGeomCacheRenderNode->SetPlaybackTime(0.0f);
			}

			pGeomCacheRenderNode->StopStreaming();
		}
	}
}

unsigned int CAnimGeomCacheNode::GetParamCount() const
{
	return AnimGeomCacheNode::s_geomCacheNodeParams.size() + CAnimEntityNode::GetParamCount();
}

CAnimParamType CAnimGeomCacheNode::GetParamType(unsigned int nIndex) const
{
	const unsigned int numOwnParams = (unsigned int)AnimGeomCacheNode::s_geomCacheNodeParams.size();

	if (nIndex < numOwnParams)
	{
		return AnimGeomCacheNode::s_geomCacheNodeParams[nIndex].paramType;
	}
	else if (nIndex >= numOwnParams && nIndex < CAnimEntityNode::GetParamCount() + numOwnParams)
	{
		const CAnimParamType type = CAnimEntityNode::GetParamType(nIndex - numOwnParams);

		// Filter out param types that make no sense on a geom cache
		if (type == eAnimParamType_Animation
		    || type == eAnimParamType_Expression
		    || type == eAnimParamType_FaceSequence
		    || type == eAnimParamType_Mannequin
		    || type == eAnimParamType_ProceduralEyes)
		{
			return eAnimParamType_Invalid;
		}

		return type;
	}

	return eAnimParamType_Invalid;
}

bool CAnimGeomCacheNode::GetParamInfoFromType(const CAnimParamType& paramType, SParamInfo& info) const
{
	for (size_t i = 0; i < AnimGeomCacheNode::s_geomCacheNodeParams.size(); ++i)
	{
		if (AnimGeomCacheNode::s_geomCacheNodeParams[i].paramType == paramType)
		{
			info = AnimGeomCacheNode::s_geomCacheNodeParams[i];
			return true;
		}
	}

	return CAnimEntityNode::GetParamInfoFromType(paramType, info);
}

IGeomCacheRenderNode* CAnimGeomCacheNode::GetGeomCacheRenderNode()
{
	IEntity* pEntity = GetEntity();

	if (pEntity)
	{
		return pEntity->GetGeomCacheRenderNode(0);
	}

	return NULL;
}

void CAnimGeomCacheNode::PrecacheDynamic(SAnimTime startTime)
{
	const ICVar* pBufferAheadTimeCVar = gEnv->pConsole->GetCVar("e_GeomCacheBufferAheadTime");
	const float startPrecacheTime = pBufferAheadTimeCVar ? pBufferAheadTimeCVar->GetFVal() : 1.0f;

	IGeomCacheRenderNode* pGeomCacheRenderNode = GetGeomCacheRenderNode();

	if (pGeomCacheRenderNode)
	{
		const unsigned int trackCount = NumTracks();

		for (unsigned int paramIndex = 0; paramIndex < trackCount; ++paramIndex)
		{
			CAnimParamType paramType = m_tracks[paramIndex]->GetParameterType();
			IAnimTrack* pTrack = m_tracks[paramIndex];

			if (pTrack && paramType == eAnimParamType_TimeRanges)
			{
				CTimeRangesTrack* pTimeRangesTrack = static_cast<CTimeRangesTrack*>(pTrack);

				if ((pTrack->GetFlags() & IAnimTrack::eAnimTrackFlags_Disabled) || pTrack->GetNumKeys() == 0)
				{
					continue;
				}

				const int currentKeyIndex = pTimeRangesTrack->GetActiveKeyIndexForTime(startTime + SAnimTime(startPrecacheTime));

				if (currentKeyIndex != -1)
				{
					STimeRangeKey key;
					pTimeRangesTrack->GetKey(currentKeyIndex, &key);

					if (startTime < key.m_time + SAnimTime(key.m_startTime))
					{
						pGeomCacheRenderNode->SetLooping(key.m_bLoop);
						pGeomCacheRenderNode->StartStreaming(key.m_startTime);
					}
				}
			}
		}
	}

	CAnimEntityNode::PrecacheDynamic(startTime);
}
#endif
