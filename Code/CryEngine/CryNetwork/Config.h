// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __NET_CONFIG_H__
#define __NET_CONFIG_H__

#pragma once

// enable a good toolkit of debug stuff for when you're developing code
#define USUAL_DEBUG_STUFF 0

#ifndef _RELEASE
	#define SHOW_FRAGMENTATION_USAGE 1        // Will quick log when ever packet size exceeds MTU size
#else
	#define SHOW_FRAGMENTATION_USAGE 0
#endif

#define SHOW_FRAGMENTATION_USAGE_VERBOSE   (SHOW_FRAGMENTATION_USAGE && 0) // Used to debug packet fragmentation
#define UDP_PACKET_FRAGMENTATION_CRC_CHECK 0                               // enable CRC check of UDP fragmented packet, to compare sent and received payloads
#define ENABLE_UDP_PACKET_FRAGMENTATION    1                               // Will automatically fragment packets rather than doing it in the network hardware (should work over internet)

//#define USE_CD_KEYS

#if defined(USE_CD_KEYS)
	#define ALWAYS_CHECK_CD_KEYS 0
#endif

#define ENABLE_CVARS_FLUSHING        0 // Not needed for Crysis 2

#define ENABLE_DEFERRED_RMI_QUEUE    1
#define DEFERRED_RMI_RESERVE_SIZE    (128)

#define CACHED_PACKET_BUFFER_MEMORY  (16384)
#if defined(_RELEASE)
	#define DEBUG_CACHED_PACKET_BUFFER 0
#else
	#define DEBUG_CACHED_PACKET_BUFFER 1
#endif // defined(_RELEASE)

/*
 * PROJECT WIDE CONSTANTS
 */
// UDP header is 28 bytes, Ethernet header is 14 bytes.  Strictly speaking we
// might not want to include the ethernet header here, but it helps for
// measuring at the moment
#define UDP_HEADER_SIZE (28 + 14)
// we make this somewhat smaller than spec because we don't expect to ever see packets this large!
//#define MAX_UDP_PACKET_SIZE 65535
#define MAX_UDP_PACKET_SIZE   4096
// Reserve some space from the packet MTU for stuff like the rebroadcaster header (see SRebroadcasterFrameHeader)
#define RESERVED_PACKET_BYTES 12
// Made a separate definition of the maximum transmission packet size to explicitly prevent
// any assumptions (i.e. don't assume it's the same as the MTU, because it won't be)
#define MAX_TRANSMISSION_PACKET_SIZE (MAX_UDP_PACKET_SIZE - RESERVED_PACKET_BYTES)

/*
 * PROJECT WIDE CONFIGURATION
 */

#define ENABLE_DEBUG_KIT 0

#ifndef _RELEASE
	#define ENABLE_NET_DEBUG_INFO        1
	#define ENABLE_NET_DEBUG_ENTITY_INFO 1
#else
	#define ENABLE_NET_DEBUG_INFO        0
	#define ENABLE_NET_DEBUG_ENTITY_INFO 0
#endif

#define TEST_BANDWIDTH_REDUCTION (0)

// debug the net timer
#define TIMER_DEBUG                        0

#define LOG_ENCODING_TO_FILE               0

#define DETAIL_REGULARLY_SYNCED_ITEM_DEBUG 0

#define CRC8_ASPECT_FORMAT                 1
#define CRC8_ENCODING_GLOBAL               1
#define CRC8_ENCODING_MESSAGE              1

#define MMM_CHECK_LEAKS                    0

#define LOG_MESSAGE_QUEUE                  0
#define LOG_MESSAGE_QUEUE_VERBOSE          (LOG_MESSAGE_QUEUE && 1)
#if CRY_PLATFORM_WINDOWS
// Only use the external message queue log on Windows, but by default it's turned off
	#define LOG_MESSAGE_QUEUE_TO_FILE  (LOG_MESSAGE_QUEUE && 0)
#else
	#define LOG_MESSAGE_QUEUE_TO_FILE  0
