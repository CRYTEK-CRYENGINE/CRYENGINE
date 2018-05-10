// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#include <CryNetwork/INetwork.h> // <> required for Interfuscator

#pragma once

struct SNoParams : public ISerializable
{
	virtual void SerializeWith(TSerialize ser)
	{
	}
};

////////////////////////////////////////////////////////////////////////////////////////
/*! helper class for implementing INetMessageSink
 *
 * class CMyMessageSink : public CNetMessageSinkHelper<CMyMessageSink, IServerSlotSink>
 * {
 * public:
 *   NET_DECLARE_MESSAGE(...);
 * };
 */

template<class T_Derived, class T_Parent, size_t MAX_STATIC_MESSAGES = 32>
class CNetMessageSinkHelper : public T_Parent
{
public:
	//! Helper to implement GetProtocolDefs.
	static SNetProtocolDef GetProtocolDef()
	{
		SNetProtocolDef def;
		def.nMessages = ms_statics.m_nMessages;
		def.vMessages = ms_statics.m_vMessages;
		return def;
	}

	//! Check whether a particular message belongs to this sink.
	static bool ClassHasDef(const SNetMessageDef* pDef)
	{
		return pDef >= ms_statics.m_vMessages && pDef < ms_statics.m_vMessages + ms_statics.m_nMessages;
	}

	bool HasDef(const SNetMessageDef* pDef)
	{
		return ClassHasDef(pDef);
	}

protected:
	//! This function should not be used, except through the supplied macros.
	static SNetMessageDef* Helper_AddMessage(SNetMessageDef::HandlerType handler,
	                                         ENetReliabilityType reliability, const char* description, uint32 parallelFlags, uint32 nUser = 0xBA5E1E55)
	{
		// Would use an assert here, but we don't want this to be compiled out (if we exceed this limit, it's a big bug).
		if (ms_statics.m_nMessages == MAX_STATIC_MESSAGES)
		{
			assert(!"Maximum static messages exceeded");
			CryFatalError("Maximum static messages exceeded");
			((void (*)())NULL)();
		}
		SNetMessageDef* def = &ms_statics.m_vMessages[ms_statics.m_nMessages++];
		def->handler = handler;
		def->reliability = reliability;
		def->description = description;
		def->nUser = nUser;
		def->parallelFlags = parallelFlags;
		return def;
	}

	// Aspect-related messages don't use NET_DECLARE_... macros, and instead
	// are instantiated by template members variating on the aspect number.
	// Correct message description is crucial for the protocol, so we compose
	// it at runtime, even though for members with static duration only.
	static SNetMessageDef* Helper_AddAspectMessage(
		SNetMessageDef::HandlerType handler, const char* szDescription, 
		const int aspectNum, uint32 parallelFlags = 0)
	{
		return Helper_AddMessage(handler, eNRT_UnreliableUnordered,
			// Leak is intentional, but constant.
			(new string(szDescription + CryStringUtils::toString(aspectNum)))->c_str(),
			parallelFlags);
	}

private:
	struct Statics
	{
		size_t         m_nMessages;
		SNetMessageDef m_vMessages[MAX_STATIC_MESSAGES];
	};

	static Statics ms_statics;
};

template<class T, class U, size_t N>
typename CNetMessageSinkHelper<T, U, N>::Statics CNetMessageSinkHelper<T, U, N>::ms_statics;

// primitive message types:
// these are provided to allow direct reading and writing from network streams
// when this is necessary (e.g. UpdateEntity -> read+lookup entity id, call serialize)

// when implementing one of these messages (with NET_IMPLEMENT_MESSAGE), we
// write the message handling code directly after the macro; it will implement
// a function that takes parameters:
//    (TSerialize pSer, uint32 nCurSeq, uint32 nOldSeq)
// you should return false if an error occurs during processing

// when using NET_*_MESSAGE style declarations, YOU must supply the implementation
// of INetMessage so that this message can be sent (be sure to keep the WritePayload,
// and the message handler code synchronised for reading and writing)

// for a simpler alternative, see NET_DECLARE_SIMPLE_MESSAGE

