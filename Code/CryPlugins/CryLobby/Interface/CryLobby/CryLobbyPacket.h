// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <CryLobby/ICryMatchMaking.h>               // <> required for Interfuscator

const uint32 CryLobbyPacketUINT8Size = 1;
const uint32 CryLobbyPacketUINT16Size = 2;
const uint32 CryLobbyPacketUINT32Size = 4;
const uint32 CryLobbyPacketUINT64Size = 8;
const uint32 CryLobbyPacketBoolSize = 1;
const uint32 CryLobbyPacketConnectionUIDSize = CryLobbyPacketUINT64Size + CryLobbyPacketUINT16Size;

const uint32 CryLobbyPacketReliablePacketHeaderSize =
  CryLobbyPacketUINT8Size +                  // eH_CryLobby
  CryLobbyPacketConnectionUIDSize +          // fromUID
  CryLobbyPacketBoolSize +                   // reliable
  CryLobbyPacketUINT8Size +                  // counterOut
  CryLobbyPacketUINT8Size;                   // counterIn

const uint32 CryLobbyPacketUnReliablePacketHeaderSize =
  CryLobbyPacketUINT8Size +                  // eH_CryLobby
  CryLobbyPacketConnectionUIDSize +          // fromUID
  CryLobbyPacketBoolSize;                    // reliable

const uint32 CryLobbyPacketReliableDataHeaderSize =
  CryLobbyPacketUINT8Size +                  // lobbyPacketType
  CryLobbyPacketUINT16Size;                  // dataSize

const uint32 CryLobbyPacketUnReliableDataHeaderSize =
  CryLobbyPacketUINT8Size;                   // lobbyPacketType

const uint32 CryLobbyPacketReliableHeaderSize = CryLobbyPacketReliablePacketHeaderSize + CryLobbyPacketReliableDataHeaderSize;
const uint32 CryLobbyPacketUnReliableHeaderSize = CryLobbyPacketUnReliablePacketHeaderSize + CryLobbyPacketUnReliableDataHeaderSize;
const uint32 CryLobbyPacketHeaderSize = CryLobbyPacketReliableHeaderSize;

struct SCryLobbyPacketHeader
{
	CTimeValue                   recvTime;   //!< Reliable and unreliable.
	SCryMatchMakingConnectionUID fromUID;    //!< Reliable and unreliable.
	bool                         reliable;   //!< Reliable and unreliable.
	uint8                        counterOut; //!< Reliable.
	uint8                        counterIn;  //!< Reliable.
};

struct SCryLobbyPacketDataHeader
{
	uint16 dataSize;                         //!< Reliable.
	uint8  lobbyPacketType;                  //!< Reliable and unreliable.
};

//! Creating a packet to send.
//! Call CreateWriteBuffer to create the buffer to write to or call SetWriteBuffer if you wish to specify your own buffer.
//! Call StartWrite with the packet id you wish to use and if the packet should be sent reliably.
//! Game user defined packet id's start at CRYLOBBY_USER_PACKET_START and end at CRYLOBBY_USER_PACKET_MAX there are 128 user defined packet id's.
//! Call the various Write functions to add your data to the packet.
//! Send the packet.
//! Call FreeWriteBuffer if the packet was created with CreateWriteBuffer.
//! Processing a received a packet.
//! Call StartRead which will return the packet id.
//! Call the various Read functions to get your data out.
class CCryLobbyPacket
{
public:
	CCryLobbyPacket()
	{
		m_pWriteBuffer = NULL;
		m_bufferSize = 0;
		m_bufferPos = 0;
		m_allocated = false;
	}

	~CCryLobbyPacket()
	{
		FreeWriteBuffer();
	}

	bool CreateWriteBuffer(uint32 bufferSize)
	{
		FreeWriteBuffer();

		m_pWriteBuffer = new uint8[bufferSize];
		m_bufferSize = bufferSize;
		m_bufferPos = 0;
		m_allocated = true;
		return true;
	}

