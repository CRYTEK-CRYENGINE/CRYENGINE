// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __NETRESOLVER_H__
#define __NETRESOLVER_H__

#pragma once

#include <CryNetwork/NetAddress.h>

#include <CryMemory/STLGlobalAllocator.h>
#include <queue>
#include <CryLobby/CommonICryLobby.h>
#include <CryThreading/IThreadManager.h>

class CNetAddressResolver;

struct IResolverRequest : public CMultiThreadRefCount
{
protected:
	IResolverRequest()
	{
		++g_objcnt.nameResolutionRequest;
	}
	~IResolverRequest()
	{
		--g_objcnt.nameResolutionRequest;
	}

private:
	friend class CNetAddressResolver;

	virtual void Execute(CNetAddressResolver* pR) = 0;
	virtual void Fail() = 0;
};

enum ENameRequestResult
{
	eNRR_Pending,
	eNRR_Succeeded,
	eNRR_Failed
};

typedef CryMutex TNameRequestLock;

class CNameRequest : public IResolverRequest
{
public:
	~CNameRequest();

	virtual ENameRequestResult GetResult(TNetAddressVec& res);
	virtual ENameRequestResult GetResult();
	virtual TNetAddressVec& GetAddrs();
	string                  GetAddrString();
	virtual void            Wait();
	bool                    TimedWait(float seconds);

private:
	friend class CNetAddressResolver;

	CNameRequest(const string& name, TNameRequestLock& parentLock, CryConditionVariable& parentCond);

	void Execute(CNetAddressResolver* pR);
	void Fail();

	typedef TNameRequestLock TLock;
	TLock&                      m_parentLock;
	CryConditionVariable&       m_parentCond;
	TNetAddressVec              m_addrs;
	TAddressString              m_str;
	volatile ENameRequestResult m_state;
};

class CToStringRequest : public IResolverRequest
{
public:
	~CToStringRequest();

	ENameRequestResult GetResult(TAddressString& res);
	ENameRequestResult GetResult();
	void               Wait();
	bool               TimedWait(float seconds);

private:
	friend class CNetAddressResolver;

	CToStringRequest(const TNetAddress& addr, TNameRequestLock& parentLock, CryConditionVariable& parentCond);

	void Execute(CNetAddressResolver* pR);
	void Fail();

	typedef TNameRequestLock TLock;
	TLock&                      m_parentLock;
	CryConditionVariable&       m_parentCond;
	TNetAddress                 m_addr;
	TAddressString              m_str;
	volatile ENameRequestResult m_state;
};

typedef _smart_ptr<CNameRequest>                                                                                                               CNameRequestPtr;

typedef std::multimap<TAddressString, TNetAddress, std::less<TAddressString>, stl::STLGlobalAllocator<std::pair<const TAddressString, TNetAddress>>> TNameToAddressCache;
typedef std::map<TNetAddress, TAddressString, std::less<TNetAddress>, stl::STLGlobalAllocator<std::pair<const  TNetAddress, TAddressString>>>         TAddressToNameCache;

class CNetAddressResolver : public IThread
{
public:
	CNetAddressResolver();
	~CNetAddressResolver();

	// Start accepting work on thread
	virtual void ThreadEntry();

	// Signals the thread that it should not accept anymore work and exit
	void                    SignalStopWork();

	TAddressString          ToString(const TNetAddress& addr, float timeout = 0.01f);
	virtual TAddressString  ToNumericString(const TNetAddress& addr);
	virtual CNameRequestPtr RequestNameLookup(const string& name);

	bool                    IsPrivateAddr(const TNetAddress& addr);

private:
	friend class CNameRequest;
	friend class CToStringRequest;

	TNameToAddressCache m_nameToAddressCache;
	TAddressToNameCache m_addressToNameCache;

	bool PopulateCacheFor(const TAddressString& str, TNetAddressVec& addrs);
	bool PopulateCacheFor(const TNetAddress& addr, TAddressString& str);
	bool SlowLookupName(TAddressString str, TNetAddressVec& addrs);

	typedef TNameRequestLock             TLock;
	typedef _smart_ptr<IResolverRequest> TRequestPtr;
	TLock                m_lock;
	CryConditionVariable m_cond;
	CryConditionVariable m_condOut;
	volatile bool        m_die;
	typedef std::queue<TRequestPtr, std::deque<TRequestPtr, stl::STLGlobalAllocator<TRequestPtr>>> TRequestQueue;
	TRequestQueue        m_requests;
};

bool        ConvertAddr(const TNetAddress& addrIn, CRYSOCKADDR_IN* pSockAddr);
bool        ConvertAddr(const TNetAddress& addrIn, CRYSOCKADDR* pSockAddr, int* addrLen);
TNetAddress ConvertAddr(const CRYSOCKADDR* pSockAddr, int addrLength);

#endif
