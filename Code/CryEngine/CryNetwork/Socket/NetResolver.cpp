// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NetResolver.h"
#include "Network.h"

#include <CryNetwork/NetAddress.h>
#include <CryNetwork/CrySocks.h>

static const TAddressString local_connection = LOCAL_CONNECTION_STRING;

class CResolverToNumericStringVisitor
{
public:
	CResolverToNumericStringVisitor(TAddressString& result) : m_result(result) {}

	bool Ok() const { return true; }

	template<class T>
	void operator()(const T& type)
	{
		Visit(type);
	}

	void Visit(const TNetAddress& var)
	{
		VisitVariant(var);
	}

	void Visit(TLocalNetAddress addr)
	{
		char buf[128];
		cry_sprintf(buf, "%s:%u", LOCAL_CONNECTION_STRING, addr);
		m_result = buf;
	}

	void Visit(LobbyIdAddr addr)
	{
		m_result.Format("%" PRIu64, addr.id);
	}

#if USE_SOCKADDR_STORAGE_ADDR
	void Visit(SSockAddrStorageAddr addr)
	{
		switch (addr.addr.ss_family)
		{
		case AF_INET6:
			{
				sockaddr_in6* pIN6 = (sockaddr_in6*)&addr.addr;

				m_result.Format("%02x%02x%02x%02x.%02x%02x%02x%02x.%02x%02x%02x%02x.%02x%02x%02x%02x:%u",
				                pIN6->sin6_addr.u.Byte[0], pIN6->sin6_addr.u.Byte[1], pIN6->sin6_addr.u.Byte[2], pIN6->sin6_addr.u.Byte[3],
				                pIN6->sin6_addr.u.Byte[4], pIN6->sin6_addr.u.Byte[5], pIN6->sin6_addr.u.Byte[6], pIN6->sin6_addr.u.Byte[7],
				                pIN6->sin6_addr.u.Byte[8], pIN6->sin6_addr.u.Byte[9], pIN6->sin6_addr.u.Byte[10], pIN6->sin6_addr.u.Byte[11],
				                pIN6->sin6_addr.u.Byte[12], pIN6->sin6_addr.u.Byte[13], pIN6->sin6_addr.u.Byte[14], pIN6->sin6_addr.u.Byte[15],
				                ntohs(pIN6->sin6_port));
			}

			break;

		default:
			CryFatalError("CResolverToNumericStringVisitor: SSockAddrStorageAddr Unknown address family %u", addr.addr.ss_family);
		}
	}
#endif

	void Visit(SNullAddr)
	{
		m_result = NULL_CONNECTION_STRING;
	}

	void Visit(SIPv4Addr addr)
	{
		uint32 a = (addr.addr >> 24) & 0xff;
		uint32 b = (addr.addr >> 16) & 0xff;
		uint32 c = (addr.addr >> 8) & 0xff;
		uint32 d = (addr.addr) & 0xff;
		m_result.Format("%u.%u.%u.%u:%u", a, b, c, d, addr.port);
	}

private:
	template<size_t I = 0>
	void VisitVariant(const TNetAddress& var)
	{
		if (var.index() == I)
		{
			Visit(stl::get<I>(var));
		}
		else
		{
			VisitVariant<I + 1>(var);
		}
	}

	TAddressString& m_result;
};
template<>
void CResolverToNumericStringVisitor::VisitVariant<stl::variant_size<TNetAddress>::value>(const TNetAddress& var)
{
	CRY_ASSERT_MESSAGE(false, "Invalid variant index.");
}

class CResolverToNumericStringVisitor_LocalBuffer
{
public:
	CResolverToNumericStringVisitor_LocalBuffer(char* result) : m_result(result) {}

	bool Ok() const { return true; }

	void Visit(TLocalNetAddress addr)
	{
		strcpy(m_result, "localhost");
	}

	void Visit(SNullAddr)
	{
		m_result[0] = 0;
	}

