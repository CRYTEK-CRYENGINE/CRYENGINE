// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  implementation of INetwork
   -------------------------------------------------------------------------
   History:
   - 26/07/2004   : Created by Craig Tiller
   !F RCON system - lluo
*************************************************************************/

#ifndef __NETWORK_H__
#define __NETWORK_H__

#pragma once

#include <vector>
#include <memory>
#include "Config.h"
#include <CrySystem/TimeValue.h>
#include "Protocol/StatsCollector.h"
#include "Protocol/FrameTypes.h"
#include "Errors.h"
#include "Socket/NetResolver.h"
#include "INetworkPrivate.h"
#include "INetworkMember.h"
#include <CryNetwork/NetHelpers.h>
#include "WorkQueue.h"
#include "NetTimer.h"
#include "MementoMemoryManager.h"
#include "NetCVars.h"
#include "NetLog.h"
#include <CryMemory/STLGlobalAllocator.h>
#if NEW_BANDWIDTH_MANAGEMENT
	#include "Protocol/NewMessageQueue.h"
#else
	#include "Protocol/MessageQueue.h"
#endif // NEW_BANDWIDTH_MANAGEMENT
#include "Socket/IStreamSocket.h"
#include "Socket/IDatagramSocket.h"
#include "Socket/ISocketIOManager.h"

#include "Cryptography/Whirlpool.h"
#include "Protocol/ExponentialKeyExchange.h"
#include "Compression/CompressionManager.h"

#include "Socket/WatchdogTimer.h"

struct ICVar;
class CNetNub;
class CNetContext;
struct INetworkMember;
struct IConsoleCmdArgs;
class CNetworkInspector;
class CServiceManager;
#if ENABLE_DISTRIBUTED_LOGGER
class CDistributedLogger;
#endif

#if defined(ENABLE_LW_PROFILERS)
	#define NET_THREAD_TIMING
#endif

#ifdef NET_THREAD_TIMING
	#define NET_START_THREAD_TIMER() CNetwork::Get()->ThreadTimerStart()
	#define NET_STOP_THREAD_TIMER()  CNetwork::Get()->ThreadTimerStop()
	#define NET_TICK_THREAD_TIMER()  CNetwork::Get()->ThreadTimerTick()
#else
	#define NET_START_THREAD_TIMER()
	#define NET_STOP_THREAD_TIMER()
	#define NET_TICK_THREAD_TIMER()
#endif

struct PerformanceGuard
{
	SNetworkPerformance* m_pCounter;
	uint64               m_nOldVal;

	PerformanceGuard(SNetworkPerformance* p, uint64 oldVal)
	{
		m_pCounter = p;
		m_nOldVal = oldVal;
		p->m_nNetworkSync = CryGetTicks();
	}

	~PerformanceGuard()
	{
		m_pCounter->m_nNetworkSync = CryGetTicks() - m_pCounter->m_nNetworkSync + m_nOldVal;
	}

};

struct SSystemBranchCounters
{
	SSystemBranchCounters()
	{
		memset(this, 0, sizeof(*this));
	}
	uint32 updateTickMain;
	uint32 updateTickSkip;
	uint32 iocpReapImmediate;
	uint32 iocpReapImmediateCollect;
	uint32 iocpReapSleep;
	uint32 iocpReapSleepCollect;
	uint32 iocpIdle;
	uint32 iocpForceSync;
	uint32 iocpBackoffCheck;
};

extern SSystemBranchCounters g_systemBranchCounters;

enum EMementoMemoryType
{
	eMMT_Memento = 0,
	eMMT_PacketData,
	eMMT_ObjectData,
	eMMT_Alphabet,
	eMMT_NUM_TYPES
};

template<class T>
class NetMutexLock
{
public:
	NetMutexLock(T& mtx, bool lock) : m_mtx(mtx), m_lock(lock) { if (m_lock) m_mtx.Lock(); }
	~NetMutexLock() { if (m_lock) m_mtx.Unlock(); }

private:
	T&   m_mtx;
	bool m_lock;
};