#define NET_DECLARE_IMMEDIATE_MESSAGE(name)                                                       \
  private:                                                                                        \
    static TNetMessageCallbackResult Trampoline ## name(uint32,                                   \
                                                        INetMessageSink*, TSerialize,             \
                                                        uint32, uint32, uint32, EntityId*, INetChannel*); \
    bool Handle ## name(TSerialize, uint32, uint32, uint32, EntityId*, INetChannel*);                     \
  public:                                                                                         \
    static const SNetMessageDef* const name

#define NET_IMPLEMENT_IMMEDIATE_MESSAGE(cls, name, reliability, parallelFlags)                                                          \
  const SNetMessageDef * const cls::name = cls::Helper_AddMessage(                                                                      \
    cls::Trampoline ## name, reliability, # cls ":" # name, (parallelFlags), 0xba5e1e55);                                               \
  TNetMessageCallbackResult cls::Trampoline ## name(                                                                                    \
    uint32,                                                                                                                             \
    INetMessageSink * handler,                                                                                                          \
    TSerialize serialize,                                                                                                               \
    uint32 curSeq,                                                                                                                      \
    uint32 oldSeq,                                                                                                                      \
    uint32 timeFraction32,                                                                                                                      \
    EntityId * pEntityId, INetChannel * pChannel)                                                                                       \
  {                                                                                                                                     \
    char* p = (char*) handler;                                                                                                          \
    p -= size_t((INetMessageSink*)(cls*)NULL);                                                                                          \
    return TNetMessageCallbackResult(((cls*)p)->Handle ## name(serialize, curSeq, oldSeq, timeFraction32, pEntityId, pChannel), (INetAtSyncItem*)NULL); \
  }                                                                                                                                     \
  inline bool cls::Handle ## name(TSerialize ser,                                                                                       \
                                  uint32 nCurSeq, uint32 nOldSeq, uint32 timeFraction32, EntityId * pEntityId, INetChannel * pNetChannel)

#define NET_DECLARE_ATSYNC_MESSAGE(name)                                                          \
  private:                                                                                        \
    static TNetMessageCallbackResult Trampoline ## name(uint32,                                   \
                                                        INetMessageSink*, TSerialize,             \
                                                        uint32, uint32, uint32, EntityId*, INetChannel*); \
    bool Handle ## name(TSerialize, uint32, uint32, uint32, EntityId, INetChannel*);                      \
  public:                                                                                         \
    static const SNetMessageDef* const name

#define NET_IMPLEMENT_ATSYNC_MESSAGE(cls, name, reliability, parallelFlags)                                           \
  const SNetMessageDef * const cls::name = cls::Helper_AddMessage(                                                    \
    cls::Trampoline ## name, reliability, # cls ":" # name, (parallelFlags));                                         \
  TNetMessageCallbackResult cls::Trampoline ## name(                                                                  \
    uint32,                                                                                                           \
    INetMessageSink * handler,                                                                                        \
    TSerialize serialize,                                                                                             \
    uint32 curSeq,                                                                                                    \
    uint32 oldSeq,                                                                                                    \
    uint32 timeFraction32,                                                                                                    \
    EntityId * entityId, INetChannel * pChannel)                                                                      \
  {                                                                                                                   \
    char* p = (char*) handler;                                                                                        \
    p -= size_t((INetMessageSink*)(cls*)NULL);                                                                        \
    return TNetMessageCallbackResult(                                                                                 \
      ((cls*)p)->Handle ## name(serialize, curSeq, oldSeq, timeFraction32, *entityId, pChannel), static_cast<INetAtSyncItem*>(NULL)); \
  }                                                                                                                   \
  inline bool cls::Handle ## name(TSerialize ser,                                                                     \
                                  uint32 nCurSeq, uint32 nOldSeq, uint32 timeFraction32, EntityId entityId, INetChannel * pChannel)

//! Helper class for NET_DECLARE_SIMPLE_MESSAGE.
template<class T>
class CSimpleNetMessage : public INetMessage
{
public:
	CSimpleNetMessage(const T& value, const SNetMessageDef* pDef) :
		INetMessage(pDef), m_value(value)
	{
	}

