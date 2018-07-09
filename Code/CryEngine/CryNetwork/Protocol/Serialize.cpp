// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  implementation of CryNetwork ISerialize classes
   -------------------------------------------------------------------------
   History:
   - 26/07/2004   10:34 : Created by Craig Tiller
*************************************************************************/
#include "StdAfx.h"
#include "Config.h"

#if USE_ARITHSTREAM

	#include "Streams/ArithStream.h"
	#include "Serialize.h"
	#include "Network.h"
	#include "Compression/SerializationChunk.h"
	#include "Compression/CompressionManager.h"
	#include "Streams/ByteStream.h"

#else

	#include "Serialize.h"
	#include "NetChannel.h"
	#include "Network.h"
	#include "Context/ContextView.h"

#endif

#if USE_ARITHSTREAM

	#if ENABLE_DEBUG_KIT
bool CNetSerialize::m_bEnableLogging;
	#endif

//
// NetOutputSerialize
//

CNetOutputSerializeImpl::CNetOutputSerializeImpl(uint8* pBuffer, size_t nSize, uint8 nBonus) :
	m_output(pBuffer, nSize, nBonus)
	#if ENABLE_SERIALIZATION_LOGGING
	, m_pChannel(NULL)
	#endif // ENABLE_SERIALIZATION_LOGGING
{
	#if ENABLE_DEBUG_KIT
	if (m_bEnableLogging)
		m_output.EnableLog();
	#endif
}

CNetOutputSerializeImpl::CNetOutputSerializeImpl(IStreamAllocator* pAllocator, size_t initialSize, uint8 bonus) :
	m_output(pAllocator, initialSize, bonus)
	#if ENABLE_SERIALIZATION_LOGGING
	, m_pChannel(NULL)
	#endif // ENABLE_SERIALIZATION_LOGGING
{
	#if ENABLE_DEBUG_KIT
	if (m_bEnableLogging)
		m_output.EnableLog();
	#endif
}

ESerializeChunkResult CNetOutputSerializeImpl::SerializeChunk(ChunkID chunk, uint8 profile, TMemHdl* phData, CTimeValue* pTimeInfo, CMementoMemoryManager& mmm)
{
	CTimeValue temp;
	if (!phData || *phData == CMementoMemoryManager::InvalidHdl)
	{
		return eSCR_Failed;
	}
	else
	{
		CByteInputStream in((const uint8*) mmm.PinHdl(*phData), mmm.GetHdlSize(*phData));
		if (pTimeInfo)
		{
			*pTimeInfo = in.GetTyped<CTimeValue>();
			Value("_tstamp", *pTimeInfo, 'tPhy');
		}
		CNetwork::Get()->GetCompressionManager().BufferToStream(chunk, profile, in, *this);
		return eSCR_Ok;
	}
}

void CNetOutputSerializeImpl::ResetLogging()
{
	#if ENABLE_DEBUG_KIT
	m_output.EnableLog(m_bEnableLogging);
	#endif
}

//
// NetInputSerialize
//

CNetInputSerializeImpl::CNetInputSerializeImpl(const uint8* pBuffer, size_t nSize, INetChannel* pChannel) :
	m_input(pBuffer, nSize), m_pChannel(pChannel)
{
	#if ENABLE_DEBUG_KIT
	if (m_bEnableLogging)
		m_input.EnableLog();
	#endif
}

ESerializeChunkResult CNetInputSerializeImpl::SerializeChunk(ChunkID chunk, uint8 profile, TMemHdl* phData, CTimeValue* pTimeInfo, CMementoMemoryManager& mmm)
{
	if (!Ok())
		return eSCR_Failed;

	CMementoStreamAllocator alloc(&mmm);
	size_t sizeHint = 8;
	if (phData && *phData != CMementoMemoryManager::InvalidHdl)
		sizeHint = mmm.GetHdlSize(*phData);
	CByteOutputStream stm(&alloc, sizeHint);
	if (pTimeInfo)
	{
		Value("_tstamp", *pTimeInfo, 'tPhy');
		stm.PutTyped<CTimeValue>() = *pTimeInfo;
	}
	CNetwork::Get()->GetCompressionManager().StreamToBuffer(chunk, profile, *this, stm);
	if (!Ok())
	{
		mmm.FreeHdl(alloc.GetHdl());
		return eSCR_Failed;
	}
	bool take = false;
	if (phData && m_bCommit)
	{
		take = true;
		if (*phData != CMementoMemoryManager::InvalidHdl)
		{
			if (mmm.GetHdlSize(*phData) == stm.GetSize())
				if (0 == memcmp(mmm.PinHdl(*phData), mmm.PinHdl(alloc.GetHdl()), stm.GetSize()))
					take = false;
			if (take)
			{
				mmm.FreeHdl(*phData);
				mmm.ResizeHdl(alloc.GetHdl(), stm.GetSize());
				*phData = alloc.GetHdl();
			}
			else
			{
				mmm.FreeHdl(alloc.GetHdl());
			}
		}
		else
		{
			mmm.ResizeHdl(alloc.GetHdl(), stm.GetSize());
			*phData = alloc.GetHdl();
		}
	}
	else
	{
		mmm.FreeHdl(alloc.GetHdl());
	}
	return take ? eSCR_Ok_Updated : eSCR_Ok;
}

