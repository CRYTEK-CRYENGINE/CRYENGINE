// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FactionSystem.h"

CFactionSystem::CFactionSystem()
{
	m_factionMap.RegisterFactionReactionChangedCallback(functor(*this, &CFactionSystem::OnFactionReactionChanged));
}

CFactionSystem::~CFactionSystem()
{
	m_factionMap.UnregisterFactionReactionChangedCallback(functor(*this, &CFactionSystem::OnFactionReactionChanged));
}

void CFactionSystem::OnFactionReactionChanged(const uint8 factionOne, const uint8 factionTwo, const IFactionMap::ReactionType reactionType)
{
	const EntitiesWithCallbackSet& entitiesInFactionSet = m_entitiesInFactionsMap[factionOne];
	for (const auto& entityWithCallback : entitiesInFactionSet)
	{
		if (entityWithCallback.second)
		{
			entityWithCallback.second(factionTwo, reactionType);
		}
	}
}

void CFactionSystem::SetEntityFaction(EntityId entityId, const SFactionID& newFactionId, const ReactionChangedCallback& reactionChangedCallback)
{
	auto findIt = m_factionForEntitiesMap.find(entityId);
	if (findIt != m_factionForEntitiesMap.end())
	{
		uint8 oldFactionIdRaw = findIt->second;
		if (oldFactionIdRaw != newFactionId.id)
		{
			EntitiesWithCallbackSet& entitiesInFactionSet = m_entitiesInFactionsMap[oldFactionIdRaw];

			stl::find_and_erase_if(entitiesInFactionSet, [=](std::pair<EntityId, ReactionChangedCallback> entry)
			{
				return entry.first == entityId;
			});

			if (newFactionId.IsValid())
			{
				findIt->second = newFactionId.id;
				m_entitiesInFactionsMap[newFactionId.id].emplace(std::make_pair(entityId, reactionChangedCallback));
			}
			else
			{
				m_factionForEntitiesMap.erase(findIt);
			}
		}
	}
	else if(newFactionId.IsValid())
	{
		m_factionForEntitiesMap[entityId] = newFactionId.id;
		m_entitiesInFactionsMap[newFactionId.id].emplace(std::make_pair(entityId, reactionChangedCallback));
	}
}

SFactionID CFactionSystem::GetEntityFaction(EntityId entityId) const
{
	const auto findIt = m_factionForEntitiesMap.find(entityId);
	if (findIt != m_factionForEntitiesMap.end())
	{
		return SFactionID(findIt->second);
	}
	return SFactionID();
}

SFactionFlagsMask CFactionSystem::GetFactionMaskByReaction(const SFactionID& factionId, const IFactionMap::ReactionType reactionType) const
{
	SFactionFlagsMask factionsMask{ 0 };

	for (uint32 otherFactionIdRaw = 0, count = m_factionMap.GetFactionCount(); otherFactionIdRaw < count; ++otherFactionIdRaw)
	{
		if (m_factionMap.GetReaction(factionId.id, otherFactionIdRaw) == reactionType)
		{
			factionsMask.mask |= 1 << otherFactionIdRaw;
		}
	}
	return factionsMask;
}