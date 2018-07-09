// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UDPDatagramSocket.h"
#include "NetCVars.h"

#include "Network.h"
#if NET_PROFILE_ENABLE
	#include "Protocol/PacketRateCalculator.h"
#endif

#include <CryLobby/CommonICryMatchMaking.h>
#include <CryNetwork/CrySocks.h>

#if CRY_PLATFORM_ORBIS
	#define IPROTO_UDPP2P_SAFE 0        // clashes with IPROTO_IP
	#define UDPP2P_VPORT       1
#endif

#define FRAGMENTED_RESET_ID (0)
#define SOCKET_BUFFER_SIZE  (64 * 1024)

uint64 g_bytesIn = 0, g_bytesOut = 0;

bool MakeSocketNonBlocking(CRYSOCKET sock)
{
#if CRY_PLATFORM_WINAPI
	unsigned long nTrue = 1;
	if (ioctlsocket(sock, FIONBIO, &nTrue) == CRY_SOCKET_ERROR)
		return false;
#elif CRY_PLATFORM_ORBIS
	int nonblocking = 1;
	if (CrySock::setsockopt(sock, SOL_SOCKET, SO_NBIO, (const char*)&nonblocking, sizeof(nonblocking)) == -1)
		return false;
#else
	int nFlags = fcntl(sock, F_GETFL);
	if (nFlags == -1)
		return false;
	nFlags |= O_NONBLOCK;
	if (fcntl(sock, F_SETFL, nFlags) == -1)
		return false;
#endif
	return true;
}

template<class T>
static bool SetSockOpt(CRYSOCKET s, int level, int optname, const T& value)
{
	return 0 == CrySock::setsockopt(s, level, optname, (const char*)&value, sizeof(T));
}

union USockAddr
{
	CRYSOCKADDR_IN ip4;
};

#if ENABLE_UDP_PACKET_FRAGMENTATION
static TNetAddress g_nullAddress = TNetAddress(SNullAddr());
#endif // ENABLE_UDP_PACKET_FRAGMENTATION

CUDPDatagramSocket::CUDPDatagramSocket()
	: m_socket(CRY_INVALID_SOCKET)
	, m_bIsIP4(false)
	, m_pSockIO(nullptr)
#if SHOW_FRAGMENTATION_USAGE_VERBOSE
	, m_fragged(false)
	, m_unfragged(false)
#endif
#if ENABLE_UDP_PACKET_FRAGMENTATION
	, m_pUDPFragBuffer(NULL)
	, m_pFragmentedPackets(NULL)
	, m_fragmentLossRateAccumulator(0.0f)
	, m_packet_sequence_num(0)
#endif
{
}

CUDPDatagramSocket::~CUDPDatagramSocket()
{
	Cleanup();
	NET_ASSERT(m_socket == CRY_INVALID_SOCKET);
	NET_ASSERT(m_sockid == 0);
#if ENABLE_UDP_PACKET_FRAGMENTATION
	if (m_pUDPFragBuffer)
		delete[] m_pUDPFragBuffer;
	if (m_pFragmentedPackets)
		delete[] m_pFragmentedPackets;
#endif
}

bool CUDPDatagramSocket::Init(SIPv4Addr addr, uint32 flags)
{
	ASSERT_GLOBAL_LOCK;

#if SHOW_FRAGMENTATION_USAGE_VERBOSE
	m_fragged = 0;
	m_unfragged = 0;
#endif//SHOW_FRAGMENTATION_USAGE_VERBOSE

#if ENABLE_UDP_PACKET_FRAGMENTATION
	m_RollingIndex = 0;
	if (m_pUDPFragBuffer)
		delete[] m_pUDPFragBuffer;
	m_pUDPFragBuffer = new uint8[FRAG_MAX_MTU_SIZE];
	if (m_pFragmentedPackets)
		delete[] m_pFragmentedPackets;
	m_pFragmentedPackets = new SFragmentedPacket[FRAG_NUM_PACKET_BUFFERS];
	for (uint32 a = 0; a < FRAG_NUM_PACKET_BUFFERS; a++)
	{
		ClearFragmentationEntry(a, FRAGMENTED_RESET_ID, g_nullAddress);
	}
#endif//ENABLE_UDP_PACKET_FRAGMENTATION

	m_socket = CRY_INVALID_SOCKET;
	m_bIsIP4 = true;
	m_pSockIO = &CNetwork::Get()->GetInternalSocketIOManager();
	if (flags & eSF_Online)
	{
		m_pSockIO = &CNetwork::Get()->GetExternalSocketIOManager();
	}

	bool initResult;

#if CRY_PLATFORM_DURANGO
	if (flags & eSF_Online)
	{
		sockaddr_in6 saddr;
		memset(&saddr, 0, sizeof(saddr));

		saddr.sin6_family = AF_INET6;
		saddr.sin6_port = htons(addr.port);

		initResult = Init(AF_INET6, flags, &saddr, sizeof(saddr));
	}
	else
#endif
	{
		CRYSOCKADDR_IN saddr;
		memset(&saddr, 0, sizeof(saddr));

		saddr.sin_family = AF_INET;
		saddr.sin_port = htons(addr.port);
		S_ADDR_IP4(saddr) = htonl(addr.addr);

		initResult = Init(AF_INET, flags, &saddr, sizeof(saddr));
	}

	if (initResult)
	{
#if 0 && CRY_PLATFORM_WINDOWS && !CHECK_ENCODING
		if (!SetSockOpt(m_socket, IPPROTO_IP, IP_DONTFRAGMENT, TRUE))
			return InitWinError();
#endif

		m_sockid = m_pSockIO->RegisterSocket(m_socket, m_protocol);
		if (!m_sockid)
		{
			CloseSocket();
			return false;
		}
		m_pSockIO->SetRecvFromTarget(m_sockid, this);
		m_pSockIO->SetSendToTarget(m_sockid, this);
		for (int i = 0; i < 640; i++)
			m_pSockIO->RequestRecvFrom(m_sockid);

		return true;
	}

	m_socket = CRY_INVALID_SOCKET;
	return false;
}

