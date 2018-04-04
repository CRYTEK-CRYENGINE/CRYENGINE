// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements the IGameNub interface for the client side.

   -------------------------------------------------------------------------
   History:
   - 11:8:2004   11:30 : Created by Marcio Martins

*************************************************************************/
#ifndef __GAMECLIENTNUB_H__
#define __GAMECLIENTNUB_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryNetwork/INetwork.h>

class CGameClientChannel;
class CGameContext;
struct IGameFramework;

class CGameClientNub :
	public IGameNub
{
public:
	CGameClientNub(IGameFramework* pFramework) : m_pGameContext(0), m_pClientChannel(0), m_pFramework(pFramework), m_fInactivityTimeout(300.0f) {};
	virtual ~CGameClientNub();

	// IGameNub
	virtual void                 Release();
	virtual SCreateChannelResult CreateChannel(INetChannel* pChannel, const char* pRequest);
	virtual void                 FailedActiveConnect(EDisconnectionCause cause, const char* description);
	// ~IGameNub

	void                Disconnect(EDisconnectionCause cause, const char* msg);
	void                SetGameContext(CGameContext* pGameContext) { m_pGameContext = pGameContext; };
	CGameClientChannel* GetGameClientChannel() const               { return m_pClientChannel; }

	void                ClientChannelClosed();

	void                GetMemoryUsage(ICrySizer* s) const;

private:
	CGameContext*       m_pGameContext;
	CGameClientChannel* m_pClientChannel;
	IGameFramework*     m_pFramework;
	float               m_fInactivityTimeout;
};

#endif // __GAMECLIENTNUB_H__
