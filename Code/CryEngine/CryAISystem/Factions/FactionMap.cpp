// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FactionMap.h"

CFactionXmlDataSource CFactionMap::s_defaultXmlDataSource("Scripts/AI/Factions.xml");

CFactionMap::CFactionMap()
{
	SetDataSource(&s_defaultXmlDataSource, EDataSourceLoad::Now);
}

uint32 CFactionMap::GetFactionCount() const
{
	return m_namesById.size();
}

uint32 CFactionMap::GetMaxFactionCount() const
{
	return maxFactionCount;
}

const char* CFactionMap::GetFactionName(const uint8 factionId) const
{
	FactionNamesById::const_iterator it = m_namesById.find(factionId);
	if (it != m_namesById.end())
	{
		return it->second.c_str();
	}

	return nullptr;
}

uint8 CFactionMap::GetFactionID(const char* szName) const
{
	FactionIdsByName::const_iterator it = m_idsByName.find(CONST_TEMP_STRING(szName));
	if (it != m_idsByName.end())
	{
		return it->second;
	}

	return InvalidFactionID;
}

void CFactionMap::Clear()
{
	m_namesById.clear();
	m_idsByName.clear();

	for (uint32 i = 0; i < maxFactionCount; ++i)
	{
		for (uint32 j = 0; j < maxFactionCount; ++j)
		{
			m_reactions[i][j] = (i != j) ? Hostile : Friendly;
		}
	}
}

void CFactionMap::Reload()
{
	Clear();

	if (m_pDataSource)
	{
		if (!m_pDataSource->Load(*this))
		{
			AILogAlways("[FactionMap] Failed to load factions from data source!");
		}
	}
}

void CFactionMap::RegisterFactionReactionChangedCallback(const FactionReactionChangedCallback& callback)
{
	m_factionReactionChangedCallback.Add(callback);
}

void CFactionMap::UnregisterFactionReactionChangedCallback(const FactionReactionChangedCallback& callback)
{
	m_factionReactionChangedCallback.Remove(callback);
}

uint8 CFactionMap::CreateOrUpdateFaction(const char* szName, uint32 reactionsCount, const uint8* pReactions)
{
	if (szName && szName[0])
	{
		std::pair<FactionNamesById::iterator, bool> namesByIdResult;
		std::pair<FactionIdsByName::iterator, bool> idByNamesResult;

		uint8 factionId = GetFactionID(szName);
		if (factionId == InvalidFactionID)
		{
			if (m_namesById.size() < maxFactionCount)
			{
				factionId = static_cast<uint8>(m_namesById.size());
				namesByIdResult = m_namesById.emplace(factionId, szName);
				idByNamesResult = m_idsByName.emplace(szName, factionId);

				if (!namesByIdResult.second || !idByNamesResult.second)
				{
					AIError("[FactionMap] CreateOrUpdateFaction(...) failed. Reason: Failed to insert faction!");

					if (namesByIdResult.second) m_namesById.erase(namesByIdResult.first);
					if (idByNamesResult.second) m_idsByName.erase(idByNamesResult.first);

					return InvalidFactionID;
				}
			}
			else
			{
				AIError("[FactionMap] CreateOrUpdateFaction(...) failed. Reason: Max faction count reached!");
				return InvalidFactionID;
			}
		}

		if (reactionsCount && pReactions)
		{
			const size_t bytesToCopy = std::min(reactionsCount, static_cast<uint32>(maxFactionCount));

			// Let the function fail in case of an overlapping copy.
			const size_t dstMin = reinterpret_cast<size_t>(m_reactions[factionId]);
			const size_t dstMax = dstMin + bytesToCopy;
			const size_t srcMin = reinterpret_cast<size_t>(pReactions);
			const size_t srcMax = srcMin + bytesToCopy;

			CRY_ASSERT(dstMin >= srcMax || dstMax <= srcMin);
			if (dstMin >= srcMax || dstMax <= srcMin)
			{
				memcpy(m_reactions[factionId], pReactions, sizeof(uint8) * bytesToCopy);
				m_reactions[factionId][factionId] = Friendly;

				return factionId;
			}
			else
			{
				AIError("[FactionMap] CreateOrUpdateFaction(...) failed. Reason: Overlapping copy detected!");

				if (namesByIdResult.second) m_namesById.erase(namesByIdResult.first);
				if (idByNamesResult.second) m_idsByName.erase(idByNamesResult.first);
			}
		}
		else
		{
			AIWarning("[FactionMap] CreateOrUpdateFaction(...) called with invalid reaction data. Parameters: 'reactionsCount' = '%d', 'pReactions' = '0x%p'", reactionsCount, pReactions);
		}
	}

	return InvalidFactionID;
}

