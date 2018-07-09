// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryTCPService.cpp
//  Version:     v1.00
//  Created:     29/03/2010 by Paul Mikell.
//  Description: CCryTCPService member definitions
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CryTCPService.h"

#if CRY_PLATFORM_ORBIS
	#include <netinet/tcp.h>
#endif

#if USE_CRY_TCPSERVICE

	#define SECONDS_BETWEEN_DNS_RETRIES   10
	#define NUM_TIMES_TO_GRAB_FROM_SOCKET 30
	#if CRY_PLATFORM_WINDOWS
		#define RECV_BUFFER_SIZE            64 * 1024
	#else
		#define RECV_BUFFER_SIZE            4 * 1024
	#endif
	#define INVALID_SOCKET_INDEX          -1
	#define MAX_URL_LENGTH                255
	#define MAX_URL_BUF_SIZE              (MAX_URL_LENGTH + 1)

CCryTCPService::CCryTCPService(CCryTCPServiceFactory* pOwner)
	: m_cnt(0),
	m_pOwner(pOwner),
	m_numSocketsOpen(0),
	m_numSocketsToFree(0),
	m_connectionStatus(eCTCPSCS_NotConnected),
	m_dataQueueLength(0),
	m_dataQueueSize(0),
	m_resolveState(eRS_NotStarted),
	m_resolveLastFailed()
{
}

// Description:
//	 Destructor for CCryTCPService.
/* dtor */
CCryTCPService::~CCryTCPService()
{
	Terminate(true);
}

// Description:
//	 Initialize.
// Arguments:
//	 serviceConfigName - service config name
//	 serverName - server name
//	 port - port
//   urlPrefix - URL prefix
// Return:
//	 Result code.
ECryTCPServiceResult CCryTCPService::Initialise(const CCryTCPServiceFactory::TServiceConfigName& serviceConfigName, const CCryTCPServiceFactory::TServerName& serverName, uint16 port, const CCryTCPServiceFactory::TURLPrefix& urlPrefix)
{
	ECryTCPServiceResult result = eCTCPSR_Ok;

	m_serviceConfigName = serviceConfigName;
	m_serverName = serverName;
	m_port = port;
	m_urlPrefix = urlPrefix;
	m_resolveState = eRS_NotStarted;

	return result;
}

// Description:
//	 Terminate.
// Arguments:
//	 isDestructing - is this object's destructor running?
// Return:
//	 Result code.
ECryTCPServiceResult
CCryTCPService::Terminate(
  bool isDestructing)
{
	DiscardQueue();

	return eCTCPSR_Ok;
}

void CCryTCPService::DiscardQueueElement()
{
	STCPServiceDataPtr pCurr = m_dataQueue.front();

	if (pCurr->tcpServCb)
	{
		pCurr->tcpServCb(eCTCPSR_Failed, pCurr->pUserArg);
	}
	else
	{
		if (pCurr->tcpServReplyCb)
		{
			pCurr->tcpServReplyCb(eCTCPSR_Failed, pCurr->pUserArg, pCurr, 0, 0, true);
		}
	}

	CloseConnection(pCurr->m_socketIdx);
	m_dataQueueLength--;
	m_dataQueueSize -= pCurr->length;

	m_dataQueue.pop();
}

// Description:
//	 Get address.
// Arguments:
//	 addr - address
// Return:
//	 Result code.
ENameRequestResult
CCryTCPService::GetAddress(
  CRYSOCKADDR_IN& addr)
{
	ENameRequestResult result = eNRR_Failed;

	switch (m_resolveState)
	{
	case eRS_NotStarted:
	case eRS_InProgress:
		result = eNRR_Pending;
		break;
	case eRS_Succeeded:
		addr.sin_family = AF_INET;
		addr.sin_addr = m_addr;
		addr.sin_port = htons(m_port);
		result = eNRR_Succeeded;
		break;
	case eRS_Failed:
	case eRS_WaitRetry:
		result = eNRR_Failed;
		break;
	}

	return result;
}

