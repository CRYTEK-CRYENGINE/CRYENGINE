// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NetCVars.h"
#include "NetDebugInfo.h"
#if NEW_BANDWIDTH_MANAGEMENT
	#include <CryGame/IGameFramework.h>
	#include "Protocol/NetNub.h"
#endif // NEW_BANDWIDTH_MANAGEMENT

CNetCVars* CNetCVars::s_pThis;

#if ENABLE_PACKET_PREDICTION
void DumpMessageApproximations(IConsoleCmdArgs* pArgs);
#endif

static void OnNetIDBitsChanged(ICVar* pCVar)
{
	CNetCVars& netCVars = CNetCVars::Get();

	netCVars.net_numNetIDLowBitIDs = 1 << netCVars.net_numNetIDLowBitBits;
	netCVars.net_numNetIDMediumBitIDs = 1 << netCVars.net_numNetIDMediumBitBits;
	netCVars.net_numNetIDHighBitIDs = 1 << netCVars.net_numNetIDHighBitBits;
	netCVars.net_netIDHighBitStart = netCVars.net_numNetIDLowBitIDs + netCVars.net_numNetIDMediumBitIDs;
	netCVars.net_numNetIDs = netCVars.net_netIDHighBitStart + netCVars.net_numNetIDHighBitIDs;
	netCVars.net_invalidNetID = netCVars.net_numNetIDLowBitIDs;
}

