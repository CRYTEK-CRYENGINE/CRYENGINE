// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   Description:  Service network implementation
   -------------------------------------------------------------------------
   History:
   - 10/04/2013   : Tomasz Jonarski, Created

*************************************************************************/
#ifndef __CSERVICENETWORK_H__
#define __CSERVICENETWORK_H__

//-----------------------------------------------------------------------------

#include <CryNetwork/IServiceNetwork.h>
#include <CryNetwork/CrySocks.h>
#include <CryThreading/IThreadManager.h>

class CServiceNetwork;

//-----------------------------------------------------------------------------

// General message buffer
class CServiceNetworkMessage : public IServiceNetworkMessage
{
private:
	void*        m_pData;
	uint32       m_id;
	uint32       m_size;
	int volatile m_refCount;

public:
	CServiceNetworkMessage(const uint32 id, const uint32 size);
	virtual ~CServiceNetworkMessage();

	// IServiceNetworMessage interface
	virtual uint32                  GetId() const;
	virtual uint32                  GetSize() const;
	virtual void*                   GetPointer();
	virtual const void*             GetPointer() const;
	virtual struct IDataReadStream* CreateReader() const;
	virtual void                    AddRef();
	virtual void                    Release();
};

//-----------------------------------------------------------------------------

// General network TCP/IP connection
class CServiceNetworkConnection : public IServiceNetworkConnection
{
public:
	friend class CServiceNetworkListener;

	// maximum size of a single message (0.5MB by default)
	static const uint32 kMaximumMessageSize = 5 << 19;

	// initialization message send period (ms)
	static const uint64 kInitializationPerior = 1000;

	// keep alive period (ms), by default every 2s
	static const uint64 kKeepAlivePeriod = 2000;

	// reconnection retries period (ms)
	static const uint64 kReconnectTryPerior = 1000;

	// timeout for assuming server side connection dead (reconnection timeout)
	static const uint64 hReconnectTimeOut = 30 * 1000;

	// communication time out (ms)
	static const uint64 kTimeout = 5000;

	// Type of endpoint
	enum EEndpoint
	{
		// This is the server side of the connection (on the side of the listening socket)
		eEndpoint_Server,

		// This is the client side of the connection (we connected to the listening socket)
		eEndpoint_Client,
	};

	// Internal state machine
	enum EState
	{
		// Connection is initializing
		eState_Initializing,

		// Connection is valid
		eState_Valid,

		// Operation on the socket failed (we may need to reconnect)
		eState_Lost,

		// Connection is closed
		eState_Closed,
	};

	// Command IDs, do not change the numerical values
	enum ECommand
	{
		// Data block command
		eCommand_Data = 1,

		// Keep alive command
		eCommand_KeepAlive = 2,

		// Initialize communication channel (sent only once)
		eCommand_Initialize = 3,
	};

#pragma pack(push)
#pragma pack(1)

	struct Header
	{
		uint8  m_cmd;
		uint32 m_size;

		void   Swap();
	};

	struct InitHeader
	{
		uint8  m_cmd;
		uint8  m_pad0;
		uint8  m_pad1;
		uint8  m_pad2;
		uint32 m_tryCount;
		uint64 m_guid0;
		uint64 m_guid1;

		void   Swap();
	};

#pragma pack(pop)

private:
	CServiceNetwork* m_pManager;

	// Type of endpoint (client/server)
	EEndpoint m_endpointType;

	// Connection state (internal)
	EState m_state;

	// Reference count (updated using CryInterlocked* functions)
	int volatile m_refCount;

	// Internal socket data
	CRYSOCKET m_socket;

	// Local address
	ServiceNetworkAddress m_localAddress;

	// Remote connection address
	ServiceNetworkAddress m_remoteAddress;

	// Internal connection ID (unique)
	CryGUID m_connectionID;

	// Internal time counters
	uint64 m_lastReconnectTime;
	uint64 m_lastKeepAliveSendTime;
	uint64 m_lastMessageReceivedTime;
	uint64 m_lastInitializationSendTime;
	uint32 m_reconnectTryCount;

	// Statistics (updated from threads using CryIntelocked* functions)
	volatile uint32 m_statsNumPacketsSend;
	volatile uint32 m_statsNumPacketsReceived;
	volatile uint32 m_statsNumDataSend;
	volatile uint32 m_statsNumDataReceived;

