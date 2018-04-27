// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VOICECONTEXT_H__
#define __VOICECONTEXT_H__

#ifndef OLD_VOICE_SYSTEM_DEPRECATED

	#pragma once

	#include "VOIP/IVoiceEncoder.h"
	#include "VOIP/IVoiceDecoder.h"
	#include "VOIP/IVoicePacketListener.h"
	#include "INetContextListener.h"
	#include "NetTimer.h"

class CNetContext;
class CVoiceContext;

enum EVoiceDirection
{
	eVD_Null,// must be first
	eVD_From,
	eVD_To,
};

class CVoiceContext : public IVoiceContext, public INetContextListener
{
	class CVoiceGroup;

public:
	typedef CryLockT<CRYLOCK_RECURSIVE> TVoiceMutex;

	CVoiceContext(CNetContext* pContext);
	~CVoiceContext();

	virtual IVoiceGroup* CreateVoiceGroup();

	virtual void         Mute(EntityId requestor, EntityId id, bool mute);
	virtual bool         IsMuted(EntityId requestor, EntityId id);

	virtual void         AddRef()
	{
		INetContextListener::AddRef();
	}

	virtual void Release()
	{
		INetContextListener::Release();
	}

	virtual bool      IsEnabled();

	virtual void      InvalidateRoutingTable() { m_routingInvalidated = true; };

	virtual void      OnObjectEvent(CNetContextState*, SNetObjectEvent* pEvent);
	virtual void      OnChannelEvent(CNetContextState*, INetChannel* pFrom, SNetChannelEvent* pEvent);
	virtual string    GetName();
	virtual void      PerformRegularCleanup() {}
	virtual bool      IsDead()                { return m_pNetContext == 0; }

	virtual void      PauseDecodingFor(EntityId id, bool pause);

	virtual void      ConfigureCallback(IVoicePacketListenerPtr pListener, EVoiceDirection dir, SNetObjectID witness);

	void              SetVoiceDataReader(EntityId id, IVoiceDataReader* rd);//main thread
	void              GetClientVoicePackets(SNetObjectID id, std::vector<TVoicePacketPtr>&);
	void              OnClientVoicePacket(SNetObjectID id, TVoicePacketPtr pkt);
	bool              GetDataFor(EntityId id, uint32 numSamples, int16* pData);//sound thread

	void              GetPacketsFor(SNetObjectID id, std::vector<std::pair<SNetObjectID, TVoicePacketPtr>>&);//network thread

	void              Die();

	void              RemoveVoiceGroup(CVoiceGroup*);
	CNetContext*      GetNetContext() { return m_pNetContext; }
	CNetContextState* GetNetContextState();

	void              OnPacketFrom(SNetObjectID obj, EVoiceDirection dir, const TVoicePacketPtr& pkt);

	void              GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CVoiceContext");
		pSizer->Add(*this);
		pSizer->AddContainer(m_decSessions);
		pSizer->AddContainer(m_encSessions);
		pSizer->AddContainer(m_voiceGroups);
		pSizer->AddContainer(m_proximity);
		pSizer->AddContainer(m_mutes);

		for (TDecSessions::const_iterator it = m_decSessions.begin(); it != m_decSessions.end(); ++it)
			it->second.m_pSession->GetMemoryStatistics(pSizer);

		for (TEncSessions::const_iterator it = m_encSessions.begin(); it != m_encSessions.end(); ++it)
			it->second.m_pSession->GetMemoryStatistics(pSizer);
	}

	TVoiceMutex m_Mutex;
private:
	struct SDecodingDesc
	{
		CVoiceDecodingSession* m_pSession;
		SDecodingStats         m_stats;
		bool                   m_reading;
		SDecodingDesc() : m_pSession(0), m_reading(false)
		{
		}
	};

	typedef VectorMap<SNetObjectID, SDecodingDesc> TDecSessions;

	struct SEncodingDesc
	{
		CVoiceEncodingSession* m_pSession;
		bool                   m_reading;
		IVoiceDataReader*      m_pReader;

		SEncodingDesc() : m_pSession(nullptr), m_reading(false), m_pReader(nullptr)
		{
		}
	};

	typedef VectorMap<SNetObjectID, SEncodingDesc> TEncSessions;

	NetTimerId   m_timer;
	TDecSessions m_decSessions;
	TEncSessions m_encSessions;

	typedef std::pair<SNetObjectID, EVoiceDirection>        TListenerId;
	typedef VectorMap<TListenerId, IVoicePacketListenerPtr> TPacketListeners;
	TPacketListeners          m_packetListeners;
	std::vector<TListenerId>  m_tempListenersRemove;

	CNetContext*              m_pNetContext;
	std::vector<CVoiceGroup*> m_voiceGroups;
	int                       m_statsFrameCount;
	IVoiceDataReader*         m_pVoiceDataReader;

	typedef std::vector<SNetObjectID> TObjects;
	TObjects m_allObjects;
	TObjects m_allObjectsBackup;
	bool     m_allObjectsInvalid;

	typedef std::pair<SNetObjectID, SNetObjectID> TObjectPair;
	typedef std::vector<TObjectPair>              TProximitySet;
	typedef TProximitySet::iterator               TProximitySetIter;
	TProximitySet m_proximity;
	TProximitySet m_proximityBackup;
	bool          m_proximityInvalid;
	TProximitySet m_mutes;

	std::vector<std::pair<EntityId, IVoiceDataReader*>> m_newDataReaders;

	typedef std::pair<SNetObjectID, IVoicePacketListenerPtr> TRoutingEntry;
	struct RoutingEntryCompareFirst
	{
		bool operator()(const TRoutingEntry& first, const TRoutingEntry& second) const
		{
			return first.first < second.first;
		}
	};
	typedef std::vector<TRoutingEntry> TRoutingEntries;
	TRoutingEntries m_routingEntries;
	bool            m_routingInvalidated;

	static int      m_voiceDebug;
	CTimeValue      m_lastSendTime;
	bool            m_timeInitialized;

	void RouteData();
	bool IsInSameGroup(SNetObjectID obj1, SNetObjectID obj2);
	void TestSimpleCompression();
	void RemoveAllSessions();
	void OnTimer();
	static void TimerCallback(NetTimerId, void*, CTimeValue);
	void AddPacketToDecodingSession(SNetObjectID id, TVoicePacketPtr pkt);

	void UpdateObjectSet();
	void UpdateProximityList();
	void UpdateRoutingTable();

	void RemoveDecSession(SNetObjectID id);
	void RemoveEncSession(SNetObjectID id);
};

#endif

#endif
