// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "ErrorHandling.h"

#include "DLCManager.h"
#include "GameCVars.h"

#include "Network/Lobby/GameLobbyManager.h"
#include "Network/Lobby/GameLobby.h"
#include "UI/WarningsManager.h"

#define LEAVE_MP_ON_SIGNOUT_DELAY 30

//-----------------------------------------------------------------------------
CErrorHandling * CErrorHandling::s_pInstance = NULL;

//-----------------------------------------------------------------------------
CErrorHandling::CErrorHandling()
	: m_multiplayer(0)
	, m_warningId(0)
	, m_bIsSignInChange(false)
{
#ifndef _RELEASE
	if (s_pInstance)
	{
		CryFatalError("Multiple error handling instances, shouldn't happen");
	}
#endif
	s_pInstance = this;
}

//-----------------------------------------------------------------------------
CErrorHandling::~CErrorHandling()
{
#ifndef _RELEASE
	if (s_pInstance != this)
	{
		CryFatalError("Removing unknown error handling instance");
	}
#endif
	s_pInstance = NULL;
}

//-----------------------------------------------------------------------------
void CErrorHandling::Update()
{
	if (m_multiplayer && (m_multiplayer <= LEAVE_MP_ON_SIGNOUT_DELAY))
	{
		-- m_multiplayer;
#ifndef EXCLUDE_NORMAL_LOG
		if (m_multiplayer == 0)
		{
			CryLog("CErrorHandling::Update() finished leaving MP");
		}
#endif
	}

	if (HasError())
	{
		bool bUpdate = true;
		while (bUpdate)
		{
			HandleError(bUpdate);
		}
	}
}

//-----------------------------------------------------------------------------
void CErrorHandling::OnEnterMultiplayer()
{
	CryLog("CErrorHandling::OnEnterMultiplayer()");
	m_multiplayer = LEAVE_MP_ON_SIGNOUT_DELAY + 1;
}

//-----------------------------------------------------------------------------
void CErrorHandling::OnLeaveMultiplayer( EExitMultiplayerReason reason )
{
	if (m_multiplayer > LEAVE_MP_ON_SIGNOUT_DELAY)
	{
		CryLog("CErrorHandling::OnLeaveMultiplayer() reason=%d", reason);
		switch (reason)
		{
		case eEMR_Menu:
			m_multiplayer = 0;
			break;
		case eEMR_SignInFailed:
		case eEMR_Other:
			m_multiplayer = LEAVE_MP_ON_SIGNOUT_DELAY;
			break;
		}
	}
}