	// Queue of messages to send (thread access possible)
	typedef CryMT::CLocklessPointerQueue<CServiceNetworkMessage> TSendQueue;
	CServiceNetworkMessage* m_pSendedMessages;
	TSendQueue              m_pSendQueue;
	uint32                  m_messageDataSentSoFar;
	volatile int            m_sendQueueDataSize;

	// Queue of received message
	typedef CryMT::CLocklessPointerQueue<CServiceNetworkMessage> TReceiveQueue;
	TReceiveQueue m_pReceiveQueue;
	uint32        m_receiveQueueDataSize;
	uint32        m_messageDataReceivedSoFar;
	uint32        m_messageReceiveLength;

	// Message being received "right now"
	CServiceNetworkMessage* m_pCurrentReceiveMessage;
	uint32                  m_messageDummyReadLength;

	// External request to close this connection was issued
	bool m_bCloseRequested;

	// Do not accept any new data for sending or receiving
	bool m_bDisableCommunication;

public:
	ILINE bool IsInitialized() const
	{
		return m_state != eState_Initializing;
	}

	ILINE bool IsSendingQueueEmpty() const
	{
		return m_pSendQueue.empty();
	}

	ILINE CServiceNetwork* GetManager() const
	{
		return m_pManager;
	}

public:
	CServiceNetworkConnection(
		class CServiceNetwork* manager,
			EEndpoint endpointType,
			CRYSOCKET socket,
			const CryGUID& connectionID,
			const ServiceNetworkAddress& localAddress,
			const ServiceNetworkAddress& remoteAddress);

	virtual ~CServiceNetworkConnection();

	// IServiceNetworkConnection interface implementation
	virtual const ServiceNetworkAddress& GetRemoteAddress() const;
	virtual const ServiceNetworkAddress& GetLocalAddress() const;
	virtual const CryGUID&               GetGUID() const;
	virtual bool                         IsAlive() const;
	virtual uint32                       GetMessageSendCount() const;
	virtual uint32                       GetMessageReceivedCount() const;
	virtual uint64                       GetMessageSendDataSize() const;
	virtual uint64                       GetMessageReceivedDataSize() const;
	virtual bool                         SendMsg(IServiceNetworkMessage* message);
	virtual IServiceNetworkMessage*      ReceiveMsg();
	virtual void                         FlushAndClose(const uint32 timeout);
	virtual void                         FlushAndWait();
	virtual void                         Close();
	virtual void                         AddRef();
	virtual void                         Release();

	// All remote connections are updated on the client side
	// This is called from service network update thread, try not to call by hand :)
	void Update();

private:
	void ProcessSendingQueue();
	void ProcessReceivingQueue();

	// Keep alive message handling
	void ProcessKeepAlive();
	void SendKeepAlive(const uint64 currentNetworkTime);
	bool HandleTimeout(const uint64 currentNetworkTime);

	// Handle the reconnection request
	bool HandleReconnect(CRYSOCKET socket, const uint32 tryCount);

	// General send/receive functions with error handling.
	// If socket error occurs the connection will be put in the lost state.
	uint32 TrySend(const void* dataBuffer, uint32 dataSize, bool autoHandleErrors);

	// Internal receive function with error handling
	uint32 TryReceive(void* dataBuffer, uint32 dataSize, bool autoHandleErrors);

	// Try to reconnect to the remote address
	bool TryReconnect();

	// Try to send the initialization header
	bool TryInitialize();

	// Low-level socket shutdown (hash way)
	void Shutdown();

	// Reset the connection (put in the lost state and reconnect)
	void Reset();
};

//-----------------------------------------------------------------------------

// TCP/IP listener
class CServiceNetworkListener : public IServiceNetworkListener
{
	typedef CServiceNetworkConnection::InitHeader TInitHeader;

	struct PendingConnection
	{
		// Connection socket
		CRYSOCKET m_socket;

		// Initialization of initialization header received so far
		uint32 m_dataReceivedSoFar;

		// Initialization header
		TInitHeader m_initHeader;

		// Remote address (as returned from accept)
		ServiceNetworkAddress m_remoteAddress;
	};

protected:
	// Owner (the manager)
	CServiceNetwork* m_pManager;

	// Reference count, updated using CryInterlocked* functions
	int volatile m_refCount;

	// Listening socket
	CRYSOCKET m_socket;

	// Local address (usually has the IP in 127.0.0.1:port form)
	ServiceNetworkAddress m_localAddress;

	// Request to close this listener was received
	bool m_closeRequestReceived;