	void FreeWriteBuffer()
	{
		if (m_allocated)
		{
			delete[] m_pWriteBuffer;
			m_pWriteBuffer = NULL;
			m_bufferSize = 0;
			m_bufferPos = 0;
			m_allocated = false;
		}
	}

	void SetWriteBuffer(uint8* pBuffer, uint32 bufferSize)
	{
		FreeWriteBuffer();
		m_pWriteBuffer = pBuffer;
		m_bufferSize = bufferSize;
		m_bufferPos = 0;
	}

	void SetReadBuffer(const uint8* pBuffer, uint32 bufferSize)
	{
		FreeWriteBuffer();
		m_pReadBuffer = pBuffer;
		m_bufferSize = bufferSize;
		m_bufferPos = 0;
	}

	void StartWrite(uint32 packetType, bool reliable)
	{
		CRY_ASSERT_MESSAGE(m_pWriteBuffer, "CCryLobbyPacket: Buffer not set");
		CRY_ASSERT_MESSAGE((reliable && (CryLobbyPacketReliableHeaderSize <= m_bufferSize)) || (!reliable && (CryLobbyPacketUnReliableHeaderSize <= m_bufferSize)), "CCryLobbyPacket: Buffer too small");

		m_packetHeader.reliable = reliable;
		m_dataHeader.lobbyPacketType = packetType;

		if (reliable)
		{
			m_bufferPos = CryLobbyPacketReliableHeaderSize;
		}
		else
		{
			m_bufferPos = CryLobbyPacketUnReliableHeaderSize;
		}
	}

	uint32 StartRead()
	{
		CRY_ASSERT_MESSAGE(m_pReadBuffer, "CCryLobbyPacket: Buffer not set");
		CRY_ASSERT_MESSAGE((m_packetHeader.reliable && (CryLobbyPacketReliableHeaderSize <= m_bufferSize)) || (!m_packetHeader.reliable && (CryLobbyPacketUnReliableHeaderSize <= m_bufferSize)), "CCryLobbyPacket: Buffer too small");

		if (m_packetHeader.reliable)
		{
			m_bufferPos = CryLobbyPacketReliableHeaderSize;
		}
		else
		{
			m_bufferPos = CryLobbyPacketUnReliableHeaderSize;
		}

		return m_dataHeader.lobbyPacketType;
	}

	void WriteUINT8(uint8 data)
	{
		CRY_ASSERT_MESSAGE(m_pWriteBuffer, "CCryLobbyPacket: Buffer not set");
		CRY_ASSERT_MESSAGE(m_bufferPos + CryLobbyPacketUINT8Size <= m_bufferSize, "CCryLobbyPacket: Buffer too small");

		if (m_pWriteBuffer && (m_bufferPos + CryLobbyPacketUINT8Size <= m_bufferSize))
		{
			m_pWriteBuffer[m_bufferPos++] = data;
		}
	}

	uint8 ReadUINT8()
	{
		CRY_ASSERT_MESSAGE(m_pReadBuffer, "CCryLobbyPacket: Buffer not set");
		CRY_ASSERT_MESSAGE(m_bufferPos + CryLobbyPacketUINT8Size <= m_bufferSize, "CCryLobbyPacket: Buffer too small");

		if (m_pReadBuffer && (m_bufferPos + CryLobbyPacketUINT8Size <= m_bufferSize))
		{
			return m_pReadBuffer[m_bufferPos++];
		}

		return 0;
	}

	void WriteUINT16(uint16 data)
	{
		CRY_ASSERT_MESSAGE(m_pWriteBuffer, "CCryLobbyPacket: Buffer not set");
		CRY_ASSERT_MESSAGE(m_bufferPos + CryLobbyPacketUINT16Size <= m_bufferSize, "CCryLobbyPacket: Buffer too small");

		if (m_pWriteBuffer && (m_bufferPos + CryLobbyPacketUINT16Size <= m_bufferSize))
		{
			m_pWriteBuffer[m_bufferPos++] = (data >> 8) & 0xff;
			m_pWriteBuffer[m_bufferPos++] = (data) & 0xff;
		}
	}