	void Visit(LobbyIdAddr addr)
	{
		sprintf(m_result, "%" PRIu64, addr.id);
	}

#if USE_SOCKADDR_STORAGE_ADDR
	void Visit(SSockAddrStorageAddr addr)
	{
		switch (addr.addr.ss_family)
		{
		case AF_INET6:
			{
				sockaddr_in6* pIN6 = (sockaddr_in6*)&addr.addr;

				sprintf(m_result, "%02x%02x%02x%02x.%02x%02x%02x%02x.%02x%02x%02x%02x.%02x%02x%02x%02x:%u",
				        pIN6->sin6_addr.u.Byte[0], pIN6->sin6_addr.u.Byte[1], pIN6->sin6_addr.u.Byte[2], pIN6->sin6_addr.u.Byte[3],
				        pIN6->sin6_addr.u.Byte[4], pIN6->sin6_addr.u.Byte[5], pIN6->sin6_addr.u.Byte[6], pIN6->sin6_addr.u.Byte[7],
				        pIN6->sin6_addr.u.Byte[8], pIN6->sin6_addr.u.Byte[9], pIN6->sin6_addr.u.Byte[10], pIN6->sin6_addr.u.Byte[11],
				        pIN6->sin6_addr.u.Byte[12], pIN6->sin6_addr.u.Byte[13], pIN6->sin6_addr.u.Byte[14], pIN6->sin6_addr.u.Byte[15],
				        ntohs(pIN6->sin6_port));
			}

			break;

		default:
			CryFatalError("CResolverToNumericStringVisitor_LocalBuffer: SSockAddrStorageAddr Unknown address family %u", addr.addr.ss_family);
		}
	}
#endif

	void Visit(SIPv4Addr addr)
	{
		CRY_PROFILE_FUNCTION(PROFILE_NETWORK);
		uint32 a = (addr.addr >> 24) & 0xff;
		uint32 b = (addr.addr >> 16) & 0xff;
		uint32 c = (addr.addr >> 8) & 0xff;
		uint32 d = (addr.addr) & 0xff;
		// TODO: maybe make this faster (punkbuster will call it often)
		sprintf(m_result, "%u.%u.%u.%u:%u", a, b, c, d, addr.port);
	}

private:
	char* m_result;
};

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
//TODO: IMPLEMENT M CORRECTLY
	#define CResolverToStringVisitor CResolverToNumericStringVisitor
#else
class CResolverToStringVisitor
{
public:
	CResolverToStringVisitor(TAddressString& result) : m_result(result), m_ok(false) {}

	bool Ok() const { return m_ok; }

	template<class T>
	void operator()(const T& type)
	{
		Visit(type);
	}

	void Visit(const TNetAddress& var)
	{
		VisitVariant(var);
	}

	void Visit(TLocalNetAddress addr)
	{
		char buf[128];
		cry_sprintf(buf, "%s:%d", LOCAL_CONNECTION_STRING, addr);
		m_result = buf;
		m_ok = true;
	}

	void Visit(SNullAddr)
	{
		m_result = NULL_CONNECTION_STRING;
		m_ok = true;
	}

	void Visit(LobbyIdAddr addr)
	{
		char buf[128];
		cry_sprintf(buf, "%llX", addr.id);
		m_result = buf;
		m_ok = true;
	}

	void Visit(SIPv4Addr addr)
	{
		CRYSOCKADDR_IN saddr;
		memset(&saddr, 0, sizeof(saddr));
		saddr.sin_family = AF_INET;
		saddr.sin_port = htons(addr.port);
		S_ADDR_IP4(saddr) = htonl(addr.addr);
		m_ok = VisitIP((CRYSOCKADDR*)&saddr, sizeof(saddr), false);
	}

private:
	TAddressString& m_result;
	bool            m_ok;

