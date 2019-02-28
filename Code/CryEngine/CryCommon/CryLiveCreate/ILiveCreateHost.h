// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _H_ILIVECREATEHOST_H_
#define _H_ILIVECREATEHOST_H_

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "ILiveCreateCommon.h"

namespace LiveCreate
{

//! Default port for LiveCreate host.
static const uint16 kDefaultHostListenPort = 30601;

//! Default port for UDP target discovery service.
static const uint16 kDefaultDiscoverySerivceListenPost = 30602;

//! Default port for TCP file transfer service.
static const uint16 kDefaultFileTransferServicePort = 30603;

//! Host information (send over the network based host discovery system).
struct CHostInfoPacket
{
	string platformName;
	string hostName;
	string buildExecutable;
	string buildDirectory;
	string gameFolder;
	string rootFolder;
	string currentLevel;
	uint8  bAllowsLiveCreate;
	uint8  bHasLiveCreateConnection;
	uint16 screenWidth;
	uint16 screenHeight;

	template<class T>
	void Serialize(T& stream)
	{
		stream << platformName;
		stream << hostName;
		stream << gameFolder;
		stream << buildExecutable;
		stream << buildDirectory;
		stream << rootFolder;
		stream << currentLevel;
		stream << bAllowsLiveCreate;
		stream << bHasLiveCreateConnection;
		stream << screenWidth;
		stream << screenHeight;
	}
};

//! Host-side (console) LiveCreate interface.
struct IHost
{
public:
	virtual ~IHost() {};

public:
	//! Initialize host at given listening port.
	virtual bool Initialize(const uint16 listeningPort = kDefaultHostListenPort,
	                        const uint16 discoveryServiceListeningPort = kDefaultDiscoverySerivceListenPost,
	                        const uint16 fileTransferServiceListeningPort = kDefaultFileTransferServicePort) = 0;

	//! Close the interface and disconnect all clients.
	virtual void Close() = 0;

	//! Enter a "loading mode" in which commands are not executed.
	virtual void SuppressCommandExecution() = 0;

	//! Exit a "loading mode" and restore command execution.
	virtual void ResumeCommandExecution() = 0;

	//! Execute pending commands.
	virtual void ExecuteCommands() = 0;

	//! Draw overlay information.
	virtual void DrawOverlay() = 0;
};

}

#endif