CNetCVars::CNetCVars()
{
	memset(this, 0, sizeof(*this));

	NET_ASSERT(!s_pThis);
	s_pThis = this;

	IConsole* c = gEnv->pConsole;

#if ALLOW_ENCRYPTION
	#if defined(_DEBUG) || defined(DEBUG)
	REGISTER_COMMAND_DEV_ONLY("net_ExpKeyXch_test", CExponentialKeyExchange::Test, 0, "");
	#endif
#endif

	REGISTER_CVAR2_DEV_ONLY("cl_tokenid", &TokenId, 0, 0, "Token id expected from client player during connection");

	//	REGISTER_CVAR2( "sys_network_CPU", &CPU, 1, 0, "Run network multithreaded" );
	REGISTER_CVAR2("net_log", &LogLevel, 0, 0, "Logging level of network system");

#if ENABLE_NETWORK_MEM_INFO
	REGISTER_CVAR2_DEV_ONLY("net_meminfo", &MemInfo, 0, 0, "Display memory usage information");
#endif
#if ENABLE_DEBUG_KIT
	REGISTER_CVAR2_DEV_ONLY("net_inspector", &NetInspector, 0, 0, "Logging level of network system");
	REGISTER_CVAR2_DEV_ONLY("net_logcomments", &LogComments, 0, 0, "Enable ultra verbose logging");
	REGISTER_CVAR2_DEV_ONLY("net_showobjlocks", &ShowObjLocks, 0, 0, "show lock debugging display");
	REGISTER_CVAR2_DEV_ONLY("net_pq_log", &EndpointPendingQueueLogging, 0, 0, "Log updates to the pending queue for each channel");
	REGISTER_CVAR2_DEV_ONLY("net_disconnect_on_uncollected_breakage", &DisconnectOnUncollectedBreakage, 0, 0, "Disconnect on uncollected breakage");
	REGISTER_CVAR2_DEV_ONLY("net_debug_connection_state", &DebugConnectionState, 0, 0, "Show current connecting status as a head-up display");
	#if LOG_MESSAGE_DROPS
	REGISTER_CVAR2_DEV_ONLY("net_log_dropped_messages", &LogDroppedMessagesVar, 0, 0, "Log debug information for dropped messages");
	#endif
	REGISTER_CVAR2_DEV_ONLY("net_perfcounters", &PerfCounters, 0, 0, "Display some dedicated server performance counters");
	REGISTER_CVAR2_DEV_ONLY("net_random_packet_corruption", &RandomPacketCorruption, 0, VF_NULL, "");
#endif

#if LOG_MESSAGE_QUEUE
	REGISTER_CVAR2_DEDI_ONLY("net_logMessageQueue", &net_logMessageQueue, 0, VF_NULL, "1 = log only when insufficient bandwidth to empty queues; 2 = log when unsent messages remain; 3 = log all messages");
	pnet_logMessageQueueInjectLabel = REGISTER_STRING("net_logMessageQueueInjectLabel", "", VF_NULL, "Injects the specified text into the message queue log");
#endif // LOG_SEND_QUEUE

#if ENABLE_NET_DEBUG_INFO
	REGISTER_CVAR2_DEDI_ONLY("net_debugInfo", &netDebugInfo, CNetDebugInfo::eP_None, 0, "Net debug screen page to display");
	REGISTER_CVAR2_DEDI_ONLY("net_debugChannelIndex", &netDebugChannelIndex, 0, 0, "Which channel to display in the net debug info screens");
#endif

#if ENABLE_NET_DEBUG_ENTITY_INFO
	REGISTER_CVAR2_DEDI_ONLY("net_debugEntityInfo", &netDebugEntityInfo, 0, VF_NULL, "Display entity debug info. White for entities bound to the network. Grey for entities not bound to the network");
	netDebugEntityInfoClassName = REGISTER_STRING("net_debugEntityInfoClassName", "", VF_NULL, "Display only entities with a class name that contains this string (Case insensitive). Empty string for all entities");
#endif

	REGISTER_CVAR2_DEV_ONLY("net_lobbyUpdateFrequency", &net_lobbyUpdateFrequency, 10, VF_NULL, "Lobby update frequency");

	pSchedulerDebug = REGISTER_STRING_DEV_ONLY("net_scheduler_debug", "0", 0, "Show scheduler debugger for some channel");
	REGISTER_CVAR2_DEDI_ONLY("net_debug_draw_scale", &DebugDrawScale, 1.5f, VF_NULL, "Debug draw scale for net_scheduler_debug and net_channelstats");
	REGISTER_CVAR2_DEDI_ONLY("net_scheduler_debug_mode", &SchedulerDebugMode, 0, VF_NULL, "Scheduler debug display mode");
	REGISTER_CVAR2_DEDI_ONLY("net_scheduler_expiration_period", &SchedulerSendExpirationPeriod, 1.0f, VF_NULL, "Scheduler send queue expiration period in seconds (used for bandwidth tracking)");
	REGISTER_CVAR2_DEDI_ONLY("net_channelstats", &ChannelStats, 0, 0, "Display bandwidth statistics per-channel");
#if !NEW_BANDWIDTH_MANAGEMENT
	REGISTER_CVAR2_DEV_ONLY("net_packetsendrate", &PacketSendRate, 10, 0, "per channel packet send rate (packets per second).  sends whenever possible if this is set to zero.");
#endif // !NEW_BANDWIDTH_MANAGEMENT
	REGISTER_CVAR2_DEV_ONLY("net_bw_aggressiveness", &BandwidthAggressiveness, 0.5f, 0, "Balances TCP friendlyness versus prioritization of game traffic");
#ifdef _RELEASE
	REGISTER_CVAR2("net_inactivitytimeout", &InactivityTimeout, 10.f, 0, "Sets how many seconds without receiving a packet a connection will stay alive for (can only be set on server)");
	REGISTER_CVAR2("net_inactivitytimeoutDevmode", &InactivityTimeoutDevmode, 10.f, 0, "Sets how many seconds without receiving a packet a connection will stay alive for while in devmode (can only be set on server)");
#else
	REGISTER_CVAR2("net_inactivitytimeout", &InactivityTimeout, 30.f, 0, "Sets how many seconds without receiving a packet a connection will stay alive for (can only be set on server)");
	REGISTER_CVAR2("net_inactivitytimeoutDevmode", &InactivityTimeoutDevmode, 30.f, 0, "Sets how many seconds without receiving a packet a connection will stay alive for while in devmode (can only be set on server)");
#endif
	REGISTER_CVAR2_DEV_ONLY("net_backofftimeout", &BackoffTimeout, 6 * 60, 0, "Maximum time to allow a remote machine to stall for before disconnecting");
	REGISTER_CVAR2_DEDI_ONLY("sv_maxmemoryusage", &MaxMemoryUsage, 0, 0, "Maximum memory a dedicated server is allowed to use");

	REGISTER_CVAR2_DEV_ONLY("net_new_queue_behaviour", &NewQueueBehaviour, 1, 0, "When set to 1, this will try to minimize the number of packets sent on a channel.");
	REGISTER_CVAR2_DEV_ONLY("net_safetysleeps", &SafetySleeps, 0, 0, "Controls whether the network layer will perform safety sleeps between context establishment tasks.");
#if NEW_BANDWIDTH_MANAGEMENT
	REGISTER_CVAR2_DEV_ONLY("net_maxpacketsize", &MaxPacketSize, 3200, 0, "Sets the maximum packet size we will attempt to send (not including UDP/other headers)");
#else
	REGISTER_CVAR2_DEV_ONLY("net_maxpacketsize", &MaxPacketSize, 1000, 0, "Sets the maximum packet size we will attempt to send (not including UDP/other headers)");
#endif // NEW_BANDWIDTH_MANAGEMENT
	REGISTER_CVAR2_DEV_ONLY("net_ping_time", &PingTime, 5.f, 0, "Controls the length of time between ping packets.");
	REGISTER_CVAR2_DEV_ONLY("net_keepalive_time", &KeepAliveTime, 10.f, 0, "Controls the length of time between keepalive packets.");

#if CRY_PLATFORM_DURANGO
	REGISTER_CVAR2_DEV_ONLY("net_threadAffinity", &networkThreadAffinity, 5, VF_DUMPTODISK, "Xbox network thread affinity");
#endif

#if LOCK_NETWORK_FREQUENCY == 0
	#if USE_ACCURATE_NET_TIMERS
	REGISTER_CVAR2_DEV_ONLY("net_channelLocalSleepTime", &channelLocalSleepTime, 0.01f, VF_DUMPTODISK, "Sleep time on a local channel. Maximum number of local packets per second = 1/sleepTime.");
	#else
	REGISTER_CVAR2_DEV_ONLY("net_channelLocalSleepTime", &channelLocalSleepTime, 0.00f, VF_DUMPTODISK, "Sleep time on a local channel. Maximum number of local packets per second = 1/sleepTime.");
	#endif // USE_ACCURATE_NET_TIMERS
#endif   // LOCK_NETWORK_FREQUENCY == 0
	REGISTER_CVAR2_DEDI_ONLY("net_socketMaxTimeout", &socketMaxTimeout, 33, VF_DUMPTODISK, "Maximum timeout (milliseconds) that a socket should wait for received data");
	REGISTER_CVAR2_DEDI_ONLY("net_socketBoostTimeout", &socketBoostTimeout, 1, VF_DUMPTODISK, "Single Player Only, acts as throttle during context establishment, the higher the value the longer it takes to load");
	REGISTER_CVAR2_DEDI_ONLY("net_socketMaxTimeoutMultiplayer", &socketMaxTimeoutMultiplayer, 4, VF_DUMPTODISK, "Maximum timeout (milliseconds) that a socket should wait for received data in multiplayer");

#if NET_ASSERT_LOGGING
	REGISTER_CVAR2_DEV_ONLY("net_assertlogging", &AssertLogging, 0, VF_DUMPTODISK, "Log network assertations");
#endif

#if LOG_INCOMING_MESSAGES || LOG_OUTGOING_MESSAGES
	REGISTER_CVAR2_DEDI_ONLY("net_logmessages", &LogNetMessages, 0, VF_NULL, "");
#endif
#if LOG_BUFFER_UPDATES
	REGISTER_CVAR2_DEDI_ONLY("net_logbuffers", &LogBufferUpdates, 0, VF_NULL, "");
#endif
#if STATS_COLLECTOR_INTERACTIVE
	REGISTER_CVAR2_DEV_ONLY("net_showdatabits", &ShowDataBits, 0, 0, "show bits used for different data");
#endif
#if defined(DEDICATED_SERVER)
	static const int DEFAULT_CHEAT_PROTECTION = 3;
#else
	#if ENABLE_DEBUG_KIT
	static const int DEFAULT_CHEAT_PROTECTION = 3;
	#else
	static const int DEFAULT_CHEAT_PROTECTION = 0;//0 for GameK01 for now
	#endif
#endif // defined(DEDICATED_SERVER)
#if !defined(OLD_VOICE_SYSTEM_DEPRECATED)
	pVoiceCodec = REGISTER_STRING_DEV_ONLY("sv_voicecodec", "speex", VF_REQUIRE_LEVEL_RELOAD, "");
	REGISTER_CVAR2_DEV_ONLY("sv_voice_enable_groups", &EnableVoiceGroups, 1, VF_NULL, "");
#endif // !defined(OLD_VOICE_SYSTEM_DEPRECATED)

	REGISTER_CVAR2_DEV_ONLY("net_enable_voice_chat", &EnableVoiceChat, 1, VF_REQUIRE_APP_RESTART, "");
#if !NEW_BANDWIDTH_MANAGEMENT
	REGISTER_CVAR2_DEV_ONLY("net_rtt_convergence_factor", &RTTConverge, 995, VF_NULL, "");
#endif // !NEW_BANDWIDTH_MANAGEMENT

#if defined(USE_CD_KEYS)
	#if !ALWAYS_CHECK_CD_KEYS
	REGISTER_CVAR2_DEDI_ONLY("sv_check_cd_keys", &CheckCDKeys, 0, VF_NULL, "Sets CDKey validation mode:\n 0 - no check\n 1 - check&disconnect unverified\n 2 - check only\n");
	#endif
#endif
	REGISTER_CVAR2_DEDI_ONLY("sv_ranked", &RankedServer, 0, VF_NULL, "Enable statistics report, for official servers only.");
	// network visualizationsv_
#if ENABLE_DEBUG_KIT
	REGISTER_CVAR2_DEV_ONLY("net_vis_mode", &VisMode, 0, VF_NULL, "");
	REGISTER_CVAR2_DEV_ONLY("net_vis_window", &VisWindow, 0 /*.3f*/, VF_NULL, "");
	REGISTER_CVAR2_DEV_ONLY("net_show_ping", &ShowPing, 0, VF_NULL, "");
#endif

#if INTERNET_SIMULATOR
	REGISTER_CVAR_CB_DEV_ONLY(net_PacketLossRate, 0.0f, VF_NULL, "", OnPacketLossRateChange);
	REGISTER_CVAR_CB_DEV_ONLY(net_PacketLagMin, 0.0f, VF_NULL, "", OnPacketExtraLagChange);
	REGISTER_CVAR_CB_DEV_ONLY(net_PacketLagMax, 0.0f, VF_NULL, "", OnPacketExtraLagChange);

	ICVar* pInternetSimLoadProfiles = REGISTER_STRING_CB_DEV_ONLY("net_sim_loadprofiles", "", VF_NULL, "", OnInternetSimLoadProfiles);
	if (pInternetSimLoadProfiles)
	{
		//-- hack because OnChangeCallback is not called when cfg file modifies a cvar.
		OnInternetSimLoadProfiles(pInternetSimLoadProfiles);
	}

	#define DEFAULT_INTERNET_SIMULATOR_PROFILE (-1)
	ICVar* pInternetSimUseProfile = REGISTER_CVAR_CB_DEV_ONLY(net_SimUseProfile, DEFAULT_INTERNET_SIMULATOR_PROFILE, VF_NULL, "", OnInternetSimUseProfileChange);
	if (pInternetSimUseProfile && pInternetSimUseProfile->GetIVal() != DEFAULT_INTERNET_SIMULATOR_PROFILE)
	{
		//-- hack because OnChangeCallback is not called when cfg file modifies a cvar.
		OnInternetSimUseProfileChange(pInternetSimUseProfile);
	}
#endif

	REGISTER_CVAR2_DEV_ONLY("net_highlatencythreshold", &HighLatencyThreshold, 0.5f, VF_NULL, "");   // must be net synched
	REGISTER_CVAR2_DEV_ONLY("net_highlatencytimelimit", &HighLatencyTimeLimit, 1.5f, VF_NULL, "");   // must be net synched

	REGISTER_CVAR2_DEV_ONLY("net_remotetimeestimationwarning", &RemoteTimeEstimationWarning, 0, VF_NULL, "");

	REGISTER_CVAR2_DEV_ONLY("net_enable_tfrc", &EnableTFRC, 1, VF_NULL, "");

	//REGISTER_CVAR2_DEV_ONLY( "net_wmi_check_interval", &WMICheckInterval, gEnv->IsDedicated() ? 0.0f : 2.0f, 0 );
	REGISTER_CVAR2_DEV_ONLY("net_connectivity_detection_interval", &NetworkConnectivityDetectionInterval, 1.0f, VF_NULL, "");

#if ENABLE_UDP_PACKET_FRAGMENTATION
	REGISTER_CVAR_CB_DEV_ONLY(net_packetfragmentlossrate, 0.0f, VF_NULL, "", OnPacketFragmentLossRateChange);
	REGISTER_CVAR2_DEV_ONLY("net_max_fragmented_packets_per_source", &net_max_fragmented_packets_per_source, 2, VF_NULL, "Maximum number of fragemented packets in flight per source");
	REGISTER_CVAR2_DEV_ONLY("net_fragment_expiration_time", &net_fragment_expiration_time, 1.0f, VF_NULL, "Expiration time for stale fragmented packets");
#endif // ENABLE_UDP_PACKET_FRAGMENTATION

	// TODO: remove/cleanup
#if NEW_BANDWIDTH_MANAGEMENT
	REGISTER_CVAR2_DEDI_ONLY("net_availableBandwidthServer", &net_availableBandwidthServer, 720.0f, VF_NULL, "upstream bandwidth in kbit/s available to UDP traffic on the server");
	REGISTER_CVAR2_DEV_ONLY("net_availableBandwidthClient", &net_availableBandwidthClient, 80.0f, VF_NULL, "upstream bandwidth in kbit/s available to UDP traffic on the client");
	REGISTER_CVAR2_DEV_ONLY("net_defaultBandwidthShares", &net_defaultBandwidthShares, 10, VF_NULL, "default number of bandwidth 'shares'");
	REGISTER_CVAR2_DEDI_ONLY("net_defaultPacketRate", &net_defaultPacketRate, 30, VF_NULL, "default number of packets per second");
	REGISTER_CVAR2_DEDI_ONLY("net_defaultPacketRateIdle", &net_defaultPacketRateIdle, 1, VF_NULL, "default number of packets per second when idle");
	REGISTER_COMMAND_DEDI_ONLY("net_getChannelPerformanceMetrics", GetChannelPerformanceMetrics, VF_NULL, "displays the given channel's performance metrics (or all channels if no argument)");
	REGISTER_COMMAND_DEDI_ONLY("net_setChannelPerformanceMetrics", SetChannelPerformanceMetrics, VF_NULL, "sets the given channel's performance metrics (-bws, -dpr, -ipr");
#else
	REGISTER_FLOAT("net_defaultChannelBitRateDesired", 200000.0f, VF_READONLY, "");
	REGISTER_FLOAT("net_defaultChannelBitRateToleranceLow", 0.5f, VF_READONLY, "");
	REGISTER_FLOAT("net_defaultChannelBitRateToleranceHigh", 0.001f, VF_READONLY, "");
	REGISTER_FLOAT("net_defaultChannelPacketRateDesired", 50.0f, VF_READONLY, "");
	REGISTER_FLOAT("net_defaultChannelPacketRateToleranceLow", 0.1f, VF_READONLY, "");
	REGISTER_FLOAT("net_defaultChannelPacketRateToleranceHigh", 2.0f, VF_READONLY, "");
	REGISTER_FLOAT("net_defaultChannelIdlePacketRateDesired", 0.05f, VF_READONLY, "");
#endif // NEW_BANDWIDTH_MANAGEMENT

#if !defined(OLD_VOICE_SYSTEM_DEPRECATED)
	REGISTER_CVAR2_DEV_ONLY("net_voice_lead_packets", &VoiceLeadPackets, 5, VF_NULL, "");
	REGISTER_CVAR2_DEV_ONLY("net_voice_trail_packets", &VoiceTrailPackets, 5, VF_NULL, "");
	REGISTER_CVAR2_DEV_ONLY("net_voice_proximity", &VoiceProximity, 0.0f, VF_NULL, "");
#endif // !defined(OLD_VOICE_SYSTEM_DEPRECATED)

	REGISTER_CVAR2("net_lan_scanport_first", &LanScanPortFirst, 64087, VF_DUMPTODISK, "Starting port for LAN games scanning");
	REGISTER_CVAR2("net_lan_scanport_num", &LanScanPortNum, 5, VF_DUMPTODISK, "Num ports for LAN games scanning");

	REGISTER_COMMAND("net_set_cdkey", SetCDKey, VF_DUMPTODISK, "");
	StatsLogin = REGISTER_STRING("net_stats_login", "", VF_DUMPTODISK, "Login for reporting stats on dedicated server");
	StatsPassword = REGISTER_STRING("net_stats_pass", "", VF_DUMPTODISK, "Password for reporting stats on dedicated server");

	REGISTER_COMMAND_DEV_ONLY("net_dump_object_state", DumpObjectState, 0, "");

#if ENABLE_DEBUG_KIT
	REGISTER_COMMAND_DEV_ONLY("net_stall", Stall, VF_NULL, "stall the network thread for a time (default 1 second, but can be passed in as a parameter");
#endif

	REGISTER_CVAR2_DEV_ONLY("net_minTCPFriendlyBitRate", &MinTCPFriendlyBitRate, 30000.f, VF_NULL, "Minimum bitrate allowed for TCP friendly transmission");

#if !defined(OLD_VOICE_SYSTEM_DEPRECATED)
	REGISTER_CVAR2_DEV_ONLY("net_UseDeprecatedVoiceSystem", &useDeprecatedVoiceSystem, 1, VF_DUMPTODISK, "Use deprecated voice system");
#endif // !defined(OLD_VOICE_SYSTEM_DEPRECATED)

	NET_PROFILE_REG_CVARS();

#if ENABLE_CORRUPT_PACKET_DUMP
	REGISTER_CVAR2_DEV_ONLY("net_packet_read_debug_output", &packetReadDebugOutput, 0, VF_NULL, "Enable debug output when reading a packet");
#endif

	// BREAKAGE
	REGISTER_CVAR2_DEV_ONLY("net_breakage_sync_entities", &breakageSyncEntities, 1, VF_NULL, "Allow net syncing/binding of breakage entities");

	REGISTER_CVAR2_DEV_ONLY("net_enable_watchdog_timer", &enableWatchdogTimer, 0, VF_NULL, "Enable watchdog timer. Not needed if CryLobby is being used.");

#if ENABLE_PACKET_PREDICTION
	REGISTER_COMMAND_DEV_ONLY("net_DumpMessageApproximations", DumpMessageApproximations, VF_NULL, "Dumps the statistics used for message queue limit avoidance");
#endif

	REGISTER_CVAR_CB(net_numNetIDLowBitBits, 5, VF_CHEAT, "Number of bits used for low bit NetIDs. By default players start allocating from low bit NetIDs.", OnNetIDBitsChanged);
	REGISTER_CVAR_CB(net_numNetIDMediumBitBits, 9, VF_CHEAT, "Number of bits used for medium bit NetIDs. By default dynamic entities start allocating from medium bit NetIDs.", OnNetIDBitsChanged);
	REGISTER_CVAR_CB(net_numNetIDHighBitBits, 13, VF_CHEAT, "Number of bits used for high bit NetIDs. By default static entities start allocating from high bit NetIDs.", OnNetIDBitsChanged);

	OnNetIDBitsChanged(NULL);

	REGISTER_CVAR2_DEDI_ONLY("net_dedi_scheduler_server_port", &net_dedi_scheduler_server_port, 47264, VF_NULL, "Dedi scheduler server port");
	REGISTER_CVAR2_DEDI_ONLY("net_dedi_scheduler_client_base_port", &net_dedi_scheduler_client_base_port, 47265, VF_NULL, "Dedi scheduler client base port");

	REGISTER_STRING_DEV_ONLY("net_profile_deep_bandwidth_logname", "profile_messages.log", VF_NULL, "log name for net deep bandwidth");
	REGISTER_CVAR2_DEV_ONLY("net_profile_deep_bandwidth_logging", &net_profile_deep_bandwidth_logging, 0, VF_NULL, "enable/disable logging");
}