	EMessageSendResult WritePayload(
	  TSerialize serialize,
	  uint32, uint32)
	{
		m_value.SerializeWith(serialize);
		return eMSR_SentOk;
	}
	void UpdateState(uint32 nFromSeq, ENetSendableStateUpdate)
	{
	}
	size_t GetSize() { return sizeof(*this); }

private:
	T m_value;
};

// simple message types:
// if we have a class that represents the parameters to a function, and that
// implements the ISerializable interface, then we can use
// NET_DECLARE_SIMPLE_MESSAGE to declare the message, and save some time and
// some bugs.

// it takes as parameters "name", which is the name of the message to declare,
// and "CClass" which is the name of the class holding our parameters

// it will declare in our class a static function:
//   static void Send"name"With( const CClass& msg, TChannelType * pChannel )
// to assist us with sending this message type; TChannelType is any class that
// implements a SendMsg function

// when implementing our message handler, we will be passed a single parameter,
// which is a "const CClass&" called param

#define NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(name, CClass)                                        \
  private:                                                                                        \
    static TNetMessageCallbackResult Trampoline ## name(uint32,                                   \
                                                        INetMessageSink*, TSerialize,             \
                                                        uint32, uint32, uint32, EntityId*, INetChannel*); \
    bool Handle ## name(const CClass &, EntityId*, bool bFromDemoSystem, INetChannel * pChannel); \
    typedef CClass TParam ## name;                                                                \
  public:                                                                                         \
    template<class T>                                                                             \
    static void Send ## name ## With(const CClass &msg, T * pChannel)                             \
    { pChannel->SendMsg(new CSimpleNetMessage<CClass>(msg, name)); }                              \
    static const SNetMessageDef* const name

#define NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(cls, name, reliability, parallelFlags)                                              \
  const SNetMessageDef * const cls::name = cls::Helper_AddMessage(                                                                 \
    cls::Trampoline ## name, reliability, # cls ":" # name, (parallelFlags) & ~eMPF_DecodeInSync);                                 \
  TNetMessageCallbackResult cls::Trampoline ## name(                                                                               \
    uint32,                                                                                                                        \
    INetMessageSink * handler,                                                                                                     \
    TSerialize serialize,                                                                                                          \
    uint32 curSeq,                                                                                                                 \
    uint32 oldSeq,                                                                                                                 \
    uint32 timeFraction32,                                                                                                         \
    EntityId * pEntityId, INetChannel * pChannel)                                                                                  \
  {                                                                                                                                \
    char* p = (char*) handler;                                                                                                     \
    p -= size_t((INetMessageSink*)(cls*)NULL);                                                                                     \
    TParam ## name param;                                                                                                          \
    param.SerializeWith(serialize);                                                                                                \
    return TNetMessageCallbackResult(((cls*)p)->Handle ## name(param, pEntityId,                                                   \
                                                               curSeq == DEMO_PLAYBACK_SEQ_NUMBER &&                               \
                                                               oldSeq == DEMO_PLAYBACK_SEQ_NUMBER, pChannel), (INetAtSyncItem*)0); \
  }                                                                                                                                \
  inline bool cls::Handle ## name(const TParam ## name & param, EntityId * pEntityId, bool bFromDemoSystem, INetChannel * pNetChannel)

template<class T, class Obj>
class CNetSimpleAtSyncItem : public INetAtSyncItem
{
public:
	typedef bool (Obj::* CallbackFunc)(const T&, EntityId, bool, INetChannel*, EDisconnectionCause& disconnectCause, string& disconnectMessage);
	CNetSimpleAtSyncItem(Obj* pObj, CallbackFunc callback, const T& data, EntityId id, bool fromDemo, INetChannel* pChannel) : m_pObj(pObj), m_callback(callback), m_data(data), m_id(id), m_fromDemo(fromDemo), m_pChannel(pChannel) {}
	bool Sync()
	{
		EDisconnectionCause disconnectCause = eDC_Unknown;
		string disconnectMessage;
		return SyncWithError(disconnectCause, disconnectMessage);
	}
	bool SyncWithError(EDisconnectionCause& disconnectCause, string& disconnectMessage)
	{
		if (!(m_pObj->*m_callback)(m_data, m_id, m_fromDemo, m_pChannel, disconnectCause, disconnectMessage))
			return false;
		return true;
	}
	void DeleteThis()
	{
		delete this;
	}
private:
	Obj*         m_pObj;
	CallbackFunc m_callback;
	T            m_data;
	EntityId     m_id;
	bool         m_fromDemo;
	INetChannel* m_pChannel;
};