typedef CryLockT<CRYLOCK_RECURSIVE> NetFastMutex;

enum EDebugMemoryMode
{
	eDMM_Context  = 1,
	eDMM_Endpoint = 2,
	eDMM_Mementos = 4,
};

enum EDebugBandwidthMode
{
	eDBM_None          = 0,
	eDBM_AspectAgeList = 1,
	eDBM_AspectAgeMap  = 2,
	eDBM_PriorityMap   = 3,
};

const char* GetTimestampString();

extern CTimeValue g_time;

#if defined(_RELEASE)
	#define ASSERT_MUTEX_LOCKED(mtx)
	#define ASSERT_MUTEX_UNLOCKED(mtx)
#else
	#define ASSERT_MUTEX_LOCKED(mtx)   if (CNetwork::Get()->IsMultithreaded()) \
	  NET_ASSERT((mtx).IsLocked())
	#define ASSERT_MUTEX_UNLOCKED(mtx) if (CNetwork::Get()->IsMultithreaded()) \
	  NET_ASSERT(!(mtx).IsLocked())
#endif

#define ASSERT_GLOBAL_LOCK ASSERT_MUTEX_LOCKED(CNetwork::Get()->GetMutex())
#define ASSERT_COMM_LOCK   ASSERT_MUTEX_LOCKED(CNetwork::Get()->GetCommMutex())

class CNetwork : public INetworkPrivate
{
public:
	//! constructor
	CNetwork();
protected:
	//! destructor
	virtual ~CNetwork();

public:
	bool Init(int ncpu);

	// interface INetwork -------------------------------------------------------------

	virtual bool     HasNetworkConnectivity();
	virtual INetNub* CreateNub(const char* address,
	                           IGameNub* pGameNub,
	                           IGameSecurity* pSecurity,
	                           IGameQuery* pGameQuery);
	virtual ILanQueryListener*    CreateLanQueryListener(IGameQueryListener* pGameQueryListener);
	virtual INetContext*          CreateNetContext(IGameContext* pGameContext, uint32 flags);
	virtual const char*           EnumerateError(NRESULT err);
	virtual void                  Release();
	virtual void                  GetMemoryStatistics(ICrySizer* pSizer);
	virtual void                  GetPerformanceStatistics(SNetworkPerformance* pSizer);
	virtual void SyncWithGame(ENetworkGameSync);
	virtual const char*           GetHostName();
	virtual INetworkServicePtr    GetService(const char* name);
	virtual void                  FastShutdown();
	virtual void                  SetCDKey(const char* key);
	virtual bool                  PbConsoleCommand(const char*, int length);           // EvenBalance - M. Quinn
	virtual void                  PbCaptureConsoleLog(const char* output, int length); // EvenBalance - M. Quinn
	virtual void                  PbServerAutoComplete(const char*, int length);       // EvenBalance - M. Quinn
	virtual void                  PbClientAutoComplete(const char*, int length);       // EvenBalance - M. Quinn
	virtual void SetNetGameInfo(SNetGameInfo);
	virtual SNetGameInfo          GetNetGameInfo();
	virtual ICryLobby*            GetLobby();

	virtual bool                  IsPbClEnabled();
	virtual bool                  IsPbSvEnabled();

	virtual void                  StartupPunkBuster(bool server);
	virtual void                  CleanupPunkBuster();

	virtual bool                  IsPbInstalled();

	virtual IRemoteControlSystem* GetRemoteControlSystemSingleton();

	virtual ISimpleHttpServer*    GetSimpleHttpServerSingleton();