	uint16 ReadUINT16()
	{
		CRY_ASSERT_MESSAGE(m_pReadBuffer, "CCryLobbyPacket: Buffer not set");
		CRY_ASSERT_MESSAGE(m_bufferPos + CryLobbyPacketUINT16Size <= m_bufferSize, "CCryLobbyPacket: Buffer too small");

		if (m_pReadBuffer && (m_bufferPos + CryLobbyPacketUINT16Size <= m_bufferSize))
		{
			uint16 ret;

			ret = m_pReadBuffer[m_bufferPos++];
			ret = (ret << 8) | m_pReadBuffer[m_bufferPos++];

			return ret;
		}

		return 0;
	}

	void WriteUINT32(uint32 data)
	{
		CRY_ASSERT_MESSAGE(m_pWriteBuffer, "CCryLobbyPacket: Buffer not set");
		CRY_ASSERT_MESSAGE(m_bufferPos + CryLobbyPacketUINT32Size <= m_bufferSize, "CCryLobbyPacket: Buffer too small");

		if (m_pWriteBuffer && (m_bufferPos + CryLobbyPacketUINT32Size <= m_bufferSize))
		{
			m_pWriteBuffer[m_bufferPos++] = (data >> 24) & 0xff;
			m_pWriteBuffer[m_bufferPos++] = (data >> 16) & 0xff;
			m_pWriteBuffer[m_bufferPos++] = (data >> 8) & 0xff;
			m_pWriteBuffer[m_bufferPos++] = (data) & 0xff;
		}
	}

	uint32 ReadUINT32()
	{
		CRY_ASSERT_MESSAGE(m_pReadBuffer, "CCryLobbyPacket: Buffer not set");
		CRY_ASSERT_MESSAGE(m_bufferPos + CryLobbyPacketUINT32Size <= m_bufferSize, "CCryLobbyPacket: Buffer too small");

		if (m_pReadBuffer && (m_bufferPos + CryLobbyPacketUINT32Size <= m_bufferSize))
		{
			uint32 ret;

			ret = m_pReadBuffer[m_bufferPos++];
			ret = (ret << 8) | m_pReadBuffer[m_bufferPos++];
			ret = (ret << 8) | m_pReadBuffer[m_bufferPos++];
			ret = (ret << 8) | m_pReadBuffer[m_bufferPos++];

			return ret;
		}

		return 0;
	}

	void WriteUINT64(uint64 data)
	{
		CRY_ASSERT_MESSAGE(m_pWriteBuffer, "CCryLobbyPacket: Buffer not set");
		CRY_ASSERT_MESSAGE(m_bufferPos + CryLobbyPacketUINT64Size <= m_bufferSize, "CCryLobbyPacket: Buffer too small");

		if (m_pWriteBuffer && (m_bufferPos + CryLobbyPacketUINT64Size <= m_bufferSize))
		{
			m_pWriteBuffer[m_bufferPos++] = (data >> 56) & 0xff;
			m_pWriteBuffer[m_bufferPos++] = (data >> 48) & 0xff;
			m_pWriteBuffer[m_bufferPos++] = (data >> 40) & 0xff;
			m_pWriteBuffer[m_bufferPos++] = (data >> 32) & 0xff;
			m_pWriteBuffer[m_bufferPos++] = (data >> 24) & 0xff;
			m_pWriteBuffer[m_bufferPos++] = (data >> 16) & 0xff;
			m_pWriteBuffer[m_bufferPos++] = (data >> 8) & 0xff;
			m_pWriteBuffer[m_bufferPos++] = (data) & 0xff;
		}
	}

