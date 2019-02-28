// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

struct IRemoteControlServer;
struct IRemoteControlClient;

struct IRemoteControlSystem
{
	// <interfuscator:shuffle>
	virtual ~IRemoteControlSystem(){}
	virtual IRemoteControlServer* GetServerSingleton() = 0;
	virtual IRemoteControlClient* GetClientSingleton() = 0;
	// </interfuscator:shuffle>
};

//! The event handler on an RCON_server.
struct IRemoteControlServerListener
{
	enum EResultDesc {eRD_Okay, eRD_Failed, eRD_AlreadyStarted};
	// <interfuscator:shuffle>
	virtual ~IRemoteControlServerListener(){}

	virtual void OnStartResult(bool started, EResultDesc desc) = 0;

	//! Invoked when a client is authorized.
	virtual void OnClientAuthorized(string clientAddr) = 0;

	//! Invoked when the authorized client disconnects.
	virtual void OnAuthorizedClientLeft(string clientAddr) = 0;

	//! Invoked when a client command is received on the server.
	virtual void OnClientCommand(uint32 commandId, string command) = 0;

	//! Required by work queueing.
	virtual void AddRef() const  {}
	virtual void Release() const {}
	virtual bool IsDead() const  { return false; }
	// </interfuscator:shuffle>
};

//! The RCON_server interface.
struct IRemoteControlServer
{
	// <interfuscator:shuffle>
	virtual ~IRemoteControlServer(){}

	//! Starts up an RCON_server.
	virtual void Start(uint16 serverPort, const string& password, IRemoteControlServerListener* pListener) = 0;

	//! Stops the RCON_server.
	virtual void Stop() = 0;

	//! Sends command result back to the command client.
	virtual void SendResult(uint32 commandId, const string& result) = 0;
	// </interfuscator:shuffle>
};

//! The event handler on an RCON_client.
struct IRemoteControlClientListener
{
	enum EResultDesc {eRD_Okay, eRD_Failed, eRD_CouldNotResolveServerAddr, eRD_UnsupportedAddressType, eRD_ConnectAgain};
	enum EStatusDesc {eSD_Authorized, eSD_ConnectFailed, eSD_ServerSessioned, eSD_AuthFailed, eSD_AuthTimeout, eSD_ServerClosed, eSD_BogusMessage};
	// <interfuscator:shuffle>
	virtual ~IRemoteControlClientListener(){}

	virtual void OnConnectResult(bool okay, EResultDesc desc) = 0;

	//! Invoked when connection is authorized or not (raw TCP connection failure are considered unauthorized).
	virtual void OnSessionStatus(bool connected, EStatusDesc desc) = 0;

	//! Invoked when the RCON_client receives a command result.
	virtual void OnCommandResult(uint32 commandId, string command, string result) = 0;

	//! Required by work queueing.
	virtual void AddRef() const  {}
	virtual void Release() const {}
	virtual bool IsDead() const  { return false; }
	// </interfuscator:shuffle>
};

//! The RCON_client interface.
struct IRemoteControlClient
{
	// <interfuscator:shuffle>
	virtual ~IRemoteControlClient(){}

	//! Connects to an RCON_server.
	virtual void Connect(const string& serverAddr, uint16 serverPort, const string& password, IRemoteControlClientListener* pListener) = 0;

	//! Disconnects from an RCON_server.
	virtual void Disconnect() = 0;

	//! Sends RCON commands to the connected RCON_server; returns a unique command ID.
	virtual uint32 SendCommand(const string& command) = 0;
	// </interfuscator:shuffle>
};

//! \endcond