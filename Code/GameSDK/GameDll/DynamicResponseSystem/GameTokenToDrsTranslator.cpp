// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameTokenToDrsTranslator.h"
#include "Game.h"
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>

//--------------------------------------------------------------------------------------------------
CGameTokenSignalCreator::CGameTokenSignalCreator(DRS::IResponseActor* pSenderForTokenSignals) : m_pSenderForTokenSignals(pSenderForTokenSignals)
{
	IGameTokenSystem* pGTS = g_pGame->GetIGameFramework()->GetIGameTokenSystem();
	if (pGTS)
	{
		pGTS->RegisterListener(this);
	}
}

//--------------------------------------------------------------------------------------------------
CGameTokenSignalCreator::~CGameTokenSignalCreator()
{
	IGameTokenSystem* pGTS = g_pGame->GetIGameFramework()->GetIGameTokenSystem();
	if (pGTS)
	{
		pGTS->UnregisterListener(this);
	}
}

//--------------------------------------------------------------------------------------------------
void CGameTokenSignalCreator::OnGameTokenEvent(EGameTokenEvent event, IGameToken* pGameToken)
{
	if (event == EGAMETOKEN_EVENT_CHANGE && pGameToken->GetType() == eFDT_Bool)
	{
		const CHashedString tokenChangedSignal = "sg_gametoken_changed";
		SET_DRS_USER_SCOPED("GameTokenToDrsSignal");
		DRS::IVariableCollectionSharedPtr pContextCollection = gEnv->pDynamicResponseSystem->CreateContextCollection();
		pContextCollection->CreateVariable("TokenName", CHashedString(pGameToken->GetName()));
		pContextCollection->CreateVariable("NewValue", CHashedString(pGameToken->GetValueAsString()));
		m_pSenderForTokenSignals->QueueSignal(tokenChangedSignal, pContextCollection);
	}
}

//--------------------------------------------------------------------------------------------------
void CGameTokenSignalCreator::GetMemoryUsage(class ICrySizer* pSizer) const
{}
