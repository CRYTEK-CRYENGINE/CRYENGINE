// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File: CTPEndpoint.cpp
//  Description: non-sequential receive protocol
//
//	History:
//	-July 25,2001:Created by Alberto Demichelis
//  -July 20,2004:Refactored by Craig Tiller
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CTPEndpoint.h"
#include "Streams/CommStream.h"
#include "Serialize.h"
#include "NetChannel.h"
#include "Network.h"
#include <CrySystem/ITimer.h>
#include "DebugKit/DebugKit.h"
#if USE_HIGH_PRIORITY_ASPECT_HACK
	#include <CrySystem/ISystem.h>
	#include <CryEntitySystem/IEntitySystem.h>
#endif // USE_HIGH_PRIORITY_ASPECT_HACK

#include "Context/ContextView.h"
#include "Config.h"
#include <CrySystem/ITextModeConsole.h>
#include "Serialize.h"

#if VERBOSE_MALFORMED_PACKET_REPORTS
	#define MALFORMED_PACKET_REPORT(fmt, ...) NetWarning(fmt, __VA_ARGS__)
#else
	#define MALFORMED_PACKET_REPORT(fmt, ...) do \
	  ; while (false)
#endif

#define PACKET_RESERVED_BYTES (32)

#if LOG_INCOMING_MESSAGES
	#define LOG_INCOMING_MESSAGE(bitMask, fmt, ...) if (0 == (CNetCVars::Get().LogNetMessages & (bitMask))); else \
	  NetLog(fmt, __VA_ARGS__)
#elif ENABLE_CORRUPT_PACKET_DUMP
	#define LOG_INCOMING_MESSAGE(bitMask, fmt, ...) if (!CNetCVars::Get().doingPacketReplay()); else \
	  NetLog(fmt, __VA_ARGS__)
#else
	#define LOG_INCOMING_MESSAGE(bitMask, fmt, ...) do \
	  ; while (false)
#endif

#if ENABLE_DEBUG_PACKET_DATA_SIZE
SDebugPacketDataSize g_debugPacketDataSize;
#endif

static const SNetMessageDef* g_processingMessage = 0;

struct SProcessingMessage
{
public:
	SProcessingMessage(const SNetMessageDef* pDef)
	{
		m_prior = g_processingMessage;
		g_processingMessage = pDef;
	}

	~SProcessingMessage()
	{
		g_processingMessage = m_prior;
	}

private:
	const SNetMessageDef* m_prior;
};

// get the 64 bit representation of a time value
inline uint64 GetIntRepresentationOfTime(CTimeValue value)
{
	return (uint64) value.GetMilliSecondsAsInt64();
}
inline CTimeValue GetTimeRepresentationOfInt(uint64 value)
{
	CTimeValue out;
	out.SetMilliSeconds(value);
	return out;
}
/*
   static const char * ReliabilityMessage( INetMessage * pMsg )
   {
   ENetReliabilityType nrt = pMsg->GetDef()->reliability;
   switch (nrt)
   {
   case eNRT_ReliableOrdered:
    return "reliable-ordered";
   case eNRT_ReliableUnordered:
    return "reliable-unordered";
   case eNRT_UnreliableOrdered:
    return "unreliable-ordered";
   default:
    return "unknown-reliability";
   }
   }
 */
//static const uint32 TimestampBits = 64;
static const int LogMaxMessagesPerPacket = 16;
static const int MaxMessagesPerPacket = 1 << LogMaxMessagesPerPacket;

static const int SequenceNumberBits = 8;
static const uint32 SequenceNumberMask = (1u << SequenceNumberBits) - 1u;
static const uint32 SequenceNumberDiameter = 1u << SequenceNumberBits;
static const uint32 SequenceNumberRadius = 1u << (SequenceNumberBits - 1);

static const CTimeValue IncomingTimeoutLength = 0.033f;

#if ENABLE_DEBUG_KIT || ENABLE_CORRUPT_PACKET_DUMP
static void DumpBytes(const uint8* p, size_t len)
{
	char l[256];

	char* o = l;
	for (size_t i = 0; i < len; i++)
	{
		o += sprintf(o, "%.2x ", p[i]);
		if ((i & 31) == 31)
		{
			NetLog("%s", l);
			o = l;
		}
	}
	if (len & 31)
		NetLog("%s", l);
}
#endif

//
// SBigEndpointState
//
#if ENCRYPTION_RIJNDAEL
CCTPEndpoint::SBigEndpointState::SBigEndpointState(size_t acks, CMessageMapper& msgMapper, Rijndael::Direction dir, const uint8* key, uint8* initVec)
#elif ENCRYPTION_STREAMCIPHER
CCTPEndpoint::SBigEndpointState::SBigEndpointState(size_t acks, CMessageMapper & msgMapper, const uint8 * key, int keyLen)
#else
CCTPEndpoint::SBigEndpointState::SBigEndpointState(size_t acks, CMessageMapper & msgMapper)
#endif
#if USE_ARITHSTREAM
	:
	m_AckAlphabet(acks),
	m_MsgAlphabet(msgMapper.GetNumberOfMsgIds()),
	m_ArithModel()
#endif
{
	++g_objcnt.bigEndpointState;

#if ENCRYPTION_RIJNDAEL
	m_crypt.init(Rijndael::CBC, dir, key, Rijndael::Key32Bytes, initVec);
#elif ENCRYPTION_STREAMCIPHER
	m_crypt.Init(key, keyLen);
#endif

#if !USE_ARITHSTREAM
	const SNetMessageDef* pDef;
	size_t proto;

	m_currentTable = eMIDT_Normal;

	// eMIDT_Normal data
	memset(&m_normalData, 0xff, sizeof(m_normalData));
	m_normalData.idBitSize = IntegerLog2_RoundUp(msgMapper.GetNumberOfMsgIds());

	// 0
	if (msgMapper.LookupMessage("CClientContextView:BeginUpdateObject", &pDef, &proto))
	{
		m_normalData.lowBitIDs[0] = msgMapper.GetMsgId(pDef);
	}
	if (msgMapper.LookupMessage("CServerContextView:BeginUpdateObject", &pDef, &proto))
	{
		m_normalData.lowBitIDs[0] = msgMapper.GetMsgId(pDef);
	}

	// eMIDT_UpdateObject data
	memset(&m_updateObjectData, 0xff, sizeof(m_updateObjectData));

	// 0
	if (msgMapper.LookupMessage("CClientContextView:EndUpdateObject", &pDef, &proto))
	{
		m_updateObjectData.lowBitIDs[0] = msgMapper.GetMsgId(pDef);
	}
	if (msgMapper.LookupMessage("CServerContextView:EndUpdateObject", &pDef, &proto))
	{
		m_updateObjectData.lowBitIDs[0] = msgMapper.GetMsgId(pDef);
	}

	// 10
	if (msgMapper.LookupMessage("CClientContextView:UpdateAspect31", &pDef, &proto))
	{
		m_updateObjectData.mediumBitIDs[0] = msgMapper.GetMsgId(pDef);
	}
	if (msgMapper.LookupMessage("CServerContextView:UpdateAspect31", &pDef, &proto))
	{
		m_updateObjectData.mediumBitIDs[0] = msgMapper.GetMsgId(pDef);
	}

	// 11 + UPDATEOBJECT_NUM_HIGH_BIT_MESSAGE_ID_BITS
	for (uint32 i = 0; i < UPDATEOBJECT_NUM_HIGH_BIT_MESSAGE_IDS; i++)
	{
		char idName[64];

		cry_sprintf(idName, "CClientContextView:UpdateAspect%d", i);

		if (msgMapper.LookupMessage(idName, &pDef, &proto))
		{
			m_updateObjectData.highBitIDs[i] = msgMapper.GetMsgId(pDef);
		}

		cry_sprintf(idName, "CServerContextView:UpdateAspect%d", i);

		if (msgMapper.LookupMessage(idName, &pDef, &proto))
		{
			m_updateObjectData.highBitIDs[i] = msgMapper.GetMsgId(pDef);
		}
	}

	// MessageID Table change IDs
	memset(m_messageIDTableChangeData, 0xff, sizeof(m_messageIDTableChangeData));

	if (msgMapper.LookupMessage("CClientContextView:EndUpdateObject", &pDef, &proto))
	{
		m_messageIDTableChangeData[0].id = msgMapper.GetMsgId(pDef);
		m_messageIDTableChangeData[0].table = eMIDT_Normal;
	}
	if (msgMapper.LookupMessage("CServerContextView:EndUpdateObject", &pDef, &proto))
	{
		m_messageIDTableChangeData[0].id = msgMapper.GetMsgId(pDef);
		m_messageIDTableChangeData[0].table = eMIDT_Normal;
	}

	if (msgMapper.LookupMessage("CClientContextView:BeginUpdateObject", &pDef, &proto))
	{
		m_messageIDTableChangeData[1].id = msgMapper.GetMsgId(pDef);
		m_messageIDTableChangeData[1].table = eMIDT_UpdateObject;
	}
	if (msgMapper.LookupMessage("CServerContextView:BeginUpdateObject", &pDef, &proto))
	{
		m_messageIDTableChangeData[1].id = msgMapper.GetMsgId(pDef);
		m_messageIDTableChangeData[1].table = eMIDT_UpdateObject;
	}

	if (msgMapper.LookupMessage("CClientContextView:BeginBindObject", &pDef, &proto))
	{
		m_messageIDTableChangeData[2].id = msgMapper.GetMsgId(pDef);
		m_messageIDTableChangeData[2].table = eMIDT_UpdateObject;
	}

	if (msgMapper.LookupMessage("CClientContextView:BeginBindStaticObject", &pDef, &proto))
	{
		m_messageIDTableChangeData[3].id = msgMapper.GetMsgId(pDef);
		m_messageIDTableChangeData[3].table = eMIDT_UpdateObject;
	}
#endif

	if (msgMapper.GetNumberOfMsgIds() == 1)
		return;
#if USE_ARITHSTREAM
	m_MsgAlphabet.RecalculateProbabilities();
#endif
}

#if USE_ARITHSTREAM
ILINE void CCTPEndpoint::SBigEndpointState::WriteMsgIDData(CCommOutputStream& stm, uint32 id)
{
	m_MsgAlphabet.WriteSymbol(stm, id);
	#if DOUBLE_WRITE_MESSAGE_HEADERS
	m_MsgAlphabet.WriteSymbol(stm, id);
	#endif
}

ILINE uint32 CCTPEndpoint::SBigEndpointState::ReadMsgIDData(CCommInputStream& stm)
{
	uint32 msg = m_MsgAlphabet.ReadSymbol(stm);
	#if DOUBLE_WRITE_MESSAGE_HEADERS
	uint32 msg2 = m_MsgAlphabet.ReadSymbol(stm);
	NET_ASSERT(msg == msg2);
	#endif

	return msg;
}
#else
ILINE void CCTPEndpoint::SBigEndpointState::WriteMsgIDData(CNetOutputSerializeImpl& stm, uint32 id, int numBits)
{
	stm.WriteBits(id, numBits);
	#if DOUBLE_WRITE_MESSAGE_HEADERS
	stm.WriteBits(id, numBits);
	#endif
}

ILINE uint32 CCTPEndpoint::SBigEndpointState::ReadMsgIDData(CNetInputSerializeImpl& stm, int numBits)
{
	uint32 msg = stm.ReadBits(numBits);
	#if DOUBLE_WRITE_MESSAGE_HEADERS
	uint32 msg2 = stm.ReadBits(numBits);
	NET_ASSERT(msg == msg2);
	#endif

	return msg;
}

ILINE void CCTPEndpoint::SBigEndpointState::SwitchTable(uint32 id)
{
	for (uint32 i = 0; i < NUM_MESSAGEID_TABLE_CHANGE_DATAS; i++)
	{
		if (m_messageIDTableChangeData[i].id == id)
		{
			m_currentTable = m_messageIDTableChangeData[i].table;
		}
	}
}
#endif

//void CCTPEndpoint::SBigEndpointState::GetMemoryStatistics(ICrySizer *pSizer)
//{
//	SIZER_COMPONENT_NAME(pSizer, "CCTPEndpoint::SBigEndpointState");
//	m_ArithModel.GetMemoryStatistics(pSizer);
//	pSizer->AddObject( &m_AckAlphabet, m_AckAlphabet.GetMemorySize() );
//	pSizer->AddObject( &m_MsgAlphabet, m_MsgAlphabet.GetMemorySize() );
//}

//void CCTPEndpoint::CBigEndpointStateManager::GetMemoryStatistics(ICrySizer *pSizer)
//{
//	SIZER_COMPONENT_NAME(pSizer, "CCTPEndpoint::CBigEndpointStateManager");
//	for (size_t i=0; i<m_vBuffer.size(); i++)
//		m_vBuffer[i]->GetMemoryStatistics(pSizer);
//}

//
// CCTPEndpoint::S*State
//

// Reset
ILINE void CCTPEndpoint::CState::Reset(CBigEndpointStateManager* pBigMgr)
{
	m_nAcks = 0;
	m_nAckedPackets = 0;
	m_nPackets = 0;
	m_nAcksThisPacket = 0;
	FreeState(pBigMgr);
}

ILINE void CCTPEndpoint::COutputState::Reset(CBigEndpointStateManager* pBigMgr)
{
	CState::Reset(pBigMgr);
	//m_queue.AckMessages( &*m_vpSentMessages.begin(), m_vpSentMessages.size(), false );
	NET_ASSERT(m_headSentMessage == InvalidSentElem);
	NET_ASSERT(!m_nStateBlockers);
	m_headSentMessage = InvalidSentElem;
	m_bAvailable = true;
	m_nStateBlockers = 0;
}

ILINE void CCTPEndpoint::CInputState::Reset(CBigEndpointStateManager* pBigMgr)
{
	CState::Reset(pBigMgr);
	m_nValidSeq = 0;
}

