// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   Description:  Service network implementation
   -------------------------------------------------------------------------
   History:
   - 10/04/2013   : Tomasz Jonarski, Created

*************************************************************************/
#include "StdAfx.h"
#include "ServiceNetwork.h"
#include "RemoteCommandHelpers.h"
#include <CryNetwork/CrySocks.h>

//-----------------------------------------------------------------------------

// network system internal logging
#ifdef RELEASE
	#define LOG_VERBOSE(level, txt, ...)
#else
	#define LOG_VERBOSE(level, txt, ...) if (GetManager()->CheckVerbose(level)) { GetManager()->Log(txt, __VA_ARGS__); }
#endif

//-----------------------------------------------------------------------------

namespace
{
union AddValueConv
{
	struct
	{
		uint8 ip0, ip1, ip2, ip3;
	}      bytes;

	uint32 u32;
};

inline void TranslateAddress(const ServiceNetworkAddress& addr, CRYSOCKADDR_IN& outAddr)
{
	AddValueConv addr_value;
	addr_value.bytes.ip0 = addr.GetAddress().m_ip0;
	addr_value.bytes.ip1 = addr.GetAddress().m_ip1;
	addr_value.bytes.ip2 = addr.GetAddress().m_ip2;
	addr_value.bytes.ip3 = addr.GetAddress().m_ip3;

	outAddr.sin_family = AF_INET;
	outAddr.sin_port = htons(addr.GetAddress().m_port);
	outAddr.sin_addr.s_addr = addr_value.u32;
}

inline void TranslateAddress(const CRYSOCKADDR_IN& addr, ServiceNetworkAddress& outAddr)
{
	AddValueConv addr_value;
	addr_value.u32 = addr.sin_addr.s_addr;
	outAddr = ServiceNetworkAddress(
	  addr_value.bytes.ip0,
	  addr_value.bytes.ip1,
	  addr_value.bytes.ip2,
	  addr_value.bytes.ip3,
	  ntohs(addr.sin_port));
}
}

//-----------------------------------------------------------------------------

CServiceNetworkMessage::CServiceNetworkMessage(const uint32 id, const uint32 size)
	: m_refCount(1)
	, m_size(size)
	, m_id(id)
{
	// Allocate buffer memory
	m_pData = CryModuleMalloc(size);
}

CServiceNetworkMessage::~CServiceNetworkMessage()
{
	// Release the memory buffer
	CryModuleFree(m_pData);
	m_pData = NULL;
}

uint32 CServiceNetworkMessage::GetSize() const
{
	return m_size;
}

uint32 CServiceNetworkMessage::GetId() const
{
	return m_id;
}

void* CServiceNetworkMessage::GetPointer()
{
	return m_pData;
}

const void* CServiceNetworkMessage::GetPointer() const
{
	return m_pData;
}

void CServiceNetworkMessage::AddRef()
{
	CryInterlockedIncrement(&m_refCount);
}

void CServiceNetworkMessage::Release()
{
	if (0 == CryInterlockedDecrement(&m_refCount))
	{
		delete this;
	}
}

IDataReadStream* CServiceNetworkMessage::CreateReader() const
{
	return new CDataReadStreamFormMessage(this);
}

//-----------------------------------------------------------------------------

void CServiceNetworkConnection::Header::Swap()
{
	// if we are on big endian system swap data to LE
	// NOTE: this is a little bit confusing so see how the eLittleEndian and eBigEndian is defined
	SwapEndian(m_size, eLittleEndian);
}

void CServiceNetworkConnection::InitHeader::Swap()
{
	// if we are on big endian system swap data to LE
	// NOTE: this is a little bit confusing so see how the eLittleEndian and eBigEndian is defined
	SwapEndian(m_tryCount, eLittleEndian);
	SwapEndian(m_guid0, eLittleEndian);
	SwapEndian(m_guid1, eLittleEndian);
}

//-----------------------------------------------------------------------------

CServiceNetworkConnection::CServiceNetworkConnection(
	class CServiceNetwork* manager,
		EEndpoint endpointType,
		CRYSOCKET socket,
		const CryGUID& connectionID,
		const ServiceNetworkAddress& localAddress,
		const ServiceNetworkAddress& remoteAddress)

	: m_pManager(manager)
	, m_connectionID(connectionID)
	, m_socket(socket)
	, m_localAddress(localAddress)
	, m_remoteAddress(remoteAddress)
	, m_endpointType(endpointType)
	, m_state(eState_Initializing)
	, m_sendQueueDataSize(0)
	, m_receiveQueueDataSize(0)
	, m_messageDataSentSoFar(0)
	, m_messageDataReceivedSoFar(0)
	, m_bCloseRequested(false)
	, m_pCurrentReceiveMessage(NULL)
	, m_messageReceiveLength(0)
	, m_messageDummyReadLength(0)
	, m_reconnectTryCount(0)
	, m_bDisableCommunication(false)
	, m_pSendedMessages(NULL)
	, m_refCount(1)
{
	// put the socket back in non blocking mode
	CrySock::MakeSocketNonBlocking(m_socket);

	// reset stats
	m_statsNumDataSend = 0;
	m_statsNumDataReceived = 0;
	m_statsNumPacketsSend = 0;
	m_statsNumPacketsReceived = 0;

	// reset timers to values at the creation time
	const uint64 currentNetworkTime = m_pManager->GetNetworkTime();
	m_lastReconnectTime = currentNetworkTime;
	m_lastMessageReceivedTime = currentNetworkTime;
	m_lastInitializationSendTime = currentNetworkTime;

	// make sure keep alive messages are sent as soon as possible
	m_lastKeepAliveSendTime = currentNetworkTime - kKeepAlivePeriod;

	LOG_VERBOSE(3, "Connection(): local='%s', remote='%s', this=%p",
	            m_localAddress.ToString().c_str(),
	            m_remoteAddress.ToString().c_str(),
	            (UINT_PTR) this);
}

CServiceNetworkConnection::~CServiceNetworkConnection()
{
	// in here we must be already closed!
	CRY_ASSERT(m_state == eState_Closed);

	LOG_VERBOSE(3, "~Connection(): local='%s', remote='%s', this=%p",
	            m_localAddress.ToString().c_str(),
	            m_remoteAddress.ToString().c_str(),
	            (UINT_PTR) this);

	// can happen
	if (m_pCurrentReceiveMessage != NULL)
	{
		m_pCurrentReceiveMessage->Release();
		m_pCurrentReceiveMessage = NULL;
	}
}

void CServiceNetworkConnection::AddRef()
{
	CryInterlockedIncrement(&m_refCount);
}

void CServiceNetworkConnection::Release()
{
	if (0 == CryInterlockedDecrement(&m_refCount))
	{
		delete this;
	}
}

void CServiceNetworkConnection::Close()
{
	LOG_VERBOSE(2, "Connection local='%s', remote='%s', this=%p: close requested",
	            m_localAddress.ToString().c_str(),
	            m_remoteAddress.ToString().c_str(),
	            (UINT_PTR) this);

	m_bCloseRequested = true;
	m_bDisableCommunication = true;
}

void CServiceNetworkConnection::FlushAndClose(const uint32 timeout)
{
	if (!m_bDisableCommunication)
	{
		// We don't have any messages on the waiting list, we can close immediately
		if (m_pSendQueue.empty())
		{
			// Normal close
			Close();
		}
		else
		{
			LOG_VERBOSE(2, "Connection local='%s', remote='%s', this=%p: flush and close requested",
			            m_localAddress.ToString().c_str(),
			            m_remoteAddress.ToString().c_str(),
			            (UINT_PTR) this);

			// Disable communication layer so no more new messages can be transmitted
			m_bDisableCommunication = true;

			// Register in the manager list of connections to close after sending queue is empty
			m_pManager->RegisterForDeferredClose(*this, timeout);
		}
	}
}

void CServiceNetworkConnection::FlushAndWait()
{
	// Disable communication layer so no more new messages can be transmitted
	m_bDisableCommunication = true;

	// Wait for the connection to be empty
	while (IsAlive() && !m_pSendQueue.empty())
	{
		CrySleep(1);
	}

	// Resume communication layer
	m_bDisableCommunication = false;
}

