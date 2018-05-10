// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

// Helpers for writing/reading command data stream from network message packets.
// Those interfaces automatically handle byteswapping for big endian systems.
// The native format for data inside the messages is little endian.

//! Write stream interface.
struct IDataWriteStream
{
public:
	virtual ~IDataWriteStream() {};

public:
	//! Virtualized write method for general data buffer.
	virtual void Write(const void* pData, const uint32 size) = 0;

	//! Virtualized write method for types with size 8 (support byteswapping, a little bit faster than general case).
	virtual void Write8(const void* pData) = 0;

	//! Virtualized write method for types with size 4 (support byteswapping, a little bit faster than general case).
	virtual void Write4(const void* pData) = 0;

	//! Virtualized write method for types with size 2 (support byteswapping, a little bit faster than general case).
	virtual void Write2(const void* pData) = 0;

	//! Virtualized write method for types with size 1 (a little bit faster than general case).
	virtual void Write1(const void* pData) = 0;

	//! Get number of bytes written.
	virtual const uint32 GetSize() const = 0;

	//! Convert to service network message.
	virtual struct IServiceNetworkMessage* BuildMessage() const = 0;

	//! Save the data from this writer stream to the provided buffer.
	virtual void CopyToBuffer(void* pData) const = 0;

	//! Destroy object (if dynamically created).
	virtual void Delete() = 0;

public:
	ILINE IDataWriteStream& operator<<(const uint8& val)
	{
		Write1(&val);
		return *this;
	}

	ILINE IDataWriteStream& operator<<(const uint16& val)
	{
		Write2(&val);
		return *this;
	}

	ILINE IDataWriteStream& operator<<(const uint32& val)
	{
		Write4(&val);
		return *this;
	}

	ILINE IDataWriteStream& operator<<(const uint64& val)
	{
		Write8(&val);
		return *this;
	}

	ILINE IDataWriteStream& operator<<(const int8& val)
	{
		Write1(&val);
		return *this;
	}

	ILINE IDataWriteStream& operator<<(const int16& val)
	{
		Write2(&val);
		return *this;
	}

	ILINE IDataWriteStream& operator<<(const int32& val)
	{
		Write4(&val);
		return *this;
	}

	ILINE IDataWriteStream& operator<<(const int64& val)
	{
		Write8(&val);
		return *this;
	}

	ILINE IDataWriteStream& operator<<(const float& val)
	{
		Write4(&val);
		return *this;
	}

	//! Bool is saved by writing an 8 bit value to make it portable.
	ILINE IDataWriteStream& operator<<(const bool& val)
	{
		const uint8 uVal = val ? 1 : 0;
		Write1(&uVal);
		return *this;
	}

public:
	//! Write C-string to stream.
	ILINE void WriteString(const char* str);

	//! Write string to stream.
	ILINE void WriteString(const string& str);

	//! Write int8 value to stream.
	ILINE void WriteInt8(const int8 val)
	{
		Write1(&val);
	}

	//! Write int16 value to stream.
	ILINE void WriteInt16(const int16 val)
	{
		Write2(&val);
	}

	//! Write int32 value to stream.
	ILINE void WriteInt32(const int32 val)
	{
		Write4(&val);
	}

	//! Write int64 value to stream.
	ILINE void WriteInt64(const int64 val)
	{
		Write8(&val);
	}

	//! Write uint8 value to stream.
	ILINE void WriteUint8(const uint8 val)
	{
		Write1(&val);
	}

	//! Write uint16 value to stream.
	ILINE void WriteUint16(const uint16 val)
	{
		Write2(&val);
	}

	//! Write uint32 value to stream.
	ILINE void WriteUint32(const uint32 val)
	{
		Write4(&val);
	}

	//! Write uint64 value to stream.
	ILINE void WriteUint64(const uint64 val)
	{
		Write8(&val);
	}

	//! Write float value to stream.
	ILINE void WriteFloat(const float val)
	{
		Write4(&val);
	}
};

//! Read stream interface.
//! This interface should support endianess swapping.
struct IDataReadStream
{
public:
	virtual ~IDataReadStream() {};

public:
	//! Destroy object (if dynamically created).
	virtual void Delete() = 0;

	//! Skip given amount of data without reading it.
	virtual void Skip(const uint32 size) = 0;

	//! Virtualized read method (for general buffers).
	virtual void Read(void* pData, const uint32 size) = 0;

	//! Virtualized read method for types with size 8 (a little bit faster than general method, supports byte swapping for BE systems).
	virtual void Read8(void* pData) = 0;