#else

	#if ENABLE_DEBUG_KIT
bool CNetSerialize::m_bEnableLogging;
	#endif

//
// NetOutputSerialize
//

CNetOutputSerializeImpl::CNetOutputSerializeImpl(uint8* buffer, size_t size, uint8 bonus)
{
	SetNetChannel(NULL);
	m_buffer = buffer;
	m_bufferSize = size;
	Reset(bonus);

	#if ENABLE_DEBUG_KIT
	//	if (m_bEnableLogging)
	//		m_output.EnableLog();
	#endif
}

ESerializeChunkResult CNetOutputSerializeImpl::SerializeChunk(ChunkID chunk, uint8 profile, TMemHdl* phData, CTimeValue* pTimeInfo, CMementoMemoryManager& mmm)
{
	CTimeValue temp;

	if (!phData || (*phData == CMementoMemoryManager::InvalidHdl))
	{
		return eSCR_Failed;
	}
	else
	{
		CByteInputStream in((const uint8*) mmm.PinHdl(*phData), mmm.GetHdlSize(*phData));

		if (pTimeInfo)
		{
			*pTimeInfo = in.GetTyped<CTimeValue>();
			Value("_tstamp", *pTimeInfo, 'tPhy');
		}

		CNetwork::Get()->GetCompressionManager().BufferToStream(chunk, profile, in, *this);

		if (Ok())
		{
			return eSCR_Ok;
		}
		else
		{
			return eSCR_Failed;
		}
	}
}

void CNetOutputSerializeImpl::ResetLogging()
{
	#if ENABLE_DEBUG_KIT
	//	m_output.EnableLog( m_bEnableLogging );
	#endif
}

void CNetOutputSerializeImpl::Reset(uint8 bonus)
{
	m_buffer[0] = bonus;
	m_bufferPos = 1;
	m_bufferPosBit = 0;
}

void CNetOutputSerializeImpl::WriteNetId(SNetObjectID id)
{
	debugPacketDataSizeStartNetIDData(id, GetBitSize());

	if (m_multiplayer)
	{
		CNetCVars& netCVars = CNetCVars::Get();

		if (id.id == SNetObjectID::InvalidId)
		{
			id.id = netCVars.net_invalidNetID;
		}

		if (id.id < netCVars.net_numNetIDLowBitIDs)
		{
			WriteBits(0, 1);
			WriteBits(id.id, netCVars.net_numNetIDLowBitBits);
		}
		else
		{
			if (id.id < netCVars.net_netIDHighBitStart)
			{
				WriteBits(2, 2);
				WriteBits(id.id - netCVars.net_numNetIDLowBitIDs, netCVars.net_numNetIDMediumBitBits);
			}
			else
			{
				WriteBits(3, 2);
				WriteBits(id.id - netCVars.net_netIDHighBitStart, netCVars.net_numNetIDHighBitBits);
			}
		}
	}
	else
	{
		WriteBits(id.id, 16);
	}

	debugPacketDataSizeEndData(eDPDST_NetID, GetBitSize());
}

void CNetOutputSerializeImpl::WriteTime(ETimeStream time, CTimeValue value)
{
	uint64 t = (uint64)value.GetMilliSecondsAsInt64();

	switch (time)
	{
	case eTS_Network:
		break;

	case eTS_NetworkPing:
		break;

	case eTS_NetworkPong:
		break;

	case eTS_PongElapsed:
		break;

	case eTS_Physics:
	case eTS_RemoteTime:
		t = t >> 5;
		break;
	}

	if (t < ((1 << 14) - 1))
	{
		WriteBits(0, 2);
		WriteBits((uint32)t, 14);
	}
	else if (t < ((1 << 18) - 1))
	{
		WriteBits(1, 2);
		WriteBits((uint32)t, 18);
	}
	else if (t < ((1 << 26) - 1))
	{
		WriteBits(2, 2);
		WriteBits((uint32)t, 26);
	}
	else
	{
		WriteBits(3, 2);
		WriteBits((uint32)(t >> 32) & 0x3f, 6);
		WriteBits((uint32)t, 32);
	}
}

