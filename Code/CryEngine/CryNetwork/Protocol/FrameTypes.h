// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FRAMETYPES_H__
#define __FRAMETYPES_H__

#pragma once

static const int NUM_SEQ_SLOTS = 64;

static const uint32 PROTOCOL_VERSION = 6;

enum EHeaders
{
	// null - never used
	eH_INVALID = 0,
	// external protocols
	eH_CryLobby,
	eH_PingServer, // 'kind' of external
	eH_QueryLan,   // also 'kind' of external - we want it in a constant non-time-varying place in the protocol
	eH_PunkBuster, // EvenBalance - M. Quinn
	eH_ConnectionSetup,
	eH_Disconnect,
	eH_DisconnectAck,
	// must be after external and before internal
	eH_FIRST_GENERAL_ENTRY,
	// internal protocols
	eH_TransportSeq0,
	eH_TransportSeq_Last     = eH_TransportSeq0 + NUM_SEQ_SLOTS - 1,
	eH_SyncTransportSeq0,
	eH_SyncTransportSeq_Last = eH_SyncTransportSeq0 + NUM_SEQ_SLOTS - 1,
	eH_ConnectionSetup_KeyExchange_0, // keys: g, p, A (from server side)
	eH_ConnectionSetup_KeyExchange_1, // Key: B (from client side)
	eH_KeepAlive,
	eH_KeepAliveReply,
	eH_BackOff,
	eH_AlreadyConnecting,
	eH_Fragmentation,
	eH_DediServerScheduler, // Expected on dedicated server scheduler socket only, CServerDefence is listener
	// following must be last
	eH_LAST_GENERAL_ENTRY,
};

extern uint8 Frame_HeaderToID[256];
extern uint8 Frame_IDToHeader[256];
extern uint8 SeqBytes[256];
extern uint8 UnseqBytes[256];

void InitFrameTypes();

#endif