	bool VisitIP(const CRYSOCKADDR* pAddr, size_t len, bool bracket)
	{
	#if CRY_PLATFORM_ORBIS
		ORBIS_TO_IMPLEMENT;
		m_result = "0:0";
		return false;
	#else
		char host[NI_MAXHOST];
		char serv[NI_MAXSERV];
		char addr[NI_MAXHOST + 3 + NI_MAXSERV + 1];
		int rv = getnameinfo(pAddr, len, host, NI_MAXHOST, serv, NI_MAXSERV, NI_DGRAM | NI_NUMERICSERV);
		if (0 != rv)
		{
		#if CRY_PLATFORM_WINDOWS
			int error = WSAGetLastError();
		#else
			int error = 0;
			switch (rv)
			{
			case EAI_AGAIN:
				error = WSATRY_AGAIN;
				break;
			case EAI_NONAME:
				error = WSAHOST_NOT_FOUND;
				break;
			default:
				error = WSANO_RECOVERY;
				break;
			}
		#endif
			const char* msg = ((CNetwork*)(gEnv->pNetwork))->EnumerateError(MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, error));
			NetWarning("[net] getnameinfo fails: %s", msg);
			m_result = "0:0";
			return false;
		}
		else
		{
			cry_sprintf(addr, bracket ? "[%s]:%s" : "%s:%s", host, serv);
			m_result = addr;
			return true;
		}
	#endif
	}

	template<size_t I = 0>
	void VisitVariant(const TNetAddress& var)
	{
		if (var.index() == I)
		{
			Visit(stl::get<I>(var));
		}
		else
		{
			VisitVariant<I + 1>(var);
		}
	}
};
template<>
void CResolverToStringVisitor::VisitVariant<stl::variant_size<TNetAddress>::value>(const TNetAddress& var)
{
	CRY_ASSERT_MESSAGE(false, "Invalid variant index.");
}
#endif

TAddressString CNetAddressResolver::ToString(const TNetAddress& addr, float timeout)
{
	m_lock.Lock();
	TAddressToNameCache::iterator iter = m_addressToNameCache.find(addr);
	if (iter != m_addressToNameCache.end())
	{
		// force reallocation!
		TAddressString s = iter->second.c_str();
		m_lock.Unlock();
		return s;
	}
	_smart_ptr<CToStringRequest> pReq = new CToStringRequest(addr, m_lock, m_condOut);
	m_requests.push(&*pReq);
	m_cond.Notify();
	m_lock.Unlock();
	if (pReq->TimedWait(timeout))
	{
		TAddressString temp;
		if (pReq->GetResult(temp) == eNRR_Succeeded)
			return temp.c_str(); // force reallocation!
	}
	TAddressString fallback;
	CResolverToNumericStringVisitor visitor(fallback);
	stl::visit(visitor, addr);
	return fallback;
}

TAddressString CNetAddressResolver::ToNumericString(const TNetAddress& addr)
{
	TAddressString output;
	CResolverToNumericStringVisitor visitor(output);
	stl::visit(visitor, addr);
	return output;
}

