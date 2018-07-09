// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   Description:  Remote command system helper classes
   -------------------------------------------------------------------------
   History:
   - 17/04/2013   : Tomasz Jonarski, Created

*************************************************************************/

#ifndef __REMOTECOMMANDHELPERS_H__
#define __REMOTECOMMANDHELPERS_H__

//------------------------------------------------------------------------

#include <CryNetwork/IRemoteCommand.h>

struct IServiceNetworkMessage;

//------------------------------------------------------------------------

// Stream reader for service network message
// Implements automatic byte swapping
class CDataReadStreamFormMessage : public IDataReadStream
{
private:
	const IServiceNetworkMessage* m_pMessage;
	const char*                   m_pData;
	uint32                        m_offset;
	uint32                        m_size;

private:
	template<typename T>
	ILINE void ReadType(void* pData)
	{
		CRY_ASSERT(m_offset + sizeof(T) < m_size);
		const T& readPos = *reinterpret_cast<const T*>(m_pData + m_offset);
		*reinterpret_cast<T*>(pData) = readPos;
		SwapEndian(*reinterpret_cast<T*>(pData));
		m_offset += sizeof(T);
	}

public:
	CDataReadStreamFormMessage(const IServiceNetworkMessage* message);
	virtual ~CDataReadStreamFormMessage();

	const uint32 GetOffset() const
	{
		return m_offset;
	}

	void SetPosition(uint32 offset)
	{
		m_offset = offset;
	}

public:
	// IDataReadStream interface
	virtual void        Delete();
	virtual void        Skip(const uint32 size);
	virtual void        Read(void* pData, const uint32 size);
	virtual void        Read8(void* pData);
	virtual void        Read4(void* pData);
	virtual void        Read2(void* pData);
	virtual void        Read1(void* pData);
	virtual const void* GetPointer();
};

//------------------------------------------------------------------------

// Stream writer that writes into the service network message
class CDataWriteStreamToMessage : public IDataWriteStream
{
private:
	IServiceNetworkMessage* m_pMessage;
	char*                   m_pData;
	uint32                  m_offset;
	uint32                  m_size;

private:
	template<typename T>
	ILINE void WriteType(const void* pData)
	{
		CRY_ASSERT(m_offset + sizeof(T) < m_size);
		T& writePos = *reinterpret_cast<T*>(m_pData + m_offset);
		writePos = *reinterpret_cast<const T*>(pData);
		SwapEndian(writePos);
		m_offset += sizeof(T);
	}

public:
	CDataWriteStreamToMessage(IServiceNetworkMessage* pMessage);
	virtual ~CDataWriteStreamToMessage();

	// IDataWriteStream interface implementation
	virtual void                           Delete();
	virtual const uint32                   GetSize() const;
	virtual struct IServiceNetworkMessage* BuildMessage() const;
	virtual void                           CopyToBuffer(void* pData) const;
	virtual void                           Write(const void* pData, const uint32 size);
	virtual void                           Write8(const void* pData);
	virtual void                           Write4(const void* pData);
	virtual void                           Write2(const void* pData);
	virtual void                           Write1(const void* pData);
};

//------------------------------------------------------------------------

/// Stream reader reading from owner memory buffer
class CDataReadStreamMemoryBuffer : public IDataReadStream
{
private:
	const uint32 m_size;
	uint8*       m_pData;
	uint32       m_offset;

public:
	// memory is copied!
	CDataReadStreamMemoryBuffer(const void* pData, const uint32 size);
	virtual ~CDataReadStreamMemoryBuffer();

	virtual void        Delete();
	virtual void        Skip(const uint32 size);
	virtual void        Read8(void* pData);
	virtual void        Read4(void* pData);
	virtual void        Read2(void* pData);
	virtual void        Read1(void* pData);
	virtual const void* GetPointer();
	virtual void        Read(void* pData, const uint32 size);
};

//------------------------------------------------------------------------