	// nub helpers
	int                           GetLogLevel();
	virtual CNetAddressResolver*  GetResolver()                        { return m_pResolver; }
	ILINE CWorkQueue&             GetToGameQueue()                     { return m_toGame; }
	ILINE CWorkQueue&             GetToGameLazyQueue()                 { return m_toGameLazyBuilding; }
	ILINE CWorkQueue&             GetFromGameQueue()                   { return m_fromGame; }
	ILINE CWorkQueue&             GetInternalQueue()                   { return m_intQueue; }
	ILINE CNetTimer&              GetTimer()                           { return m_timer; }
	ILINE CAccurateNetTimer&      GetAccurateTimer()                   { return m_accurateTimer; }
	virtual const CNetCVars&      GetCVars()                           { return m_cvars; }
	ILINE CMessageQueue::CConfig* GetMessageQueueConfig() const        { return m_pMessageQueueConfig; }
	ILINE CCompressionManager&    GetCompressionManager()              { return m_compressionManager; }
	virtual ISocketIOManager& GetExternalSocketIOManager()         { return *m_pExternalSocketIOManager; }
	virtual ISocketIOManager& GetInternalSocketIOManager()         { return *m_pInternalSocketIOManager; }
	int                       GetMessageQueueConfigVersion() const { return m_schedulerVersion; }

	CTimeValue                GetGameTime()                        { return m_gameTime; }

	void                      ReportGotPacket()                    { m_detection.ReportGotPacket(); }

	ILINE CStatsCollector*    GetStats()
	{
		static CStatsCollector m_stats("netstats.log");
		return &m_stats;
	}

#if ENABLE_DEBUG_KIT
	ILINE CNetworkInspector* GetNetInspector()
	{
		return m_pNetInspector;
	}
#endif

	void                   WakeThread();

	static ILINE CNetwork* Get()                   { return m_pThis; }
	ILINE NetFastMutex&    GetMutex()              { return m_mutex; }
	ILINE NetFastMutex&    GetCommMutex()          { return m_commMutex; }
	ILINE NetFastMutex&    GetLogMutex()           { return m_logMutex; }

	ILINE bool             IsMultithreaded() const { return (m_multithreadedMode != NETWORK_MT_OFF); }
	void                   SetMultithreadingMode(ENetwork_Multithreading_Mode threadingMode);

	CNetChannel*           FindFirstClientChannel();
	CNetChannel*           FindFirstRemoteChannel();

	string                 GetCDKey() const
	{
		return m_CDKey;
	}

	CServiceManager* GetServiceManager() { return m_pServiceManager.get(); }

	// fast lookup of channels (mainly for punkbuster)
	int          RegisterForFastLookup(CNetChannel* pChannel);
	void         UnregisterFastLookup(int id);
	CNetChannel* GetChannelByFastLookupId(int id);

	void         AddExecuteString(const string& toExec);

	virtual void BroadcastNetDump(ENetDumpType);

	static void LobbyTimerCallback(NetTimerId id, void* pUserData, CTimeValue time);

	void        LOCK_ON_STALL_TICKER()
	{
		if (IsMinimalUpdate())
		{
			m_mutex.Lock();
		}
	}

	void UNLOCK_ON_STALL_TICKER()
	{
		if (IsMinimalUpdate())
		{
			m_mutex.Unlock();
		}
	}

