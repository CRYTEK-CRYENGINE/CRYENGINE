// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef ___GAME_SERVER_LISTS_H___
#define ___GAME_SERVER_LISTS_H___

#include "Network/GameNetworkDefines.h"

//#include "FrontEnd/Multiplayer/UIServerList.h"

#if IMPLEMENT_PC_BLADES

#include <CryLobby/ICryLobby.h>
#include <IPlayerProfiles.h>
struct SServerInfo 
{
	SServerInfo():
		m_numPlayers(0),
		m_maxPlayers(0),
		m_region(0),
		m_ping(-1),
		m_maxSpectators(0),
		m_numSpectators(0),
		m_serverId(-1),
		m_sessionId(CrySessionInvalidID),
		m_official(false),
		m_ranked(false),
		m_reqPassword(false),
		m_friends(false)
	{	}

	bool IsFull() const { return (m_numPlayers == m_maxPlayers); }
	bool IsEmpty() const { return (m_numPlayers == 0); }

	typedef CryFixedStringT<32> TFixedString32;

	TFixedString32    m_hostName;
	TFixedString32    m_mapName;
	TFixedString32    m_mapDisplayName;
	TFixedString32		m_gameTypeName;
	TFixedString32		m_gameTypeDisplayName;
	TFixedString32    m_gameVariantName;
	TFixedString32    m_gameVariantDisplayName;
	TFixedString32    m_gameVersion;

	int				m_numPlayers;
	int				m_maxPlayers;
	int				m_region;
	int       m_ping;
	int				m_maxSpectators;
	int				m_numSpectators;

	int       m_serverId;
	// SPersistentGameId		m_sessionFavouriteKeyId; // TODO: michiel?
	CrySessionID  m_sessionId;

	bool			m_official;
	bool			m_ranked;
	bool			m_reqPassword;
	bool			m_friends;
};

class CGameServerLists : public IPlayerProfileListener
{
public:
	CGameServerLists();
	virtual ~CGameServerLists();

	enum EGameServerLists
	{
		eGSL_Recent,
		eGSL_Favourite,
		eGSL_Size
	};

	const bool Add(const EGameServerLists list, const char* name, const uint32 favouriteId, bool bFromProfile);
	const bool Remove(const EGameServerLists list, const uint32 favouriteId);

	void ServerFound( const SServerInfo &serverInfo, const EGameServerLists list, const uint32 favouriteId );
	void ServerNotFound( const EGameServerLists list, const uint32 favouriteId );

	const bool InList(const EGameServerLists list, const uint32 favouriteId) const;
	const int GetTotal(const EGameServerLists list) const;

	void PopulateMenu(const EGameServerLists list) const;

	void SaveChanges();

	//IPlayerProfileListener
	void SaveToProfile(IPlayerProfile* pProfile, bool online, unsigned int reason);
	void LoadFromProfile(IPlayerProfile* pProfile, bool online, unsigned int reason);
	//~IPlayerProfileListener

#if 0 && !defined(_RELEASE)
	static void CmdAddFavourite(IConsoleCmdArgs* pCmdArgs);
	static void CmdRemoveFavourite(IConsoleCmdArgs* pCmdArgs);
	static void CmdListFavourites(IConsoleCmdArgs* pCmdArgs);
	static void CmdShowFavourites(IConsoleCmdArgs* pCmdArgs);

	static void CmdAddRecent(IConsoleCmdArgs* pCmdArgs);
	static void CmdRemoveRecent(IConsoleCmdArgs* pCmdArgs);
	static void CmdListRecent(IConsoleCmdArgs* pCmdArgs);
	static void CmdShowRecent(IConsoleCmdArgs* pCmdArgs);
#endif

	static const int k_maxServersStoredInList = 50;

protected:
	struct SServerInfoInt
	{
		SServerInfoInt(const char* name, uint32 favouriteId);
		bool operator == (const SServerInfoInt & other) const;

		string m_name;
		uint32 m_favouriteId;
	};

	struct SListRules
	{
		SListRules();

		void Reset();
		void PreApply(std::list<SServerInfoInt>* pList, const SServerInfoInt &pNewInfo);

		uint m_limit;
		bool m_unique;
	};

	void Reset();

	std::list<SServerInfoInt> m_list[eGSL_Size];
	SListRules m_rule[eGSL_Size];

	bool m_bHasChanges;
};

#endif //IMPLEMENT_PC_BLADES

#endif //___GAME_SERVER_LISTS_H___