#if DEBUG_ENDPOINT_LOGIC
// Dump
void CCTPEndpoint::CState::Dump(FILE* f) const
{
	NET_ASSERT(m_pBigState);

	fprintf(f, " %.4d %.4d %.4d\n", m_nPackets, m_nAcks, m_nAckedPackets);
	fprintf(f, "AckAlphabet:\n");

	if (!m_pBigState)
		return;
	m_pBigState->m_AckAlphabet.DumpCountsToFile(f);
	fprintf(f, "MsgAlphabet:\n");
	m_pBigState->m_MsgAlphabet.DumpCountsToFile(f);
	fprintf(f, "TextAlphabet:\n");
	m_pBigState->m_ArithModel.DumpCountsToFile(f);
}
#endif

// GetMemoryStatistics
//void CCTPEndpoint::CState::GetMemoryStatistics(ICrySizer *pSizer)
//{
//	SIZER_COMPONENT_NAME(pSizer, "CCTPEndpoint::CState");
//	if (m_pBigState)
//		m_pBigState->GetMemoryStatistics( pSizer );
//}

//void CCTPEndpoint::COutputState::GetMemoryStatistics(ICrySizer *pSizer)
//{
//	SIZER_COMPONENT_NAME(pSizer, "CCTPEndpoint::COutputState");
//	CState::GetMemoryStatistics( pSizer );
//	pSizer->AddContainer( m_vpSentMessages );
//}

// Clone
ILINE void CCTPEndpoint::CState::Clone(CBigEndpointStateManager* pBigMgr,
                                       const CState& s)
{
	m_nAcks = s.m_nAcks;
	m_nAckedPackets = s.m_nAckedPackets;
	m_nPackets = s.m_nPackets + 1;
	m_nAcksThisPacket = 0;

	NET_ASSERT(s.m_pBigState);
	NET_ASSERT(!m_pBigState);
	m_pBigState = pBigMgr->Clone(s.m_pBigState);
#if USE_ARITHSTREAM
	m_pBigState->m_AckAlphabet.RecalculateProbabilities();
	m_pBigState->m_MsgAlphabet.RecalculateProbabilities();
	m_pBigState->m_ArithModel.RecalculateProbabilities();
#endif
}

void CCTPEndpoint::COutputState::Clone(CBigEndpointStateManager* pBigMgr,
                                       const COutputState& s)
{
	CCTPEndpoint::CState::Clone(pBigMgr, s);
	NET_ASSERT(m_headSentMessage == InvalidSentElem);
	m_nStateBlockers = 0;
}

void CCTPEndpoint::CInputState::Clone(CBigEndpointStateManager* pBigMgr,
                                      const CCTPEndpoint::CInputState& s, uint32 nSeq)
{
	CCTPEndpoint::CState::Clone(pBigMgr, s);

	m_nValidSeq = nSeq;
}

// acks & nacks
ILINE uint32 CCTPEndpoint::CState::AddAck(bool bAck)
{
	if (bAck)
		m_nAcks++;
	m_nAcksThisPacket++;
	m_nAckedPackets++;
	return m_nAckedPackets;
}

#if USE_ARITHSTREAM
void CCTPEndpoint::CState::WriteAck(CCommOutputStream& stm, bool bAck, CStatsCollector* pStats)
#else
void CCTPEndpoint::CState::WriteAck(CNetOutputSerializeImpl& stm, bool bAck, CStatsCollector* pStats)
#endif
{
#if DEBUG_STREAM_INTEGRITY
	stm.WriteBits(35, 8);
#endif

	uint32 ackType;

	if (bAck)
	{
		ackType = ACK_TYPE_ACK;
	}
	else
	{
		ackType = ACK_TYPE_NACK;
	}

#if USE_ARITHSTREAM
	m_pBigState->m_AckAlphabet.WriteSymbol(stm, ackType);
#else
	stm.WriteBits(ackType, ACK_TYPE_NUM_BITS);
#endif

	AddAck(bAck);
}

#if USE_ARITHSTREAM
void CCTPEndpoint::CState::WriteEndAcks(CCommOutputStream& stm, bool returnAckNeeded, CStatsCollector* pStats)
#else
void CCTPEndpoint::CState::WriteEndAcks(CNetOutputSerializeImpl& stm, bool returnAckNeeded, CStatsCollector* pStats)
#endif
{
#if DEBUG_STREAM_INTEGRITY
	stm.WriteBits(35, 8);
#endif

	uint32 ackType;

	if (returnAckNeeded)
	{
		ackType = ACK_TYPE_END_RETURN_NEEDED;
	}
	else
	{
		ackType = ACK_TYPE_END_RETURN_NOT_NEEDED;
	}

#if USE_ARITHSTREAM
	m_pBigState->m_AckAlphabet.WriteSymbol(stm, ackType);
#else
	stm.WriteBits(ackType, ACK_TYPE_NUM_BITS);
#endif
}

#if USE_ARITHSTREAM
bool CCTPEndpoint::CState::ReadAck(CCommInputStream& stm, bool& bAck, uint32& nSeq, bool& recvAckNeeded)
#else
bool CCTPEndpoint::CState::ReadAck(CNetInputSerializeImpl& stm, bool& bAck, uint32& nSeq, bool& recvAckNeeded)
#endif
{
#if DEBUG_STREAM_INTEGRITY
	uint8 stmBits = stm.ReadBits(8);
	NET_ASSERT(35 == stmBits);
	if (35 != stmBits)
		return false;
#endif

#if USE_ARITHSTREAM
	unsigned symbol = m_pBigState->m_AckAlphabet.ReadSymbol(stm);
#else
	uint8 symbol = stm.ReadBits(ACK_TYPE_NUM_BITS);
#endif

	bool bRet = true;
	switch (symbol)
	{
	case ACK_TYPE_NACK:
		bAck = false;
		nSeq = AddAck(false);
		break;
	case ACK_TYPE_ACK:
		bAck = true;
		nSeq = AddAck(true);
		break;
	case ACK_TYPE_END_RETURN_NEEDED:
		recvAckNeeded = true;
		bRet = false;
		break;
	case ACK_TYPE_END_RETURN_NOT_NEEDED:
		recvAckNeeded = false;
		bRet = false;
		break;
	default:
		NET_ASSERT(false);
	}

	return bRet;
}

// message id's
#if USE_ARITHSTREAM
ILINE bool CCTPEndpoint::CState::WriteMsgId(CCommOutputStream& stm, uint32 id, CStatsCollector* pStats, const char* name)
#else
ILINE bool CCTPEndpoint::CState::WriteMsgId(CNetOutputSerializeImpl& stm, uint32 id, CStatsCollector* pStats, const char* name)
#endif
{
	if (!m_pBigState)
		return false;

#if STATS_COLLECTOR && ENABLE_ACCURATE_BANDWIDTH_PROFILING
	float sz = stm.GetBitSize();
#endif

#if USE_ARITHSTREAM
	debugPacketDataSizeStartMessageIDData(id, stm.GetBitSize(), INVALID_MESSAGEIDTABLE);
#else
	debugPacketDataSizeStartMessageIDData(id, stm.GetBitSize(), m_pBigState->m_currentTable);
#endif

#if ENABLE_DEBUG_KIT
	DEBUGKIT_ANNOTATION(string("message: ") + name);
#endif
#if DEBUG_STREAM_INTEGRITY
	stm.WriteBits(73, 8);
#endif
	//	NetLog( "msg %s(%d): %f", name, id, m_pBigState->m_MsgAlphabet.EstimateSymbolSizeInBits(id) );

#if USE_ARITHSTREAM
	m_pBigState->WriteMsgIDData(stm, id);
#else

	if (m_pBigState->m_currentTable == eMIDT_Normal)
	{
		uint32 i;

		for (i = 0; i < NORMAL_NUM_LOW_BIT_MESSAGE_IDS; i++)
		{
			if (m_pBigState->m_normalData.lowBitIDs[i] == id)
			{
				m_pBigState->WriteMsgIDData(stm, i, NORMAL_NUM_LOW_BIT_MESSAGE_ID_BITS);
				break;
			}
		}

		if (i == NORMAL_NUM_LOW_BIT_MESSAGE_IDS)
		{
			m_pBigState->WriteMsgIDData(stm, (1 << NORMAL_NUM_LOW_BIT_MESSAGE_ID_BITS) - 1, NORMAL_NUM_LOW_BIT_MESSAGE_ID_BITS);
			m_pBigState->WriteMsgIDData(stm, id, m_pBigState->m_normalData.idBitSize);
		}
	}
	else
	{
		uint32 i;

		for (i = 0; i < UPDATEOBJECT_NUM_LOW_BIT_MESSAGE_IDS; i++)
		{
			if (m_pBigState->m_updateObjectData.lowBitIDs[i] == id)
			{
				m_pBigState->WriteMsgIDData(stm, i, UPDATEOBJECT_NUM_LOW_BIT_MESSAGE_ID_BITS);
				break;
			}
		}

		if (i == UPDATEOBJECT_NUM_LOW_BIT_MESSAGE_IDS)
		{
			m_pBigState->WriteMsgIDData(stm, (1 << UPDATEOBJECT_NUM_LOW_BIT_MESSAGE_ID_BITS) - 1, UPDATEOBJECT_NUM_LOW_BIT_MESSAGE_ID_BITS);

			for (i = 0; i < UPDATEOBJECT_NUM_MEDIUM_BIT_MESSAGE_IDS; i++)
			{
				if (m_pBigState->m_updateObjectData.mediumBitIDs[i] == id)
				{
					m_pBigState->WriteMsgIDData(stm, i, UPDATEOBJECT_NUM_MEDIUM_BIT_MESSAGE_ID_BITS);
					break;
				}
			}

			if (i == UPDATEOBJECT_NUM_MEDIUM_BIT_MESSAGE_IDS)
			{
				m_pBigState->WriteMsgIDData(stm, (1 << UPDATEOBJECT_NUM_MEDIUM_BIT_MESSAGE_ID_BITS) - 1, UPDATEOBJECT_NUM_MEDIUM_BIT_MESSAGE_ID_BITS);

				for (i = 0; i < UPDATEOBJECT_NUM_HIGH_BIT_MESSAGE_IDS; i++)
				{
					if (m_pBigState->m_updateObjectData.highBitIDs[i] == id)
					{
						m_pBigState->WriteMsgIDData(stm, i, UPDATEOBJECT_NUM_HIGH_BIT_MESSAGE_ID_BITS);
						break;
					}
				}

				if (i == UPDATEOBJECT_NUM_HIGH_BIT_MESSAGE_IDS)
				{
					m_pBigState->WriteMsgIDData(stm, (1 << UPDATEOBJECT_NUM_HIGH_BIT_MESSAGE_ID_BITS) - 1, UPDATEOBJECT_NUM_HIGH_BIT_MESSAGE_ID_BITS);
					m_pBigState->WriteMsgIDData(stm, id, m_pBigState->m_normalData.idBitSize);
				}
			}
		}
	}

	m_pBigState->SwitchTable(id);
#endif

#if DEBUG_STREAM_INTEGRITY
	stm.WriteBits(77, 8);
#endif

	debugPacketDataSizeEndData(eDPDST_MessageID, stm.GetBitSize());

#if STATS_COLLECTOR && ENABLE_ACCURATE_BANDWIDTH_PROFILING
	STATS.Message(name, stm.GetBitSize() - sz);
#endif

	return true;
}

#if USE_ARITHSTREAM
ILINE uint32 CCTPEndpoint::CState::ReadMsgId(CCommInputStream& stm)
#else
ILINE uint32 CCTPEndpoint::CState::ReadMsgId(CNetInputSerializeImpl& stm)
#endif
{
#if DEBUG_STREAM_INTEGRITY
	uint32 check = stm.ReadBits(8);
	NET_ASSERT(check == 73);
#endif

#if USE_ARITHSTREAM
	uint32 msg = m_pBigState->ReadMsgIDData(stm);
#else
	uint32 msg;

	if (m_pBigState->m_currentTable == eMIDT_Normal)
	{
		msg = m_pBigState->ReadMsgIDData(stm, NORMAL_NUM_LOW_BIT_MESSAGE_ID_BITS);

		if (msg < NORMAL_NUM_LOW_BIT_MESSAGE_IDS)
		{
			msg = m_pBigState->m_normalData.lowBitIDs[msg];
		}
		else
		{
			msg = m_pBigState->ReadMsgIDData(stm, m_pBigState->m_normalData.idBitSize);
		}
	}
	else
	{
		msg = m_pBigState->ReadMsgIDData(stm, UPDATEOBJECT_NUM_LOW_BIT_MESSAGE_ID_BITS);

		if (msg < UPDATEOBJECT_NUM_LOW_BIT_MESSAGE_IDS)
		{
			msg = m_pBigState->m_updateObjectData.lowBitIDs[msg];
		}
		else
		{
			msg = m_pBigState->ReadMsgIDData(stm, UPDATEOBJECT_NUM_MEDIUM_BIT_MESSAGE_ID_BITS);

			if (msg < UPDATEOBJECT_NUM_MEDIUM_BIT_MESSAGE_IDS)
			{
				msg = m_pBigState->m_updateObjectData.mediumBitIDs[msg];
			}
			else
			{
				msg = m_pBigState->ReadMsgIDData(stm, UPDATEOBJECT_NUM_HIGH_BIT_MESSAGE_ID_BITS);

				if (msg < UPDATEOBJECT_NUM_HIGH_BIT_MESSAGE_IDS)
				{
					msg = m_pBigState->m_updateObjectData.highBitIDs[msg];
				}
				else
				{
					msg = m_pBigState->ReadMsgIDData(stm, m_pBigState->m_normalData.idBitSize);
				}
			}
		}
	}

	m_pBigState->SwitchTable(msg);
#endif

	//	NetLog("gotmsg: %d", msg);
#if DEBUG_STREAM_INTEGRITY
	check = stm.ReadBits(8);
	NET_ASSERT(check == 77);
#endif

	return msg;
}