bool CNetAddressResolver::SlowLookupName(TAddressString str, TNetAddressVec& addrs)
{
	str.Trim();

	if (str == NULL_CONNECTION_STRING)
	{
		addrs.push_back(TNetAddress(SNullAddr()));
	}

	addrs.resize(0);

	if (str.empty())
		return false;
	string::size_type leftBracketPos = str.find('[');
	string::size_type rightBracketPos = str.find(']');
	switch (leftBracketPos)
	{
	case 0:
	// ipv6 style address string... we can't handle this yet
	// TODO: ipv6 support
	default:
		NetWarning("Invalid address: %s", str.c_str());
		return false;
	case string::npos:
		if (rightBracketPos != string::npos)
		{
			NetWarning("Invalid address (contains ']'): %s", str.c_str());
			return false;
		}
		break;
	}

	TAddressString::size_type lastColonPos = str.find(':');
	TAddressString port;
	if (lastColonPos != string::npos && (lastColonPos > rightBracketPos || rightBracketPos == string::npos))
	{
		port = str.substr(lastColonPos + 1);
		str = str.substr(0, lastColonPos);
	}
	// TODO: ipv6 stripping of []

	if (str == local_connection)
	{
		char* end;
		TLocalNetAddress portId = 0;
		if (!port.empty())
		{
			long portNum = strtol(port.c_str(), &end, 0);
			if (portNum < 0 || portNum >= (1u << (8 * sizeof(TLocalNetAddress))) || *end != 0)
			{
				NetWarning("Invalid local address port %s", port.c_str());
				return false;
			}
			portId = (TLocalNetAddress)portNum;
		}
		addrs.push_back(TNetAddress(portId));
	}
	else
	{
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
		char* end;
		long portNum = strtol(port.c_str(), &end, 0);
		if (portNum < 0 || portNum > 65535 || *end != 0)
		{
			NetWarning("Invalid ipv4 address port %s", port.c_str());
			return false;
		}

		in_addr inaddr;
		memset(&inaddr, 0, sizeof(inaddr));
		if (str == "0.0.0.0")
		{
			SIPv4Addr addr; // Defaults to 0, 0
			addr.addr = 0;
			addr.port = portNum;
			addrs.push_back(TNetAddress(addr));
		}
		else if (inet_aton(str.c_str(), &inaddr))   // try dotted decimal
		{
			SIPv4Addr addr;
			addr.addr = ntohl(inaddr.s_addr);
			addr.port = portNum;
			addrs.push_back(TNetAddress(addr));
		}
		else
		{
	#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
			//since it seems dangerous in Linux usage of gethostbyname lets use getaddrinfo
			struct addrinfo* res;
			struct addrinfo hints;
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_DGRAM;

			int resInfo = getaddrinfo(str.c_str(), port.empty() ? NULL : port.c_str(), &hints, &res);
			if (resInfo == 0)
			{
				SIPv4Addr addr;
				inaddr.s_addr = ((CRYSOCKADDR_IN*)(res->ai_addr))->sin_addr.s_addr;
				addr.addr = ntohl(inaddr.s_addr);
				addr.port = portNum;
				addrs.push_back(TNetAddress(addr));
				freeaddrinfo(res);
			}
			else
			{
				NetWarning("DNS lookup failed for '%s'", str.c_str());
			}
	#else
			CRYHOSTENT* host = gethostbyname(str.c_str());

			if (host != NULL)
			{
				SIPv4Addr addr;
				inaddr.s_addr = *(unsigned long*) host->h_addr_list[0];
				addr.addr = ntohl(inaddr.s_addr);
				addr.port = portNum;
				addrs.push_back(TNetAddress(addr));
			}
			else
			{
				NetWarning("DNS lookup failed for '%s'", str.c_str());
			}
	#endif
		}
#elif CRY_PLATFORM_WINAPI
		struct addrinfo* result = NULL;
		struct addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		int err = getaddrinfo(str.c_str(), port.empty() ? NULL : port.c_str(), &hints, &result);
		if (!err)
		{
			for (struct addrinfo* p = result; p; p = p->ai_next)
			{
				switch (p->ai_addr->sa_family)
				{
				case AF_INET:
					{
						CRYSOCKADDR_IN* pAddr = (CRYSOCKADDR_IN*) p->ai_addr;
						SIPv4Addr addr;
						addr.addr = ntohl(S_ADDR_IP4(*pAddr));
						addr.port = ntohs(pAddr->sin_port);
						addrs.push_back(TNetAddress(addr));
					}
					break;

				default:
					NetWarning("Unhandled address family in name lookup: %d", p->ai_addr->sa_family);
				}
			}
		}
		else
		{
			//FormatMessageA not currently available on Durango
	#if CRY_PLATFORM_DURANGO
			NetWarning("getaddrinfo() failed; err = %d for: %s:%s", WSAGetLastError(), str.c_str(), port.empty() ? "<empty>" : port.c_str());
	#else
			NetWarning("%s", gai_strerror(err));
	#endif
			return false;
		}

		if (result)
			freeaddrinfo(result);
#elif CRY_PLATFORM_ORBIS
		char* end;
		long portNum = strtol(port.c_str(), &end, 0);
		if (portNum < 0 || portNum > 65535 || *end != 0)
		{
			NetWarning("Invalid ipv4 address port %s", port.c_str());
			return false;
		}

		if (str == "0.0.0.0")
		{
			SIPv4Addr addr; // Defaults to 0, 0
			addr.addr = 0;
			addr.port = portNum;
			addrs.push_back(TNetAddress(addr));
		}
		else
		{
			SIPv4Addr addr;
			CRYINADDR_T addr32 = CrySock::GetAddrForHostOrIP(str.c_str(), 0);
			addr.addr = ntohl(addr32);
			addr.port = portNum;
			addrs.push_back(TNetAddress(addr));
		}
#endif
	}

	return true;
}

namespace
{
class CIsPrivateAddrVisitor
{
public:
	CIsPrivateAddrVisitor() : m_private(false) {}

	bool IsPrivateAddr() { return m_private; }

	template<class T>
	void operator()(const T& type)
	{
		Visit(type);
	}

	void Visit(const TNetAddress& var)
	{
		VisitVariant(var);
	}

	void Visit(TLocalNetAddress addr) { m_private = true; }

	void Visit(SNullAddr)             {}