// Description:
//	 Connect socket.
// Arguments:
//	 sockIdx- socket index
// Return:
//	 Result code.
ESocketError
CCryTCPService::Connect(
  int32& sockIdx)
{
	ESocketError result = eSE_MiscFatalError;

	CRYSOCKADDR_IN addr;
	ENameRequestResult ret = GetAddress(addr);

	if (ret == eNRR_Succeeded)
	{
		result = m_pOwner->Connect(addr, sockIdx);

		if (result == eSE_Ok)
		{
			m_connectionStatus = eCTCPSCS_Connected;
			++m_numSocketsOpen;
		}
		else
		{
			m_connectionStatus = eCTCPSCS_NotConnected;
		}
	}

	return result;
}

// Description:
//	 Send data.
// Arguments:
//	 pData - transaction
// Return:
//	 Result code.
ESocketError
CCryTCPService::Send(
  STCPServiceDataPtr pData)
{
	size_t sent;

	ESocketError result;

	result = m_pOwner->Send(pData->m_socketIdx, pData->pData + pData->sent, pData->length - pData->sent, sent);

	switch (result)
	{
	case eSE_Ok:

		pData->sent += sent;
		if (sent > 0)
		{
			pData->m_quietTimer = 0.0f;
		}

		break;
	}

	return result;
}

// Description:
//	 Receive data.
// Arguments:
//	 pData - transaction
//	 endOfStream - has end of stream been reached
// Return:
//	 Result code.
ESocketError
CCryTCPService::Recv(
  STCPServiceDataPtr pData,
  bool& endOfStream)
{
	int todo = NUM_TIMES_TO_GRAB_FROM_SOCKET;
	ESocketError result;

	char buf[RECV_BUFFER_SIZE];
	ECryTCPServiceResult tcpServErr;

	while (todo)
	{
		size_t bufLen = sizeof(buf);

		todo--;

		switch (result = m_pOwner->Recv(pData->m_socketIdx, buf, bufLen, endOfStream))
		{
		case eSE_Ok:

			tcpServErr = eCTCPSR_Ok;
			if (bufLen > 0)
			{
				pData->m_quietTimer = 0.0f;
			}
			else
			{
				todo = 0;
			}
			break;

		default:

			tcpServErr = eCTCPSR_Failed;
			break;
		}

		if (pData->tcpServReplyCb)
		{
			if (pData->tcpServReplyCb(tcpServErr, pData->pUserArg, pData, buf, bufLen, endOfStream))
			{
				endOfStream = true;
				break;
			}
		}
		else
		{
			endOfStream = true;
			break;
		}
	}

	return result;
}

// Description:
//	 Close connection
// Arguments:
//	 sockIdx - socket index
// Return:
//	 Result code.
ESocketError
CCryTCPService::CloseConnection(
  int32& sockIdx)
{
	ESocketError result = eSE_MiscFatalError;

	if (sockIdx != INVALID_SOCKET_INDEX)
	{
		--m_numSocketsOpen;
		result = m_pOwner->CloseConnection(sockIdx);
	}

	return result;
}

// Description:
//	 Tick.
// Arguments:
//	 delta - time delta from last tick
void
CCryTCPService::Tick(
  CTimeValue delta)
{
	switch (m_resolveState)
	{
	case eRS_NotStarted:

		if (m_dataQueueLength && m_pOwner->CanStartResolving())
		{
			StartResolve();
		}

		break;

	case eRS_InProgress:

		WaitResolve();
		break;

	case eRS_Succeeded:

		if (m_dataQueueLength > 0)
		{
			STCPServiceDataPtr pSentinel = 0;
			STCPServiceDataPtr pCurr = 0;
			STCPServiceDataPtr pKeep = 0;

			m_numSocketsToFree = m_numSocketsOpen;

			for (;; )
			{
				m_queueMutex.Lock();
				{
					if (pKeep)
					{
						// First element re-added to queue becomes sentinel to ensure
						// only one pass through the queue per tick.

						if (!pSentinel)
						{
							pSentinel = pKeep;
						}

						m_dataQueue.push(pKeep);
						m_dataQueueLength++;
						m_dataQueueSize += pKeep->length;
					}

					if (!m_dataQueue.empty() && (m_dataQueue.front() != pSentinel) && (m_numSocketsToFree || m_pOwner->NumFreeSockets()))
					{
						pCurr = m_dataQueue.front();
						m_dataQueue.pop();
						m_dataQueueLength--;
						m_dataQueueSize -= pCurr->length;
					}
					else
					{
						pCurr = 0;
					}
				}
				m_queueMutex.Unlock();

				if (!pCurr)
				{
					break;
				}

				// Discard current element unless we decide to re-add it to the
				// queue.

				if (m_pOwner->NumFreeSockets() ||                 // Can we create a socket?
				    (pCurr->m_socketIdx != INVALID_SOCKET_INDEX)) // Does this element already have a socket?
				{
					pCurr->m_quietTimer += delta.GetSeconds();
					pKeep = ProcessTransaction(pCurr);
				}
				else
				{
					// Not enough sockets to do this one; re-add it to the queue so
					// that we can try again next tick.

					pKeep = pCurr;
				}

				if (!pKeep)
				{
					CloseConnection(pCurr->m_socketIdx);
					pCurr->m_socketIdx = INVALID_SOCKET_INDEX;
				}
			}
		}

		break;

	case eRS_Failed:

		DiscardQueue();

	// FALL THROUGH

	case eRS_WaitRetry:

		RetryResolve();

		break;
	}
}