CNetCVars::~CNetCVars()
{
	NET_ASSERT(s_pThis);
	s_pThis = 0;
}

void CNetCVars::DumpObjectState(IConsoleCmdArgs* pArgs)
{
	SCOPED_GLOBAL_LOCK;
	CNetwork::Get()->BroadcastNetDump(eNDT_ObjectState);
}

void CNetCVars::SetCDKey(IConsoleCmdArgs* pArgs)
{
	SCOPED_GLOBAL_LOCK;

	if (pArgs->GetArgCount() > 1)
	{
		CNetwork::Get()->SetCDKey(pArgs->GetArg(1));
	}
	else
	{
		CNetwork::Get()->SetCDKey("");
	}
}

//------------------------------------------------------------------------
#if ENABLE_DEBUG_KIT
void CNetCVars::Stall(IConsoleCmdArgs* pArgs)
{
	float stallTime = 1.0f;
	if (pArgs && pArgs->GetArgCount() >= 2)
	{
		if (!sscanf(pArgs->GetArg(1), "%f", &stallTime))
			stallTime = 1.0f;
	}
	if (stallTime > 0.0f)
	{
		NetLogAlways("Stalling all sending for %f seconds", stallTime);
		Get().StallEndTime = g_time + stallTime;
	}
	else
	{
		NetLogAlways("Sleeping for %f seconds", -stallTime);
		SCOPED_GLOBAL_LOCK;
		CrySleep((DWORD)(-1000.0f * stallTime));
	}
}
#endif

