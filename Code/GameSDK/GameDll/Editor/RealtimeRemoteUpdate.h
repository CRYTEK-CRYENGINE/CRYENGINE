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

class CRealtimeRemoteUpdateListener:public INotificationNetworkListener,public IRealtimeRemoteUpdate
{
	//////////////////////////////////////////////////////////////////////////
	// Types & typedefs.
	public:
	protected:
	private:
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Methods
	public:
		// From IRealtimeRemoteUpdate
		bool	Enable(bool boEnable=true);
		bool	IsEnabled();

		// From INotificationNetworkListener and from IRealtimeRemoteUpdate
		void	OnNotificationNetworkReceive(const void *pBuffer, size_t length);

		// Singleton management
		static CRealtimeRemoteUpdateListener&	GetRealtimeRemoteUpdateListener();
protected:
		CRealtimeRemoteUpdateListener();
		virtual ~CRealtimeRemoteUpdateListener();

		void	LoadArchetypes(XmlNodeRef &root);
		void  LoadEntities( XmlNodeRef &root );
		void  LoadTimeOfDay( XmlNodeRef &root );
		void  LoadMaterials( XmlNodeRef &root );

		void  LoadConsoleVariables(XmlNodeRef &root );

		void  LoadParticles(XmlNodeRef &root );

		void  LoadTerrainLayer(XmlNodeRef &root, unsigned char*	uchData);
	private:
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Data
	public:
	protected:
		// As currently you can't query if the currently listener is registered
		// the control has to be done here.
		bool m_boIsEnabled;

		int  m_nPreviousFlyMode;
		bool m_boIsSyncingCamera;
	private:
	//////////////////////////////////////////////////////////////////////////
};

#endif // RealtimeRemoteUpdate_h__