//
// CCTPEndpoint
//

static CDefaultStreamAllocator s_allocator;

CCTPEndpoint::CCTPEndpoint(CMementoMemoryManagerPtr pMMM) :
	m_outputStreamImpl(m_assemblyBuffer + 1, sizeof(m_assemblyBuffer) - 1), // +1 to account for sequence bytes
	m_outputStream(m_outputStreamImpl),
	m_emptyMode(false),
	m_queue(CNetwork::Get()->GetMessageQueueConfig()),
	m_pMMM(pMMM),
	m_freeSentElem(CCTPEndpoint::InvalidSentElem)
{
#if LOG_TFRC
	char filename[sizeof("tfrc_endpoint_12345678.log")];
	cry_sprintf(filename, "tfrc_endpoint_%08x.log", (uint32)this);
	m_log_tfrc = fxopen(filename, "wt");
#endif

#if DEBUG_ENDPOINT_LOGIC
	m_log_output = NULL;
	m_log_input = NULL;
#endif
	Init(NULL);
}

CCTPEndpoint::~CCTPEndpoint()
{
	SCOPED_GLOBAL_LOCK;
	MMM_REGION(m_pMMM);

#if LOG_TFRC
	if (m_log_tfrc) fclose(m_log_tfrc);
#endif

#if DEBUG_ENDPOINT_LOGIC
	if (m_log_output) fclose(m_log_output);
	if (m_log_input) fclose(m_log_input);
#endif
	for (size_t i = 0; i < WHOLE_SEQ; i++)
	{
		m_vInputState[i].FreeState(&m_bigStateMgrInput);
		m_vOutputState[i].FreeState(&m_bigStateMgrOutput);
	}
	while (!m_incomingPackets.empty())
	{
		MMM().FreeHdl(m_incomingPackets.begin()->second.hdl);
		m_incomingPackets.erase(m_incomingPackets.begin());
	}

	m_previouslyReceivedMessages.Dump("received", false);
	m_previouslySentMessages.Dump("sent", true);

	m_bigStateMgrInput.FlushAll();
	m_bigStateMgrOutput.FlushAll();
}

void CCTPEndpoint::Init(CNetChannel* pParent)
{
	MMM_REGION(m_pMMM);
	// these are the interfaces we use
	m_pParent = pParent;
	m_queue.SetParent(this);

	m_entityId = 0;

#if DEBUG_ENDPOINT_LOGIC
	static int N = 0;
	if (m_log_output) fclose(m_log_output);
	if (m_log_input) fclose(m_log_input);
	m_log_output = NULL;
	m_log_input = NULL;
	if (pParent)
	{
		char buf[30];
		cry_sprintf(buf, "output.%d.log", ++N);
		m_log_output = fxopen(buf, "wt");
		cry_sprintf(buf, "input.%d.log", N);
		m_log_input = fxopen(buf, "wt");
		NET_ASSERT(m_log_input);
		NET_ASSERT(m_log_output);
	}
#endif

	// setup message
	struct SProtocolBuilder : public IProtocolBuilder
	{
		std::vector<SNetProtocolDef> sending;
		std::vector<SNetProtocolDef> receiving;
		std::vector<SSinkInfo>       sinks;
		virtual void AddMessageSink(INetMessageSink* pSink,
		                            const SNetProtocolDef& protocolSending,
		                            const SNetProtocolDef& protocolReceiving)
		{
			sinks.push_back(pSink);
			sending.push_back(protocolSending);
			receiving.push_back(protocolReceiving);
		}
	};
	SProtocolBuilder protocolBuilder;
	if (pParent)
		pParent->DefineProtocol(&protocolBuilder);

	if (!protocolBuilder.sending.empty())
		m_OutputMapper.Reset(eIM_LastInternalMessage,
		                     &protocolBuilder.sending[0], protocolBuilder.sending.size());
	else
		m_OutputMapper.Reset(eIM_LastInternalMessage, 0, 0);

	if (!protocolBuilder.receiving.empty())
		m_InputMapper.Reset(eIM_LastInternalMessage,
		                    &protocolBuilder.receiving[0], protocolBuilder.receiving.size());
	else
		m_InputMapper.Reset(eIM_LastInternalMessage, 0, 0);

	m_MessageSinks.swap(protocolBuilder.sinks);

	// initialize our state array
	for (uint32 i = 0; i < WHOLE_SEQ; i++)
	{
		m_vInputState[i].Reset(&m_bigStateMgrInput);
		m_vOutputState[i].Reset(&m_bigStateMgrOutput);
	}

#if ENCRYPTION_RIJNDAEL
	m_vInputState[0].CreateState(&m_bigStateMgrInput, m_InputMapper, Rijndael::Decrypt, m_pParent ? m_pParent->GetPrivateKey().key : NULL, 0);
	m_vOutputState[0].CreateState(&m_bigStateMgrOutput, m_OutputMapper, Rijndael::Encrypt, m_pParent ? m_pParent->GetPrivateKey().key : NULL, 0);
#elif ENCRYPTION_STREAMCIPHER
	m_vInputState[0].CreateState(&m_bigStateMgrInput, m_InputMapper, m_pParent ? m_pParent->GetPrivateKey().key : NULL, CExponentialKeyExchange::KEY_SIZE);
	m_vOutputState[0].CreateState(&m_bigStateMgrOutput, m_OutputMapper, m_pParent ? m_pParent->GetPrivateKey().key : NULL, CExponentialKeyExchange::KEY_SIZE);
#else
	m_vInputState[0].CreateState(&m_bigStateMgrInput, m_InputMapper);
	m_vOutputState[0].CreateState(&m_bigStateMgrOutput, m_OutputMapper);
#endif

	m_vOutputState[0].Unavailable();

	// initialize sequence numbers
	m_nOutputSeq = 0;
	m_nInputSeq = 1;
	m_nInputAck = 0;
	m_nReliableSeq = 0;
	m_bReliableWait = false;
	m_nLastBasisSeq = 0;
	m_nUrgentAcks = 0;
	m_receivedPacketSinceBackoff = true;

	// initialize debug counters
	m_nReceived = 0;
	m_nSent = 0;
	m_nDropped = 0;
	m_nReordered = 0;
	m_nQueued = 0;
	m_nRepeated = 0;

	m_nPQTimeout = 0;
	m_nPQReady = 0;

	// setup the ack structure
	m_nFrontAck = 1;
	m_dqAcks.clear();
	m_recvAckNeeded = false;
	m_sentAckNeeded = false;

	m_nStateBlockers = 0;
}

void CCTPEndpoint::MarkNotUserSink(INetMessageSink* pSink)
{
	for (size_t i = 0; i < m_MessageSinks.size(); i++)
		if (m_MessageSinks[i].pSink == pSink)
			m_MessageSinks[i].bCryNetwork = true;
}

void CCTPEndpoint::GetMemoryStatistics(ICrySizer* pSizer, bool countingThis)
{
	SIZER_COMPONENT_NAME(pSizer, "CCTPEndpoint");
	MMM_REGION(m_pMMM);

	if (countingThis)
		if (!pSizer->Add(*this))
			return;

	for (size_t i = 0; i < WHOLE_SEQ; i++)
	{
		m_vInputState[i].GetMemoryStatistics(pSizer);
		m_vOutputState[i].GetMemoryStatistics(pSizer);
	}

	m_bigStateMgrInput.GetMemoryStatistics(pSizer);
	m_bigStateMgrOutput.GetMemoryStatistics(pSizer);

	pSizer->AddContainer(m_incomingPackets);
	pSizer->AddObject(&m_timedOutPackets, m_timedOutPackets.size() * sizeof(uint32));

	m_queue.GetMemoryStatistics(pSizer);

	m_OutputMapper.GetMemoryStatistics(pSizer);
	m_InputMapper.GetMemoryStatistics(pSizer);
	pSizer->AddContainer(m_MessageSinks);
	pSizer->AddContainer(m_listeners);
	pSizer->AddContainer(m_dqAcks);

	m_outputStreamImpl.GetMemoryStatistics(pSizer);
	//m_outputStream.GetMemoryStatistics(pSizer);

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CCTPEndpoint.TMemHdl");
		for (TIncomingPacketMap::const_iterator it = m_incomingPackets.begin(); it != m_incomingPackets.end(); ++it)
			MMM().AddHdlToSizer(it->second.hdl, pSizer);
	}

	m_PacketRateCalculator.GetMemoryStatistics(pSizer);
}

void CCTPEndpoint::Reset()
{
	Init(m_pParent);
}

void CCTPEndpoint::PerformRegularCleanup()
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);
	MMM_REGION(m_pMMM);

	m_bigStateMgrInput.PerformRegularCleanup();
	m_bigStateMgrOutput.PerformRegularCleanup();
}

bool CCTPEndpoint::CallAddSubstitute(INetSendable* pMsg, int numAfterHandle, const SSendableHandle* afterHandle, SSendableHandle* handle, bool subs)
{
	if (m_nStateBlockers && pMsg->CheckParallelFlag(eMPF_StateChange))
		NET_ASSERT(!"Can't send state change with blocking messages still about");

	if (m_emptyMode)
	{
#if LOG_MESSAGE_DROPS
		if (CNetCVars::Get().LogDroppedMessages())
			NetLog("REJECTED MESSAGE: %s", pMsg->GetDescription());
#endif
		pMsg->UpdateState(0, eNSSU_Rejected);
		return false;
	}
	else
	{
		switch (m_queue.AddSendable(pMsg, numAfterHandle, afterHandle, handle, subs))
		{
		case eMQASR_Failed:
			return false;
		case eMQASR_Ok_BecomeAlerted:
			BroadcastMessage(SCTPEndpointEvent(eCEE_BecomeAlerted));
		// fall through
		case eMQASR_Ok:
			return true;
		default:
			NET_ASSERT(!"unknown return code from CMessageQueue::AddSendable");
			return false;
		}
	}
}

bool CCTPEndpoint::RemoveSendable(SSendableHandle handle)
{
	return m_queue.RemoveSendable(handle);
}

INetSendablePtr CCTPEndpoint::FindSendable(SSendableHandle handle)
{
	return m_queue.FindSendable(handle);
}

CTimeValue CCTPEndpoint::GetNextUpdateTime(CTimeValue nTime)
{
	CTimeValue backoffTime;
	if (GetBackoffTime(backoffTime, false))
		return backoffTime;

	// search for when we can get a packet out
	int age = m_nOutputSeq - m_nInputAck;
	return m_PacketRateCalculator.GetNextPacketTime(age, IsIdle());
}

void CCTPEndpoint::UpdatePendingQueue(CTimeValue nTime, bool isDisconnecting)
{
	MMM_REGION(m_pMMM);

	// check to see if we have anything that's timed out
	uint32 lastTimedOut = 0;
	for (TIncomingPacketMap::reverse_iterator it = m_incomingPackets.rbegin(); it != m_incomingPackets.rend(); ++it)
	{
		if (nTime - it->second.when > IncomingTimeoutLength)
		{
			lastTimedOut = it->first;
			break;
		}
	}
	// if something has timed out, process all packets up to and including it
	// also keep going as long as we're processing packets sequentially
	while (!m_incomingPackets.empty())
	{
		bool cont = false;
		if (m_incomingPackets.begin()->first <= lastTimedOut)
		{
			cont = true;
			m_nPQTimeout++;
#if ENABLE_DEBUG_KIT
			if (CNetCVars::Get().EndpointPendingQueueLogging)
				NetLog("PQ: process %d due to %d timing out", m_incomingPackets.begin()->first, lastTimedOut);
#endif
		}
		else if (m_incomingPackets.begin()->first == m_nInputSeq + 1)
		{
			cont = true;
			m_nPQReady++;
#if ENABLE_DEBUG_KIT
			if (CNetCVars::Get().EndpointPendingQueueLogging)
				NetLog("PQ: process %d as it's in order", m_nInputSeq);
#endif
		}
		if (!cont)
			break;
		{
			CAutoFreeHandle freeHdl(m_incomingPackets.begin()->second.hdl);
			ProcessPacket(nTime, freeHdl, false, false);
		}
		m_incomingPackets.erase(m_incomingPackets.begin());
	}
}