	void Visit(LobbyIdAddr)           { m_private = true; }

#if USE_SOCKADDR_STORAGE_ADDR
	void Visit(SSockAddrStorageAddr addr) { m_private = false; }
#endif

	void Visit(SIPv4Addr addr)
	{
		uint32 ip = addr.addr;   // in host byte order, should NOT be in net byte order, since the masks below are all in host byte order
		m_private =
		  (ip & 0xff000000U) == 0x7f000000U || // 127.0.0.0 ~ 127.255.255.255
		  (ip & 0xff000000U) == 0x0a000000U || // 10.0.0.0 ~ 10.255.255.255
		  (ip & 0xfff00000U) == 0xac100000U || // 172.16.0.0 ~ 172.31.255.255
		  (ip & 0xffff0000U) == 0xc0a80000U;   // 192.168.0.0 ~ 192.168.255.255
	}

private:
	template<size_t I = 0>
	void VisitVariant(const TNetAddress& var)
	{
		if (var.index() == I)
		{
			Visit(stl::get<I>(var));
		}
		else
		{
			VisitVariant<I + 1>(var);
		}
	}

	bool m_private;
};
template<>
void CIsPrivateAddrVisitor::VisitVariant<stl::variant_size<TNetAddress>::value>(const TNetAddress& var)
{
	CRY_ASSERT_MESSAGE(false, "Invalid variant index.");
}

}

bool CNetAddressResolver::IsPrivateAddr(const TNetAddress& addr)
{
	CIsPrivateAddrVisitor visitor;
	stl::visit(visitor, addr);
	return visitor.IsPrivateAddr();
}

bool CNetAddressResolver::PopulateCacheFor(const TAddressString& str, TNetAddressVec& addrs)
{
	m_lock.Unlock();
	TNetAddressVec addrsTemp;
	bool ok = SlowLookupName(str, addrsTemp) && !addrsTemp.empty();
	m_lock.Lock();

	if (ok)
	{
		addrs = addrsTemp;
		for (TNetAddressVec::iterator iter = addrs.begin(); iter != addrs.end(); ++iter)
			m_nameToAddressCache.insert(std::make_pair(str, *iter));
	}

	return ok;
}

bool CNetAddressResolver::PopulateCacheFor(const TNetAddress& addr, TAddressString& str)
{
	bool cache = true;
	m_lock.Unlock();
	CResolverToStringVisitor visitor(str);
	stl::visit(visitor, addr);
	if (!visitor.Ok())
	{
		CResolverToNumericStringVisitor visitor2(str);
		stl::visit(visitor2, addr);
		cache = false;
	}
	m_lock.Lock();

	if (cache)
	{
		m_addressToNameCache.insert(std::make_pair(addr, str));
	}

	return true;
}

bool ConvertAddr(const TNetAddress& in, CRYSOCKADDR_IN* pOut)
{
	NET_ASSERT(pOut);

	const SIPv4Addr* pAddr = stl::get_if<SIPv4Addr>(&in);
	if (!pAddr)
		return false;

	memset(pOut, 0, sizeof(*pOut));
	pOut->sin_family = AF_INET;
	pOut->sin_port = htons(pAddr->port);
	S_ADDR_IP4(*pOut) = htonl(pAddr->addr);
	return true;
}

TNetAddress ConvertAddr(const CRYSOCKADDR* pSockAddr, int addrLength)
{
	switch (pSockAddr->sa_family)
	{
	case AF_INET:
		{
			CRYSOCKADDR_IN* pSA = (CRYSOCKADDR_IN*)pSockAddr;
			SIPv4Addr addr;
			addr.addr = ntohl(S_ADDR_IP4(*pSA));
			addr.port = ntohs(pSA->sin_port);
			return TNetAddress(addr);
		}

#if USE_SOCKADDR_STORAGE_ADDR
	case AF_INET6:
		{
			SSockAddrStorageAddr addr;
			memcpy(&addr.addr, pSockAddr, addrLength);
			return TNetAddress(addr);
		}
#endif

	default:
		CryLog("Unknown address length: %d", addrLength);
		break;
	}

	return TNetAddress(SNullAddr());
}

class CConvertAddrVisitor
{
public:
	CConvertAddrVisitor(CRYSOCKADDR* pOut, int* pOutLen) : m_pOut(pOut), m_pOutLen(pOutLen), m_result(false) {}

