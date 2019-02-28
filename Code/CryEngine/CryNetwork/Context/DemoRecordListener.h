// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  Records a game session and serializes it to a file.
   -------------------------------------------------------------------------
   History:
   - ??/2005 : Created by Craig Tiller
   - 04/2006 : Taken over by Jan Müller
*************************************************************************/

#ifndef __DEMORECORDLISTENER_H__
#define __DEMORECORDLISTENER_H__

#pragma once

#include "Config.h"
#if INCLUDE_DEMO_RECORDING

	#include "Network.h"
	#include "INetContextListener.h"
	#include "Streams/CompressingStream.h"
	#include <CryNetwork/SimpleSerialize.h>
	#include <CryNetwork/NetHelpers.h>

class CNetContext;

class CDemoRecordListener : public INetContextListener
{
	friend class CDemoMessageSender;

public:
	CDemoRecordListener(CNetContext* pContext, const char* filename);
	~CDemoRecordListener();

	// INetContextListener
	virtual void         OnChannelEvent(CNetContextState* pState, INetChannel* pFrom, SNetChannelEvent* pEvent) {}
	virtual void         OnObjectEvent(CNetContextState* pState, SNetObjectEvent* pEvent);
	virtual string       GetName();
	virtual void         PerformRegularCleanup() {; }
	virtual bool         IsDead()                { return m_bIsDead; }
	virtual void         Die();
	virtual INetChannel* GetAssociatedChannel()  { return &m_channel; }
	// ~INetContextListener

	virtual void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CDemoRecordListener");

		pSizer->Add(*this);

		//pSizer->AddContainer(m_toSpawn);
		pSizer->AddContainer(m_changedObjects);
	}