//-----------------------------------------------------------------------------
bool CErrorHandling::OnFatalError( EFatalError error )
{
	CryLog("CErrorHandling::OnFatalError() error=%d, frameId=%d", error, gEnv->pRenderer->GetFrameID());

	if (error == eFE_LocalSignedOut)
	{
		m_bIsSignInChange = true;
	}

	if (ShouldProcessFatalError(error))
	{
		CryLog("  queuing");
		SetupErrorHandling(error, eNFE_None, true, eS_SingleplayerMenu);
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
bool CErrorHandling::ShouldProcessFatalError( EFatalError error )
{
	bool bCheckPriority = true;

	switch (error)
	{
	// Errors that we only care about in MP
	case eFE_StorageRemoved:							// Storage has been removed, have to return back to SP since partial patching may have occurred
	case eFE_ContentRemoved:							// DLC content removed, possibly it was on a removable storage device
	case eFE_EthernetCablePulled:					// Ethernet cable removed
		{
			bCheckPriority = (m_multiplayer != 0);
		}
		break;

	case eFE_ServiceSignedOut:						// Signed out of the lobby service
	case eFE_PlatformServiceSignedOut:
		{
			bCheckPriority = (m_multiplayer != 0);
			CGameLobbyManager *pGameLobbyManager = g_pGame->GetGameLobbyManager();
			if (pGameLobbyManager && !pGameLobbyManager->IsCableConnected())
			{
				// If we get online signed out errors while the cable is unplugged then we don't need to worry - cable pull is more important
				bCheckPriority = false;
			}
		}
		break;
	}

	if (bCheckPriority)
	{
		// We care about the error, make sure there aren't any higher priority ones also firing
		return (error > m_error.m_fatalError);
	}
	else
	{
		// Don't care about this error
		return false;
	}
}

//-----------------------------------------------------------------------------
bool CErrorHandling::OnNonFatalError( ENonFatalError error, bool bShowDialog )
{
	CryLog("CErrorHandling::OnNonFatalError() error=%d, frameId=%d", error, gEnv->pRenderer->GetFrameID());
	if (!HasFatalError())
	{
		if (m_error.m_nonFatalError < error)
		{
			CryLog("  queuing");
			SetupErrorHandling(eFE_None, error, bShowDialog, (gEnv->bMultiplayer ? eS_MultiplayerMenu : eS_SingleplayerMenu));
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
void CErrorHandling::SetupErrorHandling(EFatalError fatalError, ENonFatalError nonFatalError, bool bShowDialog, EState targetState)
{
	CWarningsManager *pWarningsManager = g_pGame->GetWarnings();
	if (pWarningsManager)
	{
		pWarningsManager->RemoveGameWarning("LeaveGameLobbySquadLeader");
		pWarningsManager->RemoveGameWarning("LeaveGameLobby");
	}

	// Setup error struct
	if (CGameLobby* pLobby = g_pGame->GetGameLobby())
	{
		if (pLobby->IsCurrentlyInSession())
		{
			m_error.m_bWasPrivateGame = pLobby->IsPrivateGame();
			m_error.m_bWasInGame = (pLobby->GetState() == eLS_Game);
		}
	}
	m_error.m_fatalError = fatalError;
	m_error.m_nonFatalError = nonFatalError;
	m_error.m_bShowErrorDialog = bShowDialog;
	m_error.m_targetState = targetState;

	// Cleanup game state
	g_pGame->InvalidateInviteData();
}

//-----------------------------------------------------------------------------
void CErrorHandling::SetErrorState( EState state )
{
	CryLog("CErrorHandling::SetErrorState() %d -> %d", m_error.m_currentState, state);
	m_error.m_currentState = state;
}

//-----------------------------------------------------------------------------
void CErrorHandling::HandleError(bool &bRecursiveUpdate)
{
	bRecursiveUpdate = false;

	if (m_error.m_currentState < m_error.m_targetState)
	{
		IGameFramework *pFramework = g_pGame->GetIGameFramework();
		
		switch (m_error.m_currentState)
		{
		case eS_Init:
			{
				if (pFramework->StartingGameContext() || pFramework->StartedGameContext())
				{
					SetErrorState(eS_WaitingToUnload);
					bRecursiveUpdate = true;
				}
				else if (gEnv->bMultiplayer)
				{
					SetErrorState(eS_WaitingToLeaveSession);
					bRecursiveUpdate = true;
				}
				else
				{
					SetErrorState(gEnv->bMultiplayer ? eS_MultiplayerMenu : eS_SingleplayerMenu);
					bRecursiveUpdate = true;
				}
			}
			break;
		case eS_WaitingToUnload:
			{
				// Wait until a suitable point, then disconnect - clients can bail earlier than the server
				if (!gEnv->bServer)
				{
					SetErrorState(eS_Unloading);
				}
			}
			break;
		case eS_Unloading:
			{
				if (!pFramework->StartingGameContext() && !pFramework->StartedGameContext())
				{
					SetErrorState(eS_WaitingToLeaveSession);
					bRecursiveUpdate = true;
				}
			}
			break;
		case eS_WaitingToLeaveSession:
			{
				CGameLobby* pLobby = g_pGame->GetGameLobby();
				if (pLobby && pLobby->IsCurrentlyInSession())
				{
					pLobby->LeaveSession(true, false);
					SetErrorState(eS_LeavingSession);
				}
				else
				{
					SetErrorState(gEnv->bMultiplayer ? eS_MultiplayerMenu : eS_SingleplayerMenu);
				}
			}
			break;
		case eS_LeavingSession:
			{
				CGameLobby* pLobby = g_pGame->GetGameLobby();
				if (!pLobby || !pLobby->IsCurrentlyInSession())
				{
					SetErrorState(eS_MultiplayerMenu);
					bRecursiveUpdate = true;
				}
			}
			break;
		case eS_MultiplayerMenu:
			{
				SetErrorState(eS_SwitchingToSinglePlayer);
			}
			break;
		case eS_SwitchingToSinglePlayer:
			{
				if (!gEnv->bMultiplayer)
				{
					SetErrorState(eS_SingleplayerMenu);
					bRecursiveUpdate = true;
				}
			}
			break;
		}
	}
	else
	{
		// Wait until we're back on the front end
	}
}

//-----------------------------------------------------------------------------
void CErrorHandling::ShowErrorDialog( const char *pErrorName, const char *pParam )
{
	IActionMapManager *pActionMapManager = gEnv->pGameFramework->GetIActionMapManager();
	if (pActionMapManager)
	{
		pActionMapManager->Enable(true);
	}

	if (m_error.m_bShowErrorDialog)
	{
		m_warningId = g_pGame->GetWarnings()->AddGameWarning(pErrorName, pParam, this);
	}
}

//-----------------------------------------------------------------------------
void CErrorHandling::OnLoadingError(ILevelInfo *pLevel, const char *error)
{
	if (g_pGame->IsGameSessionHostMigrating())
	{
		if (CGameLobby* pGameLobby=g_pGame->GetGameLobby())
		{
			// Check the actual host migration state to determine if we can terminate or just leave
			eHostMigrationState hostMigrationState = pGameLobby->GetMatchMakingHostMigrationState();
			if ((hostMigrationState == eHMS_Idle) || (hostMigrationState == eHMS_Unknown))
			{
				pGameLobby->LeaveSession(true, false);
			}
			else
			{
				pGameLobby->TerminateHostMigration();
			}
		}

		OnNonFatalError(eNFE_HostMigrationFailed, true);
		return;
	}

	if (pLevel && error)
	{
		if (strstr(error, "Failed to read level") == error)  // eg. "Failed to read level info (level.pak might be corrupted)!"
		{
			ENonFatalError errorNum = eNFE_HostMigrationFailed;

			CDLCManager *pDLCManager = g_pGame->GetDLCManager();
			if (pDLCManager && pDLCManager->GetRequiredDLCsForLevel( pLevel->GetName() ) )
			{
				errorNum = eNFE_DLCLoadingFailed;
			}

			if (OnNonFatalError(errorNum, true))
			{
				if (CGameLobby *pGameLobby = g_pGame->GetGameLobby())
				{
					pGameLobby->LeaveSession(true, false);
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
void CErrorHandling::OnNubDisconnect( EDisconnectionCause cause )
{
	if (!gEnv->bServer)
	{
		OnLoadingError(NULL, NULL);

		if ((cause != eDC_NubDestroyed) && (cause != eDC_UserRequested))
		{
			if (OnNonFatalError(eNFE_NubDisconnected, true))
			{
				m_error.m_nubDisconnectCause = cause;
			}
		}
	}
}

//-----------------------------------------------------------------------------
bool CErrorHandling::HandleFatalError()
{
	bool bHandled = true;

	switch (m_error.m_fatalError)
	{
	case eFE_GeneralMPLoaderError:
		{
			ShowErrorDialog("CantMultiplayerGeneric", NULL);
		}
		break;
	case eFE_CannotConnectToMultiplayer:
		{
			ShowErrorDialog("CannotConnectToMP", NULL);
		}
		break;
	case eFE_CannotCreateSquad:
		{
			ShowErrorDialog("CannotCreateSquad", NULL);
		}
		break;
	case eFE_InsufficientPrivileges:
		{
			ShowErrorDialog("CantOnlineInsufficientPrivileges", NULL);
		}
		break;
	case eFE_LocalNotSignedIn:
		{
			ShowErrorDialog("CantMultiplayerNoSignIn", NULL);
		}
		break;
	case eFE_OnlineNotSignedIn:
		{
			ShowErrorDialog("CantOnlineNotOnlineSignIn", NULL);
		}
		break;
	case eFE_SignInCancelled:
		{
			ShowErrorDialog("CantOnlineNotOnlineSignIn", NULL);
		}
		break;
	case eFE_PlatformServiceNotSignedIn:
		{
			ShowErrorDialog("CantOnlineNotOnlineSignIn", NULL);
		}
		break;
	case eFE_NoOnlineAccount:
		{
			ShowErrorDialog("CantOnlineNotOnlineSignIn", NULL);
		}
		break;
	case eFE_AgeRestricted:
		{
			ShowErrorDialog("CantAgeRestricted", NULL);
		}
		break;
	case eFE_InternetDisabled:
		{
			ShowErrorDialog("InternetDisabled", NULL);
		}
		break;
	case eFE_ServiceSignedOut:
		{
			ShowErrorDialog("OnlineSignedOutConnected", NULL);
		}
		break;
	case eFE_PlatformServiceSignedOut:
		{
			ShowErrorDialog("OnlineSignedOut", NULL);
		}
		break;
	case eFE_ContentRemoved:
		{
			ShowErrorDialog("DLCRemoved", NULL);
		}
		break;
	case eFE_StorageRemoved:
		// Warning box already shown by platform OS
		break;
	case eFE_EthernetCablePulled:
		{
			ShowErrorDialog("CableDisconnected", NULL);
		}
		break;
	case eFE_LocalSignedOut:
		{
			ShowErrorDialog("SignedOut", NULL);
		}
		break;
	}

	return bHandled;
}

//-----------------------------------------------------------------------------
bool CErrorHandling::HandleNonFatalError()
{
	bool bHandled = true;

	if (m_error.m_bWasInGame && !m_error.m_bStartedProcessing)
	{
		CryLog("CErrorHandling::HandleError() disconected from in game but with no context, need to reinitialise the render device");
		CGameLobby *pGameLobby = g_pGame->GetGameLobby();
		if (pGameLobby)
		{
			pGameLobby->AbortLoading();
		}
		m_error.m_bStartedProcessing = true;
		bHandled = false;
	}
	else
	{
		switch (m_error.m_nonFatalError)
		{
		case eNFE_DLCLoadingFailed:
			{
				ShowErrorDialog("DLCMissingOrCorrupt", NULL);
			}
			break;
		case eNFE_SessionEndFailed:
			{
				const char *pErrorName = (m_error.m_bWasPrivateGame ? "DisconnectHostMigrationFailedPrivate" : "DisconnectHostMigrationFailed");
				ShowErrorDialog(pErrorName, NULL);
			}
			break;
		case eNFE_HostMigrationFailed:
			{
				const char *pErrorName = (m_error.m_bWasPrivateGame ? "DisconnectHostMigrationFailedPrivate" : "DisconnectHostMigrationFailed");
				ShowErrorDialog(pErrorName, NULL);
			}
			break;
		case eNFE_Kicked:
			{
				ShowErrorDialog("Disconnect", "@ui_server_kicked_msg");
			}
			break;
		case eNFE_KickedHighPing:
			{
				ShowErrorDialog("Disconnect", "@ui_server_kicked_high_ping_msg");
			}
			break;
		case eNFE_KickedReservedUser:
			{
				ShowErrorDialog("Disconnect", "@ui_server_kicked_priority_user_msg");
			}
			break;
		case eNFE_KickedBanLocal:
			{
				ShowErrorDialog("Disconnect", "@ui_server_kicked_localban_msg");
			}
			break;
		case eNFE_KickedBanGlobal:
			{
				ShowErrorDialog("Disconnect", "@ui_server_kicked_globalban_msg");
			}
			break;
		case eNFE_KickedBanGlobalStage1:
			{
				ShowErrorDialog("Disconnect", "@ui_server_kicked_globalbanstg1_msg");
			}
			break;
		case eNFE_KickedBanGlobalStage2:
			{
				ShowErrorDialog("Disconnect", "@ui_server_kicked_globalbanstg2_msg");
			}
			break;
		case eNFE_NubDisconnected:
			{
				CryFixedStringT<32> errorMsg;
				errorMsg.Format("@ui_menu_disconnect_error_%d", m_error.m_nubDisconnectCause);
				ShowErrorDialog("Disconnect", errorMsg.c_str());
			}
			break;
		default:
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Unhandled type of non-fatal disconnect error, '%d'. Not displaying an Error Dialog!", m_error.m_nonFatalError);
			}
			break;
		}
	}

	return bHandled;
}

//-----------------------------------------------------------------------------
bool CErrorHandling::OnWarningReturn( THUDWarningId id, const char* returnValue )
{
	if (id == m_warningId)
	{
		CryLog("CErrorHandling::OnWarningReturn()");
		if (m_bIsSignInChange)
		{
			m_bIsSignInChange = false;
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
CErrorHandling::EFatalError CErrorHandling::GetFatalErrorFromLobbyError( ECryLobbyError error )
{
	switch(error)
	{
	case eCLE_UserNotSignedIn:
		return CErrorHandling::eFE_ServiceSignedOut;
	case eCLE_InsufficientPrivileges:
		return CErrorHandling::eFE_InsufficientPrivileges;
	case eCLE_CableNotConnected:
		return CErrorHandling::eFE_EthernetCablePulled;
	case eCLE_InternetDisabled:
		return CErrorHandling::eFE_InternetDisabled;
	case eCLE_NoOnlineAccount:
		return CErrorHandling::eFE_NoOnlineAccount;
	default:
		// Default - return generic error
		return CErrorHandling::eFE_CannotConnectToMultiplayer;
	}
}

#undef LEAVE_MP_ON_SIGNOUT_DELAY