void CUDPDatagramSocket::Die()
{
	Cleanup();
}

bool CUDPDatagramSocket::IsDead()
{
	return m_socket == CRY_INVALID_SOCKET;
}

CRYSOCKET CUDPDatagramSocket::GetSysSocket()
{
	return m_socket;
}

bool CUDPDatagramSocket::Init(int af, uint32 flags, void* pSockAddr, size_t sizeSockAddr)
{
	m_protocol = IPPROTO_UDP;
#if (USE_LOBBYIDADDR)
	if ((flags & eSF_Online) == 0)
#endif // (USE_LOBBYIDADDR)
	{
		int dgram = SOCK_DGRAM;

#if ENABLE_PLATFORM_PROTOCOL
	#if USE_LIVE
		if (flags & eSF_Online)
		{
			m_protocol = IPPROTO_VDP;
		}
	#endif
	#if USE_PSN
		if ((flags & eSF_Online) && (static_cast<CRYSOCKADDR_IN*>(pSockAddr)->sin_port == htons(CRY_PSN_PORT)))
		{
			m_protocol = IPROTO_UDPP2P_SAFE;
			dgram = CRY_ORBIS_SOCKET_DGRAM;
		}
	#endif
#endif

		m_socket = socket(af, dgram, m_protocol);
		if (m_socket == CRY_INVALID_SOCKET)
			return InitWinError();

#if USE_PSN
		if (dgram == CRY_ORBIS_SOCKET_DGRAM)
		{
	#if CRY_PLATFORM_ORBIS
			SetSockOpt(m_socket, SOL_SOCKET, CRY_ORBIS_SOCKET_USECRYPTO, TRUE);
	#endif
		}
#endif

		if (!MakeSocketNonBlocking(m_socket))
			return false;

		enum EFatality
		{
			eF_Fail,
			eF_Log,
			eF_Ignore
		};

		struct SFlag
		{
			int          so_level;
			int          so_opt;
			ESocketFlags flag;
			int          trueVal;
			int          falseVal;
			EFatality    fatality;
		};

#if CRY_PLATFORM_DURANGO
		if (flags & eSF_Online)
		{
			SetSockOpt(m_socket, IPPROTO_IPV6, IPV6_V6ONLY, FALSE);
		}

		SFlag allflagsudp[] =
		{
			{ SOL_SOCKET, SO_BROADCAST,         eSF_BroadcastSend,    1,                  0,                  eF_Ignore },
			{ IPPROTO_IP, IP_RECEIVE_BROADCAST, eSF_BroadcastReceive, 1,                  0,                  eF_Ignore },
			{ SOL_SOCKET, SO_RCVBUF,            eSF_BigBuffer,        SOCKET_BUFFER_SIZE, SOCKET_BUFFER_SIZE, eF_Ignore },
			{ SOL_SOCKET, SO_SNDBUF,            eSF_BigBuffer,        SOCKET_BUFFER_SIZE, SOCKET_BUFFER_SIZE, eF_Ignore }
		};
#else
		SFlag allflagsudp[] =
		{
			{ SOL_SOCKET, SO_BROADCAST,         eSF_BroadcastSend,    1,                  0,                  eF_Fail   },
			//{ SOL_SOCKET, SO_RCVBUF, eSF_BigBuffer, ((CNetwork::Get()->GetSocketIOManager().caps & eSIOMC_NoBuffering)==0) * 1024*1024, 4096, eF_Ignore },
			{ SOL_SOCKET, SO_RCVBUF,            eSF_BigBuffer,        SOCKET_BUFFER_SIZE, SOCKET_BUFFER_SIZE, eF_Ignore },
			//{ SOL_SOCKET, SO_SNDBUF, eSF_BigBuffer, ((CNetwork::Get()->GetSocketIOManager().caps & eSIOMC_NoBuffering)==0) * 1024*1024, 4096, eF_Ignore },
			{ SOL_SOCKET, SO_SNDBUF,            eSF_BigBuffer,        SOCKET_BUFFER_SIZE, SOCKET_BUFFER_SIZE, eF_Ignore },
			{ SOL_SOCKET, SO_REUSEADDR,         eSF_BigBuffer,        1,                  0,                  eF_Fail   },
	#if CRY_PLATFORM_WINDOWS
			{ IPPROTO_IP, IP_RECEIVE_BROADCAST, eSF_BroadcastReceive, 1,                  0,                  eF_Ignore },
	#endif
		};
#endif

#if USE_PSN
		SFlag allflagsp2pudp[] =
		{
			{ SOL_SOCKET, SO_RCVBUF, eSF_BigBuffer, SOCKET_BUFFER_SIZE, SOCKET_BUFFER_SIZE, eF_Ignore },
			{ SOL_SOCKET, SO_SNDBUF, eSF_BigBuffer, SOCKET_BUFFER_SIZE, SOCKET_BUFFER_SIZE, eF_Ignore },
		};
#endif // #if USE_PSN

#if USE_LIVE
		SFlag allflagsvdp[] =
		{
			{ SOL_SOCKET, SO_RCVBUF, eSF_BigBuffer, SOCKET_BUFFER_SIZE, SOCKET_BUFFER_SIZE, eF_Ignore },
			{ SOL_SOCKET, SO_SNDBUF, eSF_BigBuffer, SOCKET_BUFFER_SIZE, SOCKET_BUFFER_SIZE, eF_Ignore },
		};
#endif

		SFlag* allflags = NULL;
		int numflags = 0;

		switch (m_protocol)
		{
#if USE_PSN
		case IPROTO_UDPP2P_SAFE:
			allflags = allflagsp2pudp;
			numflags = CRY_ARRAY_COUNT(allflagsp2pudp);
			break;
#endif // #if USE_PSN

		case IPPROTO_UDP:
			allflags = allflagsudp;
			numflags = CRY_ARRAY_COUNT(allflagsudp);
			break;

#if USE_LIVE
		case IPPROTO_VDP:
			allflags = allflagsvdp;
			numflags = CRY_ARRAY_COUNT(allflagsvdp);
			break;
#endif
		}

		for (int i = 0; i < numflags; i++)
		{
			if (!SetSockOpt(m_socket, allflags[i].so_level, allflags[i].so_opt, ((flags & allflags[i].flag) == allflags[i].flag) ? allflags[i].trueVal : allflags[i].falseVal))
			{
				switch (allflags[i].fatality)
				{
				case eF_Fail:
					return InitWinError();
				case eF_Log:
					LogWinError();
				case eF_Ignore:
					break;
				}
			}
		}

#if USE_PSN
		sockaddr_in_p2p inP2PSock;

		if (m_protocol == IPROTO_UDPP2P_SAFE)
		{
			// Change CrySock::bind information to be udp2p2 compatible - duplicated to avoid trashing address passed in
			memset(&inP2PSock, 0, sizeof(inP2PSock));
			inP2PSock.sin_family = AF_INET;
			inP2PSock.sin_port = htons(CRY_PSN_PORT);
			inP2PSock.sin_vport = htons(UDPP2P_VPORT);

			pSockAddr = &inP2PSock;
			sizeSockAddr = sizeof(inP2PSock);
		}
#endif
		int bindret = CrySock::bind(m_socket, static_cast<CRYSOCKADDR*>(pSockAddr), sizeSockAddr);
		if (bindret)
			return InitWinError();
	}
#if (USE_LOBBYIDADDR)
	else
	{
		m_socket = 667; // The neighbour of the beast!
	}
#endif // (USE_LOBBYIDADDR)

	return true;
}