const CryGUID& CServiceNetworkConnection::GetGUID() const
{
	return m_connectionID;
}

const ServiceNetworkAddress& CServiceNetworkConnection::GetRemoteAddress() const
{
	return m_remoteAddress;
}

const ServiceNetworkAddress& CServiceNetworkConnection::GetLocalAddress() const
{
	return m_localAddress;
}

void CServiceNetworkConnection::Reset()
{
	if (m_state == eState_Initializing || m_state == eState_Valid)
	{
		// Close the socket (we wont be able to use it anyway)
		if (m_socket != 0)
		{
			CrySock::shutdown(m_socket, SD_BOTH);
			CrySock::closesocket(m_socket);
			m_socket = 0;
		}

		// Reset messages buffers pointers
		m_messageDataSentSoFar = 0;
		m_messageDataReceivedSoFar = 0;

		// release current in-flight message
		if (m_pCurrentReceiveMessage != NULL)
		{
			m_pCurrentReceiveMessage->Release();
			m_pCurrentReceiveMessage = NULL;
		}

		// reset reconnection timer
		m_lastMessageReceivedTime = m_pManager->GetNetworkTime();
		m_lastReconnectTime = m_pManager->GetNetworkTime();

		// we are in the lost state now, we can try to reconnect
		m_state = eState_Lost;

		LOG_VERBOSE(2, "Connection local='%s', remote='%s', this=%p: LOST!",
		            m_localAddress.ToString().c_str(),
		            m_remoteAddress.ToString().c_str(),
		            (UINT_PTR) this);
	}
}

void CServiceNetworkConnection::Shutdown()
{
	// Close the socket
	if (m_socket != 0)
	{
		CrySock::shutdown(m_socket, SD_BOTH);
		CrySock::closesocket(m_socket);
		m_socket = 0;
	}

	// Release all pending messages (they wont be sent anyway)
	while (!m_pSendQueue.empty())
	{
		CServiceNetworkMessage* message = m_pSendQueue.pop();
		message->Release();
	}

	// release current in-flight message
	if (m_pCurrentReceiveMessage != NULL)
	{
		m_pCurrentReceiveMessage->Release();
		m_pCurrentReceiveMessage = NULL;
	}

	// Reset internal send/recv state
	m_messageDataSentSoFar = 0;
	m_messageDataReceivedSoFar = 0;

	// Force the state
	m_state = eState_Closed;

	LOG_VERBOSE(2, "Connection local='%s', remote='%s', this=%p: CLOSED!",
	            m_localAddress.ToString().c_str(),
	            m_remoteAddress.ToString().c_str(),
	            (UINT_PTR) this);
}

void CServiceNetworkConnection::Update()
{
	const uint64 currentNetworkTime = m_pManager->GetNetworkTime();

	// We requested to close the socket
	if (m_bCloseRequested)
	{
		m_bCloseRequested = false;
		Shutdown();
		return;
	}

	// State machine
	switch (m_state)
	{
	// Connection is closed, nothing to do
	case eState_Closed:
		{
			break;
		}

	// We are still not initialized fully
	case eState_Initializing:
		{
			// receive messages when waiting for connection
			ProcessReceivingQueue();

			// if we are a client send the connection initialization messages
			if (m_endpointType == eEndpoint_Client)
			{
				// General timeout handling
				if (HandleTimeout(currentNetworkTime))
				{
					// do not send to often
					if ((currentNetworkTime - m_lastInitializationSendTime) > kInitializationPerior)
					{
						// send the initialization message
						if (TryInitialize())
						{
							// message was sent, wait a moment before sending next one
							m_lastInitializationSendTime = currentNetworkTime;
						}
					}
				}
			}
			else if (m_endpointType == eEndpoint_Server)
			{
				// Server side when waiting for full initialization is sending the "keep alive" messages
				// Note that we cannot time out on this end
				ProcessKeepAlive();
			}

			break;
		}

	// Connection is lost
	case eState_Lost:
		{
			// if we are the client endpoint we can try to reconnect to the server
			if (m_endpointType == eEndpoint_Client)
			{
				// do not try to reconnect to often (floods the network)
				if ((currentNetworkTime - m_lastReconnectTime) > kReconnectTryPerior)
				{
					// reset timer
					m_lastReconnectTime = currentNetworkTime;

					// try to reconnect
					if (TryReconnect())
					{
						// put the socket back in non blocking mode
						CrySock::MakeSocketNonBlocking(m_socket);

						// give us some slack with timeout after reconnection
						m_lastMessageReceivedTime = currentNetworkTime;

						// yeah, we got reconnected, try to reinitialize the connection
						m_messageDataReceivedSoFar = 0;
						m_state = eState_Initializing;
					}
				}
			}
			else if (m_endpointType == eEndpoint_Server)
			{
				// wait for the reconnection timeout
				if ((currentNetworkTime - m_lastMessageReceivedTime) > hReconnectTimeOut)
				{
					// reconnection time out has occurred
					LOG_VERBOSE(2, "Connection local='%s', remote='%s', this=%p: reconnection timeout",
					            m_localAddress.ToString().c_str(),
					            m_remoteAddress.ToString().c_str(),
					            (UINT_PTR) this);

					// Close
					Shutdown();
				}
			}

			break;
		}

	// Valid connection
	case eState_Valid:
		{
			// Is it time to send a keep alive message?
			ProcessKeepAlive();

			// Process the queue of messages
			ProcessSendingQueue();
			ProcessReceivingQueue();

			// General timeout handling
			if (m_endpointType == eEndpoint_Client)
			{
				HandleTimeout(currentNetworkTime);
			}

			break;
		}

	default:
		{
			// should not happen
			break;
		}
	}
}

bool CServiceNetworkConnection::HandleTimeout(const uint64 currentNetworkTime)
{
	// Connections never time out when there is a debugger attached
#if CRY_PLATFORM_WINDOWS
	if (IsDebuggerPresent())
	{
		// connection is still alive
		return true;
	}
#endif

	// Connection time out when there is a long time without any activity from server side (no keep alive or other messages)
	const uint64 timeSinceLastMessage = currentNetworkTime - m_lastMessageReceivedTime;
	if (timeSinceLastMessage > kTimeout)
	{
		// Connection has timed out
		LOG_VERBOSE(1, "Connection local='%s', remote='%s', this=%p: timed out",
		            m_localAddress.ToString().c_str(),
		            m_remoteAddress.ToString().c_str(),
		            (UINT_PTR) this);

		// Put in lost state, wait a while before reconnecting
		m_lastReconnectTime = currentNetworkTime;
		m_state = eState_Lost;

		// Close the socket now
		CrySock::closesocket(m_socket);
		m_socket = 0;

		// Connection was lost
		return false;
	}

	// Connection still alive
	return true;
}

bool CServiceNetworkConnection::TryInitialize()
{
	// This is sent only be clients trying to establish connection with server
	// Connection ID is sent over to the server so he can easily identify re connections (even when port changes)

	// Initialization data header
	InitHeader header;
	header.m_cmd = eCommand_Initialize;
	header.m_pad0 = 0;
	header.m_pad1 = 0;
	header.m_pad2 = 0;
	header.m_tryCount = m_reconnectTryCount;
	header.m_guid0 = m_connectionID.lopart;
	header.m_guid1 = m_connectionID.hipart;

	// Swap the endianess in header (for sending)
	header.Swap();

	// Try send
	const bool autoHandleErrors = false; // we do not need errors here
	const uint32 dataLeft = sizeof(header) - m_messageDataSentSoFar;
	const uint32 ret = TrySend(&header, dataLeft, autoHandleErrors);
	m_messageDataSentSoFar += ret;

	// Full packet was sent
	if (m_messageDataSentSoFar == sizeof(header))
	{
		// We sent the initialization message
		LOG_VERBOSE(1, "Connection local='%s', remote='%s', this=%p: init message sent, try counter=%d",
		            m_localAddress.ToString().c_str(),
		            m_remoteAddress.ToString().c_str(),
		            (UINT_PTR) this,
		            m_reconnectTryCount);

		// we sent the initialization packet, reset
		m_messageDataSentSoFar = 0;
		return true;
	}

	// Still not valid
	return false;
}

