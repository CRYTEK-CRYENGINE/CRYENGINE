// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _H_LIVECREATEMANAGER_H_
#define _H_LIVECREATEMANAGER_H_

#include <CryLiveCreate/ILiveCreateManager.h>
#include <CryNetwork/IRemoteCommand.h>

struct IPlatformHandler;
struct IPlatformHandlerFactory;
struct IServiceNetworkConnection;
struct IRemoteCommandClient;
struct IRemoteCommandConnection;

//-----------------------------------------------------------------------------

#if defined(LIVECREATE_FOR_PC) && !defined(NO_LIVECREATE)

namespace LiveCreate
{

//-----------------------------------------------------------------------------

// Host information interface implementation
class CHostInfo : public IHostInfo
{
protected:
	class CManager* m_pManager;

	volatile int    m_refCount;

	// Platform handler interface
	// In case of this class it's mainly needed for low-level file transfer functionality (FileSync).
	IPlatformHandler* m_pPlatform;

	// Connection to command dispatched, this can also handle raw network traffic.
	// This can exist or not, depends on whether we are connected.
	IRemoteCommandConnection* m_pConnection;

	// Last known valid address of the remote host, supplied when the host info was created.
	// This should not be used to identify the hosts since (in theory) this address can change.
	string m_address;

	// Was the connection confirmed by receiving BuildInfo packet ?
	// We need that kind of ACK message to be sure that the remote side is responding.
	bool m_bIsConnectionConfirmed;

	// Is the remote side ready to accept commands ?
	bool m_bIsRemoteSideReady;

	// Is the remote side suspended for any reason - for example level loading?
	bool m_bIsSuspended;

private:
	int64 m_lastReadyInfoRequestSentTime;

public:
	CHostInfo(
		class CManager* pManager,
			IPlatformHandler* pPlatform,
			const char* szValidAddress);

	// Update and maintain connection
	void Update();

	// Can we send to this host ?
	// This checks if we have a valid and confirmed connection and that the remote side is not suspended.
	bool CanSend() const;

	// Send message using the raw communication channel
	// If we don't have a valid connection or the remote side is suspended a message can be rejected.
	// Functions returns true only if the low-level communication layer accepted the message for sending.
	bool SendRawMessage(IServiceNetworkMessage* pMessage);

protected:
	// Process raw message (as received from LiveCreate host)
	bool ParseMessage(const string& msg, IDataReadStream& data);

	// Send a info request messages to remote connection
	void SendRequestMessage(const char* szInfoType);

public:
	// IHost interface implementation
	virtual const char*       GetTargetName() const;
	virtual const char*       GetAddress() const;
	virtual const char*       GetPlatformName() const;
	virtual bool              IsConnected() const;
	virtual bool              IsReady() const;
	virtual IPlatformHandler* GetPlatformInterface();
	virtual bool              Connect();
	virtual void              Disconnect();
	virtual void              AddRef();
	virtual void              Release();

private:
	virtual ~CHostInfo();
};

//-----------------------------------------------------------------------------

class CManager : public IManager
{
protected:
	struct PlatformDLL
	{
		PlatformDLL(IPlatformHandlerFactory* factory = NULL)
			: hLibrary(0)
			, pFactory(factory)
		{}

		void*                    hLibrary;
		IPlatformHandlerFactory* pFactory;
	};

	struct LastHostState
	{
		bool m_bIsReady;
		bool m_bIsConnected;
	};

protected:
	// Remote command dispatched
	IRemoteCommandClient* m_pCommandDispatcher;

	// Logging
	ICVar* m_pCVarLogEnabled;

	// Manager wide flags
	bool m_bIsEnabled;
	bool m_bCanSend;

	// Platform factories
	typedef std::vector<PlatformDLL> TPlatformList;
	TPlatformList m_platformFactories;

	// Registered hosts (usually never deleted)
	typedef std::vector<CHostInfo*> THostList;
	THostList m_pHosts;
	CryMutex  m_hostsLock;

	// Manager listeners
	typedef std::vector<IManagerListenerEx*> TListeners;
	TListeners m_pListeners;
	CryMutex   m_listenersLock;

	// Cached host states
	typedef std::map<CHostInfo*, LastHostState> TLastHostStateCache;
	TLastHostStateCache m_lastHostStates;

public:
	ILINE IRemoteCommandClient* GetCommandDispatcher() const
	{
		return m_pCommandDispatcher;
	}

public:
	CManager();
	virtual ~CManager();

	// IManagerEx interface implementation
	virtual bool                     IsNullImplementation() const;
	virtual bool                     CanSend() const;
	virtual bool                     IsEnabled() const;
	virtual void                     SetEnabled(bool bIsEnabled);
	virtual bool                     IsLogEnabled() const;
	virtual void                     SetLogEnabled(bool bIsEnabled);
	virtual bool                     RegisterListener(IManagerListenerEx* pListener);
	virtual bool                     UnregisterListener(IManagerListenerEx* pListener);
	virtual uint                     GetNumHosts() const;
	virtual IHostInfo*               GetHost(const uint index) const;
	virtual uint                     GetNumPlatforms() const;
	virtual IPlatformHandlerFactory* GetPlatformFactory(const uint index) const;
	virtual IHostInfo*               CreateHost(IPlatformHandler* pPlatform, const char* szValidIpAddress);
	virtual bool                     RemoveHost(IHostInfo* pHost);
	virtual void                     Update();
	virtual bool                     SendCommand(const IRemoteCommand& command);
	virtual void                     LogMessage(ELogMessageType aType, const char* pMessage);

	// Formated logging
	void LogMessagef(ELogMessageType aType, const char* pMessage, ...);

private:
	// Load platform factory form a DLL file
	bool LoadPlatformHandlerDLL(const char* pFilename, PlatformDLL& dllInfo);

	// Load all of the platform factory DLLs
	void LoadPlatformHandlerFactoryDLLs();

	// Reevaluate the sending conditions
	void EvaluateSendingStatus();
};

//-----------------------------------------------------------------------------

}

#endif

#endif