void CCTPEndpoint::SendPacketsIfNecessary(CTimeValue nTime, bool isDisconnecting, bool bAllowUserSend, bool bForce, bool bFlush)
{
	static CTimeValue lastSendTime;
	if (IsBackingOff())
		return;

	// now see if we need to send some packets
	bool bResend = false;

	// if we're disconnecting, we're somewhat more stringent about what we do
	bFlush &= !isDisconnecting;
	bForce |= isDisconnecting;
	bAllowUserSend &= !isDisconnecting;

	bForce |= bFlush;
	do
	{
		// don't ever send a new packet if we would exhaust sequence numbers
		NET_ASSERT(m_nOutputSeq >= m_nInputAck);
		if (m_nOutputSeq + 1 >= m_nInputAck + WHOLE_SEQ)
		{
			if (g_time.GetDifferenceInSeconds(lastSendTime) < 0.5f)
			{
				return;
			}
			bResend = true;
		}
		int age = m_nOutputSeq - m_nInputAck;

#if NEW_BANDWIDTH_MANAGEMENT
		uint16 maxOutputSize = m_PacketRateCalculator.GetMaxPacketSize();
#else
		uint16 maxOutputSize = m_PacketRateCalculator.GetMaxPacketSize(nTime);
#endif // NEW_BANDWIDTH_MEASUREMENT

		bool bSend = false;
		SSendPacketParams params;

		params.bAllowUserSend = bAllowUserSend & !bFlush;

#if NEW_BANDWIDTH_MANAGEMENT
		params.nSize = m_PacketRateCalculator.GetIdealPacketSize(nTime, IsIdle(), maxOutputSize);
		if (m_pParent->IsLocal())
		{
			params.nSpareBytes = 0;
		}
		else
		{
			params.nSpareBytes = m_PacketRateCalculator.GetSparePacketSize(nTime, params.nSize, maxOutputSize
	#if LOG_BANDWIDTH_SHAPING
			                                                               , m_pParent->IsLocal(), m_pParent->GetName()
	#endif // LOG_BANDWIDTH_SHAPING
			                                                               );
		}
		bSend |= ((params.nSize + params.nSpareBytes) != 0);
#else
		params.nSize = m_PacketRateCalculator.GetIdealPacketSize(age, IsIdle(), maxOutputSize);
		bSend |= (params.nSize != 0);
#endif // NEW_BANDWIDTH_MANAGEMENT

		if (CNetCVars::Get().NewQueueBehaviour == 0)
		{
			params.bForce = m_dqAcks.size() > (WHOLE_SEQ / 4);
		}
		else
		{
			params.bForce = false;
		}

		if (bForce)
		{
			params.bForce = true;
			params.nSize = maxOutputSize;
#if NEW_BANDWIDTH_MANAGEMENT
			params.nSpareBytes = 0;
#endif // NEW_BANDWIDTH_MANAGEMENT
			bSend = true;
		}

		if (bSend)
		{
			lastSendTime = g_time;

			if (bResend)
			{
				NetWarning("Resend last packet to %s", RESOLVER.ToString(m_pParent->GetIP()).c_str());
#if USE_ARITHSTREAM
				CCommOutputStream& stm = m_outputStreamImpl.GetOutput();
				size_t outSize = stm.GetOutputSize();
#else
				CNetOutputSerializeImpl* stm = &m_outputStreamImpl;
				size_t outSize = stm->GetOutputSize();
#endif
				m_pParent->Send(m_assemblyBuffer, m_assemblySize);
				m_PacketRateCalculator.SentPacket(nTime, m_nOutputSeq, static_cast<uint16>(outSize));
				m_nRepeated++;
				m_nSent++;
			}
			else
			{
				BroadcastMessage(SCTPEndpointEvent(eCEE_SendingPacket));

#if !NEW_BANDWIDTH_MANAGEMENT
				uint16 capOutputSize = maxOutputSize;
				if (capOutputSize > 500)
					capOutputSize = std::max(500, capOutputSize - PACKET_RESERVED_BYTES);
				if (params.nSize > capOutputSize)
					params.nSize = capOutputSize;
#else
				params.nSize = std::min(params.nSize, (maxOutputSize - params.nSpareBytes - PACKET_RESERVED_BYTES));
#endif  // !NEW_BANDWIDTH_MANAGEMENT

				bSend = SendPacket(nTime, params) != 0;
			}
		}
	}
	while (!m_emptyMode && !bResend && bFlush && !m_queue.IsEmpty());
	// the above while loop keeps sending packets if we've been requested to flush until the message queues are
	// completely empty
}

void CCTPEndpoint::Update(CTimeValue nTime, bool isDisconnecting, bool bAllowUserSend, bool bForce, bool bFlush)
{
	ASSERT_GLOBAL_LOCK;
	CRY_PROFILE_REGION(PROFILE_NETWORK, "CCTPEndpoint:Update");

	if (m_emptyMode)
		return;

	m_PacketRateCalculator.UpdateLatencyLab(nTime);

	UpdatePendingQueue(nTime, isDisconnecting);

	if (m_emptyMode)
		return;

#if LOG_TFRC
	fprintf(m_log_tfrc, "[time:%f] TcpFriendlyBitRate=%f\n", nTime.GetSeconds(), m_PacketRateCalculator.GetTcpFriendlyBitRate());
#endif
	SendPacketsIfNecessary(nTime, isDisconnecting, bAllowUserSend, bForce, bFlush);

#if DEBUG_ENDPOINT_LOGIC
	DumpOutputState(nTime);
#endif

	if (m_emptyMode)
		return;
}

bool CCTPEndpoint::AckPacket(CTimeValue nTime, uint32 nSeq, bool bOk)
{
	//	NET_ASSERT( m_nOutputSeq >= nSeq );
	MMM_REGION(m_pMMM);

	if (m_nOutputSeq < nSeq)
	{
		char msg[512];
		cry_sprintf(msg, "Received an ack for a packet newer than what we've sent (received %u, at %u); disconnecting", nSeq, m_nOutputSeq);
		m_pParent->Disconnect(eDC_ProtocolError, msg);
		return false;
	}

	// already acked?
	if (nSeq <= m_nInputAck)
		return true;

#if DETECT_DUPLICATE_ACKS
	if (m_ackedPackets.find(nSeq) != m_ackedPackets.end())
	{
		NET_ASSERT(false);
		NetLog("Duplicate ack detected (seq=%d)", nSeq);
		m_pParent->Disconnect(eDC_ProtocolError, "duplicate ack detected");
		return false;
	}
	m_ackedPackets.insert(nSeq);
#endif

	NET_ASSERT(nSeq == m_nInputAck + 1);

	if (m_vOutputState[m_nInputAck % WHOLE_SEQ].IsAvailable())
	{
		NetWarning("Illegal ack detected (seq=%d)", nSeq);
		m_pParent->Disconnect(eDC_ProtocolError, "Malformed packet");
		return false;
	}
	m_vOutputState[m_nInputAck % WHOLE_SEQ].Available();
	m_vOutputState[m_nInputAck % WHOLE_SEQ].FreeState(&m_bigStateMgrOutput);

	m_nInputAck = nSeq;

	if (m_bReliableWait && m_nReliableSeq == nSeq)
		m_bReliableWait = false;

	COutputState& state = m_vOutputState[nSeq % WHOLE_SEQ];

	m_PacketRateCalculator.AckedPacket(nTime, nSeq, bOk);
	AckMessages(state.m_headSentMessage, nSeq, bOk, false);
	m_nStateBlockers -= state.m_nStateBlockers;
	state.m_nStateBlockers = 0;

	if (!m_nStateBlockers && !m_queue.IsBlockingStateChange())
	{
		SCTPEndpointEvent evt(eCEE_NoBlockingMessages);
		BroadcastMessage(evt);
	}

	return true;
}

void CCTPEndpoint::EmptyMessages()
{
	MMM_REGION(m_pMMM);

	m_emptyMode = true;

	for (size_t i = 0; i < WHOLE_SEQ; i++)
	{
		AckMessages(m_vOutputState[i].m_headSentMessage, 0, false, true);
		m_nStateBlockers -= m_vOutputState[i].m_nStateBlockers;
		m_vOutputState[i].m_nStateBlockers = 0;
	}

	m_queue.Empty();

	for (size_t i = 0; i < WHOLE_SEQ; i++)
	{
		m_vOutputState[i].Reset(&m_bigStateMgrOutput);
	}
}

#if DEBUG_ENDPOINT_LOGIC
void CCTPEndpoint::DumpOutputState(const CTimeValue& time)
{
	fprintf(m_log_output, "-----------------------------------\n");
	fprintf(m_log_output, "Time: %d\n", time.GetMilliSeconds());
	fprintf(m_log_output, "OutputSeq: %d\n", m_nOutputSeq);
	fprintf(m_log_output, "InputAck: %d\n", m_nInputAck);
	fprintf(m_log_output, "ReliableSeq: %d %s\n", m_nReliableSeq, m_bReliableWait ? "waiting" : "not-waiting");
	fprintf(m_log_output, "Acks: from %d (with %d queued)\n     ", m_nFrontAck, m_dqAcks.size());
	//for (size_t i=0; i<m_dqAcks.size(); ++i)
	//	fprintf( m_log_output, m_dqAcks[i]?"#":"." );
	fprintf(m_log_output, "\n");
	fprintf(m_log_output, "Current State:");
	m_vOutputState[m_nOutputSeq % WHOLE_SEQ].Dump(m_log_output);
	fprintf(m_log_output, "Basis State:");
	m_vOutputState[m_nInputAck % WHOLE_SEQ].Dump(m_log_output);

	fflush(m_log_output);
}

void CCTPEndpoint::DumpInputState(const CTimeValue& time)
{
	fprintf(m_log_input, "-----------------------------------\n");
	fprintf(m_log_input, "Time: %d\n", time.GetMilliSeconds());
	fprintf(m_log_input, "InputSeq: %d\n", m_nInputSeq - 1);
	fprintf(m_log_input, "Acks: from %d (with %d queued)\n     ", m_nFrontAck, m_dqAcks.size());
	//for (size_t i=0; i<m_dqAcks.size(); ++i)
	//	fprintf( m_log_input, m_dqAcks[i]?"#":"." );
	fprintf(m_log_input, "\n");
	fprintf(m_log_input, "Current State:");
	m_vInputState[(m_nInputSeq - 1) % WHOLE_SEQ].Dump(m_log_input);

	fflush(m_log_input);
}
#endif

class CCTPEndpoint::CSequenceNumberParser
{
public:
	CSequenceNumberParser(const uint8* normBytes, uint32 inputSeq, bool inSync, size_t pktLen)
	{
		ok = false;
		reorderings = 0;

		// read out the current sequence number, and the basis sequence number "tags"
		// the basis is our agreed upon state with the other end of the connection
		uint32 nBasisSeqTag = Frame_HeaderToID[normBytes[0]];
		if (nBasisSeqTag >= eH_TransportSeq0 && nBasisSeqTag <= eH_TransportSeq_Last)
			nBasisSeqTag -= eH_TransportSeq0;
		else if (nBasisSeqTag >= eH_SyncTransportSeq0 && nBasisSeqTag <= eH_SyncTransportSeq_Last)
		{
			NET_ASSERT(inSync);
			nBasisSeqTag -= eH_SyncTransportSeq0;
		}
		else
		{
			NET_ASSERT(false);
			return;
		}
		uint32 nCurrentSeqTag = UnseqBytes[normBytes[1]];

		nCurrent = (inputSeq & ~SequenceNumberMask) | nCurrentSeqTag;
		if (inputSeq >= SequenceNumberRadius && nCurrent < inputSeq - SequenceNumberRadius)
			nCurrent += SequenceNumberDiameter;
		else if (nCurrent > inputSeq + SequenceNumberRadius)
			nCurrent -= SequenceNumberDiameter;
		nBasis = nCurrent - nBasisSeqTag - 1;

#if DEBUG_SEQUENCE_NUMBERS
		//DEBUG:
		uint32 nProperSeq = *(uint32*)(normBytes + pktLen + 2);
		NetLog("got tags: %d, %d with seq %d; => %d, %d [should be %d]\n", nCurrentSeqTag, nBasisSeqTag, inputSeq, nCurrent, nBasis, nProperSeq);
		NET_ASSERT(nCurrent == nProperSeq);
#endif

		ok = CheckValidResult(inputSeq);
	}

	bool IsCompatibleWithBasis(const CInputState& basis, uint32 lastBasisSeq) const
	{
		if (basis.LastValid() != nBasis)
		{
			MALFORMED_PACKET_REPORT("Ignoring bad basis state %d (still at %d) - possible malicious stream", nBasis, basis.LastValid());
			return false;
		}

		NET_ASSERT(nBasis >= lastBasisSeq);
		if (nBasis < lastBasisSeq)
		{
			MALFORMED_PACKET_REPORT("Ignoring old basis state %d (currently at %d)", nBasis, lastBasisSeq);
			return false;
		}

		return true;
	}

	uint32 nCurrent;
	uint32 nBasis;
	uint32 reorderings;
	bool   ok;

private:
	bool CheckValidResult(uint32 inputSeq)
	{
		// check our location
		if (inputSeq > nCurrent)
		{
			reorderings++;
			// already processed this packet... we can early out here
			return false;
		}

		if (nCurrent > inputSeq + CCTPEndpoint::WHOLE_SEQ)
		{
			NetWarning("Sequence %d is way in the future (I'm only up to %d) - ignoring it", nCurrent, inputSeq);
			return false;
		}

		return true;
	}
};

static uint8 QuickHashBytes(const uint8* pSrc, uint32 len)
{
	uint8 hash = 0;
	for (uint32 i = 0; i < len; i++)
		hash = 5 * hash + pSrc[i];
	return hash;
}

class CCTPEndpoint::CParsePacketContext
{
public:
	CParsePacketContext(const uint8* normBytes, uint32 pktLen, CInputState& state, CNetInputSerializeImpl& stmImpl, bool inSync, const CSequenceNumberParser& seq, uint32 timeFraction32)
		: m_bCompleted(false)
		, m_numMessages(0)
		, m_state(state)
		, m_stmImpl(stmImpl)
		, m_normBytes(normBytes)
		, m_pktLen(pktLen)
		, m_inSync(inSync)
		, m_stm(stmImpl)
		, m_seq(seq)
		, m_timeFraction32(timeFraction32)
	{
	}
	void Complete() { m_bCompleted = true; }
	bool NextMessage()
	{
		if (m_bCompleted || TooManyMessages())
			return false;
		m_numMessages++;
		return true;
	}
	ILINE bool TooManyMessages() const
	{
		return m_numMessages >= MaxMessagesPerPacket;
	}

	ILINE CInputState& GetInputState()
	{
		return m_state;
	}
	ILINE CNetInputSerializeImpl& GetStreamImpl()
	{
		return m_stmImpl;
	}
#if USE_ARITHSTREAM
	ILINE CCommInputStream& GetCommStream()
	{
		return m_stmImpl.GetInput();
	}
#else
	ILINE CNetInputSerializeImpl& GetCommStream()
	{
		return m_stmImpl;
	}
#endif

	ILINE uint32 GetPktLen() const
	{
		return m_pktLen;
	}
	ILINE const uint8* GetPktBytes() const
	{
		return m_normBytes;
	}

	ILINE bool IsInSync() const
	{
		return m_inSync;
	}

	ILINE TSerialize GetSerializer()
	{
		return TSerialize(&m_stm);
	}

	ILINE uint32 GetCurrentSeq() const
	{
		return m_seq.nCurrent;
	}

	ILINE uint32 GetBasisSeq() const
	{
		return m_seq.nBasis;
	}

