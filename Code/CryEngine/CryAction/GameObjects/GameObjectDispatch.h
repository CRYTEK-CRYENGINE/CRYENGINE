// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	This file implements dispatching RMI calls in C++ to
                relevant game object code

   -------------------------------------------------------------------------
   History:
    - 24 Oct 2005 : Created by Craig Tiller

*************************************************************************/

#ifndef __GAMEOBJECTDISPATCH_H__
#define __GAMEOBJECTDISPATCH_H__

#pragma once

#include "IGameObject.h"
#include <vector>

class CGameObjectDispatch
{
public:
	CGameObjectDispatch();
	~CGameObjectDispatch();

	void             RegisterInterface(SGameObjectExtensionRMI* pMessages, size_t nCount);

	INetMessageSink* GetServerSink() { return &m_serverDef; }
	INetMessageSink* GetClientSink() { return &m_clientDef; }

	void             GetMemoryUsage(ICrySizer* s) const;

private:
	// safety check: once we start handing out pointers to m_serverCalls, m_clientCalls
	// we should never modify them again
	bool m_bSafety;
	void LockSafety();

	// all messages we have registered
	std::vector<SGameObjectExtensionRMI*> m_messages;

	// protocol definitions
	std::vector<SNetMessageDef> m_serverCalls;
	std::vector<SNetMessageDef> m_clientCalls;

	class CProtocolDef : public INetMessageSink
	{
	public:
		virtual void DefineProtocol(IProtocolBuilder* pBuilder);
		virtual bool HasDef(const SNetMessageDef* pDef);

		bool         IsServer();
	};

	CProtocolDef                m_serverDef;
	CProtocolDef                m_clientDef;

	static CGameObjectDispatch* m_pGOD;

	static TNetMessageCallbackResult Trampoline(
	  uint32 userId,
	  INetMessageSink* handler,
	  TSerialize serialize,
	  uint32 curSeq,
	  uint32 oldSeq,
	  uint32 timeFraction32,
	  EntityId* entityId, INetChannel*);
};

#endif
