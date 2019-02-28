// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id: IRealtimeRemoveUpdate.h,v 1.1 2009/01/03 11:36:28 PauloZaffari Exp wwwrun $
   $DateTime$
   Description:  This is the interface for the realtime remote update system
              the main purpose for it is to allow users to enable or
              disable it as needed.
   -------------------------------------------------------------------------
   History:
   - 03:01:2009   11:36: Created by Paulo Zaffari
*************************************************************************/
#ifndef IRealtimeRemoteUpdate_h__
#define IRealtimeRemoteUpdate_h__

#pragma once

struct IRealtimeUpdateGameHandler
{
	// <interfuscator:shuffle>
	virtual ~IRealtimeUpdateGameHandler(){}
	virtual bool UpdateGameData(XmlNodeRef oXmlNode, unsigned char* auchBinaryData) = 0;
	// </interfuscator:shuffle>
};

struct IRealtimeRemoteUpdate
{
	// <interfuscator:shuffle>
	virtual ~IRealtimeRemoteUpdate(){}

	//! Enables or disables the realtime remote update system.
	//! \param boEnable: if true enable the realtime remote update system. If false, disables the realtime realtime remote update system.
	//! \return true if the operation succeeded, false otherwise.
	virtual bool Enable(bool boEnable = true) = 0;

	//! Checks if the realtime remote update system is enabled.
	//! \return true if the system is running, otherwise false.
	virtual bool IsEnabled() = 0;

	//! Method allowing us to use a pass-through mechanism for testing.
	//! \param pBuffer Contains the received message.
	//! \param length Contains the lenght of the received message.
	virtual void OnNotificationNetworkReceive(const void* pBuffer, size_t length) = 0;

	//! Method used to add a game specific handler to the live preview
	//! \param handler Interface to the game side handler that will be used for live preview.
	virtual void AddGameHandler(IRealtimeUpdateGameHandler* handler) = 0;

	//! Method used to remove the game specific handler from the live preview
	//! \param handler Interface to the game side handler that will be used for live preview.
	virtual void RemoveGameHandler(IRealtimeUpdateGameHandler* handler) = 0;

	//! Method used to check if the editor is currently syncing data to the current engine instance withing the timeout of 30 seconds.
	//! \return True if the editor is syncing. Otherwise, false.
	virtual bool IsSyncingWithEditor() = 0;

	//! Method used to do everything which was scheduled to be done in the main thread.
	virtual void Update() = 0;
	// </interfuscator:shuffle>
};

#endif // IRealtimeRemoteUpdate_h__
