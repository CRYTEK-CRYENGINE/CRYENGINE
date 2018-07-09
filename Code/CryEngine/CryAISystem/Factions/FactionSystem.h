// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "FactionMap.h"

struct SFactionID
{
	SFactionID(const uint8 id = IFactionMap::InvalidFactionID)
		: id(id)
	{}

	bool IsValid() const { return id != IFactionMap::InvalidFactionID; }

	inline bool operator==(const SFactionID& other) const
	{
		if (id == IFactionMap::InvalidFactionID)
			return false;

		return id == other.id;
	}

	uint8 id;
	string name;
};

struct SFactionFlagsMask
{
	SFactionFlagsMask() : mask(0) {}
	SFactionFlagsMask(uint32 mask) : mask(mask) {}
	
	inline bool operator==(const SFactionFlagsMask& other) const { return mask == other.mask; }
	
	uint32 mask;
};

class CFactionSystem
{
public:
	typedef Functor2 <uint8 /*factionID*/, IFactionMap::ReactionType /*reaction*/> ReactionChangedCallback;
	
	CFactionSystem();
	~CFactionSystem();

	CFactionMap* GetFactionMap() { return &m_factionMap; }

	void SetEntityFaction(EntityId entityId, const SFactionID& newFactionId, const ReactionChangedCallback& reactionChangedCallback);
	SFactionID GetEntityFaction(EntityId entityId) const;

	void OnFactionReactionChanged(const uint8 factionOne, const uint8 factionTwo, const IFactionMap::ReactionType reactionType);

	SFactionFlagsMask GetFactionMaskByReaction(const SFactionID& factionId, const IFactionMap::ReactionType reactionType) const;
	
private:
	typedef std::set<std::pair<EntityId, ReactionChangedCallback>> EntitiesWithCallbackSet;

	std::unordered_map<uint8, EntitiesWithCallbackSet> m_entitiesInFactionsMap;
	std::unordered_map<EntityId, uint8> m_factionForEntitiesMap;

	CFactionMap m_factionMap;
};