	//! Virtualized read method for types with size 4 (a little bit faster than general method, supports byte swapping for BE systems).
	virtual void Read4(void* pData) = 0;

	//! Virtualized read method for types with size 2 (a little bit faster than general method, supports byte swapping for BE systems).
	virtual void Read2(void* pData) = 0;

	//! Virtualized read method for types with size 1 (a little bit faster than general method, supports byte swapping for BE systems).
	virtual void Read1(void* pData) = 0;

	//! Optimization case - get direct pointer to the underlying buffer.
	virtual const void* GetPointer() = 0;

public:
	ILINE IDataReadStream& operator<<(uint8& val)
	{
		Read1(&val);
		return *this;
	}

	ILINE IDataReadStream& operator<<(uint16& val)
	{
		Read2(&val);
		return *this;
	}

	ILINE IDataReadStream& operator<<(uint32& val)
	{
		Read4(&val);
		return *this;
	}

	ILINE IDataReadStream& operator<<(uint64& val)
	{
		Read8(&val);
		return *this;
	}

	ILINE IDataReadStream& operator<<(int8& val)
	{
		Read1(&val);
		return *this;
	}

	ILINE IDataReadStream& operator<<(int16& val)
	{
		Read2(&val);
		return *this;
	}

	ILINE IDataReadStream& operator<<(int32& val)
	{
		Read4(&val);
		return *this;
	}

	ILINE IDataReadStream& operator<<(int64& val)
	{
		Read8(&val);
		return *this;
	}

	ILINE IDataReadStream& operator<<(float& val)
	{
		Read4(&val);
		return *this;
	}

	//! Bool is saved by writing an 8 bit value to make it portable.
	ILINE IDataReadStream& operator<<(bool& val)
	{
		uint8 uVal = 0;
		Read1(&uVal);
		val = (uVal != 0);
		return *this;
	}

public:
	//! Read string from stream.
	ILINE string ReadString();

	//! Skip string data in a stream without loading the data.
	ILINE void SkipString();

	//! Read int8 from stream.
	ILINE int8 ReadInt8()
	{
		int8 val = 0;
		Read1(&val);
		return val;
	}

	//! Read int16 from stream.
	ILINE int16 ReadInt16()
	{
		int16 val = 0;
		Read2(&val);
		return val;
	}

	//! Read int32 from stream.
	ILINE int32 ReadInt32()
	{
		int32 val = 0;
		Read4(&val);
		return val;
	}

	//! Read int64 from stream.
	ILINE int64 ReadInt64()
	{
		int64 val = 0;
		Read8(&val);
		return val;
	}

	//! Read uint8 from stream.
	ILINE uint8 ReadUint8()
	{
		uint8 val = 0;
		Read1(&val);
		return val;
	}

	//! Read uint16 from stream.
	ILINE uint16 ReadUint16()
	{
		uint16 val = 0;
		Read2(&val);
		return val;
	}

	//! Read uint32 from stream.
	ILINE uint32 ReadUint32()
	{
		uint32 val = 0;
		Read4(&val);
		return val;
	}

	//! Read int64 from stream.
	ILINE uint64 ReadUint64()
	{
		uint64 val = 0;
		Read8(&val);
		return val;
	}

	//! Read float from stream.
	ILINE float ReadFloat()
	{
		float val = 0.0f;
		Read4(&val);
		return val;
	}
};

//! Remote command class info (simple RTTI).
struct IRemoteCommandClass
{
public:
	virtual ~IRemoteCommandClass() {};

	//! Get class name.
	virtual const char* GetName() const = 0;

	//! Create command instance.
	virtual struct IRemoteCommand* CreateObject() = 0;
};

//! Remote command interface.
struct IRemoteCommand
{
protected:
	virtual ~IRemoteCommand() {};

public:
	//! Get command class.
	virtual IRemoteCommandClass* GetClass() const = 0;

	//! Save to data stream.
	virtual void SaveToStream(struct IDataWriteStream& writeStream) const = 0;

	//! Load from data stream.
	virtual void LoadFromStream(struct IDataReadStream& readStream) = 0;

	//! Execute (remote call) = 0;.
	virtual void Execute() = 0;

	//! Delete the command object (can be allocated from different heap).
	virtual void Delete() = 0;
};

//! This is a implementation of a synchronous listener (limited to the engine tick rate) that processes and responds to the raw messages received from clients.
struct IRemoteCommandListenerSync
{
public:
	virtual ~IRemoteCommandListenerSync() {};

