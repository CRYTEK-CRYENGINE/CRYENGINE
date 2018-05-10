// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/CryVariant.h>
#include <CryNetwork/CrySocks.h>
#include <CryLobby/CommonICryLobby.h>

#if !CRY_PLATFORM_APPLE && !CRY_PLATFORM_ORBIS && defined(__GNUC__)
	#define _SS_MAXSIZE 256
#endif

#if CRY_PLATFORM_DURANGO
	#define USE_SOCKADDR_STORAGE_ADDR 1
#else
	#define USE_SOCKADDR_STORAGE_ADDR 0
#endif

typedef uint16               TLocalNetAddress;

typedef CryFixedStringT<128> TAddressString;

#if USE_SOCKADDR_STORAGE_ADDR
struct SSockAddrStorageAddr
{
	ILINE SSockAddrStorageAddr() { memset(&addr, 0, sizeof(addr)); }
	ILINE SSockAddrStorageAddr(sockaddr_storage addr) { this->addr = addr; }
	sockaddr_storage addr;
	ILINE bool operator<(const SSockAddrStorageAddr& rhs) const
	{
		if (addr.ss_family == rhs.addr.ss_family)
		{
			switch (addr.ss_family)
			{
			case AF_INET6:
				{
					sockaddr_in6* pLHS = (sockaddr_in6*)&addr;
					sockaddr_in6* pRHS = (sockaddr_in6*)&rhs.addr;
					int addrcmp = memcmp(&pLHS->sin6_addr, &pRHS->sin6_addr, sizeof(pLHS->sin6_addr));

					return (addrcmp < 0) || ((addrcmp == 0) && (pLHS->sin6_port < pRHS->sin6_port));
				}

			default:
				CryFatalError("SSockAddrStorageAddr < Unknown address family %u", addr.ss_family);
				return false;
			}
		}
		else
		{
			CryFatalError("SSockAddrStorageAddr < Mismatched address families %u %u", addr.ss_family, rhs.addr.ss_family);
			return false;
		}
	}
	ILINE bool operator==(const SSockAddrStorageAddr& rhs) const
	{
		if (addr.ss_family == rhs.addr.ss_family)
		{
			switch (addr.ss_family)
			{
			case AF_INET6:
				{
					sockaddr_in6* pLHS = (sockaddr_in6*)&addr;
					sockaddr_in6* pRHS = (sockaddr_in6*)&rhs.addr;
					int addrcmp = memcmp(&pLHS->sin6_addr, &pRHS->sin6_addr, sizeof(pLHS->sin6_addr));
					return (addrcmp == 0) && (pLHS->sin6_port == pRHS->sin6_port);
				}

			default:
				CryFatalError("SSockAddrStorageAddr == Unknown address family %u", addr.ss_family);
				return false;
			}
		}
		else
		{
			CryFatalError("SSockAddrStorageAddr == Mismatched address families %u %u", addr.ss_family, rhs.addr.ss_family);
			return false;
		}
	}

	void GetMemoryUsage(ICrySizer* pSizer) const {}
};
#endif

struct LobbyIdAddr
{
	ILINE LobbyIdAddr(uint64 id) { this->id = id; }
	uint64 id;
	ILINE bool operator<(const LobbyIdAddr& rhs) const
	{
		return id < rhs.id;
	}

	ILINE bool operator==(const LobbyIdAddr& rhs) const
	{
		return id == rhs.id;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const {}
};

struct SIPv4Addr
{
	ILINE SIPv4Addr() : addr(0), port(0), lobbyService(eCLS_NumServices) {}
	ILINE SIPv4Addr(uint32 addr, uint16 port) { this->addr = addr; this->port = port; this->lobbyService = eCLS_NumServices; }
	ILINE SIPv4Addr(uint32 addr, uint16 port, ECryLobbyService lobbyService) { this->addr = addr; this->port = port; this->lobbyService = lobbyService; }

	uint32 addr;
	uint16 port;

	// Can be used to specify which Lobby Socket Service to use to send lobby packets with if it differs from the current service.
	uint8 lobbyService;

	ILINE bool operator<(const SIPv4Addr& rhs) const
	{
		return addr < rhs.addr || (addr == rhs.addr && port < rhs.port);
	}

	ILINE bool operator==(const SIPv4Addr& rhs) const
	{
		return addr == rhs.addr && port == rhs.port;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const {}
};

struct SNullAddr
{
	ILINE bool operator<(const SNullAddr& rhs) const
	{
		return false;
	}

	ILINE bool operator==(const SNullAddr& rhs) const
	{
		return true;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const {}
};

typedef CryVariant<
    SNullAddr, TLocalNetAddress, SIPv4Addr, LobbyIdAddr
#if USE_SOCKADDR_STORAGE_ADDR
    , SSockAddrStorageAddr
#endif
    > TNetAddress;

typedef DynArray<TNetAddress>                            TNetAddressVec;
