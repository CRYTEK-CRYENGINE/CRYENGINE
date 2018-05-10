// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a client-side IGameChannel interface.

   -------------------------------------------------------------------------
   History:
   - 11:8:2004   11:38 : Created by Márcio Martins

*************************************************************************/
#ifndef __GAMECLIENTCHANNEL_H__
#define __GAMECLIENTCHANNEL_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryNetwork/INetworkService.h>
#include "GameChannel.h"

struct SBasicSpawnParams;
class CGameClientNub;
struct SFileDownloadParameters;

struct SGameTypeParams : public ISerializable
{
	SGameTypeParams() {}
	SGameTypeParams(uint16 rules, const string& level, bool imm) : rulesClass(rules), levelName(level), immersive(imm) {}

	uint16 rulesClass;
	string levelName;
	bool   immersive;

	virtual void SerializeWith(TSerialize ser)
	{
		ser.Value("rules", rulesClass);
		ser.Value("level", levelName);
		ser.Value("immersive", immersive);
	}
};

struct SEntityClassRegistration : public ISerializable
{
	SEntityClassRegistration() {}
	SEntityClassRegistration(uint16 i, const string& n) : id(i), name(n)
	{
	}

	string name;
	uint16 id;

	virtual void SerializeWith(TSerialize ser)
	{
		ser.Value("id", id);
		ser.Value("name", name);
	}
};

struct SEntityClassHashRegistration : public ISerializable
{
	SEntityClassHashRegistration() {}
	SEntityClassHashRegistration(uint32 _crc) : crc(_crc)
	{
	}

	uint32 crc;

	virtual void SerializeWith(TSerialize ser)
	{
		ser.Value("crc", crc);
	}
};

struct SEntityIdParams : public ISerializable
{
	SEntityIdParams() : id(0) {}
	SEntityIdParams(EntityId e) : id(e) {}

	EntityId id;

	virtual void SerializeWith(TSerialize ser)
	{
		ser.Value("id", id, 'eid');
	}
};

struct SClientConsoleVariableParams : public ISerializable
{
	SClientConsoleVariableParams() {}
	SClientConsoleVariableParams(const string& key_, const string& value_) : key(key_), value(value_) {}

	string key, value;

	virtual void SerializeWith(TSerialize ser)
	{
		ser.Value("key", key);
		ser.Value("value", value);
	}
};

#define VARS_BATCH_PROFILE 0

struct SClientBatchConsoleVariablesParams : public ISerializable
{
	SClientBatchConsoleVariablesParams()
	{
		Reset();
	}

	bool Add(const string& k, const string& v)
	{
		if (batchLimit == actual)
		{
#if VARS_BATCH_PROFILE
			size_t s = GetSize();

			if (s < dataLimit)
			{
				size_t left = dataLimit - s;
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "[SClientBatchConsoleVariablesParams]: data limit isn't used effectively, left [%d], try to add more vars in batch.", left);
			}
#endif
			return false;
		}

		if ((GetSize() + k.size() + v.size()) > dataLimit)
		{
#if VARS_BATCH_PROFILE
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "[SClientBatchConsoleVariablesParams]: exceeded data limit at count [%d]", actual);
#endif
			return false;
		}

		assert(actual < batchLimit);
		vars[actual].key = k;
		vars[actual].value = v;
		actual++;

		return true;
	}

	void Reset()
	{
		actual = 0;

		for (int i = 0; i < batchLimit; ++i)
		{
			vars[i].key.resize(0);
			vars[i].value.resize(0);
		}
	}

	size_t GetSize()
	{
		size_t res = sizeof(actual);

		for (int i = 0; i < batchLimit; ++i)
		{
			res += vars[i].key.size();
			res += vars[i].value.size();
		}
		return res;
	}

	int                          actual;
	static const int             dataLimit = 1200;
	static const int             batchLimit = 50;

	SClientConsoleVariableParams vars[batchLimit];

	virtual void SerializeWith(TSerialize ser)
	{
		ser.Value("actual", actual);

		for (int i = 0; i < batchLimit; ++i)
			vars[i].SerializeWith(ser);
	}
};

struct SBreakEvent;

struct STimeOfDayInitParams
{
	float tod;

	void  SerializeWith(TSerialize ser)
	{
		ser.Value("TimeOfDay", tod);
	}
};

#if defined(GAME_CHANNEL_SYNC_CLIENT_SERVER_TIME)