#if INTERNET_SIMULATOR

	#include "Socket/InternetSimulatorSocket.h"

void CNetCVars::OnInternetSimUseProfileChange(ICVar* val)
{
	CInternetSimulatorSocket::SetProfile((CInternetSimulatorSocket::EProfile)val->GetIVal());
}

void CNetCVars::OnInternetSimLoadProfiles(ICVar* val)
{
	CInternetSimulatorSocket::LoadXMLProfiles((const char*)val->GetString());
}

#endif

#if NEW_BANDWIDTH_MANAGEMENT
void CNetCVars::GetChannelPerformanceMetrics(IConsoleCmdArgs* pArguments)
{
	CNetNub* pNetNub = (CNetNub*)gEnv->pGameFramework->GetServerNetNub();
	if (pNetNub)
	{
		uint32 id = 0;

		if (pArguments != NULL)
		{
			switch (pArguments->GetArgCount())
			{
			case 1:
				// intentionally left blank
				break;

			case 2:
				id = atoi(pArguments->GetArg(1));
				break;

			default:
				NetLogAlways("GetChannelPerformanceMetrics: supply a channel id or nothing for all channels");
				break;
			}
		}

		pNetNub->LogChannelPerformanceMetrics(id);
	}
	else
	{
		NetLogAlways("GetChannelPerformanceMetrics: Server only command");
	}
}

