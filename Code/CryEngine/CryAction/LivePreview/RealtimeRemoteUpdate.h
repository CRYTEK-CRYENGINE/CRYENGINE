// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id: RealtimeRemoveUpdate.h,v 1.1 2009/01/03 10:45:15 PauloZaffari Exp wwwrun $
   $DateTime$
   Description:  This is the header file for the module Realtime remote
   update. The purpose of this module is to allow data update to happen
   remotely so that you can, for example, edit the terrain and see the changes
   in the console.
   -------------------------------------------------------------------------
   History:
   - 03:01:2009   10:45: Created by Paulo Zaffari
*************************************************************************/

#ifndef RealtimeRemoteUpdate_h__
#define RealtimeRemoteUpdate_h__

#pragma once

#include <CryNetwork/INotificationNetwork.h>
#include <CryLiveCreate/IRealtimeRemoteUpdate.h>
#include <Cry3DEngine/I3DEngine.h>

typedef std::vector<IRealtimeUpdateGameHandler*> GameHandlerList;

class CRealtimeRemoteUpdateListener : public INotificationNetworkListener, public IRealtimeRemoteUpdate
{

	//////////////////////////////////////////////////////////////////////////
	// Methods
public:
	// From IRealtimeRemoteUpdate
	virtual bool Enable(bool boEnable = true);
	virtual bool IsEnabled();

	// Game code handlers for live preview updates
	virtual void AddGameHandler(IRealtimeUpdateGameHandler* handler);
	virtual void RemoveGameHandler(IRealtimeUpdateGameHandler* handler);

	// From INotificationNetworkListener and from IRealtimeRemoteUpdate
	virtual void OnNotificationNetworkReceive(const void* pBuffer, size_t length);

	// Singleton management
	static CRealtimeRemoteUpdateListener& GetRealtimeRemoteUpdateListener();

	virtual void                          Update();
protected:
	CRealtimeRemoteUpdateListener();
	virtual ~CRealtimeRemoteUpdateListener();

	void LoadArchetypes(XmlNodeRef& root);
	void LoadTimeOfDay(XmlNodeRef& root);
	void LoadMaterials(XmlNodeRef& root);
	void LoadConsoleVariables(XmlNodeRef& root);
	void LoadParticles(XmlNodeRef& root);
	void LoadTerrainLayer(XmlNodeRef& root, unsigned char* uchData);
	void LoadEntities(XmlNodeRef& root);

	bool IsSyncingWithEditor();
	//////////////////////////////////////////////////////////////////////////
	// Data
protected:

	// As currently you can't query if the currently listener is registered
	// the control has to be done here.
	bool            m_boIsEnabled;

	GameHandlerList m_gameHandlers;
	CTimeValue      m_lastKeepAliveMessageTime;

	typedef std::vector<unsigned char> TDBuffer;

	CryMT::CLocklessPointerQueue<TDBuffer> m_ProcessingQueue;
};

#endif // RealtimeRemoteUpdate_h__
