// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Source File.
// Copyright (C), Crytek, 1999-2014.

#include "StdAfx.h"
#include "TrackViewNodeFactories.h"
#include "TrackViewAnimNode.h"
#include "TrackViewEntityNode.h"
#include "TrackViewCameraNode.h"
#include "TrackViewTrack.h"
#include "TrackViewSequenceTrack.h"
#include "TrackViewAnimationTrack.h"
#include "TrackViewGeomCacheAnimationTrack.h"
#include "TrackViewSplineTrack.h"

CTrackViewAnimNode* CTrackViewAnimNodeFactory::BuildAnimNode(IAnimSequence* pSequence, IAnimNode* pAnimNode, CTrackViewNode* pParentNode, CEntityObject* pEntity)
{
	if (pAnimNode->GetType() == eAnimNodeType_Camera)
	{
		return new CTrackViewCameraNode(pSequence, pAnimNode, pParentNode, pEntity);
	}
	else if ((pAnimNode->GetType() == eAnimNodeType_Entity)
#if defined(USE_GEOM_CACHES)
	         || (pAnimNode->GetType() == eAnimNodeType_GeomCache)
#endif
	         )
	{
		return new CTrackViewEntityNode(pSequence, pAnimNode, pParentNode, pEntity);
	}

	return new CTrackViewAnimNode(pSequence, pAnimNode, pParentNode);
}

CTrackViewTrack* CTrackViewTrackFactory::BuildTrack(IAnimTrack* pTrack, CTrackViewAnimNode* pTrackAnimNode,
                                                    CTrackViewNode* pParentNode, bool bIsSubTrack, unsigned int subTrackIndex)
{
	if ((strcmp(pTrack->GetKeyType(), S2DBezierKey::GetType()) == 0) && (pTrack->GetSubTrackCount() == 0))
	{
		return new CTrackViewSplineTrack(pTrack, pTrackAnimNode, pParentNode, bIsSubTrack, subTrackIndex);
	}
	else if (strcmp(pTrack->GetKeyType(), SSequenceKey::GetType()) == 0)
	{
		return new CTrackViewSequenceTrack(pTrack, pTrackAnimNode, pParentNode, bIsSubTrack, subTrackIndex);
	}
	else if (strcmp(pTrack->GetKeyType(), SCharacterKey::GetType()) == 0)
	{
		return new CTrackViewAnimationTrack(pTrack, pTrackAnimNode, pParentNode, bIsSubTrack, subTrackIndex);
	}
#if defined(USE_GEOM_CACHES)
	else if (pTrack->GetParameterType() == eAnimParamType_TimeRanges && pTrackAnimNode->GetType() == eAnimNodeType_GeomCache)
	{
		return new CTrackViewGeomCacheAnimationTrack(pTrack, pTrackAnimNode, pParentNode, bIsSubTrack, subTrackIndex);
	}
#endif

	return new CTrackViewTrack(pTrack, pTrackAnimNode, pParentNode, bIsSubTrack, subTrackIndex);
}

