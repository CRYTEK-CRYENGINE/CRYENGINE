// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include <CryExtension/CryGUID.h>
//-----------------------------------------------------------------------------
//
// Service network is a simple abstract interface for connecting between instances
// of the editor and game running on various platforms. It implements it's own
// small message based communication layer and shall not be used for raw communication
// with anything else.
//
// Features currently implemented by the service network:
//  - Completely thread safe (so can be used from within other threads)
//  - Completely asynchronous (only one thread)
//  - Message based approach (both on the send and receive ends)
//  - Automatic and transparent reconnection
//  - Debug-friendly (will not time-out easily when one of the endpoints is being debugged)
//  - Easy to use
//
// Usage case (server)
//   - Create listener (IServiceListener) on some pre-defined port
//   - Poll the incoming connections by calling Accept() method
//   - Service the traffic by calling connection's ReceiveMessage()/SendMessage() methods
//   - Close() and Release() connections
//   - Close() and Release() listener
//
// Usage case (client)
//   - Connect to a remote listener by calling Connect() method
//   - Service the traffic by calling connection's ReceiveMessage()/SendMessage() methods
//   - Close() and Release() connection
//
// Both sending and receiving is asynchronous. Calling the SendMessage()/ReceiveMessage() methods
// only pushes/pops the message buffers to/from the queue.
// NOTE: Message buffers are internally reference counted by the network system and they are kept around
// until they are sent (in case of outgoing traffic) or until they are polled by ReceiveMessage().
// Be aware that this can cause memory spikes, especially when incoming traffic is not serviced fast enough.
// There are customizable limits (around 1MB) on the amount of data that can be buffered internally by
// the service network before the new messages are rejected.
// It's up to the higher layer to ensure damage control in such situation.
//
// NOTE: connection is also a reference counted object, make sure to call Close() before calling Release().
//
//-----------------------------------------------------------------------------

//! Network address abstraction.
class ServiceNetworkAddress
{
public:
	struct StringAddress
	{
		char              m_data[32];

		ILINE const char* c_str() const
		{
			return m_data;
		}
	};

	struct Address
	{
		uint8  m_ip0;
		uint8  m_ip1;
		uint8  m_ip2;
		uint8  m_ip3;
		uint16 m_port;

		ILINE Address()
			: m_ip0(0)
			, m_ip1(0)
			, m_ip2(0)
			, m_ip3(0)
			, m_port(0)
		{}
	};

private:
	Address m_address;

public:
	//! By default creates ("invalid address").
	ILINE ServiceNetworkAddress()
	{
	}

	//! Copy (with optional port change).
	ILINE ServiceNetworkAddress(const ServiceNetworkAddress& other, uint16 newPort = 0)
		: m_address(other.m_address)
	{
		if (newPort != 0)
		{
			m_address.m_port = newPort;
		}
	}

	//! Initialize from ip:host pattern (if you want to initialize from host name use the DebugNetwork interface).
	ILINE ServiceNetworkAddress(uint8 ip0, uint8 ip1, uint8 ip2, uint8 ip3, uint16 port)
	{
		m_address.m_ip0 = ip0;
		m_address.m_ip1 = ip1;
		m_address.m_ip2 = ip2;
		m_address.m_ip3 = ip3;
		m_address.m_port = port;
	}

	//! Set new port value.
	ILINE void SetPort(uint16 port)
	{
		m_address.m_port = port;
	}

	//! Is this a valid address.
	ILINE bool IsValid() const
	{
		return (m_address.m_ip0 != 0) &&
		       (m_address.m_ip1 != 1) &&
		       (m_address.m_ip2 != 1) &&
		       (m_address.m_ip3 != 1) &&
		       (m_address.m_port != 0);
	}

	//! Convert to human readable string.
	ILINE StringAddress ToString() const
	{
		// format the string buffer
		StringAddress ret;
		cry_sprintf(ret.m_data,
		            "%d.%d.%d.%d:%d",
		            m_address.m_ip0, m_address.m_ip1, m_address.m_ip2, m_address.m_ip3,
		            m_address.m_port);

		// return as managed string
		return ret;
	}

	//! Get the literal data.
	ILINE const Address& GetAddress() const
	{
		return m_address;
	}

public:
	//! Compare base address (IP only) of two connections.
	static bool CompareBaseAddress(const ServiceNetworkAddress& a, const ServiceNetworkAddress& b)
	{
		return (a.m_address.m_ip0 == b.m_address.m_ip0) &&
		       (a.m_address.m_ip1 == b.m_address.m_ip1) &&
		       (a.m_address.m_ip2 == b.m_address.m_ip2) &&
		       (a.m_address.m_ip3 == b.m_address.m_ip3);
	}

	//! Compare full address (IP+port) of two connections.
	static bool CompareFullAddress(const ServiceNetworkAddress& a, const ServiceNetworkAddress& b)
	{
		return (a.m_address.m_ip0 == b.m_address.m_ip0) &&
		       (a.m_address.m_ip1 == b.m_address.m_ip1) &&
		       (a.m_address.m_ip2 == b.m_address.m_ip2) &&
		       (a.m_address.m_ip3 == b.m_address.m_ip3) &&
		       (a.m_address.m_port == b.m_address.m_port);
	}
};