void CUDPDatagramSocket::Cleanup()
{
	if (m_sockid)
	{
		SCOPED_GLOBAL_LOCK;
		m_pSockIO->UnregisterSocket(m_sockid);
		m_sockid = SSocketID();
	}
	if (m_socket != CRY_INVALID_SOCKET)
	{
		CloseSocket();
	}
}

void CUDPDatagramSocket::CloseSocket()
{
	if (m_socket != CRY_INVALID_SOCKET)
	{
#if (USE_LOBBYIDADDR)
		if ((gEnv->pLobby != NULL) && (gEnv->pLobby->GetLobbyServiceType() == eCLS_LAN))
#endif // (USE_LOBBYIDADDR)
		{
			CrySock::closesocket(m_socket);
		}
		m_socket = CRY_INVALID_SOCKET;
	}
}

bool CUDPDatagramSocket::InitWinError()
{
	LogWinError();
	CloseSocket();
	return false;
}

void CUDPDatagramSocket::LogWinError()
{
	int error = CrySock::GetLastSocketError();
	LogWinError(error);
}

void CUDPDatagramSocket::LogWinError(int error)
{
	// ugly
	const char* msg = ((CNetwork*)(gEnv->pNetwork))->EnumerateError(MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, error));
	NetWarning("[net] socket error: %s", msg);
}