bool CServiceNetworkConnection::TryReconnect()
{
	// We can't reconnect with disabled communication
	if (m_bDisableCommunication)
	{
		return false;
	}

	// Create new socket if needed
	if (m_socket == 0)
	{
		m_socket = CrySock::socketinet();
		if (m_socket == CRY_INVALID_SOCKET)
		{
			// We sent the initialization message
			LOG_VERBOSE(1, "Connection local='%s', remote='%s', this=%p: failed to recreate socket",
			            m_localAddress.ToString().c_str(),
			            m_remoteAddress.ToString().c_str(),
			            (UINT_PTR) this);
			m_socket = 0;
			return false;
		}
	}

	// Translate remote address
	CRYSOCKADDR_IN addr;
	TranslateAddress(m_remoteAddress, addr);

	// When reconnecting always use the blocking mode
	CrySock::MakeSocketBlocking(m_socket);

	// every time we reconnect increment the internal counter so the receiving end (server)
	// will be able to identity the up-to-date connection (and discard the older one)
	CRY_ASSERT(m_endpointType == eEndpoint_Client);
	m_reconnectTryCount += 1;

	// Connect (blocking)
	const int result = CrySock::connect(m_socket, (const CRYSOCKADDR*)&addr, sizeof(addr));
	if (result == 0)
	{
		// Spew to log (important info)
		LOG_VERBOSE(0, "Connection local='%s', remote='%s', this=%p: SUCCESSFULLY RECONNECTED",
		            m_localAddress.ToString().c_str(),
		            m_remoteAddress.ToString().c_str(),
		            (UINT_PTR) this);

		// When connected put socket in the non blocking mode
		CrySock::MakeSocketNonBlocking(m_socket);

		// connected!
		return true;
	}

	// not connection, should not happen often
	LOG_VERBOSE(2, "Connection local='%s', remote='%s', this=%p: failed to reconnect",
	            m_localAddress.ToString().c_str(),
	            m_remoteAddress.ToString().c_str(),
	            (UINT_PTR) this);

	// still not connected
	return false;
}

void CServiceNetworkConnection::SendKeepAlive(const uint64 currentNetworkTime)
{
	// Keep alive message is just ONE byte (makes it easier)
	uint8 message = eCommand_KeepAlive;
	if (1 == TrySend(&message, 1, false))
	{
		// Throttle the sending
		m_lastKeepAliveSendTime = currentNetworkTime;

		// At high verbose level we need even this :)
		LOG_VERBOSE(3, "Connection local='%s', remote='%s', this=%p: keep alive SENT",
		            m_localAddress.ToString().c_str(),
		            m_remoteAddress.ToString().c_str(),
		            (UINT_PTR) this);
	}
}

void CServiceNetworkConnection::ProcessSendingQueue()
{
	// Get the top message from the send queue
	if (NULL == m_pSendedMessages)
	{
		m_pSendedMessages = m_pSendQueue.pop();
		if (NULL == m_pSendedMessages)
		{
			return;
		}
	}

	// Get the size of the data to transmit
	const uint32 messageSize = m_pSendedMessages->GetSize();
	const uint32 headerSize = sizeof(Header);

	// Nothing sent yet, send header
	if (m_messageDataSentSoFar < headerSize)
	{
		// prepare header - endian safe
		Header header;
		header.m_cmd = eCommand_Data;
		header.m_size = messageSize;

		// Swap the header for reading
		header.Swap();

		// send the header
		const uint32 dataLeft = headerSize - m_messageDataSentSoFar;
		const uint32 sent = TrySend((const char*)&header + m_messageDataSentSoFar, dataLeft, true);
		m_messageDataSentSoFar += sent;
	}

	// Send message data
	const uint32 endOfDataOffset = messageSize + headerSize;
	if (m_messageDataSentSoFar >= headerSize && m_messageDataSentSoFar < endOfDataOffset)
	{
		// send and advance
		const uint32 dataLeft = endOfDataOffset - m_messageDataSentSoFar;
		const uint32 dataOffset = m_messageDataSentSoFar - headerSize;
		const uint32 sent = TrySend((const char*)m_pSendedMessages->GetPointer() + dataOffset, dataLeft, true);
		m_messageDataSentSoFar += sent;
	}

	// All the data from the message was sent, release the message from this queue
	// Note: this message may still be in some other sent queues for connections
	if (m_messageDataSentSoFar >= endOfDataOffset)
	{
		// At high verbose level we need even this :)
		LOG_VERBOSE(3, "Connection local='%s', remote='%s', this=%p: message ID %d (size=%d) removed from queue",
		            m_localAddress.ToString().c_str(),
		            m_remoteAddress.ToString().c_str(),
		            (UINT_PTR) this,
		            m_pSendedMessages->GetId(),
		            m_pSendedMessages->GetSize());

		CryInterlockedAdd(&m_sendQueueDataSize, -(int)m_pSendedMessages->GetSize());

		// stats (not collected in release builds)
#ifndef RELEASE
		CryInterlockedIncrement((volatile int*) &m_statsNumPacketsSend);
#endif

		// release local message reference
		m_pSendedMessages->Release();
		m_pSendedMessages = NULL;

		// rewind to zero to indicate fresh message
		m_messageDataSentSoFar = 0;
	}
}

void CServiceNetworkConnection::ProcessKeepAlive()
{
	const uint64 currentNetworkTime = m_pManager->GetNetworkTime();
	if ((currentNetworkTime - m_lastKeepAliveSendTime) > kKeepAlivePeriod)
	{
		// Well, if we are in the middle of something make sure we do not interrupt it with KeepAlive
		if (m_messageDataSentSoFar == 0)
		{
			SendKeepAlive(currentNetworkTime);
		}
	}
}