#endif
#define LOG_MESSAGE_QUEUE_PREDICTION (LOG_MESSAGE_QUEUE && 1)
#define LOG_BANDWIDTH_SHAPING        0
#if defined(RELEASE)
	#undef LOG_MESSAGE_QUEUE
	#define LOG_MESSAGE_QUEUE            0
	#undef LOG_MESSAGE_QUEUE_VERBOSE
	#define LOG_MESSAGE_QUEUE_VERBOSE    0
	#undef LOG_MESSAGE_QUEUE_TO_FILE
	#define LOG_MESSAGE_QUEUE_TO_FILE    0
	#undef LOG_MESSAGE_QUEUE_PREDICTION
	#define LOG_MESSAGE_QUEUE_PREDICTION 0
	#undef LOG_BANDWIDTH_SHAPING
	#define LOG_BANDWIDTH_SHAPING        0
#endif // defined(RELEASE)

// Use net timers to update the channels
#define USE_CHANNEL_TIMERS 0

// Set to use arithmetic compression or simple bit packing in CNetOutputSerializeImpl and CNetInputSerializeImpl
#if CRY_PLATFORM_DESKTOP && !PC_CONSOLE_NET_COMPATIBLE
	#define USE_ARITHSTREAM 1
#else
	#define USE_ARITHSTREAM 0
#endif
// When not using the arith stream, the below will reduce memory/cpu footprint still further (seperate define at present
//to aid finding bugs introduced)
#define USE_MEMENTO_PREDICTORS (0 || USE_ARITHSTREAM)
// Turn off to simplify the scheduler to reduce time taken to schedule packets : Currently ignores bang/pulses
#define FULL_ON_SCHEDULING     1

// Used to predict replicated values, unit is Hz.
#define REPLICATION_TIME_PRECISION 3000.f

// Lock network thread to wake up only once in a game frame
#define LOCK_NETWORK_FREQUENCY      1
#define MAX_WAIT_TIME_FOR_SEMAPHORE (66)      // Approx 2 30fps frames, so under normal load we will wake as we want, exceptional
                                              //circumstances (like loading, we will wake up enough to not cause players to be
                                              //kicked)
#define LOG_SOCKET_POLL_TIME  (0)             // Logs the time spent polling the socket - useful for detecting bottlenecks
#if defined(_RELEASE)
	#define LOG_SOCKET_TIMEOUTS (0)               // Logs if the socket sleep timed out
#else
	#if CRY_PLATFORM_WINDOWS
		#define LOG_SOCKET_TIMEOUTS (0)               // Logs if the socket sleep timed out
	#endif
#endif // defined(_RELEASE)

#define CHANNEL_TIED_TO_NETWORK_TICK (0 && LOCK_NETWORK_FREQUENCY)

// write the name of each property into the stream, to assist in finding
// encoders that fail
#define CHECK_ENCODING 0
#define TRACK_ENCODING 0
// write a single bit before & after each property (similar to above check)
// sometimes finds bugs that the above cannot
#define MINI_CHECK_ENCODING 1

// debug machinery to ensure that message headers are encoded correctly (must be turned off for retail)
#define DOUBLE_WRITE_MESSAGE_HEADERS 1

// enable tracking the last N messages sent & received (or remove the capability to do so if == 0)
#define MESSAGE_BACKTRACE_LENGTH 0

#define CHECK_MEMENTO_AGES       1

#define LOAD_NETWORK_CODE        0
#define LOAD_NETWORK_PKTSIZE     32
#define LOAD_NETWORK_COUNT       1024

#define VERIFY_MEMENTO_BUFFERS   0

// enable this to check that buffers don't change between receiving a packet,
// and finishing processing it (requires turning off ALLOW_ENCRYPTION)
#define ENABLE_BUFFER_VERIFICATION 0

// enable this to create a much fatter stream that is useful for
// debugging whether compression is working (or not)
#define DEBUG_STREAM_INTEGRITY 0

// enable this to get a warning whenever a value is clamped by quantization
#define DEBUG_QUANTIZATION_CLAMPING 0

// enable 'accurate' (sub-bit estimation) bandwidth profiling
#define ENABLE_ACCURATE_BANDWIDTH_PROFILING 0

// enable this to start debugging that sequence numbers are not
// being upset
#define DEBUG_SEQUENCE_NUMBERS 0

