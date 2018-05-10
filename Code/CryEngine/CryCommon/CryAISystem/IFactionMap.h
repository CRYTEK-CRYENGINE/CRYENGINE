// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct IFactionMap;

struct IFactionDataSource
{
	virtual ~IFactionDataSource() {}

	//! Loads the data into the faction map.
	virtual bool Load(IFactionMap& factionMap) = 0;
};

struct IFactionMap
{
	enum ReactionType
	{
		Hostile = 0, //!< Intentionally from most-hostile to most-friendly.
		Neutral,
		Friendly,
	};

	enum
	{
		InvalidFactionID = 0xff,
	};

	enum class EDataSourceLoad
	{
		Skip = 0,
		Now,
	};

	//! \typedef Typedef for callback called when faction reaction has changed
	typedef Functor3<uint8 /*firstFactionId*/, uint8 /*secondFactionId*/, ReactionType /*reactionType*/> FactionReactionChangedCallback;

	// <interfuscator:shuffle>
	virtual ~IFactionMap(){}

	//! Gets the count of created factions.
	//! \return Number of created factions.
	virtual uint32 GetFactionCount() const = 0;

	//! Gets the maximum faction count.
	//! \return Maximum number of factions.
	virtual uint32 GetMaxFactionCount() const = 0;

	//! Gets the name of the specified faction by ID.
	//! \param factionId Id of the faction.
	//! \return Name of the faction or 'nullptr' if not found.
	virtual const char* GetFactionName(uint8 factionId) const = 0;

	//! Gets the ID of a faction by name.
	//! \param szName Name of the faction.
	//! \return ID of the faction or 'IFactionMap::InvalidFactionID' if not found.
	virtual uint8 GetFactionID(const char* szName) const = 0;

	//! Creates or updates a faction. Editor mode only!
	//! \param szName Name of the faction.
	//! \param reactionsCount Count of reactions 'pReactions' points to. Should be max faction count!
	//! \param pReactions Reactions to copy into the map.
	//! \return ID of the faction or 'IFactionMap::InvalidFactionID' in case of a failure.
	virtual uint8 CreateOrUpdateFaction(const char* szName, uint32 reactionsCount, const uint8* pReactions) = 0;

	//! Removes a faction by name.
	//! \note Editor mode only!.
	//! \param szName Name of the faction.
	virtual void RemoveFaction(const char* szName) = 0;

	//! Sets the reaction of 'factionOne' to 'factionTwo'.
	//! \param factionOne ID of the reacting faction.
	//! \param factionTwo ID of the faction 'factionOne' reacts on.
	//! \param Reaction Reaction to set.
	virtual void SetReaction(uint8 factionOne, uint8 factionTwo, ReactionType reaction) = 0;

	//! Gets the reaction of 'factionOne' to 'factionTwo'.
	//! \param factionOne ID of the reacting faction.
	//! \param factionTwo ID of the faction 'factionOne' reacts on.
	//! \return Reaction of factionOne on factionTwo.
	virtual ReactionType GetReaction(uint8 factionOne, uint8 factionTwo) const = 0;

	//! Sets a new data source, clears old data and loads the data from the new source if specified.
	//! \param pDataSource Pointer to the data source.
	//! \param bLoad Whether to load data from the source directly.
	virtual void SetDataSource(IFactionDataSource* pDataSource, EDataSourceLoad bLoad) = 0;

	//! Removes the specified data source if still set and clears data.
	//! \param pDataSource Pointer to the data source that will be removed if still set.
	virtual void RemoveDataSource(IFactionDataSource* pDataSource) = 0;

	//! Reloads the data from the current data source.
	virtual void Reload() = 0;
	
	//! Register to faction reaction callback.
	virtual void RegisterFactionReactionChangedCallback(const FactionReactionChangedCallback& callback) = 0;
	
	//! Unregister from faction reaction callback.
	virtual void UnregisterFactionReactionChangedCallback(const FactionReactionChangedCallback& callback) = 0;
	// </interfuscator:shuffle>
};