void CServiceNetworkConnection::ProcessReceivingQueue()
{
	// To much data already, do not process
	const uint32 kReceivedDataLimit = m_pManager->GetReceivedDataQueueLimit();
	if (m_receiveQueueDataSize > kReceivedDataLimit)
	{
		return;
	}

	// Internal offset
	const uint32 kOffsetHeader = 1;
	const uint32 kOffsetData = 5;

	// Dummy receive
	while (m_messageDummyReadLength > 0)
	{
		// batch size
		const uint32 kTempBufferSize = 256;

		// read dummy data
		uint8 tempBuffer[kTempBufferSize];
		const uint32 maxRead = min(kTempBufferSize, m_messageDummyReadLength);
		const uint32 readCount = TryReceive(tempBuffer, maxRead, false);
		m_messageDummyReadLength -= readCount;

		// got less
		if (readCount < maxRead)
		{
			break;
		}
	}

	// Do not process normal messages until we receive all of the bogus data
	if (m_messageDummyReadLength > 0)
	{
		return;
	}

	// First byte - header
	if (m_messageDataReceivedSoFar == 0)
	{
		// message header, read type
		uint8 messageType = 0;
		const int result = TryReceive(&messageType, 1, true);
		if (result == 1)
		{
			// keep alive received, if we are not yet fully initialized it's the signal that we are :)
			if (messageType == eCommand_KeepAlive)
			{
				// we got confirmed by server
				if (m_state == eState_Initializing)
				{
					// change state
					CRY_ASSERT(m_endpointType == eEndpoint_Client);
					m_state = eState_Valid;

					// At high verbose level we need even this :)
					LOG_VERBOSE(1, "Connection local='%s', remote='%s', this=%p: connection confirmed by server",
					            m_localAddress.ToString().c_str(),
					            m_remoteAddress.ToString().c_str(),
					            (UINT_PTR) this);
				}
				else
				{
					// At low-level verbose log even this
					LOG_VERBOSE(3, "Connection local='%s', remote='%s', this=%p: keep alive RECEIVED",
					            m_localAddress.ToString().c_str(),
					            m_remoteAddress.ToString().c_str(),
					            (UINT_PTR) this);
				}

				// update the keep alive data timer
				m_lastKeepAliveSendTime = m_pManager->GetNetworkTime();
				m_lastMessageReceivedTime = m_pManager->GetNetworkTime();
			}
			else if (messageType == eCommand_Data)
			{
				// wait for the message length
				m_messageReceiveLength = 0;
				m_messageDataReceivedSoFar = kOffsetHeader;
				m_lastMessageReceivedTime = m_pManager->GetNetworkTime();

				// At low-level verbose log even this
				LOG_VERBOSE(3, "Connection local='%s', remote='%s', this=%p: got data message header",
				            m_localAddress.ToString().c_str(),
				            m_remoteAddress.ToString().c_str(),
				            (UINT_PTR) this);

			}
			else if (messageType == eCommand_Initialize)
			{
				// let the system process the message
				m_messageDummyReadLength = sizeof(InitHeader) - 1;

				// At low-level verbose log even this
				LOG_VERBOSE(3, "Connection local='%s', remote='%s', this=%p: outdated initheader received",
				            m_localAddress.ToString().c_str(),
				            m_remoteAddress.ToString().c_str(),
				            (UINT_PTR) this);
			}
			else
			{
				// Serious error
				LOG_VERBOSE(0, "Connection local='%s', remote='%s', this=%p: received invalid command (%d)",
				            m_localAddress.ToString().c_str(),
				            m_remoteAddress.ToString().c_str(),
				            (UINT_PTR) this,
				            messageType);

				// reset the connection
				Reset();
			}
		}
	}

	// Message length
	if (m_messageDataReceivedSoFar >= kOffsetHeader && m_messageDataReceivedSoFar < kOffsetData)
	{
		// receive the message length
		const uint32 dataOffset = m_messageDataReceivedSoFar - kOffsetHeader;
		const uint32 dataLeft = sizeof(uint32) - dataOffset;
		const uint32 len = TryReceive((char*)&m_messageReceiveLength + dataOffset, dataLeft, true);
		m_messageDataReceivedSoFar += len;

		// Update last message time
		if (len > 0)
		{
			m_lastMessageReceivedTime = m_pManager->GetNetworkTime();
		}

		// full length received
		if (m_messageDataReceivedSoFar == 5)
		{
			// Swap endianess (for BE platforms)
			// NOTE: if this is a little bit confusing, see how the eLittleEndian and eBigEndian are defined
			SwapEndian(m_messageReceiveLength, eLittleEndian);

			// Sanity check on the message size
			if (m_messageReceiveLength > kMaximumMessageSize)
			{
				// Serious error
				LOG_VERBOSE(0, "Connection local='%s', remote='%s', this=%p: unsupported message size (%d)",
				            m_localAddress.ToString().c_str(),
				            m_remoteAddress.ToString().c_str(),
				            (UINT_PTR) this,
				            m_messageReceiveLength);

				Reset();
			}
			else if (m_messageReceiveLength > 0)
			{
				// Create new message that we will add the data into
				CRY_ASSERT(m_pCurrentReceiveMessage == NULL);
				m_pCurrentReceiveMessage = static_cast<CServiceNetworkMessage*>(m_pManager->AllocMessageBuffer(m_messageReceiveLength));
				CRY_ASSERT(m_pCurrentReceiveMessage != NULL);

				// Serious error
				LOG_VERBOSE(2, "Connection local='%s', remote='%s', this=%p: created receive buffer ID %d, (size=%d)",
				            m_localAddress.ToString().c_str(),
				            m_remoteAddress.ToString().c_str(),
				            (UINT_PTR) this,
				            m_pCurrentReceiveMessage ? m_pCurrentReceiveMessage->GetId() : 0,
				            m_messageReceiveLength);
			}
		}
	}

	// Message data
	if (m_messageDataReceivedSoFar >= kOffsetData)
	{
		CRY_ASSERT(m_pCurrentReceiveMessage != NULL);
		PREFAST_ASSUME(m_pCurrentReceiveMessage);

		// Message fully received, put in the receive queue (at the end!)
		const uint32 dataOffset = m_messageDataReceivedSoFar - kOffsetData;
		const uint32 dataLeft = m_pCurrentReceiveMessage->GetSize() - dataOffset;
		const uint32 len = TryReceive((char*)m_pCurrentReceiveMessage->GetPointer() + dataOffset, dataLeft, true);
		m_messageDataReceivedSoFar += len;

		// connection got lost
		if (m_state == eState_Lost)
		{
			return;
		}

		// Update last message time
		if (len > 0)
		{
			m_lastMessageReceivedTime = m_pManager->GetNetworkTime();
		}

		// Full message received!
		if (m_messageDataReceivedSoFar == (kOffsetData + m_pCurrentReceiveMessage->GetSize()))
		{
			// Serious error
			LOG_VERBOSE(2, "Connection local='%s', remote='%s', this=%p: full message received(%d), adding to queue",
			            m_localAddress.ToString().c_str(),
			            m_remoteAddress.ToString().c_str(),
			            (UINT_PTR) this,
			            m_pCurrentReceiveMessage->GetSize());

			// Put in the receive queue, only if no communication is disabled
			if (m_bDisableCommunication)
			{
				m_pCurrentReceiveMessage->Release();
			}
			else
			{
				m_pReceiveQueue.push(m_pCurrentReceiveMessage);
			}

			// Stats (not collected in release builds)
#ifndef RELEASE
			CryInterlockedIncrement((volatile int*) &m_statsNumPacketsReceived);
#endif

			// Reset
			m_pCurrentReceiveMessage = NULL;
			m_messageDataReceivedSoFar = 0;
		}
	}
}

uint32 CServiceNetworkConnection::TrySend(const void* dataBuffer, uint32 dataSize, bool autoHandleErrors /*=true*/)
{
	// Send the data
	const int ret = CrySock::send(m_socket, (const char*) dataBuffer, dataSize, 0);
	const CrySock::eCrySockError errCode = CrySock::TranslateSocketError(ret);
	if (errCode != CrySock::eCSE_NO_ERROR)
	{
		// We would block, that's not an error
		if (errCode == CrySock::eCSE_EWOULDBLOCK)
		{
			return 0;
		}

		// Report connection problems
		LOG_VERBOSE(1, "Connection local='%s', remote='%s', this=%p: send() error: %d",
		            m_localAddress.ToString().c_str(),
		            m_remoteAddress.ToString().c_str(),
		            (UINT_PTR) this,
		            ret);

		// Put connection in the lost state
		if (autoHandleErrors)
		{
			Reset();
		}

		// Nothing was sent (according to our logic)
		return 0;
	}

	// Update stats
#ifndef RELEASE
	CryInterlockedAdd((volatile int*) &m_statsNumDataSend, ret);
#endif

	// Return the true amount of data sent
	return ret;
}

uint32 CServiceNetworkConnection::TryReceive(void* dataBuffer, uint32 dataSize, bool autoHandleErrors)
{
	// Send the data
	const int ret = CrySock::recv(m_socket, (char*) dataBuffer, dataSize, 0);
	const CrySock::eCrySockError errCode = CrySock::TranslateSocketError(ret);
	if (errCode != CrySock::eCSE_NO_ERROR)
	{
		// We would block, that's not an error
		if (errCode == CrySock::eCSE_EWOULDBLOCK)
		{
			return 0;
		}

		// Connection was closed
		if (errCode == CrySock::eCSE_ECONNRESET)
		{
			// Report connection problems
			LOG_VERBOSE(1, "Connection local='%s', remote='%s', this=%p: CLOSED BY PEER",
			            m_localAddress.ToString().c_str(),
			            m_remoteAddress.ToString().c_str(),
			            (UINT_PTR) this);

			// Shutdown socket
			Shutdown();
			return 0;
		}

		// Report connection problems
		LOG_VERBOSE(1, "Connection local='%s', remote='%s', this=%p: recv() error: %d",
		            m_localAddress.ToString().c_str(),
		            m_remoteAddress.ToString().c_str(),
		            (UINT_PTR) this,
		            ret);

		// Put connection in the lost state
		if (autoHandleErrors)
		{
			Reset();
		}

		// Nothing was sent (according to our logic)
		return 0;
	}

	// Update stats
#ifndef RELEASE
	CryInterlockedAdd((volatile int*) &m_statsNumDataReceived, ret);
#endif

	// Return the true amount of data sent
	return ret;
}

