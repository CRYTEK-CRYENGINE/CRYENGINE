// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <CryNetwork/NetAddress.h>
#include <CryNetwork/CrySocketError.h>
#include <CryNetwork/CrySocks.h>

typedef uint32 CrySessionHandle;
const CrySessionHandle CrySessionInvalidHandle = 0xffffffff;

enum EDisconnectionCause
{
	//! This cause must be first! - timeout occurred.
	eDC_Timeout = 0,
	//! Incompatible protocols.
	eDC_ProtocolError,
	//! Failed to resolve an address.
	eDC_ResolveFailed,
	//! Versions mismatch.
	eDC_VersionMismatch,
	//! Server is full.
	eDC_ServerFull,
	//! User initiated kick.
	eDC_Kicked,
	//! Teamkill ban/ admin ban.
	eDC_Banned,
	//! Context database mismatch.
	eDC_ContextCorruption,
	//! Password mismatch, cdkey bad, etc.
	eDC_AuthenticationFailed,
	//! Misc. game error.
	eDC_GameError,
	//! DX11 not found.
	eDC_NotDX11Capable,
	//! The nub has been destroyed.
	eDC_NubDestroyed,
	//! Icmp reported error.
	eDC_ICMPError,
	//! NAT negotiation error.
	eDC_NatNegError,
	//! Punk buster detected something bad.
	eDC_PunkDetected,
	//! Demo playback finished.
	eDC_DemoPlaybackFinished,
	//! Demo playback file not found.
	eDC_DemoPlaybackFileNotFound,
	//! User decided to stop playing.
	eDC_UserRequested,
	//! User should have controller connected.
	eDC_NoController,
	//! Unable to connect to server.
	eDC_CantConnect,
	//! Arbitration failed in a live arbitrated session.
	eDC_ArbitrationFailed,
	//! Failed to successfully join migrated game.
	eDC_FailedToMigrateToNewHost,
	//! The session has just been deleted.
	eDC_SessionDeleted,
	//! Kicked due to having a high ping.
	eDC_KickedHighPing,
	//! Kicked due to reserved user joining.
	eDC_KickedReservedUser,
	//! Class registry mismatch.
	eDC_ClassRegistryMismatch,
	//! Global ban.
	eDC_GloballyBanned,
	//! Global ban stage 1 messaging.
	eDC_Global_Ban1,
	//! Global ban stage 2 messaging.
	eDC_Global_Ban2,
	//! This cause must be last! - unknown cause.
	eDC_Unknown
};

enum eStatusCmdMode
{
	eSCM_AllSessions = 1,
	eSCM_LegacyRConCompatabilityConnectionsOnly,
	eSCM_LegacyRConCompatabilityNoNub
};

struct SCrySessionID : public CMultiThreadRefCount
{
	// <interfuscator:shuffle>
	virtual bool operator==(const SCrySessionID& other) = 0;
	virtual bool operator<(const SCrySessionID& other) = 0;
	virtual bool IsFromInvite() const = 0;
	virtual void AsCStr(char* pOutString, int inBufferSize) const = 0;
	// </interfuscator:shuffle>
};

typedef _smart_ptr<SCrySessionID> CrySessionID;
const CrySessionID CrySessionInvalidID = NULL;

struct ICryMatchMakingConsoleCommands
{
public:
	//! Returns the sessionID from the first searchable hosted session at the ip:port specified by cl_serveraddr.
	//! \return Valid sessionId or CrySessionInvalidID if it fails to find a session.
	virtual CrySessionID GetSessionIDFromConsole(void) = 0;

	//! Logs all connections for all sessions to the console (used by 'status' command).
	//! \param mode Documented in eStatusCmdMode.
	//! \see eStatusCmdMode.
	virtual void StatusCmd(eStatusCmdMode mode) = 0;

	//! Kick console command.
	//! \param cxID - connection ID to kick (or CryLobbyInvalidConnectionID to ignore this param).
	//! \param userID - e.g. GPProfile id to kick (or 0 to ignore this param).
	//! \param pName - user name to ban (or NULL to ignore this param).
	//! \param cause - reason for the kick.
	virtual void KickCmd(uint32 cxID, uint64 userID, const char* pName, EDisconnectionCause cause) = 0;

	//! Ban console command.
	//! \param userID  e.g. GPProfile id to ban.
	//! \param timeout   Ban timeout in minutes.
	virtual void BanCmd(uint64 userID, float timeout) = 0;

	//! Ban console command.
	//! \param pUserName User name to ban.
	//! \param timeout   Ban timeout in minutes.
	virtual void BanCmd(const char* pUserName, float timeout) = 0;

	//! Unban console command.
	//! UserID - e.g. GPProfile id to unban.
	virtual void UnbanCmd(uint64 userID) = 0;

	//! Unban console command.	//! \param pUserName User name to unban.
	virtual void UnbanCmd(const char* pUserName) = 0;

	//! Logs all banned users to the console.
	virtual void BanStatus(void) = 0;
};

struct ICryMatchMakingPrivate
{
public:

	virtual TNetAddress       GetHostAddressFromSessionHandle(CrySessionHandle h) = 0;
	virtual ECryLobbyService  GetLobbyServiceTypeForNubSession() = 0;
	virtual void              SessionDisconnectRemoteConnectionFromNub(CrySessionHandle gh, EDisconnectionCause cause) = 0;
	virtual uint16            GetChannelIDFromGameSessionHandle(CrySessionHandle gh) = 0;

	virtual void         LobbyAddrIDTick() {}                // Need to implement if matchmaking service uses LobbyIdAddr's rather than SIPV4Addr
	virtual bool         LobbyAddrIDHasPendingData() { return false; } // Need to implement if matchmaking service uses LobbyIdAddr's rather than SIPV4Addr

	virtual ESocketError LobbyAddrIDSend(const uint8* buffer, uint32 size, const TNetAddress& addr) { return eSE_SocketClosed; } // Need to implement if matchmaking service uses LobbyIdAddr's rather than SIPV4Addr
	virtual void         LobbyAddrIDRecv(void(*cb)(void*, uint8* buffer, uint32 size, CRYSOCKET sock, TNetAddress& addr), void* cbData) {}                           // Need to implement if matchmaking service uses LobbyIdAddr's rather than SIPV4Addr

	virtual bool         IsNetAddressInLobbyConnection(const TNetAddress& addr) { return true; }
};