void CFactionMap::RemoveFaction(const char* szName)
{
	if (gEnv->IsEditor())
	{
		const uint8 removeFactionId = GetFactionID(szName);
		if (removeFactionId != InvalidFactionID)
		{
			uint8 oldReactions[maxFactionCount][maxFactionCount];
			memcpy(oldReactions, m_reactions, sizeof(oldReactions));

			FactionNamesById oldNamesById;
			oldNamesById.swap(m_namesById);
			m_idsByName.clear();

			const size_t bytesFirstPart = sizeof(uint8) * (removeFactionId != 0 ? removeFactionId : maxFactionCount);
			const size_t bytesSecondPart = sizeof(uint8) * (maxFactionCount - 1 - removeFactionId);
			for (const FactionNamesById::value_type& kvPair : oldNamesById)
			{
				if (kvPair.first != removeFactionId)
				{
					const uint8 newId = static_cast<uint8>(m_namesById.size());
					m_namesById.emplace(newId, kvPair.second.c_str());
					m_idsByName.emplace(kvPair.second.c_str(), newId);

					memcpy(m_reactions[newId], oldReactions[kvPair.first], bytesFirstPart);
					if (removeFactionId != 0)
					{
						memcpy(&m_reactions[newId][removeFactionId], &oldReactions[kvPair.first][removeFactionId + 1], bytesSecondPart);
					}
				}
			}
		}
	}
	else
	{
		AIWarning("[FactionMap] RemoveFaction(...) is only allowed in editor mode.");
	}
}

void CFactionMap::SetReaction(uint8 factionOne, uint8 factionTwo, IFactionMap::ReactionType reaction)
{
	if ((factionOne < maxFactionCount) && (factionTwo < maxFactionCount))
	{
		m_reactions[factionOne][factionTwo] = reaction;

		m_factionReactionChangedCallback.Call(factionOne, factionTwo, reaction);
	}
}

IFactionMap::ReactionType CFactionMap::GetReaction(const uint8 factionOne, const uint8 factionTwo) const
{
	if ((factionOne < maxFactionCount) && (factionTwo < maxFactionCount))
	{
		return static_cast<IFactionMap::ReactionType>(m_reactions[factionOne][factionTwo]);
	}

	if (factionOne != InvalidFactionID && factionTwo != InvalidFactionID)
	{
		return Neutral;
	}

	return Hostile;
}

void CFactionMap::Serialize(TSerialize ser)
{
	ser.BeginGroup("FactionMap");

	// find highest faction id
	uint32 highestId = 0;

	if (ser.IsWriting())
	{
		FactionIdsByName::iterator it = m_idsByName.begin();
		FactionIdsByName::iterator end = m_idsByName.end();

		for (; it != end; ++it)
		{
			if (it->second > highestId)
				highestId = it->second;
		}
	}

	ser.Value("SerializedFactionCount", highestId);

	stack_string nameFormatter;
	for (size_t i = 0; i < highestId; ++i)
	{
		for (size_t j = 0; j < highestId; ++j)
		{
			nameFormatter.Format("Reaction_%" PRISIZE_T "_to_%" PRISIZE_T, i, j);
			ser.Value(nameFormatter.c_str(), m_reactions[i][j]);
		}
	}

	ser.EndGroup();
}

bool CFactionMap::GetReactionType(const char* szReactionName, ReactionType* pReactionType)
{
	if (!stricmp(szReactionName, "Friendly"))
	{
		if (pReactionType)
		{
			*pReactionType = Friendly;
		}
	}
	else if (!stricmp(szReactionName, "Hostile"))
	{
		if (pReactionType)
		{
			*pReactionType = Hostile;
		}
	}
	else if (!stricmp(szReactionName, "Neutral"))
	{
		if (pReactionType)
		{
			*pReactionType = Neutral;
		}
	}
	else
	{
		return false;
	}

	return true;
}

void CFactionMap::SetDataSource(IFactionDataSource* pDataSource, EDataSourceLoad bLoad)
{
	Clear();
	m_pDataSource = pDataSource;
	if (m_pDataSource && bLoad == EDataSourceLoad::Now)
	{
		if (!m_pDataSource->Load(*this))
		{
			AILogAlways("[FactionMap] Failed to load factions from data source!");
		}
	}
}

void CFactionMap::RemoveDataSource(IFactionDataSource* pDataSource)
{
	if (m_pDataSource == pDataSource)
	{
		Clear();
		m_pDataSource = nullptr;
	}
}