void CUDPDatagramSocket::GetSocketAddresses(TNetAddressVec& addrs)
{
	if (m_socket == CRY_INVALID_SOCKET)
		return;

#if (USE_LOBBYIDADDR)
	if ((gEnv->pLobby != NULL) && (gEnv->pLobby->GetLobbyServiceType() == eCLS_LAN))
#endif // (USE_LOBBYIDADDR)
	{
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
		char addrBuf[_SS_MAXSIZE];
		int addrLen = _SS_MAXSIZE;

		if (0 == CrySock::getsockname(m_socket, (CRYSOCKADDR*)addrBuf, (CRYSOCKLEN_T*)&addrLen))
		{
			TNetAddress addr = ConvertAddr((CRYSOCKADDR*)addrBuf, addrLen);
			bool valid = true;
			if (stl::get_if<SNullAddr>(&addr))
				valid = false;
			else if (SIPv4Addr* pIPv4 = stl::get_if<SIPv4Addr>(&addr))
			{
				if (!pIPv4->addr)
					valid = false;
			}
	#if USE_SOCKADDR_STORAGE_ADDR
			else if (SSockAddrStorageAddr* pSockAddrStorage = stl::get_if<SSockAddrStorageAddr>(&addr))
			{
				switch (pSockAddrStorage->addr.ss_family)
				{
				case AF_INET6:
					{
						SSockAddrStorageAddr test;
						sockaddr_in6* pIN6 = (sockaddr_in6*)&pSockAddrStorage->addr;
						sockaddr_in6* pTestIN6 = (sockaddr_in6*)&test.addr;

						if (memcmp(&pIN6->sin6_addr, &pTestIN6->sin6_addr, sizeof(pTestIN6->sin6_addr)) == 0)
						{
							valid = false;
						}

						break;
					}

				default:
					CryFatalError("CUDPDatagramSocket::GetSocketAddresses: Unknown address family %u", pSockAddrStorage->addr.ss_family);
					valid = false;
					break;
				}
			}
	#endif
			if (valid)
			{
				addrs.push_back(addr);
				return;
			}
		}
#endif // CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS

#if !CRY_PLATFORM_DURANGO && !CRY_PLATFORM_ORBIS
		std::vector<string> hostnames;

		uint16 nPort;

		USockAddr sockAddr;
		CRYSOCKLEN_T sockAddrSize = sizeof(sockAddr);

		if (0 != CrySock::getsockname(m_socket, (CRYSOCKADDR*)&sockAddr, &sockAddrSize))
		{
			InitWinError();
			return;
		}
		if (sockAddrSize == sizeof(CRYSOCKADDR_IN))
		{
			if (!S_ADDR_IP4(sockAddr.ip4))
				hostnames.push_back("localhost");
			nPort = ntohs(sockAddr.ip4.sin_port);
		}
		else
		{
	#ifdef _DEBUG
			CryFatalError("Unhandled CRYSOCKADDR type");
	#endif
			return;
		}

		char hostnameBuffer[NI_MAXHOST];
		if (!CrySock::gethostname(hostnameBuffer, sizeof(hostnameBuffer)))
			hostnames.push_back(hostnameBuffer);

		if (hostnames.empty())
			return;

		for (std::vector<string>::const_iterator iter = hostnames.begin(); iter != hostnames.end(); ++iter)
		{
			CRYHOSTENT* hp = CrySock::gethostbyname(iter->c_str());
			if (hp)
			{
				switch (hp->h_addrtype)
				{
				case AF_INET:
					{
						SIPv4Addr addr;
						NET_ASSERT(sizeof(addr.addr) == hp->h_length);
						addr.port = nPort;
						for (size_t i = 0; hp->h_addr_list[i]; i++)
						{
							addr.addr = ntohl(*(uint32*)hp->h_addr_list[i]);
							addrs.push_back(TNetAddress(addr));
						}
					}
					break;
				default:
					NetWarning("Unhandled network address type %d length %d bytes", hp->h_addrtype, hp->h_length);
				}
			}
		}
#endif
	}
}

void CUDPDatagramSocket::RegisterBackoffAddress(const TNetAddress& addr)
{
	m_pSockIO->RegisterBackoffAddressForSocket(addr, m_sockid);
}

void CUDPDatagramSocket::UnregisterBackoffAddress(const TNetAddress& addr)
{
	m_pSockIO->UnregisterBackoffAddressForSocket(addr, m_sockid);
}