void CNetOutputSerializeImpl::WriteString(const SSerializeString* string)
{
	size_t length = string->length();
	const char* s = string->c_str();

	for (size_t i = 0; i < length + 1; i++)
	{
		WriteBits((uint8)s[i], 8);
	}
}

void CNetOutputSerializeImpl::AddBits(uint8 value, int num)
{
	if (m_bufferPos < m_bufferSize)
	{
		uint8 write;
		uint8 mask;
		int shift = (8 - num) - m_bufferPosBit;
		uint8 curByte = m_buffer[m_bufferPos];

		if (shift > 0)
		{
			write = value << shift;
			mask = ((1 << num) - 1) << shift;
		}
		else
		{
			shift = -shift;
			write = value >> shift;
			mask = ((1 << num) - 1) >> shift;
		}

		curByte &= ~mask;
		curByte |= write;
		m_buffer[m_bufferPos] = curByte;

		if (m_bufferPosBit + num > 7)
		{
			num -= (8 - m_bufferPosBit);

			m_bufferPos++;
			m_bufferPosBit = 0;

			if (num > 0)
			{
				if (m_bufferPos < m_bufferSize)
				{
					curByte = m_buffer[m_bufferPos];
					shift = (8 - num);
					write = value << shift;
					mask = ((1 << num) - 1) << shift;

					curByte &= ~mask;
					curByte |= write;
					m_buffer[m_bufferPos] = curByte;

					m_bufferPosBit += num;
				}
				else
				{
					Failed();
				}
			}
		}
		else
		{
			m_bufferPosBit += num;
		}
	}
	else
	{
		Failed();
	}
}

void CNetOutputSerializeImpl::WriteBits(uint32 value, int num)
{
	int numBytes = ((num & 7) == 0) ? num >> 3 : (num >> 3) + 1;

	switch (numBytes)
	{
	case 4:
		AddBits(value >> 24, num - 24);
	case 3:
		AddBits((value >> 16) & 0xff, num >= 24 ? 8 : num - 16);
	case 2:
		AddBits((value >> 8) & 0xff, num >= 16 ? 8 : num - 8);
	case 1:
		AddBits(value & 0xff, num >= 8 ? 8 : num);
	}
}

void CNetOutputSerializeImpl::PutZeros(int n)
{
	for (int i = 0; i < n; i++)
	{
		m_buffer[m_bufferPos++] = 0;
	}
}

void CNetOutputSerializeImpl::SetNetChannel(CNetChannel* channel)
{
	CNetContext* pNetContext = channel ? channel->GetContextView()->Context() : NULL;

	m_channel = channel;
	m_multiplayer = pNetContext ? pNetContext->IsMultiplayer() : false;
}

//
// NetInputSerialize
//

CNetInputSerializeImpl::CNetInputSerializeImpl(const uint8* buffer, size_t size, CNetChannel* channel)
{
	#if ENABLE_DEBUG_KIT
	//	if (m_bEnableLogging)
	//		m_input.EnableLog();
	#endif

	SetNetChannel(channel); // inits channel and m_mutliplayer
	m_buffer = buffer;
	m_bufferSize = size;
	m_bufferPos = 1;
	m_bufferPosBit = 0;
}

ESerializeChunkResult CNetInputSerializeImpl::SerializeChunk(ChunkID chunk, uint8 profile, TMemHdl* phData, CTimeValue* pTimeInfo, CMementoMemoryManager& mmm)
{
	if (Ok())
	{
		CMementoStreamAllocator alloc(&mmm);
		size_t sizeHint = 8;

		if (phData && (*phData != CMementoMemoryManager::InvalidHdl))
		{
			sizeHint = mmm.GetHdlSize(*phData);
		}

		CByteOutputStream stm(&alloc, sizeHint);

		if (pTimeInfo)
		{
			Value("_tstamp", *pTimeInfo, 'tPhy');
			stm.PutTyped<CTimeValue>() = *pTimeInfo;
		}

		CNetwork::Get()->GetCompressionManager().StreamToBuffer(chunk, profile, *this, stm);

		bool take = false;

		if (Ok())
		{
			if (phData && m_bCommit)
			{
				take = true;

				if (*phData != CMementoMemoryManager::InvalidHdl)
				{
					if (mmm.GetHdlSize(*phData) == stm.GetSize())
					{
						if (0 == memcmp(mmm.PinHdl(*phData), mmm.PinHdl(alloc.GetHdl()), stm.GetSize()))
						{
							take = false;
						}
					}

					if (take)
					{
						mmm.FreeHdl(*phData);
						mmm.ResizeHdl(alloc.GetHdl(), stm.GetSize());
						*phData = alloc.GetHdl();
					}
					else
					{
						mmm.FreeHdl(alloc.GetHdl());
					}
				}
				else
				{
					mmm.ResizeHdl(alloc.GetHdl(), stm.GetSize());
					*phData = alloc.GetHdl();
				}
			}
			else
			{
				mmm.FreeHdl(alloc.GetHdl());
			}
		}
		else
		{
			mmm.FreeHdl(alloc.GetHdl());
		}

		return take ? eSCR_Ok_Updated : eSCR_Ok;
	}

	return eSCR_Failed;
}