	// small hack to make adding things to the from game queue in general efficient
#define ADDTOFROMGAMEQUEUE_BODY(params) if (IsPrimaryThread()) { LOCK_ON_STALL_TICKER(); m_fromGame.Add params; UNLOCK_ON_STALL_TICKER(); } else { CryAutoLock<NetFastMutex> lk(m_fromGame_otherThreadLock); m_fromGame_otherThreadQueue.Add params; }
	template<class A> void                                                       AddToFromGameQueue(const A& a)
	{ ADDTOFROMGAMEQUEUE_BODY((a)); }
	template<class A, class B> void                                              AddToFromGameQueue(const A& a, const B& b)
	{ ADDTOFROMGAMEQUEUE_BODY((a, b)); }
	template<class A, class B, class C> void                                     AddToFromGameQueue(const A& a, const B& b, const C& c)
	{ ADDTOFROMGAMEQUEUE_BODY((a, b, c)); }
	template<class A, class B, class C, class D> void                            AddToFromGameQueue(const A& a, const B& b, const C& c, const D& d)
	{ ADDTOFROMGAMEQUEUE_BODY((a, b, c, d)); }
	template<class A, class B, class C, class D, class E> void                   AddToFromGameQueue(const A& a, const B& b, const C& c, const D& d, const E& e)
	{ ADDTOFROMGAMEQUEUE_BODY((a, b, c, d, e)); }
	template<class A, class B, class C, class D, class E, class F> void          AddToFromGameQueue(const A& a, const B& b, const C& c, const D& d, const E& e, const F& f)
	{ ADDTOFROMGAMEQUEUE_BODY((a, b, c, d, e, f)); }
	template<class A, class B, class C, class D, class E, class F, class G> void AddToFromGameQueue(const A& a, const B& b, const C& c, const D& d, const E& e, const F& f, const G& g)
	{ ADDTOFROMGAMEQUEUE_BODY((a, b, c, d, e, f, g)); }

#define ADDTOTOGAMEQUEUE_BODY(params) LOCK_ON_STALL_TICKER(); ASSERT_GLOBAL_LOCK; CNetwork::Get()->GetToGameQueue().Add params; UNLOCK_ON_STALL_TICKER();
	template<class A> void                                                       AddToToGameQueue(void (A::* ptr)(void), A* a)
	{ ADDTOTOGAMEQUEUE_BODY((ptr, a)); }
	template<class A> void                                                       AddToToGameQueue(const A& a)
	{ ADDTOTOGAMEQUEUE_BODY((a)); }
	template<class A, class B> void                                              AddToToGameQueue(const A& a, const B& b)
	{ ADDTOTOGAMEQUEUE_BODY((a, b)); }
	template<class A, class B, class C> void                                     AddToToGameQueue(const A& a, const B& b, const C& c)
	{ ADDTOTOGAMEQUEUE_BODY((a, b, c)); }
	template<class A, class B, class C, class D> void                            AddToToGameQueue(const A& a, const B& b, const C& c, const D& d)
	{ ADDTOTOGAMEQUEUE_BODY((a, b, c, d)); }
	template<class A, class B, class C, class D, class E> void                   AddToToGameQueue(const A& a, const B& b, const C& c, const D& d, const E& e)
	{ ADDTOTOGAMEQUEUE_BODY((a, b, c, d, e)); }
	template<class A, class B, class C, class D, class E, class F> void          AddToToGameQueue(const A& a, const B& b, const C& c, const D& d, const E& e, const F& f)
	{ ADDTOTOGAMEQUEUE_BODY((a, b, c, d, e, f)); }
	template<class A, class B, class C, class D, class E, class F, class G> void AddToToGameQueue(const A& a, const B& b, const C& c, const D& d, const E& e, const F& f, const G& g)
	{ ADDTOTOGAMEQUEUE_BODY((a, b, c, d, e, f, g)); }

	virtual void                   NpCountReadBits(bool count);
	virtual bool                   NpGetChildFromCurrent(const char* name, SNetProfileStackEntry** entry, bool rmi);
	virtual void                   NpRegisterBeginCall(const char* name, SNetProfileStackEntry** entry, float budget, bool rmi);
	virtual void                   NpBeginFunction(SNetProfileStackEntry* entry, bool read);
	virtual void                   NpEndFunction();
	virtual bool                   NpIsInitialised();
	virtual SNetProfileStackEntry* NpGetNullProfile();

	/////////////////////////////////////////////////////////////////////////////
	// Host Migration
public:
	virtual void EnableHostMigration(bool bEnabled);
	virtual bool IsHostMigrationEnabled(void);

	virtual void TerminateHostMigration(CrySessionHandle gh);
	virtual void AddHostMigrationEventListener(IHostMigrationEventListener* pListener, const char* pWho, EListenerPriorityType priority);
	virtual void RemoveHostMigrationEventListener(IHostMigrationEventListener* pListener);
	/////////////////////////////////////////////////////////////////////////////

	// Exposed block encryption
	virtual void   EncryptBuffer(uint8* pOutput, const uint8* pInput, uint32 bufferLength, const uint8* pKey, uint32 keyLength);
	virtual void   DecryptBuffer(uint8* pOutput, const uint8* pInput, uint32 bufferLength, const uint8* pKey, uint32 keyLength);