ESocketError CUDPDatagramSocket::Send(const uint8* pBuffer, size_t nLength, const TNetAddress& to)
{
	if (stl::get_if<TLocalNetAddress>(&to))
	{
		return eSE_Ok;
	}
#if ENABLE_DEBUG_KIT
	CAutoCorruptAndRestore acr(pBuffer, nLength, CVARS.RandomPacketCorruption == 1);
#endif

#if ENABLE_UDP_PACKET_FRAGMENTATION
	if (nLength > FRAG_MAX_MTU_SIZE)
	{
		if (nLength > ((FRAG_MAX_MTU_SIZE - fho_FragHeaderSize) * FRAG_MAX_FRAGMENTS))
		{
	#if SHOW_FRAGMENTATION_USAGE
			NetQuickLog(true, 10.f, "[Fragmentation]: Packet too big for fragmentation buffer, Packet Size = %d", static_cast<int>(nLength));
	#endif // SHOW_FRAGMENTATION_USAGE
			return eSE_BufferTooSmall;
		}

		SendFragmented(pBuffer, nLength, to);
	}
	else
#endif // ENABLE_UDP_PACKET_FRAGMENTATION
	{
#if SHOW_FRAGMENTATION_USAGE_VERBOSE
		m_unfragged++;
#endif

		g_bytesOut += nLength + UDP_HEADER_SIZE;
		if (g_time > CNetCVars::Get().StallEndTime)
		{
			SocketSend(pBuffer, nLength, to);
		}
	}

	return eSE_Ok;
}

ESocketError CUDPDatagramSocket::SendVoice(const uint8* pBuffer, size_t nLength, const TNetAddress& to)
{
#if ENABLE_DEBUG_KIT
	CAutoCorruptAndRestore acr(pBuffer, nLength, CVARS.RandomPacketCorruption == 1);
#endif

	g_bytesOut += nLength + UDP_HEADER_SIZE;
	if (g_time > CNetCVars::Get().StallEndTime)
		m_pSockIO->RequestSendVoiceTo(m_sockid, to, pBuffer, nLength);

	return eSE_Ok;
}

void CUDPDatagramSocket::OnRecvFromComplete(const TNetAddress& from, const uint8* pData, uint32 len)
{
	const uint32 thisRecvBytes = len + UDP_HEADER_SIZE;
	const uint32 thisRecvBits = thisRecvBytes << 3;
	g_bytesIn += thisRecvBytes;

#if NET_MINI_PROFILE || NET_PROFILE_ENABLE
	g_socketBandwidth.periodStats.m_totalBandwidthRecvd += thisRecvBits;
	g_socketBandwidth.periodStats.m_totalPacketsRecvd++;
	g_socketBandwidth.bandwidthStats.m_total.m_totalBandwidthRecvd += thisRecvBits;
	g_socketBandwidth.bandwidthStats.m_total.m_totalPacketsRecvd++;
#endif

#if ENABLE_UDP_PACKET_FRAGMENTATION

	pData = ReceiveFragmented(from, pData, len);    // No point checking for fragmented packets if support for them is off
	if (pData == NULL)
	{
		if (stl::get_if<LobbyIdAddr>(&from) == NULL)
		{
			m_pSockIO->RequestRecvFrom(m_sockid);       // prevent drain of recv counter
		}
		return;                                     // early out, fragmented packet is not complete
	}

#endif

	OnPacket(from, pData, len);
	if (stl::get_if<LobbyIdAddr>(&from) == NULL)
	{
		m_pSockIO->RequestRecvFrom(m_sockid);
	}
}

void CUDPDatagramSocket::OnRecvFromException(const TNetAddress& from, ESocketError err)
{
	if (err != eSE_Cancelled)
	{
		OnError(from, err);
		m_pSockIO->RequestRecvFrom(m_sockid);
	}
}

void CUDPDatagramSocket::OnSendToException(const TNetAddress& from, ESocketError err)
{
	if (err != eSE_Cancelled)
	{
		OnError(from, err);
	}
}

#if ENABLE_UDP_PACKET_FRAGMENTATION
/*static void DumpBytes( const uint8 * p, size_t len )
   {
   char l[256];

   char *o = l;
   for (size_t i=0; i<len; i++)
   {
    o += sprintf(o, "%.2x ", p[i]);
    if ((i & 31)==31)
    {
      NetLog(l);
      o = l;
    }
   }
   if (len & 31)
    NetLog(l);
   }*/

void CUDPDatagramSocket::ClearFragmentationEntry(uint32 index, TFragPacketId id, const TNetAddress& from)
{
	assert(index < FRAG_NUM_PACKET_BUFFERS);
	m_pFragmentedPackets[index].m_from = from;
	m_pFragmentedPackets[index].m_Id = id;
	m_pFragmentedPackets[index].m_ReconstitutionMask = 0;
	m_pFragmentedPackets[index].m_Length = 0;
	m_pFragmentedPackets[index].m_inUse = (stl::get_if<SNullAddr>(&from)) ? false : true;
	m_pFragmentedPackets[index].m_sequence_num = (stl::get_if<SNullAddr>(&from)) ? 0 : ++m_packet_sequence_num;
}

