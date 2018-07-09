// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RangeModule.h"
#include "Agent.h"

RangeContainer::RangeID RangeContainer::AddRange(const Range& range)
{
	assert(m_ranges.size() <= std::numeric_limits<RangeID>::max());
	RangeID id = static_cast<RangeID> (m_ranges.size());
	m_ranges.push_back(range);
	return id;
}

bool RangeContainer::GetTargetDistances(float& distToAttentionTargetSq, float& distToLiveTargetSq) const
{
	Agent agent(m_entityID);
	assert(agent.IsValid());
	IF_UNLIKELY (!agent.IsValid())
		return false;

	if (IAIObject* attentionTarget = agent.GetAttentionTarget())
	{
		distToAttentionTargetSq = agent.GetEntityPos().GetSquaredDistance(attentionTarget->GetPos());
		
		if (IAIObject* liveTarget = agent.GetLiveTarget())
		{
			distToLiveTargetSq = agent.GetEntityPos().GetSquaredDistance(liveTarget->GetPos());
			return true;
		}
	}

	return false;
}

void RangeContainer::Update(float frameTime)
{
	float distToAttentionTargetSq = 0.0f;
	float distToLiveTargetSq = 0.0f;

	if (GetTargetDistances(distToAttentionTargetSq, distToLiveTargetSq))
	{
		Ranges::iterator it = m_ranges.begin();
		Ranges::iterator end = m_ranges.end();

		for ( ; it != end; ++it)
		{
			Range& range = *it;

			float distToTargetSq = 0.0f;
			switch (range.targetMode)
			{
			case Range::UseAttentionTargetDistance:
				distToTargetSq = distToAttentionTargetSq;
				break;
			case Range::UseLiveTargetDistance:
				distToTargetSq = distToLiveTargetSq;
				break;
			default:
				assert(0);
				break;
			}

			Range::State newState = (distToTargetSq < range.rangeSq) ? Range::Inside : Range::Outside;

			if (newState != range.state)
			{
				range.state = newState;

				if (newState == Range::Inside && !range.enterSignal.empty())
					SendSignal(range.enterSignal.c_str());
				else if (newState == Range::Outside && !range.leaveSignal.empty())
					SendSignal(range.leaveSignal.c_str());
			}
		}
	}
}