SNetObjectID CNetInputSerializeImpl::ReadNetId()
{
	SNetObjectID id;

	if (m_multiplayer)
	{
		CNetCVars& netCVars = CNetCVars::Get();
		uint32 type = ReadBits(1);

		if (type == 0)
		{
			id.id = ReadBits(netCVars.net_numNetIDLowBitBits);
		}
		else
		{
			type = ReadBits(1);

			if (type == 0)
			{
				id.id = ReadBits(netCVars.net_numNetIDMediumBitBits) + netCVars.net_numNetIDLowBitIDs;
			}
			else
			{
				id.id = ReadBits(netCVars.net_numNetIDHighBitBits) + netCVars.net_netIDHighBitStart;
			}
		}

		if (id.id == netCVars.net_invalidNetID)
		{
			id.id = SNetObjectID::InvalidId;
		}
	}
	else
	{
		id.id = ReadBits(16);
	}

	m_channel->GetContextView()->ContextState()->Resaltify(id);

	return id;
}

CTimeValue CNetInputSerializeImpl::ReadTime(ETimeStream time)
{
	CTimeValue value;
	uint64 t = 0;
	uint32 r = ReadBits(2);

	switch (r)
	{
	case 0:
		t = ReadBits(14);
		break;

	case 1:
		t = ReadBits(18);
		break;

	case 2:
		t = ReadBits(26);
		break;

	case 3:
		t = ReadBits(6);
		t = (t << 32) | ReadBits(32);
		break;
	}

	switch (time)
	{
	case eTS_Network:
		break;

	case eTS_NetworkPing:
		break;

	case eTS_NetworkPong:
		break;

	case eTS_PongElapsed:
		break;

	case eTS_Physics:
	case eTS_RemoteTime:
		t = t << 5;
		break;
	}

	value.SetMilliSeconds((int64)t);

	return value;
}

void CNetInputSerializeImpl::ReadString(SSerializeString* string)
{
	char s[1024];

	for (int i = 0; i < 1024; i++)
	{
		s[i] = (char)ReadBits(8);

		if (s[i] == 0)
		{
			break;
		}
	}

	*string = s;
}

uint8 CNetInputSerializeImpl::GetBits(int num)
{
	uint8 value = 0;

	if (m_bufferPos < m_bufferSize)
	{
		int shift = (8 - num) - m_bufferPosBit;

		value = (shift < 0)
		        ? (m_buffer[m_bufferPos] << (-shift)) & ((1 << num) - 1)
		        : (m_buffer[m_bufferPos] >> shift) & ((1 << num) - 1);

		if (m_bufferPosBit + num > 7)
		{
			num -= (8 - m_bufferPosBit);

			m_bufferPos++;
			m_bufferPosBit = 0;

			if (num > 0)
			{
				if (m_bufferPos < m_bufferSize)
				{
					value |= (m_buffer[m_bufferPos] >> (8 - num));

					m_bufferPosBit += num;
				}
				else
				{
					Failed();
				}
			}
		}
		else
		{
			m_bufferPosBit += num;
		}
	}
	else
	{
		Failed();
	}

	return value;
}

uint32 CNetInputSerializeImpl::ReadBits(int num)
{
	int numBytes = ((num & 7) == 0) ? num >> 3 : (num >> 3) + 1;
	uint32 value = 0;

	switch (numBytes)
	{
	case 4:
		value |= GetBits(num - 24) << 24;
	case 3:
		value |= GetBits(num >= 24 ? 8 : num - 16) << 16;
	case 2:
		value |= GetBits(num >= 16 ? 8 : num - 8) << 8;
	case 1:
		value |= GetBits(num >= 8 ? 8 : num);
	}

	return value;
}

void CNetInputSerializeImpl::SetNetChannel(CNetChannel* channel)
{
	CNetContext* pNetContext = channel ? channel->GetContextView()->Context() : NULL;

	m_channel = channel;
	m_multiplayer = pNetContext ? pNetContext->IsMultiplayer() : false;
}

#endif