void CUDPDatagramSocket::SendFragmented(const uint8* pBuffer, size_t nLength, const TNetAddress& to)
{
	TFragSeqTransport fraggedCnt = 0;
	TFragSeqTransport expectedFragmentedPacketCount = static_cast<TFragSeqTransport>((nLength + ((FRAG_MAX_MTU_SIZE - fho_FragHeaderSize) - 1)) / (FRAG_MAX_MTU_SIZE - fho_FragHeaderSize));

	assert(expectedFragmentedPacketCount <= FRAG_MAX_FRAGMENTS);
	assert(expectedFragmentedPacketCount > 1);

	m_RollingIndex++; // Always increase the rolling index when sending a fragmented packet, helps to avoid clashes
	#if UDP_PACKET_FRAGMENTATION_CRC_CHECK
	uint crc = CCrc32::Compute(pBuffer, nLength);
	#endif

	#if SHOW_FRAGMENTATION_USAGE_VERBOSE
	m_fragged++;
		#if UDP_PACKET_FRAGMENTATION_CRC_CHECK
	NetQuickLog(true, 10.f, "[Fragmentation]: Sending fragmented packet %d (fragged %d / unFragged %d), size = %d, crc %08X, to %s", m_RollingIndex, m_fragged, m_unfragged, nLength, crc, RESOLVER.ToString(to).c_str());
		#else
	NetQuickLog(true, 10.f, "[Fragmentation]: Sending fragmented packet %d (fragged %d / unFragged %d), size = %" PRISIZE_T ", to %s", m_RollingIndex, m_fragged, m_unfragged, nLength, RESOLVER.ToString(to).c_str());
		#endif // UDP_PACKET_FRAGMENTATION_CRC_CHECK
	#endif   // SHOW_FRAGMENTATION_USAGE_VERBOSE

	while (nLength)
	{
		int blkSize = std::min(nLength, (size_t) FRAG_MAX_MTU_SIZE - fho_FragHeaderSize);

		m_pUDPFragBuffer[fho_HeaderID] = Frame_IDToHeader[eH_Fragmentation];
		memcpy(&m_pUDPFragBuffer[fho_PacketID], &m_RollingIndex, sizeof(m_RollingIndex));
		TFragSeqTransport* pFragInfo = &m_pUDPFragBuffer[fho_FragInfo];
		*pFragInfo = fraggedCnt << FRAG_SEQ_BIT_SIZE;
		*pFragInfo |= expectedFragmentedPacketCount - 1;
		fraggedCnt++;
	#if UDP_PACKET_FRAGMENTATION_CRC_CHECK
		memcpy(&m_pUDPFragBuffer[fho_Checksum], &crc, sizeof(crc));
	#endif
		memcpy(&m_pUDPFragBuffer[fho_FragHeaderSize], pBuffer, blkSize);

		nLength -= blkSize;
		pBuffer += blkSize;

		//DumpBytes(m_pUDPFragBuffer, blkSize + fho_FragHeaderSize);

		g_bytesOut += blkSize + UDP_HEADER_SIZE;
		if (g_time > CNetCVars::Get().StallEndTime)
		{
			m_fragmentLossRateAccumulator += CVARS.net_packetfragmentlossrate;
			if (m_fragmentLossRateAccumulator >= 1.0f)
			{
				m_fragmentLossRateAccumulator -= 1.0f;
			}
			else
			{
				SocketSend(m_pUDPFragBuffer, blkSize + fho_FragHeaderSize, to);
			}
		}
	}
}