	ILINE uint32 GetTimeFraction32() const
	{
		return m_timeFraction32;
	}

private:
	// it is possible for an infinite loop to occur here; a malicious packet could
	// be encoded in such a way that eIM_EndOfStream can never be found.
	// we prevent this by enforcing a maximum number of iterations through the loop;
	bool                                     m_bCompleted;
	uint32                                   m_numMessages;
	uint32                                   m_timeFraction32;

	const uint8* const                       m_normBytes;
	const uint32                             m_pktLen;
	const bool                               m_inSync;

	CInputState&                             m_state;
	CNetInputSerializeImpl&                  m_stmImpl;
	CSimpleSerialize<CNetInputSerializeImpl> m_stm;

	const CSequenceNumberParser&             m_seq;
};

void CCTPEndpoint::QueueForLater(CTimeValue nTime, CAutoFreeHandle& hdl, uint32 nCurrentSeq)
{
	if (m_incomingPackets.find(nCurrentSeq) != m_incomingPackets.end())
	{
		// already have this packet
	}
	else
	{
		TQueuedIncomingPacket pkt;
		pkt.hdl = hdl.Grab();
		pkt.when = nTime;
		m_incomingPackets[nCurrentSeq] = pkt;
#if ENABLE_DEBUG_KIT
		if (CNetCVars::Get().EndpointPendingQueueLogging)
			NetLog("Queued packet %d for later (as at %d)", nCurrentSeq, m_nInputSeq);
#endif
		m_nQueued++;
	}
}

#if CRC8_ENCODING_GLOBAL
	#if USE_ARITHSTREAM
static bool CRCCheck(CCommInputStream& stm)
	#else
static bool CRCCheck(CNetInputSerializeImpl& stm)
	#endif
{
	uint8 crcStream = stm.GetCRC();
	uint8 crcWritten = stm.ReadBits(8);
	if (crcStream != crcWritten)
		NetWarning("Packet crc mismatch %.2x != %.2x", crcStream, crcWritten);
	return crcStream == crcWritten;
}
#else
	#if USE_ARITHSTREAM
static ILINE bool CRCCheck(CCommInputStream& stm)
	#else
static bool       CRCCheck(CNetInputSerializeImpl& stm)
	#endif
{
	return true;
}
#endif

#if ENABLE_CORRUPT_PACKET_DUMP
void CCTPEndpoint::DoPacketDump()
{
	if (m_corruptPacketDumpData.processingPacket && !m_corruptPacketDumpData.doingDump)
	{
		CAutoFreeHandle hdl(m_corruptPacketDumpData.hdl);
		int startPacketReadDebugOutput = CNetCVars::Get().packetReadDebugOutput;
		int startLogLevel = CNetCVars::Get().LogLevel;
	#if LOG_INCOMING_MESSAGES
		int startLogNetMessages = CNetCVars::Get().LogNetMessages;
	#endif

		m_corruptPacketDumpData.doingDump = true;
		CNetCVars::Get().packetReadDebugOutput = 1;
		CNetCVars::Get().LogLevel = 1;
	#if LOG_INCOMING_MESSAGES
		CNetCVars::Get().LogNetMessages = 1;  // Allow ProcessPacket_NormalMessage to dump the name of messages as they are processed
	#endif

		NetLog("Packet Corruption detected starting corrupt packet dump");
		NetLog("Packet Data");
		DumpBytes((uint8*)MMM().PinHdl(hdl.Peek()), MMM().GetHdlSize(hdl.Peek()));

		ProcessPacket(CTimeValue(), hdl, false, m_corruptPacketDumpData.inSync);

		CNetwork::Get()->BroadcastNetDump(eNDT_ObjectState);
		CNetwork::Get()->AddExecuteString("es_dump_entities");

		m_corruptPacketDumpData.doingDump = false;
		CNetCVars::Get().packetReadDebugOutput = startPacketReadDebugOutput;
		CNetCVars::Get().LogLevel = startLogLevel;
	#if LOG_INCOMING_MESSAGES
		CNetCVars::Get().LogNetMessages = startLogNetMessages;
	#endif

		// Grab our handle back since we don't own it we don't want CAutoFreeHandle to free it.
		hdl.Grab();
	}
}
#endif

void CCTPEndpoint::ProcessPacket(CTimeValue nTime, CAutoFreeHandle& hdl, bool bQueueing, bool inSync)
{
#if _DEBUG && defined(USER_craig)
	//SCOPED_ENABLE_FLOAT_EXCEPTIONS;
#endif

	ASSERT_GLOBAL_LOCK;
	CRY_PROFILE_REGION(PROFILE_NETWORK, "CCTPEndpoint:ProcessPacket");

#if ENABLE_CORRUPT_PACKET_DUMP
	CAutoSetCorruptPacketDumpData autoSetCorruptPacketDumpData(m_corruptPacketDumpData, hdl.Peek(), inSync);
#endif

	if (bQueueing)
		m_receivedPacketSinceBackoff = true;

	const uint32 pktSize = MMM().GetHdlSize(hdl.Peek());

#if ENCRYPTION_RIJNDAEL
	if ((pktSize < (7 + (CRC8_ENCODING_GLOBAL))) || ((pktSize - 2 - 4 * DEBUG_SEQUENCE_NUMBERS) & 15))
#else
	if (pktSize < (7 + (CRC8_ENCODING_GLOBAL)))
#endif
	{
		//m_pParent->Disconnect(eDC_ProtocolError, "Malformed packet");
		NetWarning("Illegal packet size [size was %d, from %s]", pktSize, m_pParent->GetName());
		return;
	}

	//
	// read sequence data
	//

	NetLogPacketDebug("CCTPEndpoint::ProcessPacket size %d", pktSize);

	uint8* normBytes = (uint8*)MMM().PinHdl(hdl.Peek());
	size_t pktLen = pktSize - 2 - 4 * DEBUG_SEQUENCE_NUMBERS;

	//	DumpBytes( normBytes, MMM_PACKETDATA.GetHdlSize(hdl) );

	const CSequenceNumberParser seq(normBytes, m_nInputSeq, inSync, pktLen);

#if ENABLE_DEBUG_KIT
	SDebugPacket debugPacket(m_pParent->GetLoggingChannelID(), false, seq.nCurrent);
#endif

#if ENABLE_CORRUPT_PACKET_DUMP
	if (!m_corruptPacketDumpData.doingDump)
#endif
	{
		m_nReceived += seq.reorderings;

		if (!seq.ok)
		{
			return;
		}

		// maybe we can schedule this packet for later? - to reorder it and get
		// better packet loss rates
		if (!inSync && bQueueing && seq.nCurrent > m_nInputSeq)
		{
			QueueForLater(nTime, hdl, seq.nCurrent);
			// we'll process this later - in Update()
			return;
		}

		m_nReceived++;

		LOG_INCOMING_MESSAGE(2, "INCOMING: frame #%d from %s", seq.nCurrent, m_pParent->GetName());
	}

	// get state structs
	CInputState& basis = m_vInputState[seq.nBasis % WHOLE_SEQ];
	CInputState& state = m_vInputState[seq.nCurrent % WHOLE_SEQ];

	if (!seq.IsCompatibleWithBasis(basis, m_nLastBasisSeq))
	{
		return;
	}

#if ENABLE_CORRUPT_PACKET_DUMP
	if (!m_corruptPacketDumpData.doingDump)
#endif
	{
		for (uint32 i = m_nLastBasisSeq; i < seq.nBasis; i++)
			m_vInputState[i % WHOLE_SEQ].FreeState(&m_bigStateMgrInput);
		m_nLastBasisSeq = seq.nBasis;

#if DEBUG_ENDPOINT_LOGIC
		DumpInputState(nTime);
#endif

		// final setup of the state for decoding
		state.Clone(&m_bigStateMgrInput, basis, seq.nCurrent);
		// decrypt
		state.Decrypt(normBytes + 2, pktLen);
	}

	uint8 hash = QuickHashBytes(normBytes, pktLen);
	if ((hash ^ normBytes[pktLen]) != normBytes[pktLen + 1])
	{
		MALFORMED_PACKET_REPORT("Packet early hash mismatch (%.2x ^ %.2x -> %.2x != %.2x)", hash, normBytes[pktLen], hash ^ normBytes[pktLen], normBytes[pktLen + 1]);
		m_pParent->Disconnect(eDC_ProtocolError, "Malformed packet");
		return;
	}

#if ENABLE_CORRUPT_PACKET_DUMP
	if (!m_corruptPacketDumpData.doingDump)
#endif
	{
		// schedule acks and nacks
		while (m_nInputSeq < seq.nCurrent)
		{
			if (CVARS.LogLevel > 1)
				NetWarning("!!!!! On receiving packet %d, input seq was %d, causing a nack to be sent (basis state %d)", seq.nCurrent, m_nInputSeq, seq.nBasis);
			NET_ASSERT(m_nInputSeq == m_nFrontAck + m_dqAcks.size());
			m_dqAcks.push_back(SAckData(false, false, nTime));
			m_nInputSeq++;
			m_nDropped++;
			// dont allow connection to idle here
			m_dqAcks.back().bHadUrgentData = true;
			m_nUrgentAcks++;
		}
		NET_ASSERT(m_nInputSeq == m_nFrontAck + m_dqAcks.size());
		m_dqAcks.push_back(SAckData(true, false, nTime));
		m_nInputSeq++;

		m_PacketRateCalculator.GotPacket(nTime, pktSize);
	}

	CNetInputSerializeImpl stmImpl(normBytes + 1, pktLen - 1, m_pParent);
#if USE_ARITHSTREAM
	((CArithModel*)state.GetArithModel())->SetNetContextState(m_pParent->GetContextView()->ContextState());
	stmImpl.SetArithModel(state.GetArithModel());
#endif

	//
	// read acks & nacks
	//
	NetLogPacketDebug("Start read acks & nacks (%f)", stmImpl.GetInput().GetBitSize());
	bool bAck;
	uint32 nAckSeq;
	while (state.ReadAck(stmImpl.GetInput(), bAck, nAckSeq, m_recvAckNeeded))
	{
#if ENABLE_CORRUPT_PACKET_DUMP
		if (!m_corruptPacketDumpData.doingDump)
#endif
		{
			if (!AckPacket(nTime, nAckSeq, bAck))
			{
				return;
			}
		}
	}

	//
	// validate signing
	//
	NetLogPacketDebug("Start validate signing (%f)", stmImpl.GetInput().GetBitSize());
	uint8 signingKey;
	signingKey = stmImpl.GetInput().ReadBits(8);
	if (signingKey != normBytes[pktLen])
	{
		MALFORMED_PACKET_REPORT("Packet middle hash mismatch (%.2x != %.2x)", signingKey, normBytes[pktLen]);
		m_pParent->Disconnect(eDC_ProtocolError, "Malformed packet");
		return;
	}

	uint32 sendTime = stmImpl.GetInput().ReadBits(32);

#if USE_ARITHSTREAM
	state.GetArithModel()->SetTimeFraction(sendTime);
#endif

	//
	// read message stream
	//
	NetLogPacketDebug("Start read message stream (%f)", stmImpl.GetInput().GetBitSize());
	CParsePacketContext ppctx(normBytes, pktLen, state, stmImpl, inSync, seq, sendTime);
	while (ppctx.NextMessage())
	{
		if (m_emptyMode || m_pParent->IsDead())
			break;
		ProcessPacket_OneMessage(ppctx);
	}
	if (ppctx.TooManyMessages())
	{
		NetWarning("Too many messages in packet (will disconnect now)");
		m_pParent->Disconnect(eDC_ProtocolError, "Too many messages in packet");
	}

#if ENABLE_DEBUG_KIT
	debugPacket.Sent();
#endif
}

void CCTPEndpoint::ProcessPacket_OneMessage(CParsePacketContext& ppctx)
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);
	// read a message id and process it - we have some internal messages
	// handled by a switch, and anything higher-level is dispatched through
	// our message mapper
	NetLogPacketDebug("Start read message (%f)", ppctx.GetCommStream().GetBitSize());

	size_t msg = ppctx.GetInputState().ReadMsgId(ppctx.GetCommStream());

	// we must ensure that the memento streams are NULL at this point
	// (maybe we need only do this in debug?)
	ppctx.GetStreamImpl().SetMementoStreams(NULL, NULL, 0, 0, false);

	if (msg != eIM_EndOfStream)
		ProcessPacket_NormalMessage(msg, ppctx);
	else
		ProcessPacket_EndOfStream(ppctx);

	NetLogPacketDebug("End read message %" PRISIZE_T " (%f)", msg, ppctx.GetCommStream().GetBitSize());
}

void CCTPEndpoint::ProcessPacket_EndOfStream(CParsePacketContext& ppctx)
{
	NetLogPacketDebug("Message End of stream (%f)", ppctx.GetCommStream().GetBitSize());
	// Arithmetic compression => we need an end-of-stream marker to know
	// when the stream is really complete
	ppctx.Complete();
	bool& bHadUrgentData = m_dqAcks.back().bHadUrgentData;
	bHadUrgentData = (ppctx.GetCommStream().ReadBits(1) != 0);
	m_nUrgentAcks += bHadUrgentData;
	// final check
	if (!CRCCheck(ppctx.GetCommStream()))
	{
		m_pParent->Disconnect(eDC_ProtocolError, "Malformed packet");
		return;
	}
	uint8 signingKey = ppctx.GetCommStream().ReadBits(8);
	uint8 pktSignedByte = ppctx.GetPktBytes()[ppctx.GetPktLen()];
	if (signingKey != (pktSignedByte ^ 0xff))
	{
		MALFORMED_PACKET_REPORT("Packet end hash mismatch (%.2x != %.2x ^ ff -> %.2x)", signingKey, pktSignedByte, pktSignedByte ^ 0xff);
		m_pParent->Disconnect(eDC_ProtocolError, "Malformed packet");
		return;
	}
}