	// Pending connections (but not yet initialized)
	typedef std::vector<PendingConnection*> TPendingConnectionList;
	TPendingConnectionList m_pPendingConnections;

	// All active connections spawned from this listener
	typedef std::vector<CServiceNetworkConnection*> TConnectionList;
	TConnectionList m_pLocalConnections;

	// Access lock for the class members (thread safe)
	CryMutex m_accessLock;

public:
	ILINE CServiceNetwork* GetManager() const
	{
		return m_pManager;
	}

public:
	CServiceNetworkListener(CServiceNetwork* pManager, CRYSOCKET socket, const ServiceNetworkAddress& address);
	virtual ~CServiceNetworkListener();

	void Update();

	// IServiceNetworkListener interface implementation
	virtual const ServiceNetworkAddress& GetLocalAddress() const;
	virtual uint32                       GetConnectionCount() const;
	virtual IServiceNetworkConnection*   Accept();
	virtual bool                         IsAlive() const;
	virtual void                         AddRef();
	virtual void                         Release();
	virtual void                         Close();

private:
	void ProcessIncomingConnections();
	void ProcessPendingConnections();
};

//-----------------------------------------------------------------------------

// TCP/IP manager for service connection channels
class CServiceNetwork : public IServiceNetwork, public IThread
{
	static const uint32 kThreadAffinityDurango = 3;

protected:
	struct ConnectionToClose
	{
		CServiceNetworkConnection* pConnection;

		// timeout for forded close
		uint64 maxWaitTime;
	};

protected:
	// Local listeners
	typedef std::vector<CServiceNetworkListener*> TListenerArray;
	TListenerArray m_pListeners;

	// Local connections
	typedef std::vector<CServiceNetworkConnection*> TConnectionArray;
	TConnectionArray m_pConnections;

	// Connections that are waiting for all of their data to be sent before closing
	typedef std::vector<ConnectionToClose> TConnectionsToCloseArray;
	TConnectionsToCloseArray m_connectionsToClose;

	// We are running on threads, needed to sync the access to arrays
	CryMutex m_accessMutex;

	// Current network time (ms)
	uint64 m_networkTime;

	// Exit was requested
	bool m_bExitRequested;

	// Message verbose level
	ICVar* m_pVerboseLevel;

	// Buffer ID allocator (unique, incremented atomically using CryInterlockedIncrement)
	volatile int m_bufferID;

	// Random number generator for GUID creation
	CRndGen m_guidGenerator;

	// Send/Receive queue size limit
	ICVar* m_pReceiveDataQueueLimit;
	ICVar* m_pSendDataQueueLimit;

public:
	ILINE const uint64 GetNetworkTime() const
	{
		return m_networkTime;
	}

	ILINE const CServiceNetwork* GetManager() const
	{
		return this;
	}

	ILINE const uint32 GetReceivedDataQueueLimit() const
	{
		return m_pReceiveDataQueueLimit->GetIVal();
	}

	ILINE const uint32 GetSendDataQueueLimit() const
	{
		return m_pSendDataQueueLimit->GetIVal();
	}

public:
	CServiceNetwork();
	virtual ~CServiceNetwork();

	// IServiceNetwork interface implementation
	virtual void                       SetVerbosityLevel(const uint32 level);
	virtual IServiceNetworkMessage*    AllocMessageBuffer(const uint32 size);
	virtual struct IDataWriteStream*   CreateMessageWriter();
	virtual struct IDataReadStream*    CreateMessageReader(const void* pData, const uint32 dataSize);
	virtual ServiceNetworkAddress      GetHostAddress(const string& addressString, uint16 optionalPort = 0) const;
	virtual IServiceNetworkListener*   CreateListener(uint16 localPort);
	virtual IServiceNetworkConnection* Connect(const ServiceNetworkAddress& remoteAddress);

	// IThread
	virtual void ThreadEntry();
	void         SignalStopWork();

	// Register connection in the connection list (thread safe)
	void RegisterConnection(CServiceNetworkConnection& con);

	// Register connection for closing one all of the outgoing messages are sent
	void RegisterForDeferredClose(CServiceNetworkConnection& con, const uint32 timeout);

	// Debug print
#ifdef RELEASE
	void Log(const char* txt, ...) const        {};
	bool CheckVerbose(const uint32 level) const { return false; }
#else
	void Log(const char* txt, ...) const;
	bool CheckVerbose(const uint32 level) const;
#endif
};

//-----------------------------------------------------------------------------

#endif
