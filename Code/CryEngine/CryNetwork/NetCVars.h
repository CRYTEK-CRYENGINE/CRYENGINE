// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __NETCVARS_H__
#define __NETCVARS_H__

#pragma once

#include <CrySystem/IConsole.h>
#include "Config.h"
#include <CrySystem/TimeValue.h>

class CNetCVars
{
public:
	int   TokenId;
	int   CPU;
	int   LogLevel;
	int   EnableVoiceChat;
	int   ChannelStats;
	float BandwidthAggressiveness;
	float NetworkConnectivityDetectionInterval; // disabled if less than 1.0f
	float InactivityTimeout;
	float InactivityTimeoutDevmode;
	int   BackoffTimeout;
	int   MaxMemoryUsage;
	int   EnableVoiceGroups;
#if !NEW_BANDWIDTH_MANAGEMENT
	int   RTTConverge;
#endif // !NEW_BANDWIDTH_MANAGEMENT
	int   EnableTFRC;
#if CRY_PLATFORM_DURANGO
	int   networkThreadAffinity;
#endif

#if LOCK_NETWORK_FREQUENCY == 0
	float channelLocalSleepTime;
#endif // LOCK_NETWORK_FREQUENCY == 0
	int   socketMaxTimeout;
	int   socketBoostTimeout;
	int   socketMaxTimeoutMultiplayer;

#if NEW_BANDWIDTH_MANAGEMENT
	float net_availableBandwidthServer;
	float net_availableBandwidthClient;
	int   net_defaultBandwidthShares;
	int   net_defaultPacketRate;
	int   net_defaultPacketRateIdle;
#endif // NEW_BANDWIDTH_MANAGEMENT

#if NET_ASSERT_LOGGING
	int AssertLogging;
#endif

#if !NEW_BANDWIDTH_MANAGEMENT
	int   PacketSendRate; // packets per second
#endif                  // !NEW_BANDWIDTH_MANAGEMENT
	int   NewQueueBehaviour;
	int   SafetySleeps;
	int   MaxPacketSize;
	float KeepAliveTime;
	float PingTime;

#if LOG_MESSAGE_DROPS
	int LogDroppedMessagesVar;
	bool LogDroppedMessages()
	{
		return LogDroppedMessagesVar || LogLevel;
	}
#endif

#if ENABLE_NETWORK_MEM_INFO
	int MemInfo;
#endif
#if ENABLE_DEBUG_KIT
	int NetInspector;
	int UseCompression;
	int ShowObjLocks;
	int DebugObjUpdates;
	int LogComments;
	int EndpointPendingQueueLogging;
	int DisconnectOnUncollectedBreakage;
	int DebugConnectionState;
	int RandomPacketCorruption; // 0 - disabled; 1 - UDP level (final stage); 2 - CTPEndpoint (before signing and encryption)
	//float WMICheckInterval;
	int PerfCounters;
	int GSDebugOutput;
#endif

#if LOG_MESSAGE_QUEUE
	int    net_logMessageQueue;
	ICVar* pnet_logMessageQueueInjectLabel;
#endif // LOG_SEND_QUEUE

#if ENABLE_NET_DEBUG_INFO
	int netDebugInfo;
	int netDebugChannelIndex;
#endif

#if ENABLE_NET_DEBUG_ENTITY_INFO
	int    netDebugEntityInfo;
	ICVar* netDebugEntityInfoClassName;
#endif

	int net_lobbyUpdateFrequency;

#if !defined(OLD_VOICE_SYSTEM_DEPRECATED)
	int   VoiceLeadPackets;
	int   VoiceTrailPackets;
	float VoiceProximity;
#endif // !defined(OLD_VOICE_SYSTEM_DEPRECATED)
#if defined(USE_CD_KEYS)
	#if !ALWAYS_CHECK_CD_KEYS
	int CheckCDKeys;
	#endif
#endif
	ICVar* StatsLogin;
	ICVar* StatsPassword;

	int    RankedServer;

	int    LanScanPortFirst;
	int    LanScanPortNum;
	float  MinTCPFriendlyBitRate;

#if !defined(OLD_VOICE_SYSTEM_DEPRECATED)
	int useDeprecatedVoiceSystem;
#endif // !defined(OLD_VOICE_SYSTEM_DEPRECATED)

	CTimeValue StallEndTime;

#if INTERNET_SIMULATOR