bool CServiceNetworkConnection::SendMsg(IServiceNetworkMessage* message)
{
	// Invalid message
	if (NULL == message || message->GetSize() == 0)
	{
		return false;
	}

	// Communication layer is disabled, not possible to send any more messages
	if (m_bDisableCommunication)
	{
		return false;
	}

	// Process data size limits
	{
		const uint32 sizeAfterThisMessage = m_sendQueueDataSize + message->GetSize();
		const uint32 sendQueueLimit = m_pManager->GetSendDataQueueLimit();
		if (sizeAfterThisMessage > sendQueueLimit)
		{
			// Report connection problems
			LOG_VERBOSE(0, "Connection local='%s', remote='%s', this=%p: to much data on send queue",
			            m_localAddress.ToString().c_str(),
			            m_remoteAddress.ToString().c_str(),
			            (UINT_PTR) this);

			// To much data on the queue already, we will not be sending this message
			return false;
		}

		// Keep the local reference so the source data does not get deleted
		message->AddRef();

		// Update the queue data size
		CryInterlockedAdd(&m_sendQueueDataSize, message->GetSize());

		// Append the message to the sending queue
		m_pSendQueue.push(static_cast<CServiceNetworkMessage*>(message));

	}

	// Well, we can't tell any more than that
	return true;
}

IServiceNetworkMessage* CServiceNetworkConnection::ReceiveMsg()
{
	// Anything on the queue ?
	IServiceNetworkMessage* message = NULL;
	if (!m_pReceiveQueue.empty())
	{
		message = m_pReceiveQueue.pop();

		// Report connection problems
		LOG_VERBOSE(3, "Connection local='%s', remote='%s', this=%p: message ID %d (size=%d) popped by receive end",
		            m_localAddress.ToString().c_str(),
		            m_remoteAddress.ToString().c_str(),
		            (UINT_PTR) this,
		            message->GetId(),
		            message->GetSize());
	}

	return message;
}

bool CServiceNetworkConnection::IsAlive() const
{
	return m_state != eState_Closed;
}

uint32 CServiceNetworkConnection::GetMessageSendCount() const
{
	return m_statsNumPacketsSend;
}

uint32 CServiceNetworkConnection::GetMessageReceivedCount() const
{
	return m_statsNumPacketsReceived;
}

uint64 CServiceNetworkConnection::GetMessageSendDataSize() const
{
	return m_statsNumDataSend;
}

uint64 CServiceNetworkConnection::GetMessageReceivedDataSize() const
{
	return m_statsNumDataReceived;
}

bool CServiceNetworkConnection::HandleReconnect(CRYSOCKET socket, const uint32 tryCount)
{
	CRY_ASSERT(m_endpointType == eEndpoint_Server);

	// connection is older
	if (tryCount < m_reconnectTryCount)
	{
		LOG_VERBOSE(3, "Connection local='%s', remote='%s', this=%p: reconnection request OLDER (%d<%d)",
		            m_localAddress.ToString().c_str(),
		            m_remoteAddress.ToString().c_str(),
		            (UINT_PTR) this,
		            tryCount,
		            m_reconnectTryCount);

		return false;
	}

	// should not happen
	if (tryCount == m_reconnectTryCount)
	{
		LOG_VERBOSE(0, "Connection local='%s', remote='%s', this=%p: reconnection request COLLISION (%d==%d)",
		            m_localAddress.ToString().c_str(),
		            m_remoteAddress.ToString().c_str(),
		            (UINT_PTR) this,
		            tryCount,
		            m_reconnectTryCount);

		return false;
	}

	// newer connection, close current socket
	CrySock::shutdown(m_socket, SD_BOTH);
	CrySock::closesocket(m_socket);

	// reset send/receive counters (will resend last message from the queue)
	m_messageDataReceivedSoFar = 0;
	m_messageDataSentSoFar = 0;

	// Set new socket and update reconnection counter
	m_socket = socket;
	m_reconnectTryCount = tryCount;

	// revive the connection
	m_state = eState_Valid;

	LOG_VERBOSE(0, "Connection local='%s', remote='%s', this=%p: successfull reconnection with counter (%d)",
	            m_localAddress.ToString().c_str(),
	            m_remoteAddress.ToString().c_str(),
	            (UINT_PTR) this,
	            m_reconnectTryCount);

	// processed
	return true;
}

//-----------------------------------------------------------------------------

CServiceNetworkListener::CServiceNetworkListener(CServiceNetwork* pManager, CRYSOCKET socket, const ServiceNetworkAddress& address)
	: m_pManager(pManager)
	, m_socket(socket)
	, m_localAddress(address)
	, m_closeRequestReceived(false)
	, m_refCount(1)
{
	LOG_VERBOSE(3, "Listener() local='%s', this=%p",
	            m_localAddress.ToString().c_str(),
	            (UINT_PTR) this);
}

CServiceNetworkListener::~CServiceNetworkListener()
{
	CRY_ASSERT(m_socket == 0);

	LOG_VERBOSE(3, "~Listener() local='%s', this=%p",
	            m_localAddress.ToString().c_str(),
	            (UINT_PTR) this);
}

void CServiceNetworkListener::Update()
{
	// We requested to close this listener
	if (m_closeRequestReceived)
	{
		LOG_VERBOSE(3, "Listener local='%s', this=%p: closing due to request",
		            m_localAddress.ToString().c_str(),
		            (UINT_PTR) this);

		// Close the socket
		if (0 != m_socket)
		{
			CrySock::closesocket(m_socket);
			m_socket = 0;
		}

		// Close all connections
		for (TConnectionList::iterator it = m_pLocalConnections.begin();
		     it != m_pLocalConnections.end(); ++it)
		{
			(*it)->Close();
			(*it)->Release();
		}

		m_pLocalConnections.clear();
		m_closeRequestReceived = false;

		return;
	}

	// Process connection requests
	ProcessIncomingConnections();

	// Service the pending connection
	ProcessPendingConnections();

	// Remove any local connection that got dead
	for (TConnectionList::iterator it = m_pLocalConnections.begin();
	     it != m_pLocalConnections.end(); /*++it*/)
	{
		if (!(*it)->IsAlive())
		{
			LOG_VERBOSE(2, "Listener local='%s', this=%p: removing dead connection '%s' (%p)",
			            m_localAddress.ToString().c_str(),
			            (UINT_PTR) this,
			            (*it)->GetRemoteAddress().ToString().c_str(),
			            (UINT_PTR) (*it));

			(*it)->Release();
			it = m_pLocalConnections.erase(it);
		}
		else
		{
			++it;
		}
	}
}

const ServiceNetworkAddress& CServiceNetworkListener::GetLocalAddress() const
{
	return m_localAddress;
}

uint32 CServiceNetworkListener::GetConnectionCount() const
{
	// not safe ?
	return m_pLocalConnections.size();
}

IServiceNetworkConnection* CServiceNetworkListener::Accept()
{
	// Look for any connection on the list that is in the "initialized" state
	{
		CryAutoLock<CryMutex> lock(m_accessLock);
		for (TConnectionList::iterator it = m_pLocalConnections.begin();
		     it != m_pLocalConnections.end(); ++it)
		{
			// we are looking for connections in the "eState_Initializing" which mean that they are valid but not yet recognized by the outside world
			CServiceNetworkConnection* con = (*it);
			if (con->m_state == CServiceNetworkConnection::eState_Initializing)
			{
				LOG_VERBOSE(1, "Listener local='%s', this=%p: accepting connection from '%s' (%p)",
				            m_localAddress.ToString().c_str(),
				            (UINT_PTR) this,
				            con->GetRemoteAddress().ToString().c_str(),
				            (UINT_PTR) con);

				// switch state to "valid"
				con->m_state = CServiceNetworkConnection::eState_Valid;

				// when returning outside increment the ref count (we still want to keep our internal reference)
				con->AddRef();
				return con;
			}
		}
	}

	// No pending connections
	return NULL;
}