const uint8* CUDPDatagramSocket::ReceiveFragmented(const TNetAddress& from, const uint8* pData, uint32& len)
{
	uint8 nType = Frame_HeaderToID[pData[fho_HeaderID]];
	if (nType == eH_Fragmentation)
	{
		TFragPacketId fragmentedPacketID;

		if (len <= fho_FragHeaderSize || len > FRAG_MAX_MTU_SIZE)        // http://revuln.com/files/ReVuln_Game_Engines_0days_tale.pdf  : Fix Vuln 11.3 & 11.2
		{
	#if SHOW_FRAGMENTATION_USAGE
			NetQuickLog(true, 10.f, "[Fragmentation]: Illegal Fragmentation Packet Size (<HeaderSize || >FRAG_MAX_MTU_SIZE)");
	#endif // SHOW_FRAGMENTATION_USAGE
			return NULL;
		}

		memcpy(&fragmentedPacketID, &pData[fho_PacketID], sizeof(fragmentedPacketID));
		int bufferIndex = FindBufferIndex(from, fragmentedPacketID);
		if (bufferIndex == FRAG_NUM_PACKET_BUFFERS)
		{
	#if SHOW_FRAGMENTATION_USAGE
			NetQuickLog(true, 10.f, "[Fragmentation]: Unable to find free buffer for id %d from '%s'", fragmentedPacketID, RESOLVER.ToString(from).c_str());
	#endif // SHOW_FRAGMENTATION_USAGE
			return NULL;
		}

		const TFragSeqTransport* pFragInfo = &pData[fho_FragInfo];
		uint8 fragmentIndex = ((*pFragInfo) >> FRAG_SEQ_BIT_SIZE);
		TFragSeqStorage expected = ~(~(TFragSeqStorage(0)) << (((*pFragInfo) & ((1 << FRAG_SEQ_BIT_SIZE) - 1)) + 1));
		TFragSeqStorage sequence = TFragSeqStorage(1) << fragmentIndex;
		uint8 rSeq = fragmentIndex;
		uint32 seqStart = rSeq * (FRAG_MAX_MTU_SIZE - fho_FragHeaderSize);
		uint32 seqLength = len - fho_FragHeaderSize;

		if ((seqStart + seqLength) > MAX_UDP_PACKET_SIZE)   // Prevent buffer overflow abuse due to abuse of sequence/len combination -- not seen in the wild -- yet!
		{
	#if SHOW_FRAGMENTATION_USAGE
			NetQuickLog(true, 10.f, "[Fragmentation]: Illegal packet offset+length > MAX_UDP_PACKET_SIZE");
	#endif // SHOW_FRAGMENTATION_USAGE
			return NULL;
		}

		if (m_pFragmentedPackets[bufferIndex].m_ReconstitutionMask & sequence)
		{
	#if SHOW_FRAGMENTATION_USAGE
			NetQuickLog(true, 10.f, "[Fragmentation]: Received already reconstructed UPD fragment from %s, discarding old buffer", RESOLVER.ToString(from).c_str());
	#endif // SHOW_FRAGMENTATION_USAGE
			ClearFragmentationEntry(bufferIndex, fragmentedPacketID, from);
		}

		m_pFragmentedPackets[bufferIndex].m_lastUpdate = g_time;
		m_pFragmentedPackets[bufferIndex].m_ReconstitutionMask |= sequence;
	#if SHOW_FRAGMENTATION_USAGE_VERBOSE
		NetQuickLog(true, 10.f, "[Fragmentation]: received fragment %d, rseq %d, now have %x (expecting %x)", fragmentIndex + 1, rSeq, m_pFragmentedPackets[bufferIndex].m_ReconstitutionMask, expected);
	#endif // SHOW_FRAGMENTATION_USAGE_VERBOSE

		memcpy(&m_pFragmentedPackets[bufferIndex].m_FragPackets[0 + seqStart], &pData[fho_FragHeaderSize], seqLength);

	#if UDP_PACKET_FRAGMENTATION_CRC_CHECK
		uint crcOrigin;
		memcpy(&crcOrigin, &pData[fho_Checksum], sizeof(crcOrigin));
	#endif

		m_pFragmentedPackets[bufferIndex].m_Length += seqLength;

		//DumpBytes(pData, len);

		if (m_pFragmentedPackets[bufferIndex].m_ReconstitutionMask == expected)
		{
			// Complete packet received...
			pData = &m_pFragmentedPackets[bufferIndex].m_FragPackets[0];
			len = m_pFragmentedPackets[bufferIndex].m_Length;
	#if SHOW_FRAGMENTATION_USAGE_VERBOSE
			NetQuickLog(true, 10.f, "[Fragmentation]: buffer %d COMPLETE for packet %d", bufferIndex, m_pFragmentedPackets[bufferIndex].m_Id);
	#endif

	#if UDP_PACKET_FRAGMENTATION_CRC_CHECK
			uint crcRecv = CCrc32::Compute(pData, len);
			if (crcRecv != crcOrigin)
			{
				NetQuickLog(true, 10.f, "[Fragmentation]: Original checksum %u differs from that received %u for buffer %d", crcOrigin, crcRecv, bufferIndex);
			}
	#endif

			ClearFragmentationEntry(bufferIndex, FRAGMENTED_RESET_ID, g_nullAddress);     // fix for potential corrupted packets due to not clearing buffer states.
		}
		else
		{
			return NULL;
		}
	}

	return pData;
}

