// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "Config.h"
#include "CryLobby.h"
#include "CryHostMigration.h"
#include "CryMatchMaking.h"

#include <CryGame/IGameFramework.h>

#include <CrySystem/Profilers/IStatoscope.h>

#if NETWORK_HOST_MIGRATION

static const float HOST_MIGRATION_LOG_INTERVAL = 1.0f;

void CHostMigration::Start(SHostMigrationInfo_Private* pInfo)
{
	if (gEnv->pStatoscope)
	{
		gEnv->pStatoscope->AddUserMarker("Host Migration", "Start");
	}

	if (gEnv->bServer && gEnv->IsDedicated())
	{
		CryLogAlways("Host migration for dedicated servers is disabled");
		return;     // Might want to do this on a cvar?
	}

	pInfo->m_startTime = g_time;
	pInfo->m_logTime.SetSeconds(pInfo->m_startTime.GetSeconds() - (HOST_MIGRATION_LOG_INTERVAL * 2.0f));

	ResetListeners(pInfo);

	// Indicate we're performing host migration
	pInfo->m_state = eHMS_Initiate;
	gEnv->bHostMigrating = true;

	if (gEnv->pGameFramework)
	{
		if (gEnv->pGameFramework)
		{
			pInfo->m_shouldMigrateNub = gEnv->pGameFramework->ShouldMigrateNub(pInfo->m_session);
		}
	}

	// Need to do this here so that initiate gets called before the GameClientChannel is destroyed
	TO_GAME_FROM_LOBBY(&CHostMigration::Update, this, pInfo);
}