//! Message buffer used by the network system.
struct IServiceNetworkMessage
{
protected:
	IServiceNetworkMessage() {};
	virtual ~IServiceNetworkMessage() {};

public:
	//! Get unique message ID (message ID is used just once).
	virtual uint32 GetId() const = 0;

	//! Get the size of message buffer.
	virtual uint32 GetSize() const = 0;

	//! Get pointer to the message data.
	virtual void* GetPointer() = 0;

	//! Get pointer to the message data.
	virtual const void* GetPointer() const = 0;

	//! Create reader interface for reading message data, returned object is not reference-counted but it will hold a reference to the message.
	virtual struct IDataReadStream* CreateReader() const = 0;

	//! Add reference (buffer is internally refcounted).
	virtual void AddRef() = 0;

	//! Release reference.
	virtual void Release() = 0;
};

//! General network TCP/IP connection.
struct IServiceNetworkConnection
{
protected:
	IServiceNetworkConnection() {};
	virtual ~IServiceNetworkConnection() {};

public:
	static const uint32 kDefaultFlushTime = 10000; //!< ms.

	//! Get the unique connection ID (is shared between host and client).
	virtual const CryGUID& GetGUID() const = 0;

	//! Get remote endpoint address.
	virtual const ServiceNetworkAddress& GetRemoteAddress() const = 0;

	//! Get local endpoint address.
	virtual const ServiceNetworkAddress& GetLocalAddress() const = 0;

	//! Add a message buffer to the connection send queue.
	//! Connection can refuse to send the buffer if it's full or invalid.
	//! If a message is rejected this function returns false.
	virtual bool SendMsg(IServiceNetworkMessage* message) = 0;

	//! Get a message from connection receive queue.
	//! If there are no pending messages a NULL is returned.
	//! Since message is a ref-counted you need to call Release() when you are done with the buffer.
	virtual IServiceNetworkMessage* ReceiveMsg() = 0;

	//! Checks if connection is still alive.
	//! \return false only if connection has been damaged beyond repair.
	virtual bool IsAlive() const = 0;

	//! Get number of messages sent by this connection so far.
	virtual uint32 GetMessageSendCount() const = 0;

	//! Get number of messages received by this connection so far.
	virtual uint32 GetMessageReceivedCount() const = 0;

	//! Get size of data sent by this connection so far.
	virtual uint64 GetMessageSendDataSize() const = 0;

	//! Get size of data received by this connection so far.
	virtual uint64 GetMessageReceivedDataSize() const = 0;

	//! Request connection to be closed but not before sending out all of the pending messages. Incoming messages are ignored.
	//! Processing and sending the messages is done on the networking thread so this function will not block.
	//! As an option, connection can be forcefully closed after given amount of time (in ms).
	virtual void FlushAndClose(const uint32 timeoutMs = kDefaultFlushTime) = 0;

	//! Synchronous wait for the connection to send all outgoing messages.
	virtual void FlushAndWait() = 0;

	//! Request connection to be closed now. All pending messages are discarded.
	virtual void Close() = 0;

	//! Add reference (connection is an internally reference counted object).
	virtual void AddRef() = 0;

	//! Release reference.
	virtual void Release() = 0;
};

//! General listening socket (async).
struct IServiceNetworkListener
{
protected:
	IServiceNetworkListener() {};
	virtual ~IServiceNetworkListener() {};

public:
	//! Get the local address.
	virtual const ServiceNetworkAddress& GetLocalAddress() const = 0;

	//! Get number of active connections handled by this listener.
	virtual uint GetConnectionCount() const = 0;

	//! Accept incoming connection (asynchronously).
	//! Will return NULL if there's nothing to accept.
	//! Will return new IDebugNetworkConnection if something was received.
	virtual IServiceNetworkConnection* Accept() = 0;

	//! Is listener able to accept connections ?.
	virtual bool IsAlive() const = 0;

	//! Request listener to be closed (closes the socket).
	virtual void Close() = 0;

	//! Add reference (listener is an internally reference counted object).
	virtual void AddRef() = 0;

	//! Release reference.
	virtual void Release() = 0;
};

//! General service (background) network interface.
struct IServiceNetwork
{
public:
	virtual ~IServiceNetwork() {};

	//! Set verbosity level of debug messages that got printed to log, levels 0-3 are commonly used.
	virtual void SetVerbosityLevel(const uint32 level) = 0;

	//! Allocate empty message buffer of given size, message buffer is a reference counted object.
	virtual IServiceNetworkMessage* AllocMessageBuffer(const uint32 size) = 0;

	//! Create general message writer stream, object is not reference counted.
	virtual struct IDataWriteStream* CreateMessageWriter() = 0;

	//! Create general message reader stream and initialize it with data.
	virtual struct IDataReadStream* CreateMessageReader(const void* pData, const uint32 dataSize) = 0;

	//! Translate host address (string:port) to network address.
	virtual ServiceNetworkAddress GetHostAddress(const string& addressString, uint16 optionalPort = 0) const = 0;

	//! Create network listener on given local port, listening and accepting connections is done on network thread.
	virtual IServiceNetworkListener* CreateListener(uint16 localPort) = 0;

	//! Connect to remote address (will block until connection is made or refused).
	virtual IServiceNetworkConnection* Connect(const ServiceNetworkAddress& remoteAddress) = 0;
};

//! \endcond