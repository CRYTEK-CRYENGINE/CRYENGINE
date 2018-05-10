// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryTCPService.h
//  Version:     v1.00
//  Created:     29/03/2010 by Paul Mikell.
//  Description: CCryTCPService class definition
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYTCPSERVICE_H__
#define __CRYTCPSERVICE_H__

#include "CryLobby.h"
#include "CryTCPServiceFactory.h"
#include "Socket/NetResolver.h"

#if USE_CRY_TCPSERVICE

class CCryTCPService : public ICryTCPService
{
public:
	CCryTCPService(CCryTCPServiceFactory* pOwner);
	virtual ~CCryTCPService();

	//////////////////////////////////////////////////////////
	// ICryTCPService implementation
	//////////////////////////////////////////////////////////

	// Description:
	//	 Initialize.
	// Arguments:
	//   serviceConfigName - config name
	//	 serverName - server name
	//	 port - port
	//   urlPrefix - URL prefix
	// Return:
	//	 Result code.
	ECryTCPServiceResult Initialise(const CCryTCPServiceFactory::TServiceConfigName& serviceConfigName, const CCryTCPServiceFactory::TServerName& serverName, uint16 port, const CCryTCPServiceFactory::TURLPrefix& urlPrefix);

	// Description:
	//	 Terminate.
	// Arguments:
	//	 isDestructing - is this object's destructor running?
	// Return:
	//	 Result code.
	virtual ECryTCPServiceResult Terminate(
	  bool isDestructing);

	// Description:
	//	 Tick.
	// Arguments:
	//	 delta - time delta from last tick
	virtual void Tick(CTimeValue dt);

	// Description:
	//	 Is this a match for the given service?
	// Arguments:
	//	 serviceConfigName - service config name
	// Return:
	//	 true if it's a match, otherwise false.
	virtual bool IsService(const CCryTCPServiceFactory::TServiceConfigName& serviceConfigName);

	// Description:
	//	 Is this a match for the given service?
	// Arguments:
	//	 serverName - server name
	//   port - port
	//   urlPrefix - URL prefix
	// Return:
	//	 true if it's a match, otherwise false.
	virtual bool IsService(const CCryTCPServiceFactory::TServerName& serverName, uint16 port, const CCryTCPServiceFactory::TURLPrefix& urlPrefix);

	// Description:
	//	 Has the address resolved?
	// Return:
	//	 true if the address has resolved, otherwise false.
	virtual bool HasResolved();

	// Description:
	//	 Is the address resolving?
	// Return:
	//	 true if the address is resolving, otherwise false.
	virtual bool IsResolving();

	// UploadData
	// Queues data for uploading to remote server. Note data will be deleted after upload completes/fails.
	// return			- True if successfully queued, false indicates internal failure (queue full maybe)
	virtual bool UploadData(
	  STCPServiceDataPtr pData);

	// Description:
	//	 Increment reference count.
	// Return:
	//	 Reference count.
	virtual int AddRef();

	// Description:
	//	 Decrement reference count.
	// Return:
	//	 Reference count.
	virtual int                            Release();

	virtual uint32                         GetDataQueueLength();
	virtual uint32                         GetDataQueueSize();
	virtual ECryTCPServiceConnectionStatus GetConnectionStatus();
	virtual const char*                    GetServerName();
	virtual uint16                         GetPort();
	virtual const char*                    GetURLPrefix();

	//////////////////////////////////////////////////////////

	virtual void DiscardQueue();

protected:

	// Description:
	//	 Get address.
	// Arguments:
	//	 addr - address
	// Return:
	//	 Result code.
	virtual ENameRequestResult GetAddress(
	  CRYSOCKADDR_IN& addr);

	// Description:
	//	 Connect socket.
	// Arguments:
	//	 sockIdx- socket index
	// Return:
	//	 Result code.
	virtual ESocketError Connect(
	  int32& sockIdx);

	// Description:
	//	 Send data.
	// Arguments:
	//	 pData - transaction
	// Return:
	//	 Result code.
	virtual ESocketError Send(
	  STCPServiceDataPtr pData);

	// Description:
	//	 Receive data.
	// Arguments:
	//	 pData - transaction
	//	 endOfStream - has end of stream been reached
	// Return:
	//	 Result code.
	virtual ESocketError Recv(
	  STCPServiceDataPtr pData,
	  bool& endOfStream);

	// Description:
	//	 Close connection
	// Arguments:
	//	 sockIdx - socket index
	// Return:
	//	 Result code.
	virtual ESocketError CloseConnection(
	  int32& sockIdx);

private:

	// Description:
	//	 Discard a transaction.
	void DiscardQueueElement();

	// Description:
	//	 Process a transaction.
	// Return:
	//	 The transaction if we're not finished with it, or NULL if we are.
	STCPServiceDataPtr ProcessTransaction(
	  STCPServiceDataPtr pCurr);

protected:

	enum EResolvingState
	{
		eRS_NotStarted,
		eRS_InProgress,
		eRS_Succeeded,
		eRS_Failed,
		eRS_WaitRetry
	};

	virtual void StartResolve();
	virtual void WaitResolve();
	virtual void RetryResolve();

	EResolvingState m_resolveState;
	CTimeValue      m_resolveLastFailed;

	// Transaction queue.
	std::queue<STCPServiceDataPtr> m_dataQueue;

	// Mutex.
	CryMutex                                  m_queueMutex;

	ECryTCPServiceConnectionStatus            m_connectionStatus;

	CCryTCPServiceFactory::TServiceConfigName m_serviceConfigName;
	CCryTCPServiceFactory::TServerName        m_serverName;
	CCryTCPServiceFactory::TURLPrefix         m_urlPrefix;

	CRYINADDR                                 m_addr;
	// Port.
	uint16 m_port;

	uint32 m_dataQueueLength;

private:

	// Number of open sockets.
	uint32 m_numSocketsOpen;

	// Number of sockets we could free this tick.
	uint32 m_numSocketsToFree;

	// Owner.
	_smart_ptr<CCryTCPServiceFactory> m_pOwner;

	// Current DNS request.
	CNameRequestPtr m_pNameReq;

	// Reference count.
	volatile int m_cnt;

	uint32       m_dataQueueSize;
};

#endif // USE_CRY_TCPSERVICE
#endif // __CRYTCPSERVICE_H__