bool CServiceNetworkListener::IsAlive() const
{
	return (m_socket != 0);
}

void CServiceNetworkListener::AddRef()
{
	CryInterlockedIncrement(&m_refCount);
}

void CServiceNetworkListener::Release()
{
	if (0 == CryInterlockedDecrement(&m_refCount))
	{
		delete this;
	}
}

void CServiceNetworkListener::Close()
{
	LOG_VERBOSE(2, "Listener local='%s', this=%p: close requested",
	            m_localAddress.ToString().c_str(),
	            (UINT_PTR) this);

	m_closeRequestReceived = true;
}

void CServiceNetworkListener::ProcessIncomingConnections()
{
	// Accept all possible connections as soon as possible (the connect side is blocking)
	for (;; )
	{
		// Get the pending connection from TCP/IP layers
		CRYSOCKADDR_IN remoteAddrInet;
		CRYSOCKLEN_T len = sizeof(remoteAddrInet);
		CRYSOCKET sock = CrySock::accept(m_socket, (CRYSOCKADDR*) &remoteAddrInet, &len);

		// No more connections
		CrySock::eCrySockError sockErr = CrySock::TranslateInvalidSocket(sock);
		if (sockErr == CrySock::eCSE_NO_ERROR || sockErr == CrySock::eCSE_EWOULDBLOCK || sockErr == CrySock::eCSE_EWOULDBLOCK_CONN)
		{
			break;
		}

		// Different error
		if (sock < 0)
		{
			// Connection has other problems
			LOG_VERBOSE(1, "Listener local='%s', this=%p: accept() error: %d",
			            m_localAddress.ToString().c_str(),
			            (UINT_PTR) this,
			            (int)sock);

			break;
		}

		// Get the remote address
		ServiceNetworkAddress remoteAddress;
		TranslateAddress(remoteAddrInet, remoteAddress);

		// Any way create a new pending connection information
		PendingConnection* pendingConnection = new PendingConnection;
		pendingConnection->m_remoteAddress = remoteAddress;
		pendingConnection->m_dataReceivedSoFar = 0;
		pendingConnection->m_socket = sock;

		// Connection has other problems
		LOG_VERBOSE(2, "Listener local='%s', this=%p: new pending connection from '%s'",
		            m_localAddress.ToString().c_str(),
		            (UINT_PTR) this,
		            remoteAddress.ToString().c_str());

		// Add to list (not locked because its used only from net thread)
		m_pPendingConnections.push_back(pendingConnection);
	}
}

void CServiceNetworkListener::ProcessPendingConnections()
{
	// Process only pending connections, not locked because its used only from net thread
	for (TPendingConnectionList::iterator it = m_pPendingConnections.begin();
	     it != m_pPendingConnections.end(); /*++it*/)
	{
		PendingConnection& con = *(*it);

		// Read the incoming data
		CRY_ASSERT(con.m_dataReceivedSoFar < sizeof(con.m_initHeader));
		const uint32 dataLeft = sizeof(con.m_initHeader) - con.m_dataReceivedSoFar;
		const int size = CrySock::recv(con.m_socket, (char*)&con.m_initHeader + con.m_dataReceivedSoFar, dataLeft, 0);

		// The only no-action case: no data yet
		if (CrySock::TranslateSocketError(size) == CrySock::eCSE_EWOULDBLOCK)
		{
			++it;
			continue;
		}

		// Something more important
		if (size < 0)
		{
			// well, some problem on the way, remove the pending connection from the list (client will resend)
			LOG_VERBOSE(1, "Listener local='%s', this=%p: pending connection from '%s' lost: %d",
			            m_localAddress.ToString().c_str(),
			            (UINT_PTR) this,
			            con.m_remoteAddress.ToString().c_str(),
			            (int)size);
		}
		else
		{
			// data was received
			CRY_ASSERT(size < (int)dataLeft);
			con.m_dataReceivedSoFar += size;

			// still not enough data
			if (con.m_dataReceivedSoFar < sizeof(con.m_initHeader))
			{
				++it;
				continue;
			}

			// validate header
			if (con.m_initHeader.m_cmd == CServiceNetworkConnection::eCommand_Initialize)
			{
				// Swap the header after reading
				con.m_initHeader.Swap();

				// Extract the connection ID
				const CryGUID connectionId = CryGUID::Construct(con.m_initHeader.m_guid0, con.m_initHeader.m_guid1);

				// try find existing connection with the same connection GUID (reconnection)
				CServiceNetworkConnection* existingConnection = NULL;
				for (TConnectionList::const_iterator jt = m_pLocalConnections.begin();
				     jt != m_pLocalConnections.end(); ++jt)
				{
					if ((*jt)->GetGUID() == connectionId)
					{
						existingConnection = *jt;
						break;
					}
				}

				// if existing connection was found that we probably were trying to reconnect
				if (existingConnection != NULL)
				{
					// we already have this connection on our list and we got reconnected with the some GUID
					// this usually means that the client has lost communication with server for some time
					LOG_VERBOSE(1, "Listener local='%s', this=%p: reconnection from '%s'",
					            m_localAddress.ToString().c_str(),
					            (UINT_PTR) this,
					            con.m_remoteAddress.ToString().c_str());

					// substitute the connection with newer one (if it is really newer)
					if (!existingConnection->HandleReconnect(con.m_socket, con.m_initHeader.m_tryCount))
					{
						// well, we didn't use this connect (it was older than the current one, close it)
						CrySock::shutdown(con.m_socket, SD_BOTH);
						CrySock::closesocket(con.m_socket);
					}
					else
					{
						// add to global list of active debug connections (so we can start receiving data)
						m_pManager->RegisterConnection(*existingConnection);
					}
				}
				else
				{
					// no previous connection registered, create one now
					CServiceNetworkConnection* newConnection = new CServiceNetworkConnection(
					  m_pManager,
					  CServiceNetworkConnection::eEndpoint_Server, // this connection is created from listener side which is considered the "server"
					  con.m_socket,
					  connectionId,
					  m_localAddress,
					  con.m_remoteAddress);

					// this is the first time we see this connection on set the proper connection counter
					newConnection->m_reconnectTryCount = con.m_initHeader.m_tryCount;

					// happy moment, log it
					LOG_VERBOSE(0, "Listener local='%s', this=%p: confirmed connection from '%s'",
					            m_localAddress.ToString().c_str(),
					            (UINT_PTR) this,
					            con.m_remoteAddress.ToString().c_str());

					// make sure connection is in valid state
					CRY_ASSERT(newConnection->m_state == CServiceNetworkConnection::eState_Initializing);

					// add to local list
					CRY_ASSERT(newConnection->IsInitialized() == false);
					{
						CryAutoLock<CryMutex> lock(m_accessLock);
						m_pLocalConnections.push_back(newConnection);
						newConnection->AddRef();
					}

					// add to global list of active debug connections (so we can start receiving data)
					m_pManager->RegisterConnection(*newConnection);
				}
			}
			else
			{
				// well, some problem on the way, remove the pending connection from the list (client will resend)
				LOG_VERBOSE(0, "Listener local='%s', this=%p: invalid connection data received from '%s'",
				            m_localAddress.ToString().c_str(),
				            (UINT_PTR) this,
				            con.m_remoteAddress.ToString().c_str());

				// close the socket
				CrySock::shutdown(con.m_socket, SD_BOTH);
				CrySock::closesocket(con.m_socket);
			}
		}

		// any way, delete the connection from the pending list
		delete (*it);
		it = m_pPendingConnections.erase(it);
	}
}

//-----------------------------------------------------------------------------