void CHostMigration::Update(SHostMigrationInfo_Private* pInfo)
{
	// We can get here with an idle state if the main thread stalls for too long since these come from the network thread
	if (pInfo->m_state == eHMS_Idle)
	{
		return;
	}

	bool done = true;
	THostMigrationEventListenerContainer* pListeners = ((CCryLobby*)CCryLobby::GetLobby())->GetHostMigrationListeners();

	if (pListeners)
	{
		while (done)
		{
			THostMigrationEventListenerContainer::iterator it;

			CCryMatchMaking* pMatchMaking = static_cast<CCryMatchMaking*>(CCryLobby::GetLobby()->GetMatchMaking());
			CryLobbySessionHandle sh = pMatchMaking->GetSessionHandleFromGameSessionHandle(pInfo->m_session);
			CRY_ASSERT(sh != CryLobbyInvalidSessionHandle);
			uint32 sessionIndex = sh.GetID();

			float timeout = CLobbyCVars::Get().hostMigrationTimeout;
			float timetaken = (g_time - pInfo->m_startTime).GetSeconds();

			// Log each state once a second to reduce spam
			pInfo->m_logProgress = false;
			if ((g_time - pInfo->m_logTime).GetSeconds() > HOST_MIGRATION_LOG_INTERVAL)
			{
				switch (pInfo->m_state)
				{
				case eHMS_Initiate:
					NetLog("[Host Migration]: Initiate (" PRFORMAT_SH ") [timer %fs, timeout is %fs]", PRARG_SH(sh), timetaken, timeout);
					break;
				case eHMS_DisconnectClient:
					NetLog("[Host Migration]: Disconnect client (" PRFORMAT_SH ") [timer %fs, timeout is %fs]", PRARG_SH(sh), timetaken, timeout);
					break;
				case eHMS_DemoteToClient:
					NetLog("[Host Migration]: Demote to client (" PRFORMAT_SH ") [timer %fs, timeout is %fs]", PRARG_SH(sh), timetaken, timeout);
					break;
				case eHMS_PromoteToServer:
					NetLog("[Host Migration]: Promote to server (" PRFORMAT_SH ") [timer %fs, timeout is %fs]", PRARG_SH(sh), timetaken, timeout);
					break;
				case eHMS_WaitForNewServer:
					NetLog("[Host Migration]: Waiting for server identification (" PRFORMAT_SH ") [timer %fs, timeout is %fs]", PRARG_SH(sh), timetaken, timeout);
					break;
				case eHMS_ReconnectClient:
					NetLog("[Host Migration]: Reconnect client (" PRFORMAT_SH ") [timer %fs, timeout is %fs]", PRARG_SH(sh), timetaken, timeout);
					break;
				case eHMS_Finalise:
					NetLog("[Host Migration]: Finalise (" PRFORMAT_SH ") [timer %fs, timeout is %fs]", PRARG_SH(sh), timetaken, timeout);
					break;
				case eHMS_Terminate:
					NetLog("[Host Migration]: Terminate (" PRFORMAT_SH ")", PRARG_SH(sh));
					break;
				case eHMS_Resetting:
					NetLog("[Host Migration]: Resetting");
					break;
				}

				pInfo->m_logProgress = true;
				pInfo->m_logTime = g_time;
			}

			for (it = pListeners->begin(); it != pListeners->end(); ++it)
			{
				SHostMigrationEventListenerInfo& li = it->second;
				if (!li.m_done[sessionIndex])
				{
					IHostMigrationEventListener::EHostMigrationReturn listenerReturnValue;
					switch (pInfo->m_state)
					{
					case eHMS_Initiate:
						listenerReturnValue = li.m_pListener->OnInitiate(*pInfo, li.m_state[sessionIndex]);
						HandleListenerReturn(li, sessionIndex, pInfo, listenerReturnValue, false);

						if (pInfo->m_state == eHMS_Terminate)
						{
							return;
						}
						break;
					case eHMS_DisconnectClient:
						listenerReturnValue = li.m_pListener->OnDisconnectClient(*pInfo, li.m_state[sessionIndex]);
						HandleListenerReturn(li, sessionIndex, pInfo, listenerReturnValue, false);

						if (pInfo->m_state == eHMS_Terminate)
						{
							return;
						}
						break;
					case eHMS_WaitForNewServer:
						{
							li.m_done[sessionIndex] = false;

							pMatchMaking->HostMigrationWaitForNewServer(sh);
							if (pInfo->m_state == eHMS_Terminate)
							{
								return;
							}

							char address[HOST_MIGRATION_MAX_SERVER_NAME_SIZE];
							cry_strcpy(address, pInfo->m_newServer.c_str());

							if (pMatchMaking->GetNewHostAddress(address, pInfo))
							{
								pInfo->SetNewServer(address);
								pInfo->m_state = pInfo->IsNewHost() ? eHMS_PromoteToServer : eHMS_DemoteToClient;
								return;
							}
						}
						break;
					case eHMS_DemoteToClient:
						if (pInfo->IsNewHost())
						{
							listenerReturnValue = IHostMigrationEventListener::Listener_Done;
						}
						else
						{
							listenerReturnValue = li.m_pListener->OnDemoteToClient(*pInfo, li.m_state[sessionIndex]);
						}
						HandleListenerReturn(li, sessionIndex, pInfo, listenerReturnValue, false);

						if (pInfo->m_state == eHMS_Terminate)
						{
							return;
						}
						break;
					case eHMS_PromoteToServer:
						if (pInfo->IsNewHost())
						{
							listenerReturnValue = li.m_pListener->OnPromoteToServer(*pInfo, li.m_state[sessionIndex]);
						}
						else
						{
							listenerReturnValue = IHostMigrationEventListener::Listener_Done;
						}
						HandleListenerReturn(li, sessionIndex, pInfo, listenerReturnValue, false);

						if (pInfo->m_state == eHMS_Terminate)
						{
							return;
						}
						break;
					case eHMS_ReconnectClient:
						listenerReturnValue = li.m_pListener->OnReconnectClient(*pInfo, li.m_state[sessionIndex]);
						HandleListenerReturn(li, sessionIndex, pInfo, listenerReturnValue, false);

						if (pInfo->m_state == eHMS_Terminate)
						{
							return;
						}
						break;
					case eHMS_Finalise:
						listenerReturnValue = li.m_pListener->OnFinalise(*pInfo, li.m_state[sessionIndex]);
						HandleListenerReturn(li, sessionIndex, pInfo, listenerReturnValue, false);

						if (pInfo->m_state == eHMS_Terminate)
						{
							return;
						}
						break;
	#if ENABLE_HOST_MIGRATION_STATE_CHECK
					case eHMS_StateCheck:
						li.m_done[sessionIndex] = pInfo->m_stateCheckDone;
						break;
	#endif
					case eHMS_Terminate:
						listenerReturnValue = li.m_pListener->OnTerminate(*pInfo, li.m_state[sessionIndex]);
						HandleListenerReturn(li, sessionIndex, pInfo, listenerReturnValue, true);     // Ignore terminate request (acts as done)
						break;
					case eHMS_Resetting:
						listenerReturnValue = li.m_pListener->OnReset(*pInfo, li.m_state[sessionIndex]);
						HandleListenerReturn(li, sessionIndex, pInfo, listenerReturnValue, false);

						if (pInfo->m_state == eHMS_Terminate)
						{
							return;
						}
						break;
					}
				}

				done &= li.m_done[sessionIndex];
			}

			if (pInfo->m_state == eHMS_Finalise)
			{
				if (done)
				{
					// Hold in OnFinalise until matchmaking side of host migration is finished
					done = pMatchMaking->IsHostMigrationFinished(sh);
				}
			}

			if (done)
			{
				switch (pInfo->m_state)
				{
				case eHMS_Terminate:
					// Emergency disconnect
					//gEnv->pConsole->ExecuteString("wait_frames 1");
					//gEnv->pConsole->ExecuteString("disconnect", false, true);
					NetLog("[Host Migration]: terminated (" PRFORMAT_SH ")", PRARG_SH(sh));
				// Intentional fall-through
				case eHMS_Finalise:
	#if ENABLE_HOST_MIGRATION_STATE_CHECK
				case eHMS_StateCheck:
					if ((pInfo->m_state == eHMS_Finalise) && CLobbyCVars::Get().doHostMigrationStateCheck)
					{
						pInfo->m_state = eHMS_StateCheck;
						pMatchMaking->HostMigrationStateCheck(sh);
					}
					else
	#endif
					{
						// We're done
						for (it = pListeners->begin(); it != pListeners->end(); ++it)
						{
							it->second.m_pListener->OnComplete(*pInfo);
						}

						pInfo->m_state = eHMS_Idle;
						gEnv->bHostMigrating = static_cast<CCryMatchMaking*>(CCryLobby::GetLobby()->GetMatchMaking())->AreAnySessionsMigrating();
						pMatchMaking->HostMigrationFinalise(sh);
						NetLog("[Host Migration]: complete (" PRFORMAT_SH ") [timer %fs, timeout is %fs]", PRARG_SH(sh), timetaken, timeout);

						if (pInfo->m_storedContextState)
						{
							pInfo->m_storedContextState->Die();
							pInfo->m_storedContextState = NULL;
						}

						if (gEnv->pStatoscope)
						{
							gEnv->pStatoscope->AddUserMarker("Host Migration", "Finish");
						}

						done = false;
					}

					break;

				default:
					// Move to the next state
					for (it = pListeners->begin(); it != pListeners->end(); ++it)
					{
						it->second.Reset(sessionIndex);
					}

					// Ensure logging of state will occur with this state change
					pInfo->m_logTime.SetSeconds(g_time.GetSeconds() - (HOST_MIGRATION_LOG_INTERVAL * 2.0f));

					if (pInfo->m_state == eHMS_Resetting)
					{
						pInfo->m_state = eHMS_DisconnectClient;
					}
					else
					{
						pInfo->m_state = (eHostMigrationState)((uint32)(pInfo->m_state) + 1);
					}
					break;
				}
			}
			else
			{
				if (timetaken > timeout)
				{
					NetLog("[Host Migration]: Timed out - terminating");
					Terminate(pInfo);
				}
			}
		}
	}
}