	#define PACKET_LOSS_MAX_CLAMP (100.0f)      //-- Packet loss clamped to maximum of 100%
	#define PACKET_LAG_MAX_CLAMP  (60.0f)       //-- Packet lag clamped to max 60 seconds (60 an arbitrary number to identify the value as 'seconds')

	float net_PacketLossRate;
	float net_PacketLagMin;
	float net_PacketLagMax;
	int   net_SimUseProfile;

	static void OnPacketLossRateChange(ICVar* NewLossRate)
	{
		if (NewLossRate->GetFVal() < 0.0f)
			NewLossRate->Set(0.0f);
		else if (NewLossRate->GetFVal() > PACKET_LOSS_MAX_CLAMP)
			NewLossRate->Set(PACKET_LOSS_MAX_CLAMP);
	}

	static void OnPacketExtraLagChange(ICVar* NewExtraLag)
	{
		if (NewExtraLag->GetFVal() < 0.0f)
			NewExtraLag->Set(0.0f);
		else if (NewExtraLag->GetFVal() > PACKET_LAG_MAX_CLAMP)
			NewExtraLag->Set(PACKET_LAG_MAX_CLAMP);
	}

	static void OnInternetSimUseProfileChange(ICVar* val);
	static void OnInternetSimLoadProfiles(ICVar* val);
#endif

#ifdef ENABLE_UDP_PACKET_FRAGMENTATION
	int   net_max_fragmented_packets_per_source;
	float net_fragment_expiration_time;
	float net_packetfragmentlossrate;
	static void OnPacketFragmentLossRateChange(ICVar* NewLossRate)
	{
		if (NewLossRate->GetFVal() < 0.0f)
			NewLossRate->Set(0.0f);
		else if (NewLossRate->GetFVal() > 1.0f)
			NewLossRate->Set(1.0f);
	}
#endif

	float HighLatencyThreshold; // disabled if less than 0.0f
	float HighLatencyTimeLimit;

#if ENABLE_DEBUG_KIT
	int VisWindow;
	int VisMode;
	int ShowPing;
#endif

#if !defined(OLD_VOICE_SYSTEM_DEPRECATED)
	ICVar* pVoiceCodec;
#endif // !defined(OLD_VOICE_SYSTEM_DEPRECATED)
	ICVar* pSchedulerDebug;
	int    SchedulerDebugMode;
	float  DebugDrawScale;
	float  SchedulerSendExpirationPeriod;

#if LOG_INCOMING_MESSAGES || LOG_OUTGOING_MESSAGES
	int LogNetMessages;
#endif
#if LOG_BUFFER_UPDATES
	int LogBufferUpdates;
#endif
#if STATS_COLLECTOR_INTERACTIVE
	int ShowDataBits;
#endif

	int RemoteTimeEstimationWarning;

#if ENABLE_CORRUPT_PACKET_DUMP
	int packetReadDebugOutput;
	bool doingPacketReplay() { return packetReadDebugOutput ? true : false; }
#else
	bool doingPacketReplay() { return false; }
#endif

	// BREAKAGE
	int breakageSyncEntities;

	int enableWatchdogTimer;

	// Non arithstream NetID bits
	int net_numNetIDLowBitBits;
	int net_numNetIDLowBitIDs;
	int net_numNetIDMediumBitBits;
	int net_numNetIDMediumBitIDs;
	int net_netIDHighBitStart;
	int net_numNetIDHighBitBits;
	int net_numNetIDHighBitIDs;
	int net_numNetIDs;
	int net_invalidNetID;

	// Dedi server scheduler
	int net_dedi_scheduler_server_port;
	int net_dedi_scheduler_client_base_port;

	int net_profile_deep_bandwidth_logging;

	static ILINE CNetCVars& Get()
	{
		NET_ASSERT(s_pThis);
		return *s_pThis;
	}

private:
	friend class CNetwork; // Our only creator

	CNetCVars(); // singleton stuff
	~CNetCVars();
	CNetCVars(const CNetCVars&);
	CNetCVars& operator=(const CNetCVars&);

	static CNetCVars* s_pThis;

	static void DumpObjectState(IConsoleCmdArgs*);
	static void DumpBlockingRMIs(IConsoleCmdArgs*);
	static void Stall(IConsoleCmdArgs*);
	static void SetCDKey(IConsoleCmdArgs*);

#if NEW_BANDWIDTH_MANAGEMENT
	static void GetChannelPerformanceMetrics(IConsoleCmdArgs* pArguments);
	static void SetChannelPerformanceMetrics(IConsoleCmdArgs* pArguments);
#endif // NEW_BANDWIDTH_MANAGEMENT
};

#endif