// Description:
//	 Is this a match for the given service?
// Arguments:
//	 serviceConfigName - service config name
// Return:
//	 true if it's a match, otherwise false.
bool CCryTCPService::IsService(const CCryTCPServiceFactory::TServiceConfigName& serviceConfigName)
{
	return m_serviceConfigName == serviceConfigName;
}

bool CCryTCPService::IsService(const CCryTCPServiceFactory::TServerName& serverName, uint16 port, const CCryTCPServiceFactory::TURLPrefix& urlPrefix)
{
	return (m_serverName == serverName) && (m_port == port) && (m_urlPrefix == urlPrefix);
}

// Description:
//	 Has the address resolved?
// Return:
//	 true if the address has resolved, otherwise false.
bool
CCryTCPService::HasResolved()
{
	return (m_resolveState == eRS_Succeeded);
}

// Description:
//	 Is the address resolving?
// Return:
//	 true if the address is resolving, otherwise false.
bool
CCryTCPService::IsResolving()
{
	return (m_resolveState == eRS_InProgress);
}

// UploadData
// Queues data for uploading to remote server. Note data will be deleted after upload completes/fails.
// return			- True if successfully queued, false indicates internal failure (queue full maybe)
bool
CCryTCPService::UploadData(
  STCPServiceDataPtr pData)
{
	bool result = false;
	bool canUpload = false;

	if (m_queueMutex.TryLock())
	{
		if (!((m_resolveState == eRS_Failed) || (m_resolveState == eRS_WaitRetry)))
		{
			canUpload = true;
		}
		m_queueMutex.Unlock();
	}
	if (canUpload)
	{
		if (pData->pData)
		{
			m_queueMutex.Lock();
			{
				m_dataQueue.push(pData);
				m_dataQueueLength++;
				m_dataQueueSize += pData->length;
			}
			m_queueMutex.Unlock();

			result = true;
		}
	}

	return result;
}

// Description:
//	 Process a transaction.
// Return:
//	 The transaction if we're not finished with it, or NULL if we are.
STCPServiceDataPtr
CCryTCPService::ProcessTransaction(
  STCPServiceDataPtr pCurr)
{
	STCPServiceDataPtr pResult = 0;

	ECryTCPServiceResult result = eCTCPSR_Failed;
	bool endOfStream = true;

	if (pCurr->m_socketIdx == INVALID_SOCKET_INDEX)
	{
		Connect(pCurr->m_socketIdx);
	}
	else
	{
		// This is one of the elements for which processing could
		// have freed a socket.

		--m_numSocketsToFree;
	}

	ESocketError sendResult = eSE_Ok;

	if (pCurr->sent < pCurr->length)
	{
		sendResult = Send(pCurr);

		switch (sendResult)
		{
		case eSE_Ok:

			result = eCTCPSR_Ok;
			break;

		default:

			result = eCTCPSR_Failed;
			break;
		}
	}

	if (((pCurr->sent == pCurr->length) || (sendResult != eSE_Ok)) && pCurr->tcpServCb)
	{
		// All data sent or send failed, no reply expected, invoke callback.

		pCurr->tcpServCb(result, pCurr->pUserArg);
	}
	else if ((pCurr->sent == pCurr->length) && pCurr->tcpServReplyCb)
	{
		// All data sent, reply expected, read data from socket.

		Recv(pCurr, endOfStream);

		if (!endOfStream)
		{
			pResult = pCurr;
		}
	}
	else if ((sendResult != eSE_Ok) && pCurr->tcpServReplyCb)
	{
		// Send failed, reply expected, invoke callback.

		pCurr->tcpServReplyCb(eCTCPSR_Failed, pCurr->pUserArg, pCurr, 0, 0, true);
	}
	else
	{
		pResult = pCurr;
	}

	return pResult;
}