void CNetCVars::SetChannelPerformanceMetrics(IConsoleCmdArgs* pArguments)
{
	CNetNub* pNetNub = (CNetNub*)gEnv->pGameFramework->GetServerNetNub();
	if (pNetNub)
	{
		INetChannel::SPerformanceMetrics metrics;
		uint32 id = 0;
		uint32 argument = 1;
		uint32 argc = pArguments->GetArgCount();

		if ((pArguments != NULL) && (argc >= 4))
		{
			id = atoi(pArguments->GetArg(argument));

			while (++argument < argc)
			{
				if (memicmp(pArguments->GetArg(argument), "-bws", 4) == 0)
				{
					if (++argument < argc)
					{
						metrics.m_bandwidthShares = atoi(pArguments->GetArg(argument));
						continue;
					}
				}

				if (memicmp(pArguments->GetArg(argument), "-dpr", 4) == 0)
				{
					if (++argument < argc)
					{
						metrics.m_packetRate = atoi(pArguments->GetArg(argument));
						continue;
					}
				}

				if (memicmp(pArguments->GetArg(argument), "-ipr", 4) == 0)
				{
					if (++argument < argc)
					{
						metrics.m_packetRateIdle = atoi(pArguments->GetArg(argument));
						continue;
					}
				}

				NetLogAlways("Unrecognised argument [%s]", pArguments->GetArg(argument));
			}

			pNetNub->SetChannelPerformanceMetrics(id, metrics);
		}
		else
		{
			NetLogAlways("Usage: -bws <bandwidth shares>");
			NetLogAlways("       -dpr <desired packet rate>");
			NetLogAlways("       -ipr <idle packet rate>");
		}
	}
	else
	{
		NetLogAlways("GetChannelPerformanceMetrics: Server only command");
	}
}
#endif // NEW_BANDWIDTH_MANAGEMENT