bool CFactionXmlDataSource::Load(IFactionMap& factionMap)
{
	if (!gEnv->pCryPak->IsFileExist(m_fileName.c_str()))
	{
		return false;
	}

	XmlNodeRef rootNode = GetISystem()->LoadXmlFromFile(m_fileName.c_str());
	if (!rootNode)
	{
		AIWarning("Failed to open faction XML file '%s'.", m_fileName.c_str());
		return false;
	}

	uint8 reactions[CFactionMap::maxFactionCount][CFactionMap::maxFactionCount];

	const char* szRootName = rootNode->getTag();
	if (!stricmp(szRootName, "Factions"))
	{
		// Create factions with default reactions
		const int32 factionNodeCount = rootNode->getChildCount();
		for (int32 factionIdx = 0; factionIdx < factionNodeCount; ++factionIdx)
		{
			const XmlNodeRef factionNode = rootNode->getChild(factionIdx);

			if (!stricmp(factionNode->getTag(), "Faction"))
			{
				if (factionIdx >= CFactionMap::maxFactionCount)
				{
					AIWarning("Maximum number of allowed factions reached in file '%s' at line '%d'!", m_fileName.c_str(), factionNode->getLine());
					return false;
				}

				const char* szFactionName = nullptr;
				if (!factionNode->getAttr("name", &szFactionName) || szFactionName == nullptr || szFactionName[0] == '\0')
				{
					AIWarning("Missing or empty 'name' attribute for 'Faction' tag in file '%s' at line %d...", m_fileName.c_str(), factionNode->getLine());
					return false;
				}

				const uint8 factionId = factionMap.GetFactionID(szFactionName);
				if (factionId != CFactionMap::InvalidFactionID)
				{
					AIWarning("Duplicate faction '%s' in file '%s' at line %d...", szFactionName, m_fileName.c_str(), factionNode->getLine());
					return false;
				}

				CFactionMap::ReactionType defaultReactionType = CFactionMap::Hostile;

				const char* szDefaultReaction = nullptr;
				if (factionNode->getAttr("default", &szDefaultReaction) && szDefaultReaction && szDefaultReaction[0])
				{
					if (!CFactionMap::GetReactionType(szDefaultReaction, &defaultReactionType))
					{
						AIWarning("Invalid default reaction '%s' in file '%s' at line '%d'...", szDefaultReaction, m_fileName.c_str(), factionNode->getLine());
						return false;
					}
				}
				memset(reactions[factionIdx], defaultReactionType, sizeof(reactions[factionIdx]));

				factionMap.CreateOrUpdateFaction(szFactionName, CFactionMap::maxFactionCount, reactions[factionIdx]);
			}
			else
			{
				AIWarning("Unexpected tag '%s' in file '%s' at line %d...", factionNode->getTag(), m_fileName.c_str(), factionNode->getLine());
				return false;
			}
		}

		// Update factions reactions
		for (int32 factionIdx = 0; factionIdx < factionNodeCount; ++factionIdx)
		{
			const XmlNodeRef factionNode = rootNode->getChild(factionIdx);

			const char* szFactionName;
			factionNode->getAttr("name", &szFactionName);
			const uint8 factionId = factionMap.GetFactionID(szFactionName);

			const int32 reactionNodeCount = factionNode->getChildCount();
			for (int32 reactionIdx = 0; reactionIdx < reactionNodeCount; ++reactionIdx)
			{
				XmlNodeRef reactionNode = factionNode->getChild(reactionIdx);
				if (!stricmp(reactionNode->getTag(), "Reaction"))
				{
					const char* szReactionOnFaction = nullptr;
					if (!reactionNode->getAttr("faction", &szReactionOnFaction) || szReactionOnFaction == nullptr || szReactionOnFaction[0] == '\0')
					{
						AIWarning("Missing or empty 'faction' attribute for 'Reaction' tag in file '%s' at line %d...", m_fileName.c_str(), reactionNode->getLine());
						return false;
					}

					const uint8 reactionOnfactionId = factionMap.GetFactionID(szReactionOnFaction);
					if (reactionOnfactionId == CFactionMap::InvalidFactionID)
					{
						AIWarning("Attribute 'faction' for 'Reaction' tag in file '%s' at line %d defines an unknown faction...", m_fileName.c_str(), reactionNode->getLine());
						return false;
					}

					const char* szReaction = nullptr;
					if (!reactionNode->getAttr("reaction", &szReaction) || szReaction == nullptr || szReaction[0] == '\0')
					{
						AIWarning("Missing or empty 'reaction' attribute for 'Reaction' tag in file '%s' at line %d...", m_fileName.c_str(), reactionNode->getLine());
						return false;
					}

					CFactionMap::ReactionType reactionType = CFactionMap::Neutral;
					if (!CFactionMap::GetReactionType(szReaction, &reactionType))
					{
						AIWarning("Invalid reaction '%s' in file '%s' at line '%d'...", szReaction, m_fileName.c_str(), reactionNode->getLine());
						//return false;
					}

					reactions[factionId][reactionOnfactionId] = static_cast<uint8>(reactionType);
				}
				else
				{
					AIWarning("Unexpected tag '%s' in file '%s' at line %d...", reactionNode->getTag(), m_fileName.c_str(), reactionNode->getLine());
					return false;
				}
			}

			factionMap.CreateOrUpdateFaction(szFactionName, CFactionMap::maxFactionCount, reactions[factionId]);
		}
	}
	else
	{
		AIWarning("Unexpected tag '%s' in file '%s' at line %d...", szRootName, m_fileName.c_str(), rootNode->getLine());
		return false;
	}

	return true;
}