	//! Process a raw message and optionally provide an answer to the request, return true if you have processed the message.
	//! Messages is accessible via the data reader. Response can be written to a data writer.
	virtual bool OnRawMessageSync(const class ServiceNetworkAddress& remoteAddress, struct IDataReadStream& msg, struct IDataWriteStream& response) = 0;
};

//! This is a implementation of a asynchronous listener (called from network thread) that processes and responds to the raw messages received from clients.
struct IRemoteCommandListenerAsync
{
public:
	virtual ~IRemoteCommandListenerAsync() {};

	//! Process a raw message and optionally provide an answer to the request, return true if you have processed the message.
	//! Messages is accessible via the data reader. Response can be written to a data writer.
	virtual bool OnRawMessageAsync(const class ServiceNetworkAddress& remoteAddress, struct IDataReadStream& msg, struct IDataWriteStream& response) = 0;
};

//! Remote command server.
struct IRemoteCommandServer
{
protected:
	virtual ~IRemoteCommandServer() {};

public:
	//! Execute all of the received pending commands.
	//! This should be called from a safe place (main thread).
	virtual void FlushCommandQueue() = 0;

	//! Suppress command execution.
	virtual void SuppressCommands() = 0;

	//! Resume command execution.
	virtual void ResumeCommands() = 0;

	//! Register/Unregister synchronous message listener (limited to tick rate).
	virtual void RegisterSyncMessageListener(IRemoteCommandListenerSync* pListener) = 0;
	virtual void UnregisterSyncMessageListener(IRemoteCommandListenerSync* pListener) = 0;

	//! Register/Unregister asynchronous message listener (called from network thread).
	virtual void RegisterAsyncMessageListener(IRemoteCommandListenerAsync* pListener) = 0;
	virtual void UnregisterAsyncMessageListener(IRemoteCommandListenerAsync* pListener) = 0;

	//! Broadcast a message to all connected clients.
	virtual void Broadcast(IServiceNetworkMessage* pMessage) = 0;

	//! Do we have any clients connected?
	virtual bool HasConnectedClients() const = 0;

	//! Delete the client.
	virtual void Delete() = 0;
};

//! Connection to remote command server.
struct IRemoteCommandConnection
{
protected:
	virtual ~IRemoteCommandConnection() {};

public:
	//! Are we connected?
	//! Returns false when the underlying network connection has failed (sockets error).
	//! Also returns false if the remote connection was closed by remote peer.
	virtual bool IsAlive() const = 0;

	//! Get address of remote command server.
	//! This returns the full address of the endpoint (with valid port).
	virtual const ServiceNetworkAddress& GetRemoteAddress() const = 0;

	//! Send raw message to the other side of this connection.
	//! Raw messages are not buffer and are sent right away, they also have precedence over internal command traffic.
	//! The idea is that you need some kind of bidirectional signaling channel to extend  the rather one-directional nature of commands.
	//! \return true if message was added to the send queue.
	virtual bool SendRawMessage(IServiceNetworkMessage* pMessage) = 0;

	//! See if there's a raw message waiting for us and if it is, get it.
	//! Be aware that messages are reference counted.
	virtual IServiceNetworkMessage* ReceiveRawMessage() = 0;

	//! Close connection.
	//! Pending commands are not sent.
	//! Pending raw messages are sent or not (depending on the flag).
	virtual void Close(bool bFlushQueueBeforeClosing = false) = 0;

	//! Add internal reference to object (Refcounting interface).
	virtual void AddRef() = 0;

	//! Release internal reference to object (Refcounting interface).
	virtual void Release() = 0;
};

//! Remote command client.
struct IRemoteCommandClient
{
protected:
	virtual ~IRemoteCommandClient() {};

public:
	//! Connect to remote server, returns true on success, false on failure.
	virtual IRemoteCommandConnection* ConnectToServer(const class ServiceNetworkAddress& serverAddress) = 0;

	//! Schedule command to be executed on the all of the remote servers.
	virtual bool Schedule(const IRemoteCommand& command) = 0;

	//! Delete the client object.
	virtual void Delete() = 0;
};

//! Remote command manager.
struct IRemoteCommandManager
{
public:
	virtual ~IRemoteCommandManager() {};

	//! Set debug message verbose level.
	virtual void SetVerbosityLevel(const uint32 level) = 0;

	//! Create local server for executing remote commands on given local port.
	virtual IRemoteCommandServer* CreateServer(uint16 localPort) = 0;

	//! Create client interface for executing remote commands on remote servers.
	virtual IRemoteCommandClient* CreateClient() = 0;