#define DEBUG_TIME_COMPRESSION 0

// enable this to ensure that no duplicate sequence numbers are acked/nacked
#define DETECT_DUPLICATE_ACKS 0

// enable this to write input and output state logs for the connection
// (for debugging logic in the endpoint)
#define DEBUG_ENDPOINT_LOGIC 0

// enable this to write equation based bit rate to a log file per ctpendpoint
// TODO: remove this when we move over to NEW_BANDWIDTH_MEASUREMENT permanently
#define LOG_TFRC 0

// enable a small check that synchronizations have the same basis/non-basis values
#define CHECK_SYNC_BASIS 0

// enable this for debug logging to be available (controlled:)
#define DEBUG_LOGGING                    0

#define VERBOSE_MALFORMED_PACKET_REPORTS 1

#if !PC_CONSOLE_NET_COMPATIBLE
// Using rijndael cipher will pad data to multiple of 16 bytes
// Using stream cipher there is no padding
	#define ENCRYPTION_RIJNDAEL     1
	#define ENCRYPTION_STREAMCIPHER 0
#else
	#define ENCRYPTION_RIJNDAEL     0
	#define ENCRYPTION_STREAMCIPHER 0
#endif

// this is here so that console and pc can talk to each other with the online services
// enabled. once the platform specific services are up and running this shouldn't be needed
#if defined(PC_CONSOLE_NET_COMPATIBLE) && (PC_CONSOLE_NET_COMPATIBLE)
	#define ENABLE_PLATFORM_PROTOCOL 1
#else
	#define ENABLE_PLATFORM_PROTOCOL 1
#endif

#define ALLOW_ENCRYPTION (ENCRYPTION_RIJNDAEL || ENCRYPTION_STREAMCIPHER)

// never ever release with this defined
#define INTERNET_SIMULATOR   1

#define USE_SYSTEM_ALLOCATOR 0

// also never release with these defined
#define STATS_COLLECTOR_FILE        0
#define STATS_COLLECTOR_DEBUG_KIT   0
#define STATS_COLLECTOR_INTERACTIVE 0

#define INCLUDE_DEMO_RECORDING      1

// should the endpoint log incoming or outgoing messages - enable with net_logmessages
#define LOG_INCOMING_MESSAGES   1
#define LOG_OUTGOING_MESSAGES   1
// logging capability for when buffers get updated - enable with net_logbuffers
#define LOG_BUFFER_UPDATES      0

#define LOG_NETCONTEXT_MESSAGES 0

// these switches will cause a file to be written containing the raw data
// that was read or written each block (= LOTS of files, but useful for
// debugging) -- for compressing stream
#define COMPRESSING_STREAM_DEBUG_WHAT_GETS_WRITTEN 0
#define COMPRESSING_STREAM_DEBUG_WHAT_GETS_READ    0
#define COMPRESSING_STREAM_SANITY_CHECK_EVERYTHING 0
#define COMPRESSING_STREAM_USE_BURROWS_WHEELER     0

// enable some debug logs for profile switches
#define ENABLE_PROFILE_DEBUGGING 0

// this switch enables tracking session id's
// it also enables sending them to an internal server
#define ENABLE_SESSION_IDS 0

//Entity id 0000028f is not known in the network system - probably has not been bound
//Entity id 0000028f is not known in the network system - probably has not been bound
//Entity id 0000028f is not known in the network system - probably has not been bound
#define ENABLE_UNBOUND_ENTITY_DEBUGGING 0

#define ENABLE_DISTRIBUTED_LOGGER       0

// log every time a lock gets taken by the place it's taken from
#define LOG_LOCKING 0

// insert checks into CPolymorphicQueue to ensure consistency
#define CHECKING_POLYMORPHIC_QUEUE 0

// for checking very special sequences of problems
#define FORCE_DECODING_TO_GAME_THREAD 0

// insert checks for who's binding/unbinding objects
#define CHECKING_BIND_REFCOUNTS 0

#define LOG_BREAKABILITY        1
#define LOG_ENTITYID_ERRORS     1
#define LOG_MESSAGE_DROPS       1