	virtual void   EncryptBuffer(TCipher cipher, uint8* pOutput, const uint8* pInput, uint32 bufferLength);
	virtual void   DecryptBuffer(TCipher cipher, uint8* pOutput, const uint8* pInput, uint32 bufferLength);

	virtual uint32 RijndaelEncryptBuffer(uint8* pOutput, const uint8* pInput, uint32 bufferLength, const uint8* pKey, uint32 keyLength);
	virtual uint32 RijndaelDecryptBuffer(uint8* pOutput, const uint8* pInput, uint32 bufferLength, const uint8* pKey, uint32 keyLength);

	// Exposed streamed encryption
	virtual TCipher BeginCipher(const uint8* pKey, uint32 keyLength);
	virtual void    Encrypt(TCipher cipher, uint8* pOutput, const uint8* pInput, uint32 bufferLength);
	virtual void    Decrypt(TCipher cipher, uint8* pOutput, const uint8* pInput, uint32 bufferLength);
	virtual void    EndCipher(TCipher cipher);

#ifdef DEDICATED_SERVER
	// Dedicated server scheduler
	virtual IDatagramSocketPtr GetDedicatedServerSchedulerSocket();
#endif

	virtual void             GetBandwidthStatistics(SBandwidthStats* const pStats);
	virtual void             GetProfilingStatistics(SNetworkProfilingStats* const pProfilingStats);

	virtual bool             ConvertAddr(const TNetAddress& addrIn, CRYSOCKADDR_IN* pSockAddr);
	virtual bool             ConvertAddr(const TNetAddress& addrIn, CRYSOCKADDR* pSockAddr, int* addrLen);

	bool                     IsMinimalUpdate() { return m_allowMinimalUpdate; }

	virtual TMemHdl          MemAlloc(size_t sz);
	virtual void             MemFree(TMemHdl h);
	virtual void*            MemGetPtr(TMemHdl h);

	virtual uint8            FrameHeaderToID(uint8 header) { return Frame_HeaderToID[header]; }
	virtual uint8            FrameIDToHeader(uint8 id)     { return Frame_IDToHeader[id]; }

	virtual SObjectCounters& GetObjectCounters()           { return g_objcnt; }

#ifdef NET_THREAD_TIMING
	void ThreadTimerStart();
	void ThreadTimerStop();
	void ThreadTimerTick();
#endif

private:
	static CNetwork*    m_pThis;

	bool                m_allowMinimalUpdate;
	bool                m_bOverideChannelTickToGoNow;

	SNetworkPerformance m_networkPerformance;

	// NOTE: NetCVars is the most dependent component in CryNetwork, it should be constructed before any other CryNetwork components and
	// destructed after all other CryNetwork components get destroyed (the order of member contructions is the order of declarations of
	// the class, and the order of member destruction is the reverse order of member constructions)
	CNetCVars m_cvars;

	// m_vFastChannelLookup must be constructed before any of the CWorkQueue members
	std::vector<CNetChannel*, stl::STLGlobalAllocator<CNetChannel*>> m_vFastChannelLookup;

	void LogNetworkInfo();
	void DoSyncWithGame(ENetworkGameSync);
	void DoSyncWithGameMinimal();
	void AddMember(INetworkMemberPtr pMember);
	// will all of our members will be dead soon?
	bool AllSuicidal();
	void UpdateLoop(ENetworkGameSync sync);
#if ENABLE_DISTRIBUTED_LOGGER
	std::auto_ptr<CDistributedLogger> m_pLogger;
#endif

	typedef std::map<NRESULT, string>                                                  TErrorMap;
	typedef std::vector<INetworkMemberPtr, stl::STLGlobalAllocator<INetworkMemberPtr>> VMembers;

	struct SNetError
	{
		NRESULT     nrErrorCode;
		const char* sErrorDescription;
	};

	TErrorMap              m_mapErrors;
	static SNetError       m_neNetErrors[];
	VMembers               m_vMembers;

	bool                   m_bQuitting;