void CCTPEndpoint::ProcessPacket_NormalMessage(uint32 msg, CParsePacketContext& ppctx)
{
	ENSURE_REALTIME;

	m_dqAcks.back().bHadData = true;

	size_t nSink;
	const SNetMessageDef* pDef = m_InputMapper.GetDispatchInfo(msg, &nSink);

	if (pDef)
	{
		SProcessingMessage processingMessage(pDef);

		m_previouslyReceivedMessages.Put(pDef);
		LOG_INCOMING_MESSAGE(1, "INCOMING: %s [id=%u;state=%d]", pDef->description, (unsigned)msg, m_pParent->GetContextViewState());
		NetLogPacketDebug("Message %d %s (%f)", msg, pDef->description, ppctx.GetCommStream().GetBitSize());

		if (pDef->CheckParallelFlag(eMPF_DecodeInSync) && !ppctx.IsInSync())
			m_pParent->Disconnect(eDC_ProtocolError, "received %s without having the packet marked for synchronous processing", pDef->description);
		INetMessageSink* pSink = m_MessageSinks[nSink].pSink;

		EntityId entityId = m_entityId; // make sure internal structures don't get broken, entityId is passed by reference!
		TNetMessageCallbackResult r = pDef->handler(pDef->nUser, pSink, ppctx.GetSerializer(), ppctx.GetCurrentSeq(), ppctx.GetBasisSeq(), ppctx.GetTimeFraction32(), &entityId, m_pParent);
		if (!CRCCheck(ppctx.GetCommStream()))
		{
			m_pParent->Disconnect(eDC_ProtocolError, "Malformed packet");
			return;
		}

		if (!r.first)
		{
			if (r.second)
				r.second->DeleteThis();
			NetLog("Protocol error handling message %s", pDef->description);
			m_pParent->Disconnect(eDC_ProtocolError, pDef->description);
			ppctx.Complete();
		}
		else if (r.second)
		{
			if (ppctx.IsInSync())
			{
				r.second->Sync();
				r.second->DeleteThis();
			}
			else
			{
				if (!entityId && !m_entityId && pDef->CheckParallelFlag(eMPF_DiscardIfNoEntity))
				{
					NetLog("%s message with no entity id; discarding", pDef->description);
					r.second->DeleteThis();
				}
				else if (pDef->CheckParallelFlag(eMPF_BlocksStateChange))
				{
					m_nStateBlockers++;
					TO_GAME(r.second, m_pParent, m_pParent, &CNetChannel::UnblockMessages);
				}
				else
				{
					TO_GAME(r.second, m_pParent);
				}
			}
		}
	}
	else
	{
		m_pParent->Disconnect(eDC_ProtocolError, "No such message id");
	}
}

// this class is for collecting regular messages
class CCTPEndpoint::CMessageSender : /*public INetMessageSender,*/ public IMessageOutput, public INetSender
{
public:
	ILINE CMessageSender(CCTPEndpoint* pEndpoint, COutputState& State,
	                     size_t nSize, int& nMessages, CStatsCollector* pStats,
	                     INetChannel* pChannel, CTimeValue now, uint32 timeFraction32) :
		INetSender(TSerialize(&pEndpoint->m_outputStream), pEndpoint->m_nOutputSeq, pEndpoint->m_nInputAck, timeFraction32, pEndpoint->m_pParent->IsServer()),
		m_State(State),
		m_nSize(nSize),
		m_nMessages(nMessages),
		m_pStats(pStats),
		m_pEndpoint(pEndpoint),
		m_pChannel(pChannel),
		m_inWrite(false),
		m_now(now),
		m_bHadUrgentData(false),
		m_bIsCorrupt(false),
		m_brackets(((CNetChannel*)pChannel)->GetUpdateMessageBrackets()),
		m_bWritingPacketNeedsInSyncProcessing(false),
		m_nStateBlockers(0),
		m_nMessagesInBlock(0)
	{
	}

	bool HadUrgentData() const
	{
		return m_bHadUrgentData;
	}

	void BeginMessage(const SNetMessageDef* pDef)
	{
#if HIGH_PRIORITY_LOGGING
		// N.B. If UpdateAspect31 isn't used for the priority hack, this logging code will need changing
		uint32 id = m_pEndpoint->m_OutputMapper.GetMsgId(pDef);
		const SNetMessageDef* def;
		size_t proto;
		if (m_pEndpoint->m_OutputMapper.LookupMessage("CServerContextView:UpdateAspect31", &def, &proto))
		{
			uint32 myid = m_pEndpoint->m_OutputMapper.GetMsgId(def);
			if (myid == id)
			{
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_pEndpoint->m_pParent->GetContextView()->ContextState()->GetEntityID(m_updatingId));
				if (pEntity)
				{
					NetLog("[PlayerUpdate]: Client sending player update for %s", pEntity->GetName());
				}
			}
		}
		if (m_pEndpoint->m_OutputMapper.LookupMessage("CClientContextView:UpdateAspect31", &def, &proto))
		{
			uint32 myid = m_pEndpoint->m_OutputMapper.GetMsgId(def);
			if (myid == id)
			{
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_pEndpoint->m_pParent->GetContextView()->ContextState()->GetEntityID(m_updatingId));
				if (pEntity)
				{
					NetLog("[PlayerUpdate]: Server sending player update for %s", pEntity->GetName());
				}
			}
		}
#endif // HIGH_PRIORITY_LOGGING

		if (!m_updatingId && m_lastUpdatingId)
		{
			BeginMessage_Primitive(m_brackets.second);
			m_lastUpdatingId = SNetObjectID();
		}
		BeginMessage_Primitive(pDef);
	}

	void BeginUpdateMessage(SNetObjectID id)
	{
		NET_ASSERT(!(!id));
		if (!m_updatingId && m_lastUpdatingId == id)
		{
			m_updatingId = id;
		}
		else
		{
			if (m_lastUpdatingId)
				BeginMessage_Primitive(m_brackets.second);
			m_updatingId = m_lastUpdatingId = id;
			BeginMessage_Primitive(m_brackets.first);
			ser.Value("netID", id, 'eid');
		}
	}

	void EndUpdateMessage()
	{
		NET_ASSERT(!(!m_updatingId));
		NET_ASSERT(m_updatingId == m_lastUpdatingId);
		m_updatingId = SNetObjectID();
	}

#if ENABLE_PACKET_PREDICTION
	CMessageMapper* GetOutputMapper() { return &m_pEndpoint->m_OutputMapper; }
#endif

	void BeginMessage_Primitive(const SNetMessageDef* pDef)
	{
		uint32 id = m_pEndpoint->m_OutputMapper.GetMsgId(pDef);
#if LOG_OUTGOING_MESSAGES
		if (CNetCVars::Get().LogNetMessages & 4)
			NetLog("OUTGOING: %s [id=%u;state=%d]", pDef->description, (unsigned)id, m_pEndpoint->m_pParent->GetContextViewState());
#endif
#if CRC8_ENCODING_MESSAGE
		if (m_nMessages)
			m_pEndpoint->m_outputStreamImpl.GetOutput().WriteBits(m_pEndpoint->m_outputStreamImpl.GetOutput().GetCRC(), 8);
#endif
		if (!m_State.WriteMsgId(m_pEndpoint->m_outputStreamImpl.GetOutput(), id, m_pStats, pDef->description))
		{
			NetWarning("Sending %s fails - no state", pDef->description);
			return;
		}
		m_pEndpoint->m_previouslySentMessages.Put(pDef);
		m_nMessages++;
		m_nMessagesInBlock++;
		m_pEndpoint->m_outputStreamImpl.SetMementoStreams(NULL, NULL, 0, 0, false);

		if (pDef->CheckParallelFlag(eMPF_DecodeInSync))
			m_bWritingPacketNeedsInSyncProcessing = true;
		if (pDef->CheckParallelFlag(eMPF_BlocksStateChange))
		{
			++m_nStateBlockers;
		}
	}

	EMessageSendResult HeaderBody(void* p)
	{
		return m_pEndpoint->m_pParent->WriteHeader(this);
	}

	EMessageSendResult FooterBody(void* p)
	{
		return m_pEndpoint->m_pParent->WriteFooter(this);
	}

	EMessageSendResult MessageBody(void* p)
	{
		SSentMessage* pMsg = static_cast<SSentMessage*>(p);
		EMessageSendResult result = pMsg->pSendable->Send(this);
		if (result == eMSR_FailedMessage || result == eMSR_FailedConnection || result == eMSR_FailedPacket)
		{
			NetWarning("Failed to send message: %s", pMsg->pSendable->GetDescription());
		}
		return result;
	}

	const char* GetName()
	{
		return m_pEndpoint->m_pParent->GetName();
	}

	EWriteMessageResult TranslateResult(EMessageSendResult res)
	{
		EWriteMessageResult out = eWMR_Fail_Finish;

		while (true)
		{
			switch (res)
			{
			case eMSR_SentOk:
				out = eWMR_Ok_Continue;
				break;
			case eMSR_NotReady:
				if (m_nMessagesInBlock)
				{
					NetWarning("Message declared not-ready, but data was sent; upgrading to a connection failure");
					res = eMSR_FailedConnection;
					continue;
				}
				out = eWMR_Delay;
				break;
			case eMSR_FailedMessage:
				if (m_nMessagesInBlock)
				{
					NetWarning("Message declared failed, but data was sent; upgrading to a packet failure");
					res = eMSR_FailedPacket;
					continue;
				}
				out = eWMR_Fail_Continue;
				break;
			case eMSR_FailedPacket:
				m_pEndpoint->m_pParent->Disconnect(eDC_ProtocolError, "Message send failed packet");
				out = eWMR_Fail_Finish;
				m_bIsCorrupt = true;
				break;
			case eMSR_FailedConnection:
				m_pEndpoint->m_pParent->Disconnect(eDC_ProtocolError, "Message send failed connection");
				m_bIsCorrupt = true;
				out = eWMR_Fail_Finish;
				break;
			}

			break;
		}
		if (out == eWMR_Ok_Continue || out == eWMR_Fail_Continue)
		{
			bool cont = m_pEndpoint->m_outputStreamImpl.GetOutput().GetApproximateSize() < m_nSize;
			cont &= m_nMessages < MaxMessagesPerPacket;

			if (!cont)
			{
				if (out == eWMR_Ok_Continue)
					out = eWMR_Ok_Finish;
				else
					out = eWMR_Fail_Finish;
			}
		}

		return out;
	}

	uint32 GetStreamSize()
	{
#if ENABLE_ACCURATE_BANDWIDTH_PROFILING
		return (uint32)GetStream()->GetBitSize();
#else
		return GetStream()->GetApproximateSize();
#endif
	}

	ILINE EWriteMessageResult DoWrite(EMessageSendResult (CMessageSender::* sendBody)(void*), void* pUser)
	{
		NET_ASSERT(!m_inWrite);
		m_inWrite = true;
		m_bWritingPacketNeedsInSyncProcessing = false;
		m_nStateBlockers = 0;
		m_nMessagesInBlock = 0;
		EMessageSendResult res = (this->*sendBody)(pUser);

		m_inWrite = false;

		return TranslateResult(res);
	}

	EWriteMessageResult WriteHeader()
	{
		return DoWrite(&CMessageSender::HeaderBody, NULL);
	}

	EWriteMessageResult WriteFooter()
	{
		NET_ASSERT(!m_updatingId);
		if (m_lastUpdatingId)
		{
			BeginMessage_Primitive(m_brackets.second);
			m_lastUpdatingId = SNetObjectID();
		}

		return DoWrite(&CMessageSender::FooterBody, NULL);
	}

	EWriteMessageResult WriteMessage(SSentMessage& msg, SSendableHandle hdl)
	{
		if (!msg.pSendable->CheckParallelFlag(eMPF_DontAwake))
			m_bHadUrgentData = true;

		EWriteMessageResult out = DoWrite(&CMessageSender::MessageBody, &msg);

		if (out == eWMR_Ok_Continue || out == eWMR_Ok_Finish)
		{
			m_State.SentMessage(*m_pEndpoint, hdl, m_nStateBlockers);
			m_pEndpoint->m_nStateBlockers += m_nStateBlockers;
			m_pEndpoint->m_bWritingPacketNeedsInSyncProcessing |= m_bWritingPacketNeedsInSyncProcessing;
		}

		/*
		   if (CVARS.NetInspector)
		   {
		   NET_INSPECTOR.AddMessage(msg.pSendable->GetDescription(), RESOLVER.ToString(m_pEndpoint->m_pParent->GetIP()).c_str(),
		    (GetStream()->GetBitSize() - sizeBefore)/8.0f);
		   }
		 */

		return out;
	}

#if USE_ARITHSTREAM
	CCommOutputStream* GetStream()
	{
		return &m_pEndpoint->m_outputStreamImpl.GetOutput();
	}
#else
	CNetOutputSerializeImpl* GetStream()
	{
		return &m_pEndpoint->m_outputStreamImpl;
	}
#endif

	INetChannel* GetChannel()
	{
		return m_pChannel;
	}

	size_t CurrentSizeEstimation()
	{
		return m_pEndpoint->m_outputStreamImpl.GetOutput().GetApproximateSize();
	}

	bool IsCorrupt()
	{
		return m_bIsCorrupt;
	}

private:
	CCTPEndpoint*          m_pEndpoint;
	COutputState&          m_State;
	size_t                 m_nSize;
	int&                   m_nMessages;
	CStatsCollector*       m_pStats;
	INetChannel*           m_pChannel;
	CTimeValue             m_now;

	SNetObjectID           m_updatingId;
	SNetObjectID           m_lastUpdatingId;

	TUpdateMessageBrackets m_brackets;

	// per-message-chain attributes
	bool m_inWrite;
	bool m_bWritingPacketNeedsInSyncProcessing;
	int  m_nStateBlockers;
	int  m_nMessagesInBlock;

	bool m_bHadUrgentData;
	bool m_bIsCorrupt;
};