#define NET_DECLARE_SIMPLE_ATSYNC_MESSAGE_BODY(name, CClass)                                                                                              \
  private:                                                                                                                                                \
    static TNetMessageCallbackResult Trampoline ## name(uint32,                                                                                           \
                                                        INetMessageSink*, TSerialize,                                                                     \
                                                        uint32, uint32, uint32, EntityId*, INetChannel*);                                                 \
    bool Handle ## name(const CClass &, EntityId, bool bFromDemoSystem, INetChannel*, EDisconnectionCause & disconnectCause, string & disconnectMessage); \
    typedef CClass TParam ## name;                                                                                                                        \
  public:                                                                                                                                                 \
    static const SNetMessageDef* const name;

#define NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(name, CClass)             \
  NET_DECLARE_SIMPLE_ATSYNC_MESSAGE_BODY(name, CClass)              \
  template<class T>                                                 \
  static void Send ## name ## With(const CClass &msg, T * pChannel) \
  { pChannel->SendMsg(new CSimpleNetMessage<CClass>(msg, name)); }

#define NET_DECLARE_SIMPLE_ATSYNC_MESSAGE_WITHOUT_SEND(name, CClass) \
  NET_DECLARE_SIMPLE_ATSYNC_MESSAGE_BODY(name, CClass)               \
  template<class T>                                                  \
  static void Send ## name ## With(const CClass &msg, T * pChannel);

#define NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE_SEND(cls, name, CClass) \
  template<class T>                                                 \
  void cls::Send ## name ## With(const CClass &msg, T * pChannel)   \
  { pChannel->SendMsg(new CSimpleNetMessage<CClass>(msg, name)); }

#define NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(cls, name, reliability, parallelFlags)                                           \
  const SNetMessageDef * const cls::name = cls::Helper_AddMessage(                                                           \
    cls::Trampoline ## name, reliability, # cls ":" # name, (parallelFlags) & ~eMPF_DecodeInSync);                           \
  TNetMessageCallbackResult cls::Trampoline ## name(                                                                         \
    uint32,                                                                                                                  \
    INetMessageSink * handler,                                                                                               \
    TSerialize serialize,                                                                                                    \
    uint32 curSeq,                                                                                                           \
    uint32 oldSeq,                                                                                                           \
    uint32 timeFraction32,                                                                                                   \
    EntityId * pEntityId, INetChannel * pChannel)                                                                            \
  {                                                                                                                          \
    cls* p = static_cast<cls*>(handler);                                                                                     \
    TParam ## name param;                                                                                                    \
    param.SerializeWith(serialize);                                                                                          \
    return TNetMessageCallbackResult(true,                                                                                   \
                                     new CNetSimpleAtSyncItem<TParam ## name, cls>(                                          \
                                       p, &cls::Handle ## name, param, *pEntityId,                                           \
                                       curSeq == DEMO_PLAYBACK_SEQ_NUMBER && oldSeq == DEMO_PLAYBACK_SEQ_NUMBER, pChannel)); \
  }                                                                                                                          \
  inline bool cls::Handle ## name(const TParam ## name & param, EntityId entityId, bool bFromDemoSystem, INetChannel * pNetChannel, EDisconnectionCause & disconnectCause, string & disconnectMessage)

//! Helpers for writing channel establishment tasks.
class CCET_Base : public IContextEstablishTask
{
public:
	virtual ~CCET_Base()
	{
	}
	virtual void Release()
	{
		delete this;
	}
	virtual void OnFailLoading(bool hasEntered)
	{
	}
	virtual void OnLoadingComplete()
	{
	}
};

template<class T>
class CCET_SetValue : public CCET_Base
{
public:
	CCET_SetValue(T* pWhere, T what, string name) : m_pSetWhere(pWhere), m_setWhat(what), m_name(name) {}

	const char*                 GetName() { return m_name.c_str(); }