	uint64 ReadUINT64()
	{
		CRY_ASSERT_MESSAGE(m_pReadBuffer, "CCryLobbyPacket: Buffer not set");
		CRY_ASSERT_MESSAGE(m_bufferPos + CryLobbyPacketUINT64Size <= m_bufferSize, "CCryLobbyPacket: Buffer too small");

		if (m_pReadBuffer && (m_bufferPos + CryLobbyPacketUINT64Size <= m_bufferSize))
		{
			uint64 ret;

			ret = m_pReadBuffer[m_bufferPos++];
			ret = (ret << 8) | m_pReadBuffer[m_bufferPos++];
			ret = (ret << 8) | m_pReadBuffer[m_bufferPos++];
			ret = (ret << 8) | m_pReadBuffer[m_bufferPos++];
			ret = (ret << 8) | m_pReadBuffer[m_bufferPos++];
			ret = (ret << 8) | m_pReadBuffer[m_bufferPos++];
			ret = (ret << 8) | m_pReadBuffer[m_bufferPos++];
			ret = (ret << 8) | m_pReadBuffer[m_bufferPos++];

			return ret;
		}

		return 0;
	}

	void WriteBool(bool data)
	{
		CRY_ASSERT_MESSAGE(m_pWriteBuffer, "CCryLobbyPacket: Buffer not set");
		CRY_ASSERT_MESSAGE(m_bufferPos + CryLobbyPacketBoolSize <= m_bufferSize, "CCryLobbyPacket: Buffer too small");

		if (m_pWriteBuffer && (m_bufferPos + CryLobbyPacketBoolSize <= m_bufferSize))
		{
			m_pWriteBuffer[m_bufferPos++] = data ? 1 : 0;
		}
	}

	bool ReadBool()
	{
		CRY_ASSERT_MESSAGE(m_pReadBuffer, "CCryLobbyPacket: Buffer not set");
		CRY_ASSERT_MESSAGE(m_bufferPos + CryLobbyPacketBoolSize <= m_bufferSize, "CCryLobbyPacket: Buffer too small");

		if (m_pReadBuffer && (m_bufferPos + CryLobbyPacketBoolSize <= m_bufferSize))
		{
			return m_pReadBuffer[m_bufferPos++] ? true : false;
		}

		return false;
	}

	void WriteData(const void* pData, uint32 dataSize)
	{
		CRY_ASSERT_MESSAGE(m_pWriteBuffer, "CCryLobbyPacket: Buffer not set");
		CRY_ASSERT_MESSAGE(m_bufferPos + dataSize <= m_bufferSize, "CCryLobbyPacket: Buffer too small");

		if (m_pWriteBuffer && (m_bufferPos + dataSize <= m_bufferSize))
		{
			memcpy(&m_pWriteBuffer[m_bufferPos], pData, dataSize);
			m_bufferPos += dataSize;
		}
	}

	void ReadData(void* pData, uint32 dataSize)
	{
		CRY_ASSERT_MESSAGE(m_pReadBuffer, "CCryLobbyPacket: Buffer not set");
		CRY_ASSERT_MESSAGE(m_bufferPos + dataSize <= m_bufferSize, "CCryLobbyPacket: Buffer too small");

		if (m_pReadBuffer && (m_bufferPos + dataSize <= m_bufferSize))
		{
			memcpy(pData, &m_pReadBuffer[m_bufferPos], dataSize);
			m_bufferPos += dataSize;
		}
	}

	//! Include the null terminator in the length - it won't be sent across the wire, but will set correctly on read.
	void WriteString(const char* pData, uint32 dataSize)
	{
		dataSize--;          // Don't bother sending null terminator
		CRY_ASSERT_MESSAGE(m_pWriteBuffer, "CCryLobbyPacket: Buffer not set");
		CRY_ASSERT_MESSAGE(m_bufferPos + dataSize <= m_bufferSize, "CCryLobbyPacket: Buffer too small");

		if (m_pWriteBuffer && (m_bufferPos + dataSize <= m_bufferSize))
		{
			memcpy(&m_pWriteBuffer[m_bufferPos], pData, dataSize);
			m_bufferPos += dataSize;
		}
	}