// Description:
//	 Increment reference count.
// Return:
//	 Reference count
int
CCryTCPService::AddRef()
{
	return CryInterlockedIncrement(&m_cnt);
}

// Description:
//	 Decrement reference count.
// Return:
//	 Reference count
int
CCryTCPService::Release()
{
	const int nCount = CryInterlockedDecrement(&m_cnt);

	assert(nCount >= 0);

	if (nCount == 0)
	{
		delete this;
	}
	else
	{
		if (nCount < 0)
		{
			assert(0);
			CryFatalError("Deleting Reference Counted Object Twice");
		}
	}

	return nCount;
}

ECryTCPServiceConnectionStatus CCryTCPService::GetConnectionStatus()
{
	return m_connectionStatus;
}

uint32 CCryTCPService::GetDataQueueLength()
{
	return m_dataQueueLength;
}

uint32 CCryTCPService::GetDataQueueSize()
{
	return m_dataQueueSize;
}

const char* CCryTCPService::GetServerName()
{
	return m_serverName.c_str();
}

uint16 CCryTCPService::GetPort()
{
	return m_port;
}

const char* CCryTCPService::GetURLPrefix()
{
	return m_urlPrefix.c_str();
}

void CCryTCPService::StartResolve()
{
	CNetAddressResolver* pResolver = ((CCryLobby*)gEnv->pLobby)->GetResolver();

	if (pResolver)
	{
		m_pNameReq = pResolver->RequestNameLookup(m_serverName.c_str());
	}

	if (m_pNameReq)
	{
		m_resolveState = eRS_InProgress;
	}
	else
	{
		m_resolveState = eRS_Failed;
	}
}

void CCryTCPService::WaitResolve()
{
	ENameRequestResult result = m_pNameReq->GetResult();

	switch (result)
	{
	case eNRR_Pending:

		break;

	case eNRR_Succeeded:
		{
			TNetAddressVec& nav = m_pNameReq->GetAddrs();
			size_t naddrs = nav.size();
			SIPv4Addr* pIpv4Addr = NULL;

			for (size_t i = 0; i < naddrs; ++i)
			{
				TNetAddress netAddr = nav[i];

				pIpv4Addr = stl::get_if<SIPv4Addr>(&netAddr);

				if (pIpv4Addr)
				{
					pIpv4Addr->port = htons(m_port);
					m_addr.s_addr = htonl(pIpv4Addr->addr);
					m_resolveState = eRS_Succeeded;
					break;
				}
			}

			m_pNameReq = 0;

			if (m_resolveState != eRS_Succeeded)
			{
				m_resolveState = eRS_Failed;
			}
		}

		break;

	case eNRR_Failed:

		m_resolveState = eRS_Failed;
		m_pNameReq = 0;

		break;
	}
}

void CCryTCPService::RetryResolve()
{
	if (m_resolveState == eRS_Failed)
	{
		m_resolveLastFailed = g_time;
		m_resolveState = eRS_WaitRetry;
	}
	else if ((g_time - m_resolveLastFailed).GetMilliSecondsAsInt64() > (SECONDS_BETWEEN_DNS_RETRIES * 1000))
	{
		m_resolveState = eRS_NotStarted;
	}
}

void CCryTCPService::DiscardQueue()
{
	m_queueMutex.Lock();

	while (!m_dataQueue.empty())
	{
		DiscardQueueElement();
	}

	m_queueMutex.Unlock();
}

#endif // USE_CRY_TCPSERVICE