// server file synchronization mode:
//   0 = don't synchronize files
//   1 = synchronize files from server (INetContext::RegisterServerControlledFile)
//   2 = synchronize files from server *IF* in devmode
#define SERVER_FILE_SYNC_MODE     0

#ifdef _RELEASE
	#define ENABLE_CORRUPT_PACKET_DUMP               0
	#define ENABLE_DEBUG_PACKET_DATA_SIZE            0
#else
	#define ENABLE_CORRUPT_PACKET_DUMP               1
// ENABLE_DEBUG_PACKET_DATA_SIZE isn't supported by arithstream
	#define ENABLE_DEBUG_PACKET_DATA_SIZE            (0 && !USE_ARITHSTREAM)
#endif

#define FILTER_DELEGATED_ASPECTS 0

//do not release with Mem Info turned on
#define ENABLE_NETWORK_MEM_INFO 1

// This shows all the message/RMI serialisations, their values and policies in the log - spammy!
// N.B. It's *NOT* necessarily what ends up on the wire, but rather what the game *WANTS* to send.
// (To find out what's actually sent, use the LOG_MESSAGE_QUEUE #define above, and net_logMessageQueue cvar)
#define ENABLE_SERIALIZATION_LOGGING 0
//

// Optimisation to prevent dirtying all the aspects when binding
#define ENABLE_THIN_BINDS 0

/*
 * from here on is validation of the above
 */