// Stream writer that writes into the internal memory buffer
class CDataWriteStreamBuffer : public IDataWriteStream
{
	static const uint32 kStaticPartitionSize = 4096;

private:
	// Default (preallocated) partition
	char m_defaultPartition[kStaticPartitionSize];

	// Allocated dynamic partitions
	std::vector<char*> m_pPartitions;

	// Size of the dynamic message partitions
	std::vector<uint32> m_partitionSizes;

	// Pointer to current writing position in the current partition
	char* m_pCurrentPointer;

	// Space left in current partition
	uint32 m_leftInPartition;

	// Total message size so far
	uint32 m_size;

private:
	// Directly write typed data into the stream
	template<typename T>
	ILINE void WriteType(const void* pData)
	{
		// try to use the faster path if we are not crossing the partition boundary
		if (m_leftInPartition >= sizeof(T))
		{
			// faster case
			T& writePos = *reinterpret_cast<T*>(m_pCurrentPointer);
			writePos = *reinterpret_cast<const T*>(pData);
			SwapEndian(writePos);
			m_pCurrentPointer += sizeof(T);
			m_leftInPartition -= sizeof(T);
			m_size += sizeof(T);
		}
		else
		{
			// slower case (more generic)
			T tempVal(*reinterpret_cast<const T*>(pData));
			SwapEndian(tempVal);
			Write(&tempVal, sizeof(tempVal));
		}
	}

public:
	CDataWriteStreamBuffer();
	virtual ~CDataWriteStreamBuffer();

	// IDataWriteStream interface implementation
	virtual void                    Delete();
	virtual const uint32            GetSize() const;
	virtual IServiceNetworkMessage* BuildMessage() const;
	virtual void                    CopyToBuffer(void* pData) const;
	virtual void                    Write(const void* pData, const uint32 size);
	virtual void                    Write8(const void* pData);
	virtual void                    Write4(const void* pData);
	virtual void                    Write2(const void* pData);
	virtual void                    Write1(const void* pData);
};

//-----------------------------------------------------------------------------

// Packet header
struct PackedHeader
{
	// Estimation (or better yet, exact value) of how much data this header will take when written.
	// Please make sure that actual size after serialization is not bigger than this value.
	static const uint32 kSerializationSize = sizeof(uint8) + sizeof(uint32) + sizeof(uint32);

	// Magic value that identifies command messages vs raw messages
	static const uint32 kMagic = 0xABBAF00D;

	// Command type
	// Keep the values unchanged as this may break the protocol
	enum ECommand
	{
		// Server class list mapping
		eCommand_ClassList = 0,

		// Command data
		eCommand_Command = 1,

		// Disconnect signal
		eCommand_Disconnect = 2,

		// ACK packet
		eCommand_ACK = 3,
	};

	uint32 magic;
	uint8  msgType;
	uint32 count;

	// serialization operator
	template<class T>
	friend T& operator<<(T& stream, PackedHeader& header)
	{
		stream << header.magic;
		stream << header.msgType;
		stream << header.count;
		return stream;
	}
};

// Header sent with every command
struct CommandHeader
{
	uint32 commandId;
	uint32 classId;
	uint32 size;

	CommandHeader()
		: commandId(0)
		, classId(0)
		, size(0)
	{}

	// serialization operator
	template<class T>
	friend T& operator<<(T& stream, CommandHeader& header)
	{
		stream << header.commandId;
		stream << header.classId;
		stream << header.size;
		return stream;
	}
};

// General Response/ACK header
struct ResponseHeader
{
	uint32 magic;
	uint8  msgType;
	uint32 lastCommandReceived;
	uint32 lastCommandExecuted;

	ResponseHeader()
		: lastCommandReceived(0)
		, lastCommandExecuted(0)
		, msgType(PackedHeader::eCommand_ACK)
	{}

	// serialization operator
	template<class T>
	friend T& operator<<(T& stream, ResponseHeader& header)
	{
		stream << header.magic;
		stream << header.msgType;
		stream << header.lastCommandReceived;
		stream << header.lastCommandExecuted;
		return stream;
	}
};

//-----------------------------------------------------------------------------

#endif