	bool Ok() const { return m_result; }

	template<class T>
	void operator()(const T& type)
	{
		Visit(type);
	}

	void Visit(const TNetAddress& var)
	{
		VisitVariant(var);
	}

	void Visit(TLocalNetAddress addr)
	{
	}

	void Visit(SNullAddr)
	{
	}

	void Visit(LobbyIdAddr)
	{
	}

#if USE_SOCKADDR_STORAGE_ADDR
	void Visit(SSockAddrStorageAddr addr)
	{
		if (*m_pOutLen >= sizeof(addr.addr))
		{
			memcpy(m_pOut, &addr.addr, sizeof(addr.addr));
			*m_pOutLen = sizeof(addr.addr);
			m_result = true;
		}
	}
#endif

	void Visit(SIPv4Addr addr)
	{
		if (*m_pOutLen < sizeof(CRYSOCKADDR_IN))
			return;
		CRYSOCKADDR_IN* pOut = (CRYSOCKADDR_IN*)m_pOut;
		memset(pOut, 0, sizeof(*pOut));
		pOut->sin_family = AF_INET;
		S_ADDR_IP4(*pOut) = htonl(addr.addr);
		pOut->sin_port = htons(addr.port);
		*m_pOutLen = sizeof(CRYSOCKADDR_IN);
		m_result = true;
	}

private:
	template<size_t I = 0>
	void VisitVariant(const TNetAddress& var)
	{
		if (var.index() == I)
		{
			Visit(stl::get<I>(var));
		}
		else
		{
			VisitVariant<I + 1>(var);
		}
	}

	CRYSOCKADDR* m_pOut;
	int*         m_pOutLen;
	bool         m_result;
};
template<>
void CConvertAddrVisitor::VisitVariant<stl::variant_size<TNetAddress>::value>(const TNetAddress& var)
{
	CRY_ASSERT_MESSAGE(false, "Invalid variant index.");
}

bool ConvertAddr(const TNetAddress& addrIn, CRYSOCKADDR* pSockAddr, int* addrLen)
{
	CConvertAddrVisitor v(pSockAddr, addrLen);
	stl::visit(v, addrIn);
	return v.Ok();
}

CNetAddressResolver::CNetAddressResolver() : m_die(false)
{
	if (!gEnv->pThreadManager->SpawnThread(this, "NetAddressSolver"))
	{
		CryFatalError("Error spawning \"NetAddressSolver\" thread.");
	}
}

CNetAddressResolver::~CNetAddressResolver()
{
	SignalStopWork();
	gEnv->pThreadManager->JoinThread(this, eJM_Join);
}

void CNetAddressResolver::ThreadEntry()
{
	while (true)
	{
		m_lock.Lock();
		while (!m_die && m_requests.empty())
			m_cond.Wait(m_lock);
		if (m_die)
			break;
		TRequestPtr pReq = m_requests.front();
		m_requests.pop();
		m_lock.Unlock();

		pReq->Execute(this);
	}

	while (!m_requests.empty())
	{
		m_requests.front()->Fail();
		m_requests.pop();
	}
	m_lock.Unlock();
}

void CNetAddressResolver::SignalStopWork()
{
	if (m_die)
		return;

	m_lock.Lock();
	m_die = true;
	m_cond.Notify();
	m_lock.Unlock();
}

CNameRequestPtr CNetAddressResolver::RequestNameLookup(const string& name)
{
	CNameRequestPtr pNR = new CNameRequest(name, m_lock, m_condOut);
	m_lock.Lock();
	m_requests.push(&*pNR);
	m_cond.Notify();
	m_lock.Unlock();
	return pNR;
}

void CNameRequest::Execute(CNetAddressResolver* pR)
{
	CryAutoLock<TLock> lkP(m_parentLock);
	NET_ASSERT(m_state == eNRR_Pending);

	TNameToAddressCache::iterator iter = pR->m_nameToAddressCache.lower_bound(m_str);
	if (iter != pR->m_nameToAddressCache.end() && iter->first == m_str)
	{
		do
		{
			m_addrs.push_back(iter->second);
			++iter;
		}
		while (iter != pR->m_nameToAddressCache.end() && iter->first == m_str);
		m_state = eNRR_Succeeded;
		m_parentCond.Notify();
		return;
	}

	if (pR->PopulateCacheFor(m_str, m_addrs))
		m_state = eNRR_Succeeded;
	else
		m_state = eNRR_Failed;

	m_parentCond.Notify();
}