#if CRYNETWORK_RELEASEBUILD

	#undef USUAL_DEBUG_STUFF
	#undef ENABLE_DEBUG_KIT
	#undef TIMER_DEBUG
	#undef LOG_ENCODING_TO_FILE
	#undef DETAIL_REGULARLY_SYNCED_ITEM_DEBUG
	#undef CRC8_ASPECT_FORMAT
	#undef CRC8_ENCODING_GLOBAL
	#undef CRC8_ENCODING_MESSAGE
	#undef MMM_CHECK_LEAKS
	#undef CHECK_ENCODING
	#undef TRACK_ENCODING
	#undef MINI_CHECK_ENCODING
	#undef DOUBLE_WRITE_MESSAGE_HEADERS
	#undef MESSAGE_BACKTRACE_LENGTH
	#undef CHECK_MEMENTO_AGES
	#undef LOAD_NETWORK_CODE
	#undef VERIFY_MEMENTO_BUFFERS
	#undef ENABLE_BUFFER_VERIFICATION
	#undef DEBUG_STREAM_INTEGRITY
	#undef DEBUG_QUANTIZATION_CLAMPING
	#undef ENABLE_ACCURATE_BANDWIDTH_PROFILING
	#undef DEBUG_SEQUENCE_NUMBERS
	#undef DEBUG_TIME_COMPRESSION
	#undef DETECT_DUPLICATE_ACKS
	#undef DEBUG_ENDPOINT_LOGIC
	#undef LOG_TFRC
	#undef CHECK_SYNC_BASIS
	#undef DEBUG_LOGGING
	#undef VERBOSE_MALFORMED_PACKET_REPORTS
	#undef INTERNET_SIMULATOR
	#undef USE_SYSTEM_ALLOCATOR
	#undef STATS_COLLECTOR_FILE
	#undef STATS_COLLECTOR_DEBUG_KIT
	#undef STATS_COLLECTOR_INTERACTIVE
	#undef LOG_INCOMING_MESSAGES
	#undef LOG_OUTGOING_MESSAGES
	#undef LOG_BUFFER_UPDATES
	#undef LOG_NETCONTEXT_MESSAGES
	#undef COMPRESSING_STREAM_DEBUG_WHAT_GETS_WRITTEN
	#undef COMPRESSING_STREAM_DEBUG_WHAT_GETS_READ
	#undef COMPRESSING_STREAM_SANITY_CHECK_EVERYTHING
	#undef ENABLE_PROFILE_DEBUGGING
	#undef ENABLE_SESSION_IDS
	#undef ENABLE_UNBOUND_ENTITY_DEBUGGING
	#undef ENABLE_DISTRIBUTED_LOGGER
	#undef LOG_LOCKING
	#undef CHECKING_POLYMORPHIC_QUEUE
	#undef FORCE_DECODING_TO_GAME_THREAD
	#undef CHECKING_BIND_REFCOUNTS
	#undef LOG_BREAKABILITY
	#undef LOG_ENTITYID_ERRORS
	#undef LOG_MESSAGE_DROPS
	#undef ENABLE_NETWORK_MEM_INFO
	#undef ENABLE_SERIALIZATION_LOGGING

	#define USUAL_DEBUG_STUFF 0

	#if !defined(_RELEASE) // guard against shipping this feature in cases where it cannot be rebuilt by the developer
		#define INTERNET_SIMULATOR      1
		#define ENABLE_DEBUG_KIT        0
		#define ENABLE_NETWORK_MEM_INFO 1
	#else
		#define INTERNET_SIMULATOR      0
		#define ENABLE_DEBUG_KIT        0
		#define ENABLE_NETWORK_MEM_INFO 0
	#endif

	#define TIMER_DEBUG                                0
	#define LOG_ENCODING_TO_FILE                       0
	#define DETAIL_REGULARLY_SYNCED_ITEM_DEBUG         0
	#define CRC8_ASPECT_FORMAT                         0
	#define CRC8_ENCODING_GLOBAL                       0
	#define CRC8_ENCODING_MESSAGE                      0
	#define MMM_CHECK_LEAKS                            0
	#define CHECK_ENCODING                             0
	#define TRACK_ENCODING                             0
	#define MINI_CHECK_ENCODING                        0
	#define DOUBLE_WRITE_MESSAGE_HEADERS               0
	#define MESSAGE_BACKTRACE_LENGTH                   0
	#define CHECK_MEMENTO_AGES                         0
	#define LOAD_NETWORK_CODE                          0
	#define VERIFY_MEMENTO_BUFFERS                     0
	#define ENABLE_BUFFER_VERIFICATION                 0
	#define DEBUG_STREAM_INTEGRITY                     0
	#define DEBUG_QUANTIZATION_CLAMPING                0
	#define ENABLE_ACCURATE_BANDWIDTH_PROFILING        0
	#define DEBUG_SEQUENCE_NUMBERS                     0
	#define DEBUG_TIME_COMPRESSION                     0
	#define DETECT_DUPLICATE_ACKS                      0
	#define DEBUG_ENDPOINT_LOGIC                       0
	#define LOG_TFRC                                   0
	#define CHECK_SYNC_BASIS                           0
	#define DEBUG_LOGGING                              0
	#define VERBOSE_MALFORMED_PACKET_REPORTS           0
	#define USE_SYSTEM_ALLOCATOR                       0
	#define STATS_COLLECTOR_FILE                       0
	#define STATS_COLLECTOR_DEBUG_KIT                  0
	#define STATS_COLLECTOR_INTERACTIVE                0
	#define LOG_INCOMING_MESSAGES                      0
	#define LOG_OUTGOING_MESSAGES                      0
	#define LOG_BUFFER_UPDATES                         0
	#define LOG_NETCONTEXT_MESSAGES                    0
	#define COMPRESSING_STREAM_DEBUG_WHAT_GETS_WRITTEN 0
	#define COMPRESSING_STREAM_DEBUG_WHAT_GETS_READ    0
	#define COMPRESSING_STREAM_SANITY_CHECK_EVERYTHING 0
	#define ENABLE_PROFILE_DEBUGGING                   0
	#define ENABLE_SESSION_IDS                         0
	#define ENABLE_UNBOUND_ENTITY_DEBUGGING            0
	#define ENABLE_DISTRIBUTED_LOGGER                  0
	#define LOG_LOCKING                                0
	#define CHECKING_POLYMORPHIC_QUEUE                 0
	#define FORCE_DECODING_TO_GAME_THREAD              0
	#define CHECKING_BIND_REFCOUNTS                    0
	#define LOG_BREAKABILITY                           0
	#define LOG_ENTITYID_ERRORS                        0
	#define LOG_MESSAGE_DROPS                          0
	#define ENABLE_SERIALIZATION_LOGGING               0

#endif

#ifndef _DEBUG
	#undef CHECKING_POLYMORPHIC_QUEUE
	#define CHECKING_POLYMORPHIC_QUEUE 0
#endif