void CUDPDatagramSocket::DiscardStaleEntries()
{
	TNetAddress sources[FRAG_NUM_PACKET_BUFFERS];  // unique sources
	int sources_occupied[FRAG_NUM_PACKET_BUFFERS]; // number of packets per source

	uint sources_oldest_packets_seq[FRAG_NUM_PACKET_BUFFERS];  // smallest packet id per source
	int sources_oldest_packets_index[FRAG_NUM_PACKET_BUFFERS]; // smallest packet id per source

	// collect unique sources
	int last_source = 0;
	for (int i = 0; i != FRAG_NUM_PACKET_BUFFERS; ++i)
	{
		SFragmentedPacket& packet = m_pFragmentedPackets[i];
		if (!packet.m_inUse)
			continue;

		if (g_time.GetDifferenceInSeconds(packet.m_lastUpdate) > CNetCVars::Get().net_fragment_expiration_time)
		{
	#if SHOW_FRAGMENTATION_USAGE
			NetQuickLog(true, 10.f, "[Fragmentation]: reconstruction buffer %d from %s is stale (time) - REUSING", i, RESOLVER.ToString(packet.m_from).c_str());
	#endif // SHOW_FRAGMENTATION_USAGE
			ClearFragmentationEntry(i, FRAGMENTED_RESET_ID, g_nullAddress);
			continue;
		}

		int s;
		for (s = 0; s != last_source; ++s)
		{
			if (sources[s] == packet.m_from)
			{
				++sources_occupied[s];
				if (packet.m_sequence_num < sources_oldest_packets_seq[s])
				{
					sources_oldest_packets_seq[s] = packet.m_sequence_num;
					sources_oldest_packets_index[s] = i;
				}
				break;
			}
		}

		if (s == last_source)
		{
			sources[last_source] = packet.m_from;
			sources_occupied[last_source] = 1;
			sources_oldest_packets_seq[last_source] = packet.m_sequence_num;
			sources_oldest_packets_index[last_source] = i;
			++last_source;
		}
	}

	// discard packets
	for (int s = 0; s != last_source; ++s)
	{
		if (sources_occupied[s] <= CNetCVars::Get().net_max_fragmented_packets_per_source)
			continue;

		int idx = sources_oldest_packets_index[s];
		SFragmentedPacket& packet = m_pFragmentedPackets[idx];

	#if SHOW_FRAGMENTATION_USAGE
		NetQuickLog(true, 10.f, "[Fragmentation]: Dropping stale packet assembly buffer %d from %s, packet id %u", idx, RESOLVER.ToString(packet.m_from).c_str(), packet.m_Id);
	#endif // SHOW_FRAGMENTATION_USAGE

		ClearFragmentationEntry(idx, FRAGMENTED_RESET_ID, g_nullAddress);
	}
}

uint8 CUDPDatagramSocket::FindBufferIndex(const TNetAddress& from, TFragPacketId id)
{
	DiscardStaleEntries();

	int match = FRAG_NUM_PACKET_BUFFERS, unused = FRAG_NUM_PACKET_BUFFERS;

	for (int i = 0; i != FRAG_NUM_PACKET_BUFFERS; ++i)
	{
		SFragmentedPacket& packet = m_pFragmentedPackets[i];
		if (!packet.m_inUse)
		{
			unused = i;
			continue;
		}

		if ((packet.m_from == from) && (packet.m_Id == id))
		{
			match = i;
			break;
		}
	}

	if (match != FRAG_NUM_PACKET_BUFFERS)
	{
	#if SHOW_FRAGMENTATION_USAGE_VERBOSE
		NetQuickLog(true, 10.f, "[Fragmentation]: using EXISTING buffer %d for packet %d from %s", match, id, RESOLVER.ToString(from).c_str());
	#endif // SHOW_FRAGMENTATION_USAGE_VERBOSE
		return match;
	}
	else if (unused != FRAG_NUM_PACKET_BUFFERS)
	{
	#if SHOW_FRAGMENTATION_USAGE_VERBOSE
		NetQuickLog(true, 10.f, "[Fragmentation]: using NEW buffer %d for packet %d from %s", unused, id, RESOLVER.ToString(from).c_str());
	#endif // SHOW_FRAGMENTATION_USAGE_VERBOSE
		ClearFragmentationEntry(unused, id, from);
		return unused;
	}

	return FRAG_NUM_PACKET_BUFFERS;
}

#endif // ENABLE_UDP_PACKET_FRAGMENTATION

void CUDPDatagramSocket::SocketSend(const uint8* pBuffer, size_t nLength, const TNetAddress& to)
{
	ICryMatchMakingPrivate* pMMPrivate = gEnv->pLobby ? gEnv->pLobby->GetMatchMakingPrivate() : nullptr;
	if (stl::get_if<LobbyIdAddr>(&to) && pMMPrivate)
	{
		pMMPrivate->LobbyAddrIDSend(pBuffer, nLength, to);
#if NET_MINI_PROFILE || NET_PROFILE_ENABLE
		CSocketIOManager* pSockIO = (CSocketIOManager*)m_pSockIO;
		pSockIO->RecordPacketSendStatistics(pBuffer, nLength);     // Lobby Sends don't hit the io manager for Send - this ensures they still record the stats
#endif
	}
	else
	{
		m_pSockIO->RequestSendTo(m_sockid, to, pBuffer, nLength);
	}
}
