// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CET_GameRules.h"
#include "IGameRulesSystem.h"
#include "GameClientChannel.h"
#include "GameServerChannel.h"
#include "GameContext.h"
#include "ActionGame.h"

#include <CryNetwork/NetHelpers.h>
#include <CryNetwork/INetwork.h>

class CCET_OnClient : public CCET_Base
{
public:
	CCET_OnClient(bool(INetworkedClientListener::* func)(int, bool), const char* name, bool isReset) : m_func(func), m_name(name), m_isReset(isReset) {}

	const char*                 GetName() { return m_name; }

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		if (INetChannel* pNC = state.pSender)
		{
			if (CGameServerChannel* pSC = (CGameServerChannel*)pNC->GetGameChannel())
			{
				for (INetworkedClientListener* pListener : CCryAction::GetCryAction()->GetNetworkClientListeners())
				{
					if (!(pListener->*m_func)(pSC->GetChannelId(), m_isReset))
					{
						return eCETR_Failed;
					}
				}

				return eCETR_Ok;
			}
		}
		
		GameWarning("%s: No channel id", m_name);
		return eCETR_Failed;
	}

private:
	bool (INetworkedClientListener::* m_func)(int, bool);
	const char* m_name;
	bool        m_isReset;
};

void AddOnClientConnect(IContextEstablisher* pEst, EContextViewState state, bool isReset)
{
	if (!(gEnv->bHostMigrating && gEnv->bMultiplayer))
	{
		pEst->AddTask(state, new CCET_OnClient(&INetworkedClientListener::OnClientConnectionReceived, "OnClientConnectionReceived", isReset));
	}
}

void AddOnClientEnteredGame(IContextEstablisher* pEst, EContextViewState state, bool isReset)
{
	if (!(gEnv->bHostMigrating && gEnv->bMultiplayer))
	{
		pEst->AddTask(state, new CCET_OnClient(&INetworkedClientListener::OnClientReadyForGameplay, "OnClientReadyForGameplay", isReset));
	}
}