void CHostMigration::Terminate(SHostMigrationInfo_Private* pInfo)
{
	#if HOST_MIGRATION_SOAK_TEST_BREAK_DETECTION
	ICVar* pVar = gEnv->pConsole->GetCVar("gl_debugLobbyHMTerminations");
	if (pVar != NULL)
	{
		pVar->Set(pVar->GetIVal() + 1);
	}
	#endif

	if (pInfo->m_state != eHMS_Idle)
	{
		ResetListeners(pInfo);
		pInfo->m_state = eHMS_Terminate;
	}
}

void CHostMigration::Reset(SHostMigrationInfo_Private* pInfo)
{
	ResetListeners(pInfo);
	pInfo->m_state = eHMS_Resetting;
}

void CHostMigration::ResetListeners(SHostMigrationInfo_Private* pInfo)
{
	THostMigrationEventListenerContainer* pListeners = ((CCryLobby*)CCryLobby::GetLobby())->GetHostMigrationListeners();

	if (pListeners)
	{
		CCryMatchMaking* pMatchMaking = static_cast<CCryMatchMaking*>(CCryLobby::GetLobby()->GetMatchMaking());
		CryLobbySessionHandle sh = pMatchMaking->GetSessionHandleFromGameSessionHandle(pInfo->m_session);
		uint32 sessionIndex = sh.GetID();
		CRY_ASSERT(sh != CryLobbyInvalidSessionHandle);

		for (THostMigrationEventListenerContainer::iterator it = pListeners->begin(); it != pListeners->end(); ++it)
		{
			it->second.Reset(sessionIndex);
		}
	}
}

void CHostMigration::HandleListenerReturn(SHostMigrationEventListenerInfo& li, int sessionIndex, SHostMigrationInfo_Private* pInfo, IHostMigrationEventListener::EHostMigrationReturn listenerReturn, bool ignoreTerminate)
{
	switch (listenerReturn)
	{
	case IHostMigrationEventListener::Listener_Done:
		li.m_done[sessionIndex] = true;
		break;
	case IHostMigrationEventListener::Listener_Wait:
		li.m_done[sessionIndex] = false;
		break;
	case IHostMigrationEventListener::Listener_Terminate:
		if (!ignoreTerminate)
		{
			Terminate(pInfo);
		}
		li.m_done[sessionIndex] = true;
		break;
	default:
		CryFatalError("Unknown host migration listener return value!!");
		break;
	}
}

#endif