uint32 CCTPEndpoint::SendPacket(CTimeValue nTime, const SSendPacketParams& params)
{
	ASSERT_GLOBAL_LOCK;
	MMM_REGION(m_pMMM);

	CRY_PROFILE_REGION(PROFILE_NETWORK, "CCTPEndpoint:SendPacket");

	SSchedulingParams schedParams;
	schedParams.now = nTime;
	schedParams.next = nTime + 0.1f;
	schedParams.targetBytes = (m_pParent->IsLocal() ? CNetCVars::Get().MaxPacketSize : (params.nSize
#if NEW_BANDWIDTH_MANAGEMENT
	                                                                                    + params.nSpareBytes
#endif // NEW_BANDWIDTH_MANAGEMENT
	                                                                                    ));
	schedParams.transportLatency = m_PacketRateCalculator.GetPing(true) * 0.5f;

#if FULL_ON_SCHEDULING
	schedParams.haveWitnessPosition = m_pParent->GetWitnessPosition(schedParams.witnessPosition);
	schedParams.haveWitnessDirection = m_pParent->GetWitnessDirection(schedParams.witnessDirection);
	schedParams.haveWitnessFov = m_pParent->GetWitnessFov(schedParams.witnessFov);
#endif

	schedParams.pChannel = m_pParent;

	COutputState& basis = m_vOutputState[m_nInputAck % WHOLE_SEQ];

	// and remove any old acks from our ack cache
	while (m_nFrontAck <= basis.GetNumberOfAckedPackets())
	{
		NET_ASSERT(!m_dqAcks.empty());
		m_nFrontAck++;
		SAckData& ack = m_dqAcks.front();
		m_nUrgentAcks -= ack.bHadUrgentData;
		m_dqAcks.pop_front();
	}

	bool returnAckNeeded = true;

	if (CNetCVars::Get().NewQueueBehaviour == 0)
	{
		const bool bAnyData = params.bForce || !m_queue.IsEmpty()
#if !TEST_BANDWIDTH_REDUCTION
		                      || !m_dqAcks.empty()
#endif
		;

		if (!bAnyData)
			return 0;
	}
	else
	{
		bool hadData = false;

		for (TAckDeque::const_iterator pb = m_dqAcks.begin(); pb != m_dqAcks.end(); ++pb)
		{
			if (pb->bHadData)
			{
				hadData = true;
				break;
			}
		}

		bool messagesToWrite = m_queue.AreMessagesToWrite(schedParams);
		returnAckNeeded = hadData || messagesToWrite;
		bool forcedAckSend = (m_recvAckNeeded || m_sentAckNeeded || returnAckNeeded || m_queue.IsBlockingStateChange() || (m_nStateBlockers > 0) || (m_pParent->GetContextViewState() < eCVS_InGame));
		m_sentAckNeeded = returnAckNeeded;

		bool bAnyData = params.bForce || messagesToWrite || forcedAckSend;

		if (!bAnyData)
		{
			m_queue.FinishFrame(&schedParams);
			return 0;
		}
	}

	m_bWritingPacketNeedsInSyncProcessing = false;

	//
	// update output state
	//

	m_nOutputSeq++;

	m_outputStreamImpl.ResetLogging();

	STATS.BeginPacket(m_pParent->GetIP(), nTime, m_nOutputSeq);

	// remove any old states from our state cache
	COutputState& state = m_vOutputState[m_nOutputSeq % WHOLE_SEQ];
	state.Clone(&m_bigStateMgrOutput, basis);
	//
	// send sequence data
	//

	NET_ASSERT(m_nOutputSeq > m_nInputAck);
	NET_ASSERT(m_nOutputSeq - m_nInputAck <= WHOLE_SEQ);
	// create the stream, with the bonus (first) byte specifying it's a ctp frame,
	// and our basis offset, and the second byte indicating the current sequence number obfuscated a little
	uint8 nBasisSeqTag = m_nOutputSeq - m_nInputAck - 1;
	m_assemblyBuffer[0] = Frame_IDToHeader[eH_TransportSeq0 + nBasisSeqTag];
	m_outputStreamImpl.GetOutput().Reset(SeqBytes[m_nOutputSeq & SequenceNumberMask]);

	//NetLog("Send packet %.8x basis %.8x => tags %.2x, %.2x", m_nOutputSeq, m_nInputAck, nBasisSeqTag, m_nOutputSeq&SequenceNumberMask);

#if LOG_OUTGOING_MESSAGES
	if (CNetCVars::Get().LogNetMessages & 8)
		NetLog("OUTGOING: frame #%d to %s", m_nOutputSeq, m_pParent->GetName());
#endif

#if ENABLE_DEBUG_KIT
	SDebugPacket debugPacket(-(int)m_pParent->GetLoggingChannelID(), true, m_nOutputSeq);
#endif

#if USE_ARITHSTREAM
	state.GetArithModel()->SetNetContextState(m_pParent->GetContextView()->ContextState());
	m_outputStreamImpl.SetArithModel(state.GetArithModel());
	#if ENABLE_SERIALIZATION_LOGGING
	m_outputStreamImpl.SetNetChannel(m_pParent);
	#endif // ENABLE_SERIALIZATION_LOGGING
#endif

#if !USE_ARITHSTREAM
	m_outputStreamImpl.SetNetChannel(m_pParent);
#endif

#if DEBUG_SEQUENCE_NUMBERS
	char debugBuffer[512];
	int pos = 0;
	for (size_t i = m_nInputAck; (i % WHOLE_SEQ) != ((m_nInputAck - 1) % WHOLE_SEQ); i++)
	{
		if (i == m_nOutputSeq)
			pos += sprintf(debugBuffer + pos, "|");
		pos += sprintf(debugBuffer + pos, "%d", m_vOutputState[i % WHOLE_SEQ].IsAvailable());
	}
	NetLog("SENDSEQ[%p]: %s", this, debugBuffer);
	NetLog("send tags: %d, %d for %d, %d", nBasisSeqTag, m_nOutputSeq & 0xff, m_nOutputSeq, m_nInputAck);
#endif

	NET_ASSERT(!m_vOutputState[m_nInputAck % WHOLE_SEQ].IsAvailable());

	int nMessages = 0;

	debugPacketDataSizeStartPacket(m_pParent->IsLocal(), &m_OutputMapper);

	//
	// send acknowledgement data
	//
	debugPacketDataSizeStartData(eDPDST_Ack, m_outputStreamImpl.GetBitSize());

	for (TAckDeque::const_iterator pb = m_dqAcks.begin(); pb != m_dqAcks.end(); ++pb)
		state.WriteAck(m_outputStreamImpl.GetOutput(), pb->bReceived, &STATS);
	state.WriteEndAcks(m_outputStreamImpl.GetOutput(), returnAckNeeded, &STATS);

	debugPacketDataSizeEndData(eDPDST_Ack, m_outputStreamImpl.GetBitSize());

	//
	// signing key
	//

	uint8 signingKey = (cry_random_uint32() >> 6) & 0xff;
	debugPacketDataSizeSigningKey(8);
	m_outputStreamImpl.GetOutput().WriteBits(signingKey, 8);

	//
	// send message stream
	//

	uint32 timeToSend = (uint32)(gEnv->pTimer->GetReplicationTime() * REPLICATION_TIME_PRECISION);

	m_outputStreamImpl.GetOutput().WriteBits(timeToSend, 32);

#if USE_ARITHSTREAM
	state.GetArithModel()->SetTimeFraction(timeToSend);
#endif

	schedParams.nSeq = m_nOutputSeq;
	CMessageSender sender(this, state, schedParams.targetBytes, nMessages, &STATS, m_pParent, nTime, timeToSend);
	bool actuallySend = m_queue.BuildPacket(&sender, schedParams);
	actuallySend &= !sender.IsCorrupt();

	uint32 nSent = 0;
	// make sure we write the terminating data (needed for the arithmetic compression)
#if CRC8_ENCODING_MESSAGE
	if (nMessages)
		m_outputStreamImpl.GetOutput().WriteBits(m_outputStreamImpl.GetOutput().GetCRC(), 8);
#endif
	debugPacketDataSizeStartData(eDPDST_MessageBody, m_outputStreamImpl.GetBitSize());
	if (!state.WriteMsgId(m_outputStreamImpl.GetOutput(), eIM_EndOfStream, &STATS, "CCTPEndpoint:EndOfStream"))
	{
		if (CNetCVars::Get().LogLevel > 0)
			NetWarning("Failed to terminate packet; aborting send");
		m_pParent->Disconnect(eDC_ProtocolError, "Failed to terminate packet");
		return 0;
	}
	debugPacketDataSizeEndData(eDPDST_MessageBody, m_outputStreamImpl.GetBitSize());
	debugPacketDataSizeStartData(eDPDST_HadUrgentData, m_outputStreamImpl.GetBitSize());
	m_outputStreamImpl.GetOutput().WriteBits(sender.HadUrgentData(), 1);
	debugPacketDataSizeEndData(eDPDST_HadUrgentData, m_outputStreamImpl.GetBitSize());
#if CRC8_ENCODING_GLOBAL
	m_outputStreamImpl.GetOutput().WriteBits(m_outputStreamImpl.GetOutput().GetCRC(), 8);
#endif
	debugPacketDataSizeSigningKey(8);
	m_outputStreamImpl.GetOutput().WriteBits(signingKey ^ 0xff, 8);

	// final tidying

	state.Unavailable();

	debugPacketDataSizeStartData(eDPDST_Padding, m_outputStreamImpl.GetBitSize());
	nSent = uint32(m_outputStreamImpl.GetOutput().Flush());

	if (m_bWritingPacketNeedsInSyncProcessing)
	{
		uint8& hdr = m_assemblyBuffer[0];
		hdr = Frame_IDToHeader[Frame_HeaderToID[hdr] + eH_SyncTransportSeq0 - eH_TransportSeq0];
	}

#if ENABLE_DEBUG_KIT
	CAutoCorruptAndRestore acr(m_assemblyBuffer + 1, nSent, CVARS.RandomPacketCorruption == 2);
#endif

	// sign
	int nBig = nSent + 1 + 2; // one byte for the header that the range encoder needs, 2 bytes for signing

#if ENCRYPTION_RIJNDAEL
	while ((nBig - 2) & 15)
		nBig++;

	m_outputStreamImpl.GetOutput().PutZeros(nBig - nSent - 1);

	NET_ASSERT(0 == ((nBig - 2) & 15));
#endif

	debugPacketDataSizeEndData(eDPDST_Padding, m_outputStreamImpl.GetBitSize());
	debugPacketDataSizeSigningKey(16);
	uint8* normBytes = m_assemblyBuffer;
	uint8* endBytes = normBytes + nBig - 2;
	endBytes[0] = signingKey;
	uint8 hash = QuickHashBytes(normBytes, nBig - 2);
	endBytes[1] = hash ^ signingKey;

	state.Encrypt(normBytes + 2, nBig - 2);
	STATS.EndPacket(nBig);

#if DEBUG_SEQUENCE_NUMBERS
	*(uint32*)(normBytes + nBig) = m_nOutputSeq;
#endif
	//	DumpBytes( normBytes, nBig + 4*DEBUG_SEQUENCE_NUMBERS );
	if (actuallySend)
		m_pParent->Send(normBytes, nBig + 4 * DEBUG_SEQUENCE_NUMBERS);
	else
		NetWarning("Refusing to send corrupted packet");
	m_assemblySize = nBig + 4 * DEBUG_SEQUENCE_NUMBERS;
	m_PacketRateCalculator.SentPacket(nTime, m_nOutputSeq, nBig);
#if ENABLE_DEBUG_KIT
	debugPacket.Sent();
#endif

	debugPacketDataSizeEndPacket(8 * nBig);

	m_nSent++;

	return nSent;
}

bool CCTPEndpoint::LookupMessage(const char* name, SNetMessageDef const** ppDef, INetMessageSink** ppSink)
{
	bool result;
	size_t nProto;
	if (result = m_InputMapper.LookupMessage(name, ppDef, &nProto))
		*ppSink = m_MessageSinks[nProto].pSink;
	return result;
}

