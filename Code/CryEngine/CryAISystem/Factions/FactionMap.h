// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/IFactionMap.h>

class CFactionXmlDataSource : public IFactionDataSource
{
public:
	CFactionXmlDataSource(const char* szFileName)
		: m_fileName(szFileName)
	{}

	// IFactionDataSource
	virtual bool Load(IFactionMap& factionMap) override;
	// ~IFactionDataSource

private:
	string m_fileName;
};

class CFactionMap : public IFactionMap
{
	friend class CFactionXmlDataSource;

	enum { maxFactionCount = 32 };

	typedef std::unordered_map<uint8, string>                                                       FactionNamesById;
	typedef std::unordered_map<string, uint8, stl::hash_stricmp<string>, stl::hash_stricmp<string>> FactionIdsByName;

public:
	typedef IFactionMap::ReactionType EReaction;

	CFactionMap();

	// IFactionMap
	virtual uint32      GetFactionCount() const override;
	virtual uint32      GetMaxFactionCount() const override;

	virtual const char* GetFactionName(uint8 factionId) const override;
	virtual uint8       GetFactionID(const char* szName) const override;

	virtual uint8       CreateOrUpdateFaction(const char* szName, uint32 reactionsCount, const uint8* pReactions) override;
	virtual void        RemoveFaction(const char* szName) override;

	virtual void        SetReaction(uint8 factionOne, uint8 factionTwo, IFactionMap::ReactionType reaction) override;
	virtual EReaction   GetReaction(uint8 factionOne, uint8 factionTwo) const override;

	virtual void        SetDataSource(IFactionDataSource* pDataSource, EDataSourceLoad bLoad) override;
	virtual void        RemoveDataSource(IFactionDataSource* pDataSource) override;

	virtual void        Reload() override;

	virtual void        RegisterFactionReactionChangedCallback(const FactionReactionChangedCallback& callback) override;
	virtual void        UnregisterFactionReactionChangedCallback(const FactionReactionChangedCallback& callback) override;
	// ~IFactionMap

	void Clear();
	void Serialize(TSerialize ser);

private:
	static bool GetReactionType(const char* szReactionName, EReaction* pReactionType);

	static CFactionXmlDataSource s_defaultXmlDataSource;

	IFactionDataSource*          m_pDataSource;
	FactionNamesById             m_namesById;
	FactionIdsByName             m_idsByName;
	uint8                        m_reactions[maxFactionCount][maxFactionCount];
	CFunctorsList<FactionReactionChangedCallback> m_factionReactionChangedCallback;
};