	EContextEstablishTaskResult OnStep(SContextEstablishState&)
	{
		*m_pSetWhere = m_setWhat;
		return eCETR_Ok;
	}

private:
	T*     m_pSetWhere;
	T      m_setWhat;
	string m_name;
};

template<class T>
void AddSetValue(IContextEstablisher* pEst, EContextViewState state, T* pWhere, T what, const char* name)
{
	pEst->AddTask(state, new CCET_SetValue<T>(pWhere, what, name));
}

template<class T>
class CCET_StoreValueThenChangeTo : public CCET_Base
{
public:
	CCET_StoreValueThenChangeTo(T init, T what, string name) : m_value(init), m_setWhat(what), m_name(name) {}

	const char*                 GetName() { return m_name.c_str(); }

	EContextEstablishTaskResult OnStep(SContextEstablishState&)
	{
		m_value = m_setWhat;
		return eCETR_Ok;
	}

private:
	T      m_value;
	T      m_setWhat;
	string m_name;
};

template<class T>
void AddStoreValueThenChangeTo(IContextEstablisher* pEst, EContextViewState state, T init, T what, const char* name)
{
	pEst->AddTask(state, new CCET_StoreValueThenChangeTo<T>(init, what, name));
}

template<class T>
class CCET_WaitValue : public CCET_Base
{
public:
	CCET_WaitValue(T* pWhere, T what, string name, CTimeValue notificationInterval)
		: m_pWaitWhere(pWhere)
		, m_waitWhat(what)
		, m_name(name)
		, m_lastNotification(0.0f)
		, m_notificationInterval(notificationInterval)
	{
	}

	const char*                 GetName() { return m_name.c_str(); }

	EContextEstablishTaskResult OnStep(SContextEstablishState&)
	{
		if (*m_pWaitWhere == m_waitWhat)
			return eCETR_Ok;

		CTimeValue now = gEnv->pTimer->GetAsyncTime();
		if (m_lastNotification == 0.0f)
			m_lastNotification = now;
		else if (now - m_lastNotification > m_notificationInterval)
		{
			CryLog("Waiting for value %s", m_name.c_str());
			m_lastNotification = now;
		}

		return eCETR_Wait;
	}

private:
	T*               m_pWaitWhere;
	T                m_waitWhat;
	string           m_name;

	CTimeValue       m_lastNotification;
	const CTimeValue m_notificationInterval;
};

template<class T>
void AddWaitValue(IContextEstablisher* pEst, EContextViewState state, T* pWhere, T what, const char* name, CTimeValue notificationInterval)
{
	pEst->AddTask(state, new CCET_WaitValue<T>(pWhere, what, name, notificationInterval));
}

template<class T>
class CCET_AddValue : public CCET_Base
{
public:
	CCET_AddValue(T* pWhere, T what, string name) : m_pWaitWhere(pWhere), m_waitWhat(what), m_name(name) {}

	const char*                 GetName() { return m_name.c_str(); }

	EContextEstablishTaskResult OnStep(SContextEstablishState&)
	{
		*m_pWaitWhere += m_waitWhat;
		return eCETR_Ok;
	}

private:
	T*     m_pWaitWhere;
	T      m_waitWhat;
	string m_name;
};

template<class T>
void AddAddValue(IContextEstablisher* pEst, EContextViewState state, T* pWhere, T what, const char* name)
{
	pEst->AddTask(state, new CCET_AddValue<T>(pWhere, what, name));
}

template<class T>
class CCET_CallMemberFunction : public CCET_Base
{
public:
	CCET_CallMemberFunction(T* pObj, void(T::* memberFunc)(), string name) : m_pObj(pObj), m_memberFunc(memberFunc), m_name(name) {}

	const char*                 GetName() { return m_name.c_str(); }

	EContextEstablishTaskResult OnStep(SContextEstablishState&)
	{
		(m_pObj->*m_memberFunc)();
		return eCETR_Ok;
	}

private:
	T*     m_pObj;
	void   (T::* m_memberFunc)();
	string m_name;
};

template<class T>
void AddCallMemberFunction(IContextEstablisher* pEst, EContextViewState state, T* pObj, void (T::* memberFunc)(), const char* name)
{
	pEst->AddTask(state, new CCET_CallMemberFunction<T>(pObj, memberFunc, name));
}

//! \endcond