#if LOG_ENCODING_TO_FILE
	#undef ENABLE_DEBUG_KIT
	#define ENABLE_DEBUG_KIT 1
#endif

#if USUAL_DEBUG_STUFF
	#undef CHECK_ENCODING
	#define CHECK_ENCODING         1
	#undef DEBUG_STREAM_INTEGRITY
	#define DEBUG_STREAM_INTEGRITY 1
	#undef CHECK_SYNC_BASIS
	#define CHECK_SYNC_BASIS       1
	#undef ENABLE_SESSION_IDS
	#define ENABLE_SESSION_IDS     1
	#undef CHECK_MEMENTO_AGES
	#define CHECK_MEMENTO_AGES     1
#endif

#if CHECK_ENCODING
	#if MINI_CHECK_ENCODING
		#error Cannot define CHECK_ENCODING and MINI_CHECK_ENCODING at once
	#endif
#endif

#if ALLOW_ENCRYPTION
	#if ENABLE_BUFFER_VERIFICATION
		#error Cannot define ENABLE_BUFFER_VERIFICATION and ALLOW_ENCRYPTION at once
	#endif
#endif

#if INTERNET_SIMULATOR
	#if _MSC_VER > 1000
		#pragma message ("Internet Simulator is turned on: do not release this binary")
	#endif
#endif

#if ENABLE_NETWORK_MEM_INFO
	#if _MSC_VER > 1000
		#pragma message ("Network MemInfo is turned on: do not release this binary")
	#endif
#endif

#if ENABLE_DEBUG_KIT
	#if _MSC_VER > 1000
		#pragma message ("Network Debug Kit is turned on: do not release this binary")
	#endif
#endif

#if ENABLE_DEBUG_KIT && !ENABLE_SESSION_IDS
	#undef ENABLE_SESSION_IDS
	#define ENABLE_SESSION_IDS 1
	#if _MSC_VER > 1000
		#pragma message ("Session debugging is turned on: do not release this binary")
	#endif
#endif

#if !ENABLE_DEBUG_KIT
	#undef STATS_COLLECTOR_DEBUG_KIT
	#define STATS_COLLECTOR_DEBUG_KIT 0
#endif

#define STATS_COLLECTOR (STATS_COLLECTOR_DEBUG_KIT || STATS_COLLECTOR_FILE || STATS_COLLECTOR_INTERACTIVE)

#if STATS_COLLECTOR
	#undef TRACK_ENCODING
	#define TRACK_ENCODING 1
#endif

#if CHECK_ENCODING
	#undef TRACK_ENCODING
	#define TRACK_ENCODING 1
#endif

#ifndef _RELEASE
	#if CRY_PLATFORM_DESKTOP
		#define DEEP_BANDWIDTH_ANALYSIS 1
	#else
		#define DEEP_BANDWIDTH_ANALYSIS 0
	#endif
#else
	#define DEEP_BANDWIDTH_ANALYSIS 0
#endif

#if DEEP_BANDWIDTH_ANALYSIS
	#undef TRACK_ENCODING
	#define TRACK_ENCODING                      1
	#undef ENABLE_ACCURATE_BANDWIDTH_PROFILING
	#define ENABLE_ACCURATE_BANDWIDTH_PROFILING 1
#endif

#if STATS_COLLECTOR
	#if _MSC_VER > 1000
		#pragma message ("Stats collection is turned on: do not release this binary")
	#endif
#endif

#if ENABLE_DEBUG_KIT && (MESSAGE_BACKTRACE_LENGTH < 100)
	#undef MESSAGE_BACKTRACE_LENGTH
	#define MESSAGE_BACKTRACE_LENGTH 100
#endif

#define CRC8_ENCODING (CRC8_ENCODING_ASPECT || CRC8_ENCODING_GLOBAL)

#if SERVER_FILE_SYNC_MODE
static inline bool ServerFileSyncEnabled()
{
	#if SERVER_FILE_SYNC_MODE == 1
	return true;
	#elif SERVER_FILE_SYNC_MODE == 2
	return gEnv->pSystem->IsDevMode();
	#else
		#error unknown server file sync mode
	#endif
}
#endif // SERVER_FILE_SYNC_MODE

#endif
