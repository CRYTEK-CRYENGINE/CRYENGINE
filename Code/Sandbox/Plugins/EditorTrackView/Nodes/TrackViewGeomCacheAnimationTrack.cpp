// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Source File.
// Copyright (C), Crytek, 1999-2014.

#include "StdAfx.h"

#if defined(USE_GEOM_CACHES)
	#include "TrackViewGeomCacheAnimationTrack.h"
	#include "TrackViewSequence.h"
	#include "TrackViewEntityNode.h"

	#include <Cry3DEngine/IGeomCache.h>

CTrackViewKeyHandle CTrackViewGeomCacheAnimationTrack::CreateKey(const CTimeValue& time)
{
	CTrackViewSequence* pSequence = GetSequence();
	CTrackViewSequenceNotificationContext context(GetSequence());

	CTrackViewKeyHandle keyHandle = CTrackViewTrack::CreateKey(time);

	// Find editor object which owns this node.
	IEntity* pEntity = static_cast<CTrackViewEntityNode*>(GetParentNode())->GetEntity();
	if (pEntity)
	{
		const IGeomCacheRenderNode* pGeomCacheRenderNode = pEntity->GetGeomCacheRenderNode(0);
		if (pGeomCacheRenderNode)
		{
			const IGeomCache* pGeomCache = pGeomCacheRenderNode->GetGeomCache();

			if (pGeomCache)
			{
				STimeRangeKey timeRangeKey;
				keyHandle.GetKey(&timeRangeKey);
				timeRangeKey.m_endTime = pGeomCache->GetDuration();
				keyHandle.SetKey(&timeRangeKey);
				GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_KeysChanged);
			}
		}
	}

	return keyHandle;
}

CTimeValue CTrackViewGeomCacheAnimationTrack::GetKeyDuration(const uint index) const
{
	CTrackViewKeyConstHandle handle = GetKey(index);
	STimeRangeKey key;
	handle.GetKey(&key);

	return (key.m_endTime - key.m_startTime) / key.m_speed;
}

CTimeValue CTrackViewGeomCacheAnimationTrack::GetKeyAnimDuration(const uint index) const
{
	IEntity* pEntity = static_cast<CTrackViewEntityNode*>(GetParentNode())->GetEntity();
	if (pEntity)
	{
		const IGeomCacheRenderNode* pGeomCacheRenderNode = pEntity->GetGeomCacheRenderNode(0);
		if (pGeomCacheRenderNode)
		{
			const IGeomCache* pGeomCache = pGeomCacheRenderNode->GetGeomCache();

			if (pGeomCache)
			{
				CTrackViewKeyConstHandle handle = GetKey(index);
				STimeRangeKey key;
				handle.GetKey(&key);

				return (pGeomCache->GetDuration() / key.m_speed);
			}
		}
	}

	return 0;
}

CTimeValue CTrackViewGeomCacheAnimationTrack::GetKeyAnimStart(const uint index) const
{
	CTrackViewKeyConstHandle handle = GetKey(index);
	STimeRangeKey key;
	handle.GetKey(&key);
	return key.m_startTime;
}

bool CTrackViewGeomCacheAnimationTrack::IsKeyAnimLoopable(const uint index) const
{
	CTrackViewKeyConstHandle handle = GetKey(index);
	STimeRangeKey key;
	handle.GetKey(&key);
	return key.m_bLoop;
}

#endif