CNameRequest::CNameRequest(const string& name, TNameRequestLock& parentLock, CryConditionVariable& parentCond) :
	m_str(name.c_str() /* force realloc */),
	m_state(eNRR_Pending),
	m_parentLock(parentLock),
	m_parentCond(parentCond)
{
}

CNameRequest::~CNameRequest()
{
	NET_ASSERT(m_state != eNRR_Pending);
}

void CNameRequest::Fail()
{
	CryAutoLock<TLock> lkP(m_parentLock);
	NET_ASSERT(m_state == eNRR_Pending);
	m_state = eNRR_Failed;
	m_parentCond.Notify();
}

void CNameRequest::Wait()
{
	CryAutoLock<TLock> lkP(m_parentLock);
	while (m_state == eNRR_Pending)
		m_parentCond.Wait(m_parentLock);
}

bool CNameRequest::TimedWait(float seconds)
{
	CryAutoLock<TLock> lkP(m_parentLock);
	bool done = true;
	if (m_state == eNRR_Pending)
		done = m_parentCond.TimedWait(m_parentLock, uint32(1000 * seconds));
	return done;
}

ENameRequestResult CNameRequest::GetResult()
{
	CryAutoLock<TLock> lkP(m_parentLock);
	return m_state;
}

TNetAddressVec& CNameRequest::GetAddrs()
{
	CryAutoLock<TLock> lkP(m_parentLock);
	return m_addrs;
}

ENameRequestResult CNameRequest::GetResult(TNetAddressVec& v)
{
	CryAutoLock<TLock> lkP(m_parentLock);
	if (m_state == eNRR_Succeeded)
		v = m_addrs;
	return m_state;
}

string CNameRequest::GetAddrString()
{
	CryAutoLock<TLock> lkP(m_parentLock);
	return m_str.c_str(); // force realloc
}

void CToStringRequest::Execute(CNetAddressResolver* pR)
{
	CryAutoLock<TLock> lkP(m_parentLock);
	NET_ASSERT(m_state == eNRR_Pending);

	TAddressToNameCache::iterator iter = pR->m_addressToNameCache.lower_bound(m_addr);
	if (iter != pR->m_addressToNameCache.end() && iter->first == m_addr)
	{
		m_str = iter->second;
		m_state = eNRR_Succeeded;
		m_parentCond.Notify();
		return;
	}

	if (pR->PopulateCacheFor(m_addr, m_str))
		m_state = eNRR_Succeeded;
	else
		m_state = eNRR_Failed;

	m_parentCond.Notify();
}

CToStringRequest::CToStringRequest(const TNetAddress& name, TNameRequestLock& parentLock, CryConditionVariable& parentCond) :
	m_addr(name),
	m_state(eNRR_Pending),
	m_parentLock(parentLock),
	m_parentCond(parentCond)
{
}

CToStringRequest::~CToStringRequest()
{
	NET_ASSERT(m_state != eNRR_Pending);
}

void CToStringRequest::Fail()
{
	CryAutoLock<TLock> lkP(m_parentLock);
	NET_ASSERT(m_state == eNRR_Pending);
	m_state = eNRR_Failed;
	m_parentCond.Notify();
}

void CToStringRequest::Wait()
{
	CryAutoLock<TLock> lkP(m_parentLock);
	while (m_state == eNRR_Pending)
		m_parentCond.Wait(m_parentLock);
}

bool CToStringRequest::TimedWait(float seconds)
{
	CryAutoLock<TLock> lkP(m_parentLock);
	bool done = true;
	if (m_state == eNRR_Pending)
		done = m_parentCond.TimedWait(m_parentLock, uint32(1000 * seconds));
	return done;
}

ENameRequestResult CToStringRequest::GetResult()
{
	CryAutoLock<TLock> lkP(m_parentLock);
	return m_state;
}

ENameRequestResult CToStringRequest::GetResult(TAddressString& s)
{
	CryAutoLock<TLock> lkP(m_parentLock);
	if (m_state == eNRR_Succeeded)
		s = m_str;
	return m_state;
}
