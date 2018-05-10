// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/************************************************************************

   a simple bridge between game tokens and DRS data.
   it will send an DRS signal every time a (bool) gametoken changes its value

   /************************************************************************/

#pragma once

#include <CryGame/IGameTokens.h>

class CGameTokenSignalCreator:public IGameTokenEventListener
{
public:
	CGameTokenSignalCreator(DRS::IResponseActor* pSenderForTokenSignals);
	~CGameTokenSignalCreator();

	//////////////////////////////////////////////////////////
	// IGameTokenEventListener implementation
	virtual void OnGameTokenEvent(EGameTokenEvent event, IGameToken* pGameToken);
	virtual void GetMemoryUsage(class ICrySizer* pSizer) const;
	//////////////////////////////////////////////////////////

private:
	DRS::IResponseActor* m_pSenderForTokenSignals;
};