CServiceNetwork::CServiceNetwork()
	: m_networkTime(0)
	, m_bExitRequested(false)
	, m_bufferID(1)
{
	// Create the CVAR
	m_pVerboseLevel = REGISTER_INT("net_debugVerboseLevel", 0, VF_DEV_ONLY, "");

	// Send/receive Queue size limits
	m_pReceiveDataQueueLimit = REGISTER_INT("net_receiveQueueSize", 20 << 20, VF_DEV_ONLY, "");
	m_pSendDataQueueLimit = REGISTER_INT("net_sendQueueSize", 5 << 20, VF_DEV_ONLY, "");

	// Reinitialize the random number generator with independent seed value
	m_guidGenerator.Seed((uint32)GetNetworkTime());

	// Start thread
	if (!gEnv->pThreadManager->SpawnThread(this, "ServiceNetwork"))
	{
		CryFatalError("Error spawning \"ServiceNetwork\" thread.");
	}
}

CServiceNetwork::~CServiceNetwork()
{
	// Signal the network thread to stop
	SignalStopWork();
	gEnv->pThreadManager->JoinThread(this, eJM_Join);

	// Release all closing connections
	for (TConnectionsToCloseArray::const_iterator it = m_connectionsToClose.begin();
	     it != m_connectionsToClose.end(); ++it)
	{
		(*it).pConnection->Release();
	}

	// Release and close all connections
	for (TConnectionArray::const_iterator it = m_pConnections.begin();
	     it != m_pConnections.end(); ++it)
	{
		(*it)->Close();
		(*it)->Release();
	}

	// Release all listeners
	for (TListenerArray::const_iterator it = m_pListeners.begin();
	     it != m_pListeners.end(); ++it)
	{
		(*it)->Release();
	}

	// Release the CVars
	SAFE_RELEASE(m_pVerboseLevel);
	SAFE_RELEASE(m_pReceiveDataQueueLimit);
	SAFE_RELEASE(m_pSendDataQueueLimit);
}

#ifndef RELEASE
bool CServiceNetwork::CheckVerbose(const uint32 level) const
{
	const int verboseLevel = m_pVerboseLevel->GetIVal();
	return (int)level < verboseLevel;
}

void CServiceNetwork::Log(const char* txt, ...)  const
{
	// format the print buffer
	char buffer[512];
	va_list args;
	va_start(args, txt);
	cry_vsprintf(buffer, txt, args);
	va_end(args);

	// pass to log
	gEnv->pLog->LogAlways("%s", buffer);
}
#endif

void CServiceNetwork::SignalStopWork()
{
	m_bExitRequested = true;
}

void CServiceNetwork::ThreadEntry()
{
	TListenerArray updatingListeners;
	TConnectionArray updatingConnections;
	TConnectionsToCloseArray updatingConnectionsToClose;

	// Process messages
	while (!m_bExitRequested)
	{
		// Well, copy the lists for the duration of update
		{
			CryAutoLock<CryMutex> lock(m_accessMutex);
			updatingListeners = m_pListeners;
			updatingConnections = m_pConnections;
			updatingConnectionsToClose = m_connectionsToClose;
		}

		// Update network time
		m_networkTime = gEnv->pTimer->GetAsyncTime().GetMilliSecondsAsInt64();

		// Process the listeners (accepts and pending connections)
		for (TListenerArray::const_iterator it = updatingListeners.begin();
		     it != updatingListeners.end(); ++it)
		{
			(*it)->Update();

			// Remove dead listeners from the main list
			if (!(*it)->IsAlive())
			{
				CryAutoLock<CryMutex> lock(m_accessMutex);

				// remove from array
				TListenerArray::iterator jt = std::find(m_pListeners.begin(), m_pListeners.end(), *it);
				CRY_ASSERT(jt != m_pListeners.end());
				m_pListeners.erase(jt);

				// release internal reference (may delete object if no longer used on main thread)
				(*it)->Release();
			}
		}

		// Process the closing connections
		for (TConnectionsToCloseArray::const_iterator it = updatingConnectionsToClose.begin();
		     it != updatingConnectionsToClose.end(); ++it)
		{
			const ConnectionToClose& info = *it;

			bool bTimeout = false;
			if (info.maxWaitTime && m_networkTime > info.maxWaitTime)
			{
				bTimeout = true;
			}

			// should we close it now ?
			if (bTimeout || !it->pConnection->IsAlive() || it->pConnection->IsSendingQueueEmpty())
			{
				info.pConnection->Close();
				info.pConnection->Release();

				// erase from list
				{
					CryAutoLock<CryMutex> lock(m_accessMutex);
					for (TConnectionsToCloseArray::iterator jt = m_connectionsToClose.begin();
					     jt != m_connectionsToClose.end(); ++jt)
					{
						if ((*jt).pConnection == info.pConnection)
						{
							m_connectionsToClose.erase(jt);
							break;
						}
					}
				}
			}
		}

		// Process the connections
		for (TConnectionArray::const_iterator it = updatingConnections.begin();
		     it != updatingConnections.end(); ++it)
		{
			(*it)->Update();

			// Remove dead connections from the main list
			if (!(*it)->IsAlive())
			{
				CryAutoLock<CryMutex> lock(m_accessMutex);

				// remove from array
				TConnectionArray::iterator jt = std::find(m_pConnections.begin(), m_pConnections.end(), *it);
				CRY_ASSERT(jt != m_pConnections.end());
				m_pConnections.erase(jt);

				// release internal reference (may delete object if no longer used on main thread)
				(*it)->Release();
			}
		}

		// Internal delay
		// TODO: this is guess work right now
		CrySleep(5);
	}
}

void CServiceNetwork::SetVerbosityLevel(const uint32 level)
{
	// propagate the value to CVar (so it is consistent across the engine)
	if (NULL != m_pVerboseLevel)
	{
		m_pVerboseLevel->Set((int)level);
	}
}

IServiceNetworkMessage* CServiceNetwork::AllocMessageBuffer(const uint32 size)
{
	// Allocate message with new ID
	const uint32 bufferID = CryInterlockedIncrement(&m_bufferID);
	return new CServiceNetworkMessage(bufferID, size);
}

IDataWriteStream* CServiceNetwork::CreateMessageWriter()
{
	return new CDataWriteStreamBuffer();
}

IDataReadStream* CServiceNetwork::CreateMessageReader(const void* pData, const uint32 dataSize)
{
	if (pData != NULL && dataSize > 0)
	{
		return new CDataReadStreamMemoryBuffer(pData, dataSize);
	}

	return NULL;
}

ServiceNetworkAddress CServiceNetwork::GetHostAddress(const string& addressString, uint16 optionalPort /*=0*/) const
{
	union AddrUnion
	{
		uint32 addr;
		struct
		{
			uint8 byte0;
			uint8 byte1;
			uint8 byte2;
			uint8 byte3;
		} ip;
	};

	// cut the address into base and port part
	AddrUnion addr;
	const int pos = addressString.rfind(':');
	if (pos != -1)
	{
		// substitute the port number from the part in string
		if (optionalPort == 0)
		{
			const char* portNumberStr = addressString.c_str() + pos + 1;
			optionalPort = (uint16)atoi(portNumberStr);
		}

		// remove the port part from base address
		const string baseAddress = addressString.Left(pos);
		addr.addr = CrySock::GetAddrForHostOrIP(baseAddress.c_str(), 1000);
	}
	else
	{
		// lookup the whole address
		addr.addr = CrySock::GetAddrForHostOrIP(addressString.c_str(), 1000);
	}

	// log on hi verbose mode
	LOG_VERBOSE(3, "GetHostAddress(%s) -> %d:%d:%d:%d.%d",
	            addressString.c_str(),
	            addr.ip.byte0, addr.ip.byte1, addr.ip.byte2, addr.ip.byte3,
	            optionalPort);

	// format the network address
	return ServiceNetworkAddress(
	  addr.ip.byte0, addr.ip.byte1, addr.ip.byte2, addr.ip.byte3,
	  optionalPort);
}