	//! Register command class (will be accessible by both clients and server).
	virtual void RegisterCommandClass(IRemoteCommandClass& commandClass) = 0;
};

//! Class RTTI wrapper for remote command classes.
template<typename T>
class CRemoteCommandClass : public IRemoteCommandClass
{
private:
	const char* m_szName;

public:
	ILINE CRemoteCommandClass(const char* szName)
		: m_szName(szName)
	{}

	virtual const char* GetName() const
	{
		return m_szName;
	}

	virtual struct IRemoteCommand* CreateObject()
	{
		return new T();
	}
};

#define DECLARE_REMOTE_COMMAND(x)                                                                                                             \
  public: static IRemoteCommandClass& GetStaticClass() {                                                                                      \
      static IRemoteCommandClass* theClass = new CRemoteCommandClass<x>( # x); return *theClass; }                                            \
  public: virtual IRemoteCommandClass* GetClass() const { return &GetStaticClass(); }                                                         \
  public: virtual void Delete() { delete this; }                                                                                              \
  public: virtual void SaveToStream(IDataWriteStream & writeStream) const { const_cast<x*>(this)->Serialize<IDataWriteStream>(writeStream); } \
  public: virtual void LoadFromStream(IDataReadStream & readStream) { Serialize<IDataReadStream>(readStream); }

//! CryString serialization helper (read).
ILINE IDataReadStream& operator<<(IDataReadStream& stream, string& outString)
{
	const uint32 kMaxTempString = 256;

	// read length
	uint32 length = 0;
	stream << length;

	// load string
	if (length > 0)
	{
		if (length < kMaxTempString)
		{
			// load the string into temporary buffer
			char temp[kMaxTempString];
			stream.Read(&temp, length);
			temp[length] = 0;

			// set the string with new value
			outString = temp;
		}
		else
		{
			// allocate temporary memory and load the string
			std::vector<char> temp;
			temp.resize(length + 1, 0);
			stream.Read(&temp[0], length);

			// set the string with new value
			outString = &temp[0];
		}
	}
	else
	{
		// empty string
		outString.clear();
	}

	return stream;
}

//! CryString serialization helper (write).
ILINE IDataWriteStream& operator<<(IDataWriteStream& stream, const string& str)
{
	// write length
	const uint32 length = str.length();
	stream << length;

	// write string data
	if (length > 0)
	{
		stream.Write(str.c_str(), length);
	}

	return stream;
}

//! Vector serialization helper (reading).
template<class T>
ILINE IDataReadStream& operator<<(IDataReadStream& ar, std::vector<T>& outVector)
{
	// Load item count
	uint32 count = 0;
	ar << count;

	// Adapt the vector size (exact fit)
	outVector.resize(count);

	// Load items
	for (uint32 i = 0; i < count; ++i)
	{
		ar << outVector[i];
	}

	return ar;
}

//! Vector serialization helper (writing).
template<class T>
ILINE IDataWriteStream& operator<<(IDataWriteStream& ar, const std::vector<T>& vec)
{
	const uint32 count = vec.size();
	ar << count;

	for (uint32 i = 0; i < count; ++i)
	{
		ar << const_cast<T&>(vec[i]);
	}

	return ar;
}

ILINE void IDataWriteStream::WriteString(const char* str)
{
	string tempString(str);
	*this << tempString;
}

ILINE void IDataWriteStream::WriteString(const string& str)
{
	*this << str;
}

ILINE string IDataReadStream::ReadString()
{
	string ret;
	*this << ret;
	return ret;
}

ILINE void IDataReadStream::SkipString()
{
	// read length
	uint32 length = 0;
	*this << length;
	Skip(length);
}

//! Helper class for using the data reader and writer classes.
//! The only major differce betwen auto_ptr is that we call Delete() instead of operator delete.
template<class T>
class TAutoDelete
{
public:
	T* m_ptr;

public:
	ILINE TAutoDelete(T* ptr)
		: m_ptr(ptr)
	{
	}

	ILINE ~TAutoDelete()
	{
		if (NULL != m_ptr)
		{
			m_ptr->Delete();
			m_ptr = NULL;
		}
	}

	ILINE operator bool()
	{
		return (NULL != m_ptr);
	}

	ILINE operator T&()
	{
		return *m_ptr;
	}

	ILINE T* operator->()
	{
		return m_ptr;
	}

private:
	TAutoDelete(const TAutoDelete& other) : m_ptr(NULL){};
	TAutoDelete& operator=(const TAutoDelete& other) { return *this; }
};

//! \endcond