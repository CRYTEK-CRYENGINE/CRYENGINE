// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __EntityCoverSampler_h__
#define __EntityCoverSampler_h__

#pragma once

#include "Cover.h"

class EntityCoverSampler
{
public:
	enum ESide
	{
		Left     = 0,
		Right    = 1,
		Front    = 2,
		Back     = 3,
		LastSide = Back,
	};

	typedef Functor3<EntityId, EntityCoverSampler::ESide, const ICoverSystem::SurfaceInfo&> Callback;

	EntityCoverSampler();
	~EntityCoverSampler();

	void Clear();
	void Queue(EntityId entityID, const Callback& callback);
	void Cancel(EntityId entityID);
	void Update();
	void DebugDraw();

private:
	struct QueuedEntity
	{
		enum EState
		{
			Queued = 0,
			SamplingLeft,
			SamplingRight,
			SamplingFront,
			SamplingBack,
		};

		QueuedEntity()
			: distanceSq(FLT_MAX)
			, state(Queued)
		{
		}

		QueuedEntity(EntityId _entityID, Callback _callback)
			: entityID(_entityID)
			, callback(_callback)
			, distanceSq(FLT_MAX)
			, state(Queued)
		{
		}

		EntityId entityID;
		Callback callback;
		float    distanceSq;
		EState   state;
	};

	struct QueuedEntitySorter
	{
		bool operator()(const QueuedEntity& lhs, const QueuedEntity& rhs) const
		{
			return lhs.distanceSq < rhs.distanceSq;
		}
	};

	typedef std::deque<QueuedEntity> QueuedEntities;
	QueuedEntities        m_queue;

	ICoverSampler*        m_sampler;
	ICoverSampler::Params m_params;
	uint32                m_currentSide;

	CTimeValue            m_lastSort;
	OBB                   m_obb;
};

#endif // __EntityCoverSampler_h__