struct SSyncTimeClient
{
	SSyncTimeClient() {};
	SSyncTimeClient(int id, int64 originTime, int64 serverTimeGuess, int64 serverTimeActual)
	{
		this->id = id;
		this->originTime = originTime;
		this->serverTimeGuess = serverTimeGuess;
		this->serverTimeActual = serverTimeActual;
	};

	int          id;
	int64        originTime;
	int64        serverTimeGuess;
	int64        serverTimeActual;

	virtual void SerializeWith(TSerialize ser)
	{
		ser.Value("id", id);
		ser.Value("clientTime", originTime);
		ser.Value("serverTimeGuess", serverTimeGuess);
		ser.Value("serverTimeActual", serverTimeActual);
	}
};

class CGameClientChannel;

class CClientClock
{
public:
	enum eSyncState
	{
		eSS_NotDone,
		eSS_Pinging,
		eSS_Refining,
		eSS_Done
	};

	CClientClock(CGameClientChannel& clientChannel);

	void             StartSync();
	ILINE eSyncState GetSyncState() const        { return m_syncState; }
	ILINE CTimeValue GetServerTimeOffset() const { return m_serverTimeOffset; }

	bool             OnSyncTimeMessage(const SSyncTimeClient& messageParams);

private:
	void SendSyncPing(const int id, const CTimeValue currentTime, const CTimeValue serverTime = CTimeValue());

	CGameClientChannel&     m_clientChannel;

	int                     m_pings;
	eSyncState              m_syncState;

	std::vector<CTimeValue> m_timeOffsets;
	CTimeValue              m_serverTimeOffset;

	// Used for debug, see 'OnSyncTimeMessage'
	//std::vector<float>      m_diffs;
};

#endif //GAME_CHANNEL_SYNC_CLIENT_SERVER_TIME

class CGameClientChannel :
	public CNetMessageSinkHelper<CGameClientChannel, CGameChannel>
{
public:
	CGameClientChannel(INetChannel* pNetChannel, CGameContext* pContext, CGameClientNub* pNub);
	virtual ~CGameClientChannel();

	void ClearPlayer() { SetPlayerId(0); }

	// IGameChannel
	virtual void Release() override;
	virtual void OnDisconnect(EDisconnectionCause cause, const char* description) override;
	// ~IGameChannel

	// INetMessageSink
	virtual void DefineProtocol(IProtocolBuilder* pBuilder) override;
	// ~INetMessageSink

	void GetMemoryStatistics(ICrySizer* s)
	{
		s->Add(*this);
	}

	void AddUpdateLevelLoaded(IContextEstablisher* pEst);
	bool CheckLevelLoaded() const;

	// GameChannel
	virtual bool IsServer() const override { return false; }
	//~GameChannel

	// simple messages
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(RegisterEntityClass, SEntityClassRegistration);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(RegisterEntityClassHash, SEntityClassHashRegistration);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(SetGameType, SGameTypeParams);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(ResetMap, SNoParams);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(SetConsoleVariable, SClientConsoleVariableParams);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(SetBatchConsoleVariables, SClientBatchConsoleVariablesParams);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(SetPlayerId_LocalOnly, SEntityIdParams);

	NET_DECLARE_ATSYNC_MESSAGE(ConfigureContextMessage);

	// spawners - must be unreliable ordered
	// (the net framework takes care of reliability in a special way for
	//  these - as there is a group of messages that must be reliably sent)
	NET_DECLARE_IMMEDIATE_MESSAGE(DefaultSpawn);

#if defined(GAME_CHANNEL_SYNC_CLIENT_SERVER_TIME)
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(SyncTimeClient, SSyncTimeClient);

	CClientClock& GetClock() { return m_clock; }
#endif

	void SetPlayerIdOnMigration(EntityId id) { m_playerId = id; }
private:
	CGameClientNub* m_pNub;

	void        SetPlayerId(EntityId id);
	void        CallOnSetPlayerId();
	bool        SetConsoleVar(const string& key, const string& val);

	static bool HookCreateActor(IEntity*, IGameObject*, void*);

	bool                     m_hasLoadedLevel;
	std::map<string, string> m_originalCVarValues;

#if defined(GAME_CHANNEL_SYNC_CLIENT_SERVER_TIME)
	CClientClock m_clock;
#endif
};

#endif //__GAMECLIENTCHANNEL_H__
