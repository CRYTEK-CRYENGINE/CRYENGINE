// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CryTCPServiceFactory.h
//  Version:     v1.00
//  Created:     29/03/2010 by Paul Mikell.
//  Description: ICryDRM interface definition
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYTCPSERVICEFACTORY_H__

#define __CRYTCPSERVICEFACTORY_H__

#pragma once

#include "CryLobby.h"

#if USE_CRY_TCPSERVICE

	#include <CryNetwork/CrySocks.h>
	#include "Socket/SocketError.h"

class CCryTCPService;
typedef _smart_ptr<CCryTCPService> CCryTCPServicePtr;

	#define INVALID_SOCKET_INDEX             -1

	#define CRY_TCP_SERVICE_CONFIG_NAME_SIZE 32
	#define CRY_TCP_SERVICE_SERVER_NAME_SIZE 64
	#define CRY_TCP_SERVICE_SERVER_PATH_SIZE 128

class CCryTCPServiceFactory
{
public:

	static const uint32 k_defaultMaxSockets = 10;

	typedef CryFixedStringT<CRY_TCP_SERVICE_CONFIG_NAME_SIZE> TServiceConfigName;
	typedef CryFixedStringT<CRY_TCP_SERVICE_SERVER_NAME_SIZE> TServerName;
	typedef CryFixedStringT<CRY_TCP_SERVICE_SERVER_PATH_SIZE> TURLPrefix;

	// Description:
	//	 Default constructor for CCryTCPServiceFactory.
	/* ctor */ CCryTCPServiceFactory();

	// Description:
	//	 Destructor for CCryTCPServiceFactory.
	virtual /* dtor */ ~CCryTCPServiceFactory();

	//////////////////////////////////////////////////////////
	// ICryTCPServiceFactory implementation
	//////////////////////////////////////////////////////////

	// Description:
	//	 Initialise the ICryTCPServiceFactory.
	// Arguments:
	//   pTCPServicesFile - XML file of TCP service configurations
	//	 maxSockets - maximum number of sockets
	// Return:
	//	 Result code.
	virtual ECryLobbyError Initialise(const char* pTCPServicesFile, uint32 maxSockets);

	// Description:
	//	 Terminate the ICryTCPServiceFactory.
	// Return:
	//	 Result code.
	virtual ECryLobbyError Terminate(
	  bool isDestructing);

	// Description:
	//	 Get an ICryTCPService.
	// Arguments:
	//	 serviceConfigName - service config name
	// Return:
	//	 Pointer to ICryTCPService for given config.
	virtual ICryTCPServicePtr GetTCPService(const TServiceConfigName& serviceConfigName);

	// Description:
	//	 Get an ICryTCPService.
	// Arguments:
	//	 serverName - server name
	//   port - port
	//   urlPrefix - URL prefix
	// Return:
	//	 Pointer to ICryTCPService for given config.
	virtual ICryTCPServicePtr GetTCPService(const TServerName& serverName, uint16 port, const TURLPrefix urlPrefix);

	// Description:
	//	 Tick.
	// Arguments:
	//	 tv - time.
	virtual void Tick(
	  CTimeValue tv);

	// Description:
	//	 Increment reference count.
	// Return:
	//	 Reference count.
	virtual int AddRef();

	// Description:
	//	 Decrement reference count.
	// Return:
	//	 Reference count.
	virtual int Release();

	//////////////////////////////////////////////////////////

	// Description:
	//	 Get number of free sockets.
	// Return:
	//	 Number of free sockets.
	uint32 NumFreeSockets();

	// Description:
	//	 Connect a socket.
	// Arguments:
	//	 addr - address.
	//	 sockIdx - index of socket.
	// Return:
	//	 Result code.
	virtual ESocketError Connect(
	  CRYSOCKADDR_IN& addr,
	  int32& sockIdx);

	// Description:
	//	 Send data to a socket.
	// Arguments:
	//	 sockIdx - socket index.
	//	 pData - data,
	//	 dataLen - size of data
	// Return:
	//	 Result code.
	virtual ESocketError Send(
	  int32 sockIdx,
	  const char* pData,
	  size_t dataLen,
	  size_t& sent);

	// Description:
	//	 Receive data from a socket.
	// Arguments:
	//	 sockIdx - socket index.
	//	 pData - data,
	//	 dataLen - size of data
	//	 endOfStream - has the end of the stream been reached,
	// Return:
	//	 Result code.
	virtual ESocketError Recv(
	  int32 sockIdx,
	  char* pData,
	  size_t& dataLen,
	  bool& endOfStream);

	// Description:
	//	 Close a connection
	// Arguments:
	//	 sockIdx - index of socket
	// Return:
	//	 Result code.
	virtual ESocketError CloseConnection(
	  int32& sockIdx);

	// Description:
	//	 Can a service start resolving?
	// Return:
	//	 true if the service can start resolving, otherwise false
	virtual bool CanStartResolving();

protected:

	struct STCPServiceConfig
	{
		TServiceConfigName name;
		TServerName        server;
		TURLPrefix         urlPrefix;
		uint16             port;
	};

	typedef std::vector<CCryTCPServicePtr, stl::STLGlobalAllocator<CCryTCPServicePtr>> TServiceVector;

	// Description:
	//	 Create an ICryTCPService.
	// Arguments:
	//	 serviceConfigName - service config name
	// Return:
	//	 Pointer to ICryTCPService for given server & port.
	virtual CCryTCPServicePtr CreateTCPService(const TServiceConfigName& serviceConfigName);
	// Description:
	//	 Create an ICryTCPService.
	// Arguments:
	//	 serverName - server name
	//   port - port
	//   urlPrefix - URL prefix
	// Return:
	//	 Pointer to ICryTCPService for given server & port.
	virtual CCryTCPServicePtr CreateTCPService(const TServerName& serverName, uint16 port, const TURLPrefix urlPrefix);

	virtual void              ReserveServiceConfigs(uint32 numServices);
	virtual void              ReadServiceConfig(XmlNodeRef node);

	std::vector<STCPServiceConfig> m_configs;

	// Services.
	TServiceVector m_services;

private:

	// used to work out deltas from last tick
	CTimeValue m_lastTickTimer;

	// Number of open sockets.
	uint32 m_numSockets;

	// Maximum number of sockets.
	uint32 m_maxSockets;

	// Sockets.
	CRYSOCKET* m_pSockets;

	// Services mutex.
	CryMutex m_servicesMutex;

	// Reference count.
	volatile int m_cnt;
};

typedef _smart_ptr<CCryTCPServiceFactory> CCryTCPServiceFactoryPtr;

#endif // USE_CRY_TCPSERVICE
#endif // __CRYTCPSERVICEFACTORY_H__