	CNetAddressResolver*   m_pResolver;
	CMementoMemoryManager* m_pMMM;
#if ENABLE_DEBUG_KIT
	CNetworkInspector*     m_pNetInspector;
#endif
	// m_vFastChannelLookup must be constructed before any of the CWorkQueue's
	CWorkQueue                     m_toGame;
	CWorkQueue                     m_fromGame;
	NetFastMutex                   m_fromGame_otherThreadLock;
	CWorkQueue                     m_fromGame_otherThreadQueue;
	CWorkQueue                     m_intQueue;
	CWorkQueue                     m_toGameLazyBuilding;
	CWorkQueue                     m_toGameLazyProcessing;
	CNetTimer                      m_timer;
	CAccurateNetTimer              m_accurateTimer;
	CCompressionManager            m_compressionManager;
	CMessageQueue::CConfig*        m_pMessageQueueConfig;
	NetTimerId                     m_lobbyTimer;
	CTimeValue                     m_lobbyTimerInterval;
	int                            m_schedulerVersion;
	int                            m_inSync[eNGS_NUM_ITEMS + 2]; // one extra for the continuous update loop, and the other for flushing the lazy queue

	std::auto_ptr<CServiceManager> m_pServiceManager;

	class CNetworkThread;
	std::auto_ptr<CNetworkThread> m_pThread;
	NetFastMutex                  m_mutex;
	NetFastMutex                  m_commMutex;
	NetFastMutex                  m_logMutex;

	CTimeValue                    m_gameTime;

	string                        m_CDKey;

	class CBufferWakeups;
	int                          m_bufferWakeups;
	bool                         m_wokenUp;

	ENetwork_Multithreading_Mode m_multithreadedMode;
	int                          m_occasionalCounter;
	int                          m_cleanupMember;
	CTimeValue                   m_nextCleanup;
	std::vector<string>          m_consoleCommandsToExec;
	ISocketIOManager*            m_pExternalSocketIOManager;
	ISocketIOManager*            m_pInternalSocketIOManager;
	int                          m_cpuCount;

	CTimeValue                   m_lastUpdateLock;
	bool                         m_forceLock;
	bool                         m_bDelayedExternalWork;

	SNetGameInfo                 m_gameInfo;

	bool UpdateTick(bool mt);

	enum eTickReturnState
	{
		eTRS_Quit,
		eTRS_Continue,
		eTRS_TimedOut
	};

	eTickReturnState DoMainTick(bool mt);

	bool m_isPbSvActive;
	bool m_isPbClActive;

	class CNetworkConnectivityDetection
	{
	public:
		CNetworkConnectivityDetection() : m_hasNetworkConnectivity(true), m_lastCheck(0.0f), m_lastPacketReceived(0.0f) {}

		bool HasNetworkConnectivity();
		void ReportGotPacket() { m_lastPacketReceived = std::max(g_time, m_lastPacketReceived); }

		void AddRef()          {}
		void Release()         {}
		bool IsDead()          { return false; }

	private:
		bool       m_hasNetworkConnectivity;
		CTimeValue m_lastCheck;
		CTimeValue m_lastPacketReceived;

		void DetectNetworkConnectivity();
	};

	CNetworkConnectivityDetection m_detection; // NOTE: this relies on NetTimer and WorkQueue objects to function correctly, so it must be declared/defined after them

#ifdef DEDICATED_SERVER
	IDatagramSocketPtr m_pDedicatedServerSchedulerSocket;
#endif

#ifdef NET_THREAD_TIMING
	CTimeValue m_threadTimeCur;
	int        m_threadTimeDepth;
	float      m_threadTime;
#endif
};

extern int g_lockCount;
#define SCOPED_GLOBAL_LOCK_NO_LOG                                                                         \
  ASSERT_MUTEX_UNLOCKED(CNetwork::Get()->GetCommMutex());                                                 \
  NetMutexLock<NetFastMutex> globalLock(CNetwork::Get()->GetMutex(), CNetwork::Get()->IsMultithreaded()); \
  g_lockCount++;                                                                                          \
  CAutoUpdateWatchdogCounters updateWatchdogTimers