IServiceNetworkListener* CServiceNetwork::CreateListener(uint16 localPort)
{
	// Create socket
	CRYSOCKET createdSocket = CrySock::socketinet();
	if (createdSocket < 0)
	{
		// Connection has other problems
		LOG_VERBOSE(0, "CreateListener(%d): socket() failed: %d",
		            localPort,
		            (int)createdSocket);

		return NULL;
	}

	// Disable merging of small blocks to fight high latency connection
	// NOTE: consoles support this mode by default
#if CRY_PLATFORM_WINDOWS
	const int yes = 1;
	const int ret3 = CrySock::setsockopt(createdSocket, SOL_SOCKET, TCP_NODELAY, (const char*)&yes, sizeof(int));
	if (ret3 < 0)
	{
		// Connection has other problems
		LOG_VERBOSE(0, "CreateListener(%d): setsockopt() failed: %d",
		            localPort,
		            ret3);

		CrySock::closesocket(createdSocket);
		return NULL;
	}
#endif

	// Reuse address
	{
		int valYes = 1;
		if (CrySock::setsockopt(createdSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&valYes, sizeof(int)) < 0)
		{
			CrySock::closesocket(createdSocket);

			// Connection has other problems
			LOG_VERBOSE(0, "CreateListener(%d): setsockopt() (reuse) failed",
			            localPort);

			// cleanup
			CrySock::closesocket(createdSocket);
			return NULL;
		}
	}

	// Put the listener socket in the non blocking mode
	if (!CrySock::MakeSocketNonBlocking(createdSocket))
	{
		// Connection has other problems
		LOG_VERBOSE(0, "CreateListener(%d): setsockopt() failed",
		            localPort);

		// cleanup
		CrySock::closesocket(createdSocket);
		return NULL;
	}

	// Setup local bind address
	CRYSOCKADDR_IN service;
	memset(&service, 0, sizeof(service));
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = htonl(INADDR_ANY);
	service.sin_port = htons(localPort);

	// Bind socket
	const int ret = CrySock::bind(createdSocket, (CRYSOCKADDR*) &service, sizeof(service));
	if (ret < 0)
	{
		// Connection has other problems
		LOG_VERBOSE(0, "CreateListener(%d): bind() failed: %d",
		            localPort,
		            (int)createdSocket);

		// cleanup
		CrySock::closesocket(createdSocket);
		return NULL;
	}

	// Listen for incoming connection requests on the created socket
	const int ret2 = CrySock::listen(createdSocket, 64 /*backLogSize*/);
	if (ret2 < 0)
	{
		// Connection has other problems
		LOG_VERBOSE(0, "CreateListener(%d): listen() failed: %d",
		            localPort,
		            (int)createdSocket);

		// cleanup
		CrySock::closesocket(createdSocket);
		return NULL;
	}

	// Get our local address
	CRYSOCKADDR_IN localAddressInet;
	memset(&localAddressInet, 0, sizeof(localAddressInet));
	CRYSOCKLEN_T nameLen = sizeof(localAddressInet);
	CrySock::getsockname(createdSocket, (CRYSOCKADDR*)&localAddressInet, &nameLen);

	// Translate to debug network address data
	ServiceNetworkAddress localAddress;
	TranslateAddress(localAddressInet, localAddress);

	// Spew to log (important info)
	LOG_VERBOSE(0, "bind() to '%s'",
	            localAddress.ToString().c_str());

	// Create the listener wrapper
	CServiceNetworkListener* listener = new CServiceNetworkListener(this, createdSocket, localAddress);

	// Listener was created
	LOG_VERBOSE(0, "CreateListener(%d): listener created, local address=%s",
	            localPort,
	            listener->GetLocalAddress().ToString().c_str());

	// Add to list of local listeners
	{
		CryAutoLock<CryMutex> lock(m_accessMutex);
		m_pListeners.push_back(listener);
		listener->AddRef();
	}

	// Return wrapping interface
	return listener;
}

void CServiceNetwork::RegisterConnection(CServiceNetworkConnection& con)
{
	CryAutoLock<CryMutex> lock(m_accessMutex);

	// Make sure to register each connection only once
	TConnectionArray::const_iterator it = std::find(m_pConnections.begin(), m_pConnections.end(), &con);
	if (it == m_pConnections.end())
	{
		// Low-level info
		LOG_VERBOSE(3, "RegisterConnection(): registered connection from '%s' to '%s', %p",
		            con.GetLocalAddress().ToString().c_str(),
		            con.GetRemoteAddress().ToString().c_str(),
		            (UINT_PTR) &con);

		// add to connections list, that means we need to also increment the refcount
		m_pConnections.push_back(&con);
		con.AddRef();
	}
}

void CServiceNetwork::RegisterForDeferredClose(CServiceNetworkConnection& con, const uint32 timeout)
{
	CryAutoLock<CryMutex> lock(m_accessMutex);

	// Low-level info
	LOG_VERBOSE(3, "RegisterConnection(): registered connection from '%s' to '%s', %p for defered close, timeout=%d",
	            con.GetLocalAddress().ToString().c_str(),
	            con.GetRemoteAddress().ToString().c_str(),
	            (UINT_PTR) &con,
	            timeout);

	// Add to connections list, that means we need to also increment the refcount
	ConnectionToClose info;
	info.pConnection = &con;
	info.maxWaitTime = (timeout > 0) ? (GetNetworkTime() + timeout) : 0;
	m_connectionsToClose.push_back(info);

	// Keep internal reference
	con.AddRef();
}

IServiceNetworkConnection* CServiceNetwork::Connect(const ServiceNetworkAddress& remoteAddress)
{
	// Create new socket if needed
	CRYSOCKET socket = CrySock::socketinet();
	if (socket < 0)
	{
		// Connection has problems
		LOG_VERBOSE(0, "Connect(%s): socket() failed: %d",
		            remoteAddress.ToString().c_str(),
		            (int)socket);

		return NULL;
	}

	// Translate remote address
	CRYSOCKADDR_IN addr;
	TranslateAddress(remoteAddress, addr);

	// Spew to log (important info)
	LOG_VERBOSE(1, "Connecting to '%s'...",
	            remoteAddress.ToString().c_str());

	// Connect (blocking)
	const int result = CrySock::connect(socket, (const CRYSOCKADDR*)&addr, sizeof(addr));
	if (result != 0)
	{
		// Spew to log (important info)
		LOG_VERBOSE(0, "connect() to '%s' failed: %d",
		            remoteAddress.ToString().c_str(),
		            result);

		return NULL;
	}

	// Get the address of local socket endpoint
	CRYSOCKADDR_IN localAddressInet;
	memset(&localAddressInet, 0, sizeof(localAddressInet));
	CRYSOCKLEN_T nameLen = sizeof(localAddressInet);
	CrySock::getsockname(socket, (CRYSOCKADDR*)&localAddressInet, &nameLen);

	// Translate to debug network address data
	ServiceNetworkAddress localAddress;
	TranslateAddress(localAddressInet, localAddress);

	// Spew to log (important info)
	LOG_VERBOSE(1, "connected() from '%s' to '%s'",
	            localAddress.ToString().c_str(),
	            remoteAddress.ToString().c_str());

	// Allocate some connection ID
	const uint64 loPart = m_guidGenerator.GenerateUint64();
	const uint64 hiPart = m_guidGenerator.GenerateUint64();
	const CryGUID connectionID = CryGUID::Construct(loPart, hiPart);

	// Spew to log (important info)
	LOG_VERBOSE(3, "New connection GUID: %08x-%08x-%08x-%08x",
	            (uint64)(hiPart >> 32), (uint64)(hiPart & 0xFFFFFFFF),
	            (uint64)(loPart >> 32), (uint64)(loPart & 0xFFFFFFFF));

	// Create connection wrapper
	CServiceNetworkConnection* newConnection = new CServiceNetworkConnection(
	  this,
	  CServiceNetworkConnection::eEndpoint_Client, // we are the client, we are connecting to remove destination
	  socket,
	  connectionID,
	  localAddress,
	  remoteAddress);

	// Add to list of connections
	{
		CryAutoLock<CryMutex> lock(m_accessMutex);
		m_pConnections.push_back(newConnection);

		// since we add the object to our internal list keep an extra reference to it
		newConnection->AddRef();
	}

	// Well, good luck and have fun :)
	return newConnection;
}

//-----------------------------------------------------------------------------

// Do not remove (can mess up the uber file builds)
#undef LOG_VERBOSE

//-----------------------------------------------------------------------------
