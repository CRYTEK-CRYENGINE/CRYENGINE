// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

typedef uint32 TGameWebDebugClientId;
#define GAME_WEBDEBUG_INVALID_CLIENT_ID TGameWebDebugClientId(~0)

//! Main interface to send debug information through web-sockets to remote client.
//! Interface is supposed to be implemented by game code.
//! \see IGameWebDebugEntityChannel, IGame.
struct IGameWebDebugEntityChannel;

struct IGameWebDebugService
{
	struct SMessage
	{
		SMessage()
			: clientId(GAME_WEBDEBUG_INVALID_CLIENT_ID)
			, pData(nullptr)
			, dataSize(0)
		{
		}

		TGameWebDebugClientId clientId;
		const char*           pData;
		uint32                dataSize;
	};

	virtual ~IGameWebDebugService() {};

	virtual void AddChannel(IGameWebDebugEntityChannel* pChannelProvider) = 0;
	virtual void RemoveChannel(IGameWebDebugEntityChannel* pChannelProvider) = 0;

	virtual void Publish(const SMessage& message) = 0;
};

//! Debug data channel for remote debugging.
//! Interface to be implemented by systems which want to send some data for remote debugging.
//! Channels have to be registered with IGameWebService to get notified about when to start/stop streaming data.
//! \see IGameWebDebugService.
struct IGameWebDebugEntityChannel
{
	virtual ~IGameWebDebugEntityChannel() {};

	virtual void Subscribe(const TGameWebDebugClientId clientId, const EntityId entityId) = 0;
	virtual void Unsubscribe(const TGameWebDebugClientId clientId) = 0;
};

//! \endcond