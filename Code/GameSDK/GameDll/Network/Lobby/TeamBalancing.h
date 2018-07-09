// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef ___TEAM_BALANCING_H___
#define ___TEAM_BALANCING_H___

#include <CryLobby/ICryLobby.h>
#include "SessionNames.h"
#include "GameUserPackets.h"

#define TEAM_BALANCING_MAX_PLAYERS			MAX_SESSION_NAMES

struct SPlayerScores;
struct SCryMatchMakingConnectionUID;

class CTeamBalancing
{
public:
	enum EUpdateTeamType
	{
		eUTT_Unlock,
		eUTT_Switch
	};

	CTeamBalancing()
		: m_pSessionNames(0)
		, m_maxPlayers(0)
		, m_bGameIsBalanced(false)
		, m_bGameHasStarted(false)
		, m_bBalancedTeamsForced(false)
	{

	}

	void Init(SSessionNames *pSessionNames);
	void Reset();

	void AddPlayer(SSessionNames::SSessionName *pPlayer);
	void RemovePlayer(const SCryMatchMakingConnectionUID &uid);
	void UpdatePlayer(SSessionNames::SSessionName *pPlayer, uint32 previousSkill);

	void UpdatePlayerScores(SPlayerScores *pScores, int numScores);

	void OnPlayerSpawned(const SCryMatchMakingConnectionUID &uid);
	void OnPlayerSwitchedTeam(const SCryMatchMakingConnectionUID &uid, uint8 teamId);
	void OnGameFinished(EUpdateTeamType updateType);

	uint8 GetTeamId(const SCryMatchMakingConnectionUID &uid);
	bool IsGameBalanced() const;

	void SetLobbyPlayerCounts(int maxPlayers);
	void ForceBalanceTeams();

	int GetMaxNewSquadSize();

	void WritePacket(CCryLobbyPacket *pPacket, GameUserPacketDefinitions packetType, SCryMatchMakingConnectionUID playerUID); 
	void ReadPacket(CCryLobbyPacket *pPacket, uint32 packetType);

private:
	enum EPlayerGroupType
	{
		ePGT_Unknown,
		ePGT_Squad,
		ePGT_Clan,
		ePGT_Individual,
	};

	typedef uint8 TGroupIndex;

	struct STeamBalancing_PlayerSlot
	{
		STeamBalancing_PlayerSlot();
		void Reset();

		SCryMatchMakingConnectionUID m_uid;

		uint32 m_skill;
		uint32 m_previousScore;
		uint8 m_teamId;
		TGroupIndex m_groupIndex;
		bool m_bLockedOnTeam;
		bool m_bUsed;
	};

	typedef uint8 TPlayerIndex;
	typedef CryFixedStringT<6> TSmallString;
	typedef CryFixedArray<TPlayerIndex, TEAM_BALANCING_MAX_PLAYERS> TPlayerList;

	struct STeamBalancing_Group
	{
		STeamBalancing_Group();
		void Reset();

		void InitClan(CTeamBalancing *pTeamBalancing, TPlayerIndex playerIndex, const char *pClanName);
		void InitIndividual(CTeamBalancing *pTeamBalancing, TPlayerIndex playerIndex);
		void InitSquad(CTeamBalancing *pTeamBalancing, TPlayerIndex playerIndex, uint32 leaderUID);

		void AddMember(CTeamBalancing *pTeamBalancing, TPlayerIndex playerIndex);
		void RemoveMember(CTeamBalancing *pTeamBalancing, TPlayerIndex playerIndex);

		TSmallString m_clanTag;
		uint32 m_leaderUID;

		uint32 m_totalSkill;
		uint32 m_totalPrevScore;

		EPlayerGroupType m_type;

		TPlayerList m_members;

		uint8 m_teamId;
		bool m_bLockedOnTeam;
		bool m_bPresentAtGameStart;
		bool m_bUsed;
	};

	struct SBalanceGroup
	{
		SBalanceGroup()
			: m_numPlayers(0)
			, m_totalScore(0)
			, m_desiredTeamId(0)
			, m_bPresentAtGameStart(false)
		{
			memset(m_pPlayers, 0, sizeof(m_pPlayers));
		}

		CTeamBalancing::STeamBalancing_PlayerSlot *m_pPlayers[TEAM_BALANCING_MAX_PLAYERS];
		int m_numPlayers;
		uint32 m_totalScore;
		uint8 m_desiredTeamId;
		bool m_bPresentAtGameStart;

		bool operator<(const SBalanceGroup &rhs) const
		{
			if (m_numPlayers > rhs.m_numPlayers)
			{
				return true;
			}
			else if (m_numPlayers < rhs.m_numPlayers)
			{
				return false;
			}

			if (m_totalScore > rhs.m_totalScore)
			{
				return true;
			}
			else if (m_totalScore < rhs.m_totalScore)
			{
				return false;
			}

			// Equal
			return false;
		}
	};

	STeamBalancing_Group *FindGroupByClan(const char *pClanName);
	STeamBalancing_Group *FindGroupBySquad(uint32 leaderUID);
	STeamBalancing_Group *FindEmptyGroup();

	STeamBalancing_PlayerSlot *FindPlayerSlot(const SCryMatchMakingConnectionUID &conId);
	const STeamBalancing_PlayerSlot *FindPlayerSlot(const SCryMatchMakingConnectionUID &conId) const;

	STeamBalancing_PlayerSlot* AddPlayer(SCryMatchMakingConnectionUID uid);

	void UpdatePlayer(STeamBalancing_PlayerSlot *pSlot, SSessionNames::SSessionName *pPlayer, bool bBalanceTeams);
	void UpdatePlayer( STeamBalancing_PlayerSlot *pSlot, uint16 skillRank, uint32 squadLeaderUID, CryFixedStringT<CLAN_TAG_LENGTH> &clanTag, bool bBalanceTeams );

		TPlayerIndex GetIndexFromSlot(STeamBalancing_PlayerSlot* pPlayerSlot) const;
	TGroupIndex GetIndexFromGroup(STeamBalancing_Group* pGroup) const;
	void BalanceTeams();

	void CreateBalanceGroups(SBalanceGroup *pGroups, int *pNumGroups, int *pNumPlayersOnTeams, int *pNumTotalPlayers, bool bAllowCommit);
	void AssignDesiredTeamGroups(SBalanceGroup *pGroups, int *pNumGroups, int *pNumPlayersOnTeams, int numTotalPlayers, bool bAllowCommit);
	void AssignMaxPlayersToTeam(SBalanceGroup *pGroups, int *pNumGroups, int *pNumPlayersOnTeam);

	void WritePlayerToPacket(CCryLobbyPacket *pPacket, const SSessionNames::SSessionName *pPlayer);
	void ReadPlayerFromPacket(CCryLobbyPacket *pPacket, bool bBalanceTeams);

	STeamBalancing_Group m_groups[TEAM_BALANCING_MAX_PLAYERS];
	STeamBalancing_PlayerSlot m_players[TEAM_BALANCING_MAX_PLAYERS];

	SSessionNames *m_pSessionNames;
	int m_maxPlayers;
	bool m_bGameIsBalanced;
	bool m_bGameHasStarted;
	bool m_bBalancedTeamsForced;
};

#endif // ___TEAM_BALANCING_H___
