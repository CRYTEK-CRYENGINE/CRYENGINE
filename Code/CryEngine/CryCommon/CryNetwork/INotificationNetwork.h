// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __INotificationNetwork_h__
#define __INotificationNetwork_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#define NN_CHANNEL_NAME_LENGTH_MAX 16

struct INotificationNetworkClient;

// User Interfaces.

struct INotificationNetworkListener
{
	// <interfuscator:shuffle>
	virtual ~INotificationNetworkListener(){}

	//! Called upon receiving data from the Channel the Listener is binded to.
	virtual void OnNotificationNetworkReceive(const void* pBuffer, size_t length) = 0;
	// </interfuscator:shuffle>
};

struct INotificationNetworkConnectionCallback
{
	// <interfuscator:shuffle>
	virtual ~INotificationNetworkConnectionCallback(){}
	virtual void OnConnect(INotificationNetworkClient* pClient, bool bSucceeded) = 0;
	virtual void OnDisconnected(INotificationNetworkClient* pClient) = 0;
	// </interfuscator:shuffle>
};

// Interfaces.

struct INotificationNetworkClient
{
	// <interfuscator:shuffle>
	virtual ~INotificationNetworkClient(){}
	virtual void Release() = 0;

	//! Binds a Listener to the given Notification Channel.
	//! Each Listener can be binded only to one Channel, calling the method again with
	//! an already added Listener and a different Channel will rebind it.
	//! The Channel name cannot exceed NN_CHANNEL_NAME_LENGTH_MAX chars.
	virtual bool ListenerBind(const char* channelName, INotificationNetworkListener* pListener) = 0;

	//! Remove the given Listener form the Notification Network (if the listener exists).
	virtual bool ListenerRemove(INotificationNetworkListener* pListener) = 0;

	//! Sends arbitrary data to the Notification Network the Client is connected to.
	virtual bool Send(const char* channelName, const void* pBuffer, size_t length) = 0;

	//! Check if the current client is connected.
	//! \return true if is connected, false otherwise.
	virtual bool IsConnected() = 0;

	//! Checks if the connection attempt failed.
	//! \return true if it failed to connect by any reason (such as timeout).
	virtual bool IsFailedToConnect() const = 0;

	//! Start the connection request for this particular client.
	//! \param address	Is the host name or IPv4 (for now) address string to which we want to connect.
	//! \param port     Is the TCP port to which we want to connect.
	//! \note Port 9432 is already being used by the live preview.
	virtual bool Connect(const char* address, uint16 port) = 0;

	//! Tries to register a callback listener object.
	//! A callback listener object will receive events from the client element, such as connection result information.
	//! pConnectionCallback which will be called when the events happen, such as connection, disconnection and failed attempt to connect.
	//! \param pConnectionCallback Pointer to an object implementing interface INotificationNetworkConnectionCallback.
	//! \retval true if registered the callback object successfully.
	//! \retval false if the callback object is already registered.
	virtual bool RegisterCallbackListener(INotificationNetworkConnectionCallback* pConnectionCallback) = 0;

	//! Tries to unregister a callback listener object.
	//! A callback listener object will receive events from the client element, such as connection result information.
	//! pConnectionCallback will be called when the events happen, such as connection, disconnection and failed attempt to connect and that we want to unregister.
	//! \param pConnectionCallback Pointer to an object implementing interface INotificationNetworkConnectionCallback
	//! \retval true if unregistered the callback object successfully.
	//! \retval false when no object matching the one requested is found in the object.
	virtual bool UnregisterCallbackListener(INotificationNetworkConnectionCallback* pConnectionCallback) = 0;
	// </interfuscator:shuffle>
};

struct INotificationNetwork
{
	// <interfuscator:shuffle>
	virtual ~INotificationNetwork(){}

	virtual void Release() = 0;

	//! Creates a disconnected client.
	virtual INotificationNetworkClient* CreateClient() = 0;

	//! Attempts to connect to the Notification Network at the given address.
	//! Returns a Client interface if communication is possible.
	virtual INotificationNetworkClient* Connect(const char* address, uint16 port) = 0;

	//! Returns the Connection count of the given Channel.
	//! If NULL is passed instead of a valid Channel name the total count of all Connections is returned.
	virtual size_t GetConnectionCount(const char* channelName = NULL) = 0;

	//! Has to be called from the main thread to process received notifications.
	virtual void Update() = 0;

	//! Binds a Listener to the given Notification Channel.
	//! Each Listener can be bound only to one Channel.
	//! Calling the method again with an already added Listener and a different Channel will rebind it.
	//! The Channel name cannot exceed NN_CHANNEL_NAME_LENGTH_MAX chars.
	virtual bool ListenerBind(const char* channelName, INotificationNetworkListener* pListener) = 0;

	//! Remove the given Listener form the Notification Network (if the listener exists).
	virtual bool ListenerRemove(INotificationNetworkListener* pListener) = 0;

	//! Send arbitrary data to all the Connections listening to the given Channel.
	virtual uint32 Send(const char* channel, const void* pBuffer, size_t length) = 0;
	// </interfuscator:shuffle>
};

#endif // __INotificationNetwork_h__