private:
	CNetContext* m_pContext;
	CTimeValue   m_startTime;
	bool         m_bStarted;
	bool         m_bInGame;
	bool         m_bIsDead;

	//std::set<SNetObjectID> m_toSpawn;

	//void GC_GetEstablishmentOrder();
	void GC_DoBindObject(SNetObjectID id);

	void DoUpdate();

	void DoBindObject(SNetObjectID id);
	void DoUnbindObject(SNetObjectID id);
	void DoObjectAspectChange(const std::pair<SNetObjectID, SNetObjectAspectChange>* pChanges);
	void DoSetAspectProfile(SNetObjectID id, NetworkAspectType aspects, uint8 profile);

	void DoChangeContext(CNetContextState* pNewState);

	typedef std::map<SNetObjectID, NetworkAspectType> TChangedObjects;
	TChangedObjects                            m_changedObjects;

	const std::unique_ptr<CSimpleOutputStream> m_output;

	std::set<EntityId>                         m_boundObjects;

	inline void DoUpdateObject(const SContextObjectRef& obj, NetworkAspectType aspects);

	class CSimpleMemoryOutputStream : public CSimpleOutputStream
	{
	public:
		CSimpleMemoryOutputStream() : CSimpleOutputStream(1) {}

		CSimpleMemoryOutputStream(const CSimpleMemoryOutputStream& rhs) : CSimpleOutputStream(1)
		{
			for (size_t i = 0; i < rhs.m_buffer.size(); ++i)
				m_buffer.push_back(rhs.m_buffer[i]);

			for (std::set<EntityId>::const_iterator it = rhs.m_unboundEntites.begin(); it != rhs.m_unboundEntites.end(); ++it)
				m_unboundEntites.insert(*it);
		}

		CSimpleMemoryOutputStream& operator=(const CSimpleMemoryOutputStream& rhs)
		{
			m_buffer.clear();
			m_unboundEntites.clear();

			for (size_t i = 0; i < rhs.m_buffer.size(); ++i)
				m_buffer.push_back(rhs.m_buffer[i]);

			for (std::set<EntityId>::const_iterator it = rhs.m_unboundEntites.begin(); it != rhs.m_unboundEntites.end(); ++it)
				m_unboundEntites.insert(*it);

			return *this;
		}

		void Serialize(CSimpleOutputStream* pOther)
		{
			for (size_t i = 0; i < m_buffer.size(); ++i)
				pOther->Put(m_buffer[i]);
		}

		void AppendUnboundEntities(const std::set<EntityId>& unboundEntites)
		{
			for (std::set<EntityId>::const_iterator it = unboundEntites.begin(); it != unboundEntites.end(); ++it)
				m_unboundEntites.insert(*it);
		}

		void AppendUnboundEntity(EntityId unboundEntity)
		{
			m_unboundEntites.insert(unboundEntity);
		}

		const std::set<EntityId>& GetUnboundEntites() { return m_unboundEntites; }

	private:
		void Flush(const SStreamRecord* pRecords, size_t numRecords)
		{
			for (size_t i = 0; i < numRecords; ++i)
				m_buffer.push_back(pRecords[i]);
		}

		std::vector<SStreamRecord> m_buffer;
		std::set<EntityId>         m_unboundEntites;
	};

	std::vector<CSimpleMemoryOutputStream> m_pendingWrites;

	void FlushPendingWrites()
	{
		std::vector<CSimpleMemoryOutputStream>::iterator itor = m_pendingWrites.begin();
		while (itor != m_pendingWrites.end())
		{
			const std::set<EntityId>& unboundEntities = itor->GetUnboundEntites();
			for (std::set<EntityId>::const_iterator it = unboundEntities.begin(); it != unboundEntities.end(); ++it)
				if (m_boundObjects.find(*it) == m_boundObjects.end())
				{
					// one of the unbound entities is still not bound
					++itor;
					goto L_continue;
				}

			itor->Serialize(m_output.get());
			itor = m_pendingWrites.erase(itor);

L_continue:;
		}
	}

	class CDemoRecordSerializeImpl : public CSimpleSerializeImpl<false, eST_Network>
	{
	public:
		CDemoRecordSerializeImpl(CSimpleOutputStream* out, const std::set<EntityId>& boundObjects) :
			m_pOut(out), m_boundObjects(boundObjects)
		{
		}
		~CDemoRecordSerializeImpl()
		{
		}

		template<class T_Value>
		void Value(const char* name, const T_Value& value)
		{
			m_pOut->Put(name, value);
		}
		void Value(const char* name, const SSerializeString& value)
		{
			m_pOut->Put(name, value.c_str());
		}
		void Value(const char* name, const CTimeValue& value)
		{
			m_pOut->Put(name, value.GetSeconds());
		}
		void Value(const char* name, const ScriptAnyValue& value)
		{
		}
		void Value(const char* name, const XmlNodeRef& value)
		{
		}
		template<class T_Value, class T_Policy>
		ILINE void Value(const char* szName, const T_Value& value, const T_Policy& policy)
		{
			Value(szName, value);
		}
		void Value(const char* szName, EntityId value, uint32 policy)
		{
			//assert(policy == 'eid');
			if (policy == 'eid' && m_boundObjects.find(value) == m_boundObjects.end())
				// trying to serialize an entity that hasn't been bound, queue it for later
				m_unboundEntities.insert(value);
			Value(szName, value);
		}
		bool BeginOptionalGroup(const char* name, bool condition)
		{
			Value(name, condition);
			return condition;
		}

		const std::set<EntityId>& GetUnboundEntities() const { return m_unboundEntities; }

	private:
		CSimpleOutputStream*      m_pOut;

		const std::set<EntityId>& m_boundObjects;
		std::set<EntityId>        m_unboundEntities;
	};

	void SendMessage(INetBaseSendable* pSendable, bool immediate = false);

	class CDemoRecorderChannel : public CNetMessageSinkHelper<CDemoRecorderChannel, INetChannel>
	{
	public:
		CDemoRecorderChannel(CDemoRecordListener* pParent);
		~CDemoRecorderChannel();

		NET_DECLARE_IMMEDIATE_MESSAGE(RMI_UnreliableOrdered);
		NET_DECLARE_IMMEDIATE_MESSAGE(RMI_ReliableUnordered);
		NET_DECLARE_IMMEDIATE_MESSAGE(RMI_ReliableOrdered);
		NET_DECLARE_IMMEDIATE_MESSAGE(RMI_Attachment);

		virtual void            SetPerformanceMetrics(SPerformanceMetrics* pMetrics)        {}
		virtual void            SetInactivityTimeout(float fTimeout)                        {}
		virtual ChannelMaskType GetChannelMask()                                            { return m_channelMask; }
		virtual void            SetChannelMask(ChannelMaskType newMask)                     { m_channelMask = newMask; }
		virtual void            SetClient(INetContext* pNetContext)                         {}
		virtual void            SetServer(INetContext* pNetContext)                         {}
		virtual void            SetPeer(INetContext* pNetContext)                           {}
		virtual void            Disconnect(EDisconnectionCause cause, const char* fmt, ...) { m_connected = false; }
		virtual void            SendMsg(INetMessage*);
		virtual bool            AddSendable(INetSendablePtr pSendable, int numAfterHandle, const SSendableHandle* afterHandle, SSendableHandle* handle);
		virtual bool            SubstituteSendable(INetSendablePtr pSendable, int numAfterHandle, const SSendableHandle* afterHandle, SSendableHandle* handle)
		{
			return AddSendable(pSendable, numAfterHandle, afterHandle, handle);
		}
		virtual bool                    RemoveSendable(SSendableHandle handle)                          { return true; }
		virtual const SStatistics&      GetStatistics()                                                 { static SStatistics stats; return stats; }
		virtual void                    SetPassword(const char* password)                               {}
		virtual CrySessionHandle        GetSession() const                                              { return CrySessionInvalidHandle; }
		virtual IGameChannel*           GetGameChannel()                                                { return m_pGameChannel; }
		virtual CTimeValue              GetRemoteTime() const                                           { return g_time; }
		virtual float                   GetPing(bool smoothed) const                                    { return 0.0f; }
		virtual bool                    IsSufferingHighLatency(CTimeValue nTime) const                  { return false; }
		virtual CTimeValue              GetTimeSinceRecv() const                                        { return 0LL; }
		virtual void                    DispatchRMI(IRMIMessageBodyPtr pBody);
		virtual void                    DeclareWitness(EntityId id)                                     {}
		virtual bool                    IsLocal() const                                                 { return false; }
		virtual bool                    IsFakeChannel() const                                           { return true; }
		virtual bool                    IsConnectionEstablished() const                                 { return m_connected; }
		virtual string                  GetRemoteAddressString() const                                  { return ""; }
		virtual bool                    GetRemoteNetAddress(uint32& uip, uint16& port, bool firstLocal) { return false; }
		virtual bool                    SendTo(const uint8* pData, size_t nSize, const TNetAddress& to) { return false; }
		virtual const char*             GetName()                                                       { return "DemoRecorder"; }
		virtual const char*             GetNickname()                                                   { return 0; }
		virtual void                    SetNickname(const char* name)                                   {}
		virtual TNetChannelID           GetLocalChannelID()                                             { return 0; }
		virtual TNetChannelID           GetRemoteChannelID()                                            { return 0; }

		virtual EChannelConnectionState GetChannelConnectionState() const                               { return eCCS_InGame; }
		virtual EContextViewState       GetContextViewState() const                                     { return eCVS_InGame; }
		virtual int                     GetContextViewStateDebugCode() const                            { return 0; }
		virtual bool                    IsTimeReady() const                                             { return true; }

	#ifndef OLD_VOICE_SYSTEM_DEPRECATED
		virtual CTimeValue TimeSinceVoiceTransmission()       { return 0.0f; }
		virtual CTimeValue TimeSinceVoiceReceipt(EntityId id) { return 0.0f; }
		virtual void       AllowVoiceTransmission(bool allow) {}
	#endif

		virtual bool IsPreordered() const { return false; }
		virtual int  GetProfileId() const { return 0; }

		virtual bool IsInTransition()     { return false; }

		virtual void DefineProtocol(IProtocolBuilder* pBuilder);

		virtual void GetMemoryStatistics(ICrySizer*, bool)                                         {}

		virtual void AddWaitForFileSyncComplete(IContextEstablisher* pEst, EContextViewState when) {}

		virtual bool HasGameRequestedUpdate()                                                      { return false; }
		virtual void RequestUpdate(CTimeValue time)                                                { CallUpdate(time); }

		virtual void SetMigratingChannel(bool bIsMigrating)                                        {};
		virtual bool IsMigratingChannel() const                                                    { return false; }

	#if ENABLE_RMI_BENCHMARK
		virtual void LogRMIBenchmark(ERMIBenchmarkAction action, const SRMIBenchmarkParams& params, void (* pCallback)(ERMIBenchmarkLogPoint point0, ERMIBenchmarkLogPoint point1, int64 delay, void* pUserData), void* pUserData) {}
	#endif

	private:
		bool                 m_connected;
		IGameChannel*        m_pGameChannel;
		CDemoRecordListener* m_pParent;

		ChannelMaskType      m_channelMask;

		struct SScriptConfig
		{
			const SNetMessageDef* pRMIMsgs[eNRT_NumReliabilityTypes + 1];
		};

		SScriptConfig              m_config;
		static const SScriptConfig s_defaultConfig;
	};

	CDemoRecorderChannel m_channel;
};

#endif

#endif