	//! Include the null terminator in the length - it won't be sent across the wire, but will set correctly on read.
	void ReadString(char* pData, uint32 dataSize)
	{
		dataSize--;          // NULL terminator is not in stream
		CRY_ASSERT_MESSAGE(m_pReadBuffer, "CCryLobbyPacket: Buffer not set");
		CRY_ASSERT_MESSAGE(m_bufferPos + dataSize <= m_bufferSize, "CCryLobbyPacket: Buffer too small");

		if (m_pReadBuffer && (m_bufferPos + dataSize <= m_bufferSize))
		{
			memcpy(pData, &m_pReadBuffer[m_bufferPos], dataSize);
			m_bufferPos += dataSize;
		}
		pData[dataSize] = 0;   // Add null terminator back onto buffer
	}

	void WriteConnectionUID(SCryMatchMakingConnectionUID data)
	{
		WriteUINT64(data.m_sid);
		WriteUINT16(data.m_uid);
	}

	SCryMatchMakingConnectionUID ReadConnectionUID()
	{
		SCryMatchMakingConnectionUID temp;
		temp.m_sid = ReadUINT64();
		temp.m_uid = ReadUINT16();
		return temp;
	}

	void WriteCryLobbyUserData(const SCryLobbyUserData* pData)
	{
		switch (pData->m_type)
		{
		case eCLUDT_Int64:
		case eCLUDT_Float64:
			WriteUINT64(pData->m_int64);
			break;

		case eCLUDT_Int32:
		case eCLUDT_Float32:
			WriteUINT32(pData->m_int32);
			break;

		case eCLUDT_Int16:
			WriteUINT16(pData->m_int16);
			break;

		case eCLUDT_Int8:
			WriteUINT8(pData->m_int8);
			break;

		case eCLUDT_Int64NoEndianSwap:
			WriteData(&pData->m_int64, sizeof(pData->m_int64));
			break;

		default:
			CRY_ASSERT_MESSAGE(0, "CCryLobbyPacket::WriteCryLobbyUserData: Undefined data type");
			break;
		}
	}

	void ReadCryLobbyUserData(SCryLobbyUserData* pData)
	{
		switch (pData->m_type)
		{
		case eCLUDT_Int64:
		case eCLUDT_Float64:
			pData->m_int64 = ReadUINT64();
			break;

		case eCLUDT_Int32:
		case eCLUDT_Float32:
			pData->m_int32 = ReadUINT32();
			break;

		case eCLUDT_Int16:
			pData->m_int16 = ReadUINT16();
			break;

		case eCLUDT_Int8:
			pData->m_int8 = ReadUINT8();
			break;

		case eCLUDT_Int64NoEndianSwap:
			ReadData(&pData->m_int64, sizeof(pData->m_int64));
			break;

		default:
			CRY_ASSERT_MESSAGE(0, "CCryLobbyPacket::ReadCryLobbyUserData: Undefined data type");
			break;
		}
	}

	bool       GetReliable() { return m_packetHeader.reliable; }
	CTimeValue GetRecvTime() { return m_packetHeader.recvTime; }

	bool       IsValidToSend() const
	{
		return (m_bufferPos > 0);
	}

	uint8*       GetWriteBuffer()              { return m_pWriteBuffer; }
	uint32       GetWriteBufferSize()          { return m_bufferSize; }
	uint32       GetWriteBufferPos()           { return m_bufferPos; }
	void         SetWriteBufferPos(uint32 pos) { m_bufferPos = pos; }

	const uint8* GetReadBuffer()               { return m_pReadBuffer; }
	uint32       GetReadBufferSize()           { return m_bufferSize; }
	uint32       GetReadBufferPos()            { return m_bufferPos; }
	void         SetReadBufferPos(uint32 pos)  { m_bufferPos = pos; }

protected:
	SCryLobbyPacketHeader     m_packetHeader;
	SCryLobbyPacketDataHeader m_dataHeader;

	union
	{
		uint8*       m_pWriteBuffer;
		const uint8* m_pReadBuffer;
	};

	uint32 m_bufferSize;
	uint32 m_bufferPos;
	bool   m_allocated;
};