#if LOG_LOCKING
	#define SCOPED_GLOBAL_LOCK SCOPED_GLOBAL_LOCK_NO_LOG; NetLog("lock %s(%d)", __FUNCTION__, __LINE__); ENSURE_REALTIME
#else
	#define SCOPED_GLOBAL_LOCK SCOPED_GLOBAL_LOCK_NO_LOG; ENSURE_REALTIME
#endif

// should be used in the NetLog functions only
#define SCOPED_GLOBAL_LOG_LOCK CryAutoLock<NetFastMutex> globalLogLock(CNetwork::Get()->GetLogMutex())
#define SCOPED_COMM_LOCK       NetMutexLock<NetFastMutex> commMutexLock(CNetwork::Get()->GetCommMutex(), CNetwork::Get()->IsMultithreaded())

// never place these macros in a block without braces...
// BAD: if(x) TO_GAME(blah);
// GOOD: if(x) {TO_GAME(blah);}

// pass a message callback from network system to game, to execute during SyncToGame
// must hold the global lock
// global lock will be held in the callback
#define TO_GAME CNetwork::Get()->AddToToGameQueue
// pass a message callback from network system to game, to execute in parallel with the network thread
// global lock will *NOT* be held in the callback
#define TO_GAME_LAZY ASSERT_GLOBAL_LOCK; CNetwork::Get()->GetToGameLazyQueue().Add
// pass a message from the game to the network;
// not thread-safe!
// the game must guarantee that its calls to the network system are synchronized
// the network code must guarantee that it never calls FROM_GAME in one of its threads
// global lock will be held in the callback
#define FROM_GAME CNetwork::Get()->AddToFromGameQueue
// pass a message from the network engine to the network engine to be executed later
// must hold the global lock
// global lock will be held in the callback
#define NET_TO_NET         ASSERT_GLOBAL_LOCK; CNetwork::Get()->WakeThread(); CNetwork::Get()->GetInternalQueue().Add

#define RESOLVER           (*CNetwork::Get()->GetResolver())
#if USE_ACCURATE_NET_TIMERS
	#define TIMER            CNetwork::Get()->GetAccurateTimer()
#else
	#define TIMER            CNetwork::Get()->GetTimer()
#endif // USE_ACCURATE_NET_TIMERS
#define ACCURATE_NET_TIMER CNetwork::Get()->GetAccurateTimer()
#if TIMER_DEBUG
	#define ADDTIMER(_when, _callback, _userdata, _name) AddTimer(_when, _callback, _userdata, __FILE__, __LINE__, _name)
#else
	#define ADDTIMER(_when, _callback, _userdata, _name) AddTimer(_when, _callback, _userdata)
#endif // TIMER_DEBUG
#define CVARS         CNetwork::Get()->GetCVars()
#define NET_INSPECTOR (*CNetwork::Get()->GetNetInspector())
#define STATS         (*CNetwork::Get()->GetStats())

class CMementoMemoryRegion
{
public:
	ILINE CMementoMemoryRegion(CMementoMemoryManager* pMMM)
	{
		ASSERT_GLOBAL_LOCK;
		m_pPrev = m_pMMM;
		m_pMMM = pMMM;
	}

	ILINE ~CMementoMemoryRegion()
	{
		ASSERT_GLOBAL_LOCK;
		m_pMMM = m_pPrev;
	}

	static ILINE CMementoMemoryManager& Get()
	{
		assert(m_pMMM);
		return *m_pMMM;
	}

private:
	CMementoMemoryRegion(const CMementoMemoryRegion&);
	CMementoMemoryRegion& operator=(const CMementoMemoryRegion&);

	CMementoMemoryManager*        m_pPrev;
	static CMementoMemoryManager* m_pMMM;
};

#define MMM_REGION(pMMM) CMementoMemoryRegion _mmmrgn(pMMM)

ILINE CMementoMemoryManager& MMM()
{
	return CMementoMemoryRegion::Get();
}

#endif