void CCTPEndpoint::BroadcastMessage(const SCTPEndpointEvent& evt)
{
	for (std::vector<SListener>::iterator iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
	{
		if (iter->eventMask & evt.event)
			iter->pListener->OnEndpointEvent(evt);
	}
}

void CCTPEndpoint::ChangeSubscription(ICTPEndpointListener* pListener, uint32 eventMask)
{
	for (std::vector<SListener>::iterator iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
	{
		if (iter->pListener == pListener)
		{
			if (eventMask)
				iter->eventMask = eventMask;
			else
				m_listeners.erase(iter);
			return;
		}
	}
	SListener listener;
	listener.pListener = pListener;
	listener.eventMask = eventMask;
	m_listeners.push_back(listener);
}

const char* CCTPEndpoint::GetName(void) const
{
	return (m_pParent != NULL) ? m_pParent->GetName() : "<unknown>";
}

void CCTPEndpoint::GetBandwidthStatistics(uint32 channelIndex, SBandwidthStats* pStats)
{
	m_queue.GetBandwidthStatistics(channelIndex, pStats);

#if NEW_BANDWIDTH_MANAGEMENT
	const INetChannel::SPerformanceMetrics* pMetrics = m_PacketRateCalculator.GetPerformanceMetrics();
#endif // NEW_BANDWIDTH_MANAGEMENT

	SNetChannelStats& stats = pStats->m_channel[channelIndex];
	stats.m_ping = static_cast<uint32>(m_PacketRateCalculator.GetPing(false) * 1000.0f);
	stats.m_pingSmoothed = static_cast<uint32>(m_PacketRateCalculator.GetPing(true) * 1000.0f);
	stats.m_bandwidthInbound = m_PacketRateCalculator.GetBandwidthUsage(g_time, CPacketRateCalculator::eIO_Incoming);
	stats.m_bandwidthOutbound = m_PacketRateCalculator.GetBandwidthUsage(g_time, CPacketRateCalculator::eIO_Outgoing);
#if NEW_BANDWIDTH_MANAGEMENT
	stats.m_bandwidthShares = pMetrics->m_bandwidthShares;
	stats.m_desiredPacketRate = pMetrics->m_packetRate;
#else
	stats.m_bandwidthShares = 0;
	stats.m_desiredPacketRate = static_cast<uint32>(m_PacketRateCalculator.GetPacketRate(IsIdle(), g_time));
#endif // NEW_BANDWIDTH_MANAGEMENT
	stats.m_currentPacketRate = m_PacketRateCalculator.GetCurrentPacketRate(g_time);
	stats.m_packetLossRate = m_PacketRateCalculator.GetPacketLossPerPacketSent(g_time) * 100.0f;
#if NEW_BANDWIDTH_MANAGEMENT
	stats.m_maxPacketSize = m_PacketRateCalculator.GetMaxPacketSize();
	stats.m_idealPacketSize = m_PacketRateCalculator.GetIdealPacketSize(g_time, IsIdle(), stats.m_maxPacketSize);
	stats.m_sparePacketSize = m_PacketRateCalculator.GetSparePacketSize(g_time, stats.m_idealPacketSize, stats.m_maxPacketSize
	#if LOG_BANDWIDTH_SHAPING
	                                                                    , true, NULL // effectively turns off the logging for this call
	#endif                                                                           // LOG_BANDWIDTH_SHAPING
	                                                                    );
#else
	stats.m_maxPacketSize = m_PacketRateCalculator.GetMaxPacketSize(g_time);
	stats.m_idealPacketSize = m_PacketRateCalculator.GetIdealPacketSize(m_nOutputSeq - m_nInputAck, IsIdle(), stats.m_maxPacketSize);
#endif // NEW_BANDWIDTH_MEASUREMENT
	stats.m_idle = IsIdle();
}

void CCTPEndpoint::SchedulerDebugDraw()
{
	float drawScale = CNetCVars::Get().DebugDrawScale;
	float white[] = { 1, 1, 1, 1 };
	m_queue.DrawLabel(0.0f, 0.0f, white, "Channel: [%s]", m_pParent->GetName());
	m_queue.DrawLabel(0.0f, drawScale * 10.0f, white, "OutputSeq:%d", m_nOutputSeq);
	m_queue.DrawLabel(drawScale * 150.0f, drawScale * 10.0f, white, "InputAck:%d", m_nInputAck);
	m_queue.DrawLabel(drawScale * 250.0f, drawScale * 10.0f, white, "InputSeq:%d", m_nInputSeq);
	m_queue.DrawLabel(drawScale * 350.0f, drawScale * 10.0f, white, "PacketRateCalculator Ping:%f", m_PacketRateCalculator.GetPing(false));

	SSchedulingParams schedParams;
	schedParams.now = g_time;
	schedParams.next = schedParams.now + 0.1f;
	schedParams.nSeq = m_nOutputSeq;
	//	int age = m_nOutputSeq - m_nInputAck;
#if !NEW_BANDWIDTH_MANAGEMENT
	schedParams.targetBytes = m_PacketRateCalculator.GetMaxPacketSize(schedParams.now);
#else
	schedParams.targetBytes = m_PacketRateCalculator.GetMaxPacketSize();
#endif // !NEW_BANDWIDTH_MANAGEMENT
	schedParams.transportLatency = m_PacketRateCalculator.GetPing(true) * 0.5f;
#if FULL_ON_SCHEDULING
	schedParams.haveWitnessPosition = m_pParent->GetWitnessPosition(schedParams.witnessPosition);
	schedParams.haveWitnessDirection = m_pParent->GetWitnessDirection(schedParams.witnessDirection);
	schedParams.haveWitnessFov = m_pParent->GetWitnessFov(schedParams.witnessFov);
#endif
	schedParams.pChannel = m_pParent;
	m_queue.DebugDraw(schedParams);
}

void CCTPEndpoint::ChannelStatsDraw()
{
	ITextModeConsole* pTMC = gEnv->pSystem->GetITextModeConsole();
	float drawScale = CNetCVars::Get().DebugDrawScale;
	static float y = drawScale * 30.f;
	static int yframe = -1;
	float white[4] = { 1, 1, 1, 1 };
	float cyan[4] = { 0, 1, 1, 1 };

	int frame = gEnv->pRenderer->GetFrameID();
	if (frame != yframe)
	{
		yframe = frame;
		y = drawScale * 30.f;

		m_queue.DrawLabel(0.0f, y, cyan, "Ping(ms)");
		m_queue.DrawLabel(drawScale * 40.0f, y, cyan, "[Smooth]");
		//m_queue.DrawLabel(drawScale * 80.0f, y, cyan, "TCPRate");
		m_queue.DrawLabel(drawScale * 120.0f, y, cyan, "BWIn");
		m_queue.DrawLabel(drawScale * 160.0f, y, cyan, "BWOut");
		m_queue.DrawLabel(drawScale * 200.0f, y, cyan, "PktRate");
		m_queue.DrawLabel(drawScale * 240.0f, y, cyan, "cur");
		m_queue.DrawLabel(drawScale * 280.0f, y, cyan, "loss%%");
		m_queue.DrawLabel(drawScale * 320.0f, y, cyan, "MaxSz");
		m_queue.DrawLabel(drawScale * 360.0f, y, cyan, "PktSz");
		m_queue.DrawLabel(drawScale * 400.0f, y, cyan, "Idle");
		m_queue.DrawLabel(drawScale * 440.0f, y, cyan, "KaTime/");
		m_queue.DrawLabel(drawScale * 480.0f, y, cyan, "KfTime");
		m_queue.DrawLabel(drawScale * 520.0f, y, cyan, "DcTime");
		m_queue.DrawLabel(drawScale * 560.0f, y, cyan, "BoTime");
	}
#if !NEW_BANDWIDTH_MANAGEMENT
	uint16 maxsz = m_PacketRateCalculator.GetMaxPacketSize(g_time);
#else
	uint16 maxsz = m_PacketRateCalculator.GetMaxPacketSize();
#endif // !NEW_BANDWIDTH_MANAGEMENT
	y += (drawScale * 10.0f);
	m_queue.DrawLabel(drawScale * 10.f, y, cyan, m_pParent->GetName());

	CTimeValue backoffTime;
	typedef CryFixedStringT<64> TBOStr;
	TBOStr backoffTimeStr = "      ";
	bool backingOff;
	if (backingOff = GetBackoffTime(backoffTime, true))
		backoffTimeStr.Format("%4.2f/", backoffTime.GetMilliSeconds());
	else
		backoffTimeStr = "../";
	if (GetBackoffTime(backoffTime, false))
		backoffTimeStr += TBOStr().Format("%4.2f", backoffTime.GetMilliSeconds());
	else
		backoffTimeStr += "..";

	y += (drawScale * 10.0f);
	m_queue.DrawLabel(0.0f, y, white, "%4d", static_cast<uint32>(m_PacketRateCalculator.GetPing(false) / 1000.0f));
	m_queue.DrawLabel(drawScale * 40.0f, y, white, "[%4d]", static_cast<uint32>(m_PacketRateCalculator.GetPing(true) / 1000.0f));
	//m_queue.DrawLabel(drawScale * 80.0f, y, white, "%13.2f", m_PacketRateCalculator.GetTcpFriendlyBitRate());
	m_queue.DrawLabel(drawScale * 120.0f, y, white, "%-6d", m_PacketRateCalculator.GetBandwidthUsage(g_time, CPacketRateCalculator::eIO_Incoming));
	m_queue.DrawLabel(drawScale * 160.0f, y, white, "%-6d", m_PacketRateCalculator.GetBandwidthUsage(g_time, CPacketRateCalculator::eIO_Outgoing));
	m_queue.DrawLabel(drawScale * 200.0f, y, white, "%3.2f", m_PacketRateCalculator.GetPacketRate(IsIdle(), g_time));
	m_queue.DrawLabel(drawScale * 240.0f, y, white, "%3.2f", m_PacketRateCalculator.GetCurrentPacketRate(g_time));
	m_queue.DrawLabel(drawScale * 280.0f, y, white, "%3.1f", m_PacketRateCalculator.GetPacketLossPerPacketSent(g_time) * 100.0f);
	m_queue.DrawLabel(drawScale * 320.0f, y, white, "%4d", maxsz);
#if NEW_BANDWIDTH_MANAGEMENT
	m_queue.DrawLabel(drawScale * 360.0f, y, white, "%4d", m_PacketRateCalculator.GetIdealPacketSize(g_time, IsIdle(), maxsz));
#else
	m_queue.DrawLabel(drawScale * 360.0f, y, white, "%4d", m_PacketRateCalculator.GetIdealPacketSize(m_nOutputSeq - m_nInputAck, IsIdle(), maxsz));
#endif // NEW_BANDWIDTH_MANAGEMENT
	m_queue.DrawLabel(drawScale * 400.0f, y, white, "%s", IsIdle() ? "yes" : "no");
	m_queue.DrawLabel(drawScale * 440.0f, y, white, "%2.2f", m_pParent->GetIdleTime(true).GetSeconds());
	m_queue.DrawLabel(drawScale * 480.0f, y, white, "%2.2f", m_pParent->GetIdleTime(false).GetSeconds());
	m_queue.DrawLabel(drawScale * 520.0f, y, white, "%2.2f", m_pParent->GetInactivityTimeout(backingOff).GetSeconds());
	m_queue.DrawLabel(drawScale * 560.0f, y, white, "%s", backoffTimeStr.c_str());

	if (CNetCVars::Get().ChannelStats > 1)
	{
		y += (drawScale * 10.0f);
		m_queue.DrawLabel(0.0f, y, white, "rcvd:%d", m_nReceived);
		m_queue.DrawLabel(drawScale * 60.0f, y, white, "sent:%d", m_nSent);
		m_queue.DrawLabel(drawScale * 120.0f, y, white, "dropped:%d", m_nDropped);
		m_queue.DrawLabel(drawScale * 180.0f, y, white, "reord:%d", m_nReordered);
		m_queue.DrawLabel(drawScale * 240.0f, y, white, "queued:%d", m_nQueued);
		m_queue.DrawLabel(drawScale * 300.0f, y, white, "rptsend:%d", m_nRepeated);
		m_queue.DrawLabel(drawScale * 360.0f, y, white, "pq to:%d", m_nPQTimeout);
		m_queue.DrawLabel(drawScale * 420.0f, y, white, "pq rdy:%d", m_nPQReady);
		m_queue.DrawLabel(drawScale * 480.0f, y, white, "urgack:%d", m_nUrgentAcks);
	}
}

void CCTPEndpoint::SetAfterSpawning(bool afterSpawning)
{
	if (afterSpawning)
		m_queue.SetFlags(m_queue.GetFlags() | eMQF_IsAfterSpawning);
	else
		m_queue.SetFlags(m_queue.GetFlags() & ~eMQF_IsAfterSpawning);
}

void CCTPEndpoint::SentElem(uint32& head, const SSendableHandle& hdl)
{
	ASSERT_GLOBAL_LOCK;
	uint32 cur;
	if (m_freeSentElem == InvalidSentElem)
	{
		cur = m_sentElems.size();
		m_sentElems.push_back(SSentElem());
	}
	else
	{
		cur = m_freeSentElem;
		m_freeSentElem = m_sentElems[cur].next;
	}
	NET_ASSERT(!m_sentElems[cur].hdl);
	m_sentElems[cur].hdl = hdl;
	m_sentElems[cur].next = head;
	head = cur;
}

void CCTPEndpoint::AckMessages(uint32& head, uint32 nSeq, bool ack, bool clear)
{
	static const size_t MESSAGE_BUFFER_SIZE = 4096;
	static CryFixedArray<SSendableHandle, MESSAGE_BUFFER_SIZE / sizeof(SSendableHandle)> temp;
	uint32 cur = head;
	while (cur != InvalidSentElem)
	{
		SSentElem& elem = m_sentElems[cur];
		if (temp.isfull())
		{
			m_queue.AckMessages(&temp[0], temp.size(), nSeq, ack, clear);
			temp.clear();
		}
		temp.push_back(elem.hdl);
		elem.hdl = SSendableHandle();
		uint32 next = elem.next;
		elem.next = m_freeSentElem;
		m_freeSentElem = cur;
		cur = next;
	}
	head = InvalidSentElem;
	if (temp.size() != 0)
	{
		m_queue.AckMessages(&temp[0], temp.size(), nSeq, ack, clear);
		temp.clear();
	}
}

bool CCTPEndpoint::IsIdle()
{
	return m_nUrgentAcks == 0 && m_queue.IsIdle() && m_pParent->IsIdle();
}

bool CCTPEndpoint::IsLocal() const
{
	return (m_pParent != NULL) ? m_pParent->IsLocal() : true;
}

const char* CCTPEndpoint::GetCurrentProcessingMessageDescription()
{
	if (g_processingMessage)
		return g_processingMessage->description;
	else
		return "no-message";
}

void CCTPEndpoint::BackOff()
{
	if (m_receivedPacketSinceBackoff)
	{
		m_receivedPacketSinceBackoff = false;
		m_backoffTimer = g_time;
	}
}

bool CCTPEndpoint::GetBackoffTime(CTimeValue& tm, bool total)
{
	tm = g_time - m_backoffTimer;
	float timeSince = tm.GetSeconds();
	if (!m_receivedPacketSinceBackoff)
	{
		if (!total && timeSince > 5.0f)
		{
			BroadcastMessage(SCTPEndpointEvent(eCEE_BackoffTooLong));
			return false;
		}
		if (!total)
			tm = 0.2f;
		return true;
	}
	if (timeSince > 0.1f)
		return false;
	if (!total)
		tm = 0.2f;
	return true;
}
