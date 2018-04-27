// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Network/LoginManager/LoginManager.h"
#include "ICrysis3Lobby.h"
#include <CryLobby/ICryLobby.h>
#include "UI/WarningsManager.h"
#include "UI/ProfileOptions.h"
#include "UI/HUD/HUDUtils.h"
#include <CryString/StringUtils.h>
#include "Network/Lobby/GameLobby.h"
#include "Network/RecentPlayers/RecentPlayersMgr.h"
#include "PlayerProgression.h"

#pragma message("Blaze will be removed!")

//-------------------------------------------------------------------------

#ifdef DEDICATED_SERVER
	ICVar *g_pDediEmail = NULL;
	ICVar *g_pDediPassword = NULL;
#elif !defined(_RELEASE)
	const int k_maxLocalBlazeAccounts = 16;
	ICVar *g_pBlazeEmail[k_maxLocalBlazeAccounts] = { NULL };
	ICVar *g_pBlazePassword[k_maxLocalBlazeAccounts] = { NULL };
	ICVar *g_pBlazePersona[k_maxLocalBlazeAccounts] = { NULL };
#endif

	const char* const	CLoginManager::s_pEmailLegal = "%+_-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	const char* const	CLoginManager::s_pEmailPartEnd = "@";

#define EMAIL_LEGAL_NAME_PART_ONLY_LEN	3
#define EMAIL_REQUIRED_VALID_PART_COUNT	2

//-------------------------------------------------------------------------
CLoginManager::CLoginManager(ICrysis3Lobby* pCrysis3Lobby)
	: m_pCrysis3Lobby(pCrysis3Lobby)
	, m_taskId(CryLobbyInvalidTaskID)
	, m_logoutTaskId(CryLobbyInvalidTaskID)
	, m_currentState(eOS_SignedOut)
	, m_logoutAction(eLA_default)
	, m_currentUser(0)
	, m_flags(0)
{
#ifdef DEDICATED_SERVER
	g_pDediEmail = REGISTER_STRING("g_dedi_email", "", VF_INVISIBLE, "");
	g_pDediPassword = REGISTER_STRING("g_dedi_password", "", VF_INVISIBLE, "");
#elif !defined(_RELEASE)
	InitDebugVars();
#endif

	memset(&m_passwordRules, 0, sizeof(m_passwordRules));

	// Kick off here as a fudge for now. Not final.
	//StartLoginProcess();
}

//-------------------------------------------------------------------------
CLoginManager::~CLoginManager()
{
#ifdef DEDICATED_SERVER
	gEnv->pConsole->UnregisterVariable("g_dedi_email", true);
	gEnv->pConsole->UnregisterVariable("g_dedi_password", true);
#elif !defined(_RELEASE)
	ReleaseDebugVars();
#endif

	gEnv->pLobby->UnregisterEventInterest(eCLSE_OnlineState, CLoginManager::OnlineStateChangedCallback, this);

	if (CWarningsManager *pWarnings = g_pGame->GetWarnings())
	{
		pWarnings->CancelCallbacks(this);
	}
}

#if !defined(_RELEASE) && !defined(DEDICATED_SERVER)
//-------------------------------------------------------------------------
void CLoginManager::InitDebugVars()
{
	CryFixedStringT<32> cvarName;
	for (int i = 0; i < k_maxLocalBlazeAccounts; ++ i)
	{
		cvarName.Format("g_blaze_email_%d", i);
		g_pBlazeEmail[i] = REGISTER_STRING(cvarName.c_str(), "", VF_COPYNAME, "");

		cvarName.Format("g_blaze_password_%d", i);
		g_pBlazePassword[i] = REGISTER_STRING(cvarName.c_str(), "", VF_COPYNAME, "");

		cvarName.Format("g_blaze_persona_%d", i);
		g_pBlazePersona[i] = REGISTER_STRING(cvarName.c_str(), "", VF_COPYNAME, "");
	}

	REGISTER_COMMAND("g_blazeStartLoginProcess", CmdStartLoginProcess, VF_NULL, "");
	REGISTER_COMMAND("g_blazeRepromptDisplay", CmdReprompt, VF_NULL, "");
	REGISTER_COMMAND("g_blazeChangeEmail", CmdChangeEmail, VF_NULL, "");
	REGISTER_COMMAND("g_blazeChangePassword", CmdChangePassword, VF_NULL, "");
}

//-------------------------------------------------------------------------
void CLoginManager::ReleaseDebugVars()
{
	CryFixedStringT<32> cvarName;
	for (int i = 0; i < k_maxLocalBlazeAccounts; ++ i)
	{
		cvarName.Format("g_blaze_email_%d", i);
		gEnv->pConsole->UnregisterVariable(cvarName.c_str(), true);

		cvarName.Format("g_blaze_password_%d", i);
		gEnv->pConsole->UnregisterVariable(cvarName.c_str(), true);

		cvarName.Format("g_blaze_persona_%d", i);
		gEnv->pConsole->UnregisterVariable(cvarName.c_str(), true);
	}

	gEnv->pConsole->RemoveCommand("g_blazeStartLoginProcess");
	gEnv->pConsole->RemoveCommand("g_blazeRepromptDisplay");
	gEnv->pConsole->RemoveCommand("g_blazeChangeEmail");
	gEnv->pConsole->RemoveCommand("g_blazeChangePassword");
}

//-------------------------------------------------------------------------
void CLoginManager::CmdStartLoginProcess( IConsoleCmdArgs* pCmdArgs )
{
	bool bDriveMenu = false;
	if (pCmdArgs->GetArgCount()>2)
	{
		bDriveMenu = (atoi(pCmdArgs->GetArg(1))!=0);
	}
	g_pGame->GetLoginManager()->StartLoginProcess(bDriveMenu, false);
}

//-------------------------------------------------------------------------
void CLoginManager::CmdReprompt(IConsoleCmdArgs *pArgs)
{
	CLoginManager *pThis = g_pGame->GetLoginManager();
	pThis->m_pCrysis3Lobby->RepromptDisplay(pThis->m_currentUser);
}

//-------------------------------------------------------------------------
void CLoginManager::CmdChangeEmail( IConsoleCmdArgs *pArgs )
{
	if (pArgs->GetArgCount() >= 3)
	{
		CLoginManager *pThis = g_pGame->GetLoginManager();
		pThis->m_pCrysis3Lobby->UpdateAccount(pThis->m_currentUser, pArgs->GetArg(1), pArgs->GetArg(2), NULL, NULL, NULL, NULL);
	}
	else
	{
		CryLogAlways("Usage: g_blazeChangeEmail <oldpass> <email>");
	}
}

//-------------------------------------------------------------------------
void CLoginManager::CmdChangePassword( IConsoleCmdArgs *pArgs )
{
	if (pArgs->GetArgCount() >= 3)
	{
		CLoginManager *pThis = g_pGame->GetLoginManager();
		pThis->m_pCrysis3Lobby->UpdateAccount(pThis->m_currentUser, pArgs->GetArg(1), NULL, pArgs->GetArg(2), NULL, NULL, NULL);
	}
	else
	{
		CryLogAlways("Usage: g_blazeChangePassword <oldPass> <newPass>");
	}
}
#endif

#ifndef _RELEASE
//-------------------------------------------------------------------------
const char* CLoginManager::Debug_GetEmail()
{
#ifdef DEDICATED_SERVER
	return "";
#else
	int appInstance = gEnv->pSystem->GetApplicationInstance();
	return g_pBlazeEmail[appInstance]->GetString();
#endif
}

//-------------------------------------------------------------------------
const char* CLoginManager::Debug_GetPassword()
{
#ifdef DEDICATED_SERVER
	return "";
#else
	int appInstance = gEnv->pSystem->GetApplicationInstance();
	return g_pBlazePassword[appInstance]->GetString();
#endif
}

//-------------------------------------------------------------------------
const char* CLoginManager::Debug_GetPersona()
{
#ifdef DEDICATED_SERVER
	return "";
#else
	int appInstance = gEnv->pSystem->GetApplicationInstance();
	return g_pBlazePersona[appInstance]->GetString();
#endif
}
#endif

//-------------------------------------------------------------------------
void CLoginManager::StartLoginProcess(bool bDriveMenu, bool bSwitchGameTypeWhenSignedIn)
{
	CryLog("CLoginManager::StartLoginProcess() %d", bDriveMenu);

	if (m_taskId != CryLobbyInvalidTaskID)
	{
		CryLog("CLoginManager::StartLoginProcess() function called but we're already in the process of logging in");

		if( bDriveMenu && ! AreAnyFlagsSet(eLF_DriveMenu) )
		{
			CryLog( "New login request is to drive menu while in process login is silent, queuing login" );

			SetFlag(eLF_DriveMenu, bDriveMenu);
			SetFlag(eLF_SwitchGameTypeWhenSignedIn, bSwitchGameTypeWhenSignedIn);
			AddFlag(eLF_NeedToLogin);
		}

		return;
	}

	m_currentUser = g_pGame->GetExclusiveControllerDeviceIndex();
	SetFlag(eLF_DriveMenu, bDriveMenu);
	SetFlag(eLF_SwitchGameTypeWhenSignedIn, bSwitchGameTypeWhenSignedIn);
	RemoveFlag(eLF_DeclinedTOS);
	RemoveFlag(eLF_IsCreatingAccount);

	if (m_logoutTaskId == CryLobbyInvalidTaskID)
	{
		Internal_StartLoginProcess();
	}
	else
	{
		AddFlag(eLF_NeedToLogin);
	}
}

//-------------------------------------------------------------------------
void CLoginManager::Internal_StartLoginProcess()
{
	m_pCrysis3Lobby->StartLoginProcess(this, m_currentUser, AreAnyFlagsSet(eLF_DriveMenu), &m_taskId, CLoginManager::LoginProcessCallback, this);
	gEnv->pLobby->RegisterEventInterest(eCLSE_OnlineState, CLoginManager::OnlineStateChangedCallback, this);

	RemoveFlag(eLF_HasPasswordRules);
	m_pCrysis3Lobby->GetPasswordRules(m_currentUser, NULL, Crysis3LobbyPasswordCallback, this);
}

//-------------------------------------------------------------------------
void CLoginManager::OnlineStateChangedCallback( UCryLobbyEventData eventData, void *pArg )
{
	CLoginManager *pThis = (CLoginManager*) pArg;
	if (pThis->m_currentUser == eventData.pOnlineStateData->m_user)
	{
		EOnlineState newOnlineState = eventData.pOnlineStateData->m_curState;
		CryLog("CLoginManager::OnlineStateChangedCallback() new online state = %d", newOnlineState);

		pThis->m_currentState = newOnlineState;

		if (newOnlineState == eOS_SignedIn)
		{
			if (pThis->AreAnyFlagsSet(eLF_DriveMenu))
			{
				if (pThis->AreAnyFlagsSet(eLF_IsCreatingAccount))
				{
					g_pGame->GetWarnings()->AddGameWarning("MPAccountCreated", NULL);
				}
				else
				{
					pThis->FinishLogin();
				}
			}

			ICryLobbyService *pOnlineService = gEnv->pNetwork->GetLobby()->GetLobbyService(eCLS_Online);
			if (pOnlineService)
			{
				// Get our current local user ID
				uint32 currentUserIndex = g_pGame->GetExclusiveControllerDeviceIndex();
				CryUserID localUserId = gEnv->pNetwork->GetLobby()->GetLobbyService()->GetUserID(currentUserIndex);

				if (pThis->m_lastSignedInUser != localUserId)
				{
					pThis->m_lastSignedInUser = localUserId;

					CRecentPlayersMgr *pRecentPlayersMgr = g_pGame->GetRecentPlayersMgr();
					if (pRecentPlayersMgr)
					{
						pRecentPlayersMgr->OnUserChanged(localUserId);
					}

					CPlayerProgression *pPlayerProgression = CPlayerProgression::GetInstance();
					if (pPlayerProgression)
					{
						pPlayerProgression->OnUserChanged();
					}
				}
			}
		}
		//else if (newOnlineState == eOS_SignedOut) //	Handling of signing out & returning to SP is in CMPMenuHub::OnlineStateChanged
	}
}

//-------------------------------------------------------------------------
void CLoginManager::FinishLogin()
{
	if ((AreAnyFlagsSet(eLF_SwitchGameTypeWhenSignedIn) || (g_pGame->GetInviteAcceptedState() == CGame::eIAS_WaitForInitMultiplayer)) && !gEnv->bMultiplayer)
	{
#if 0 // old frontend
		CFlashFrontEnd *pFlashMenu = g_pGame->GetFlashMenu();
		pFlashMenu->Execute(CFlashFrontEnd::eFECMD_switch_game, "multiplayer");
#endif
	}
	else
	{
		Menu_GoToPage("main");
	}
}

//-------------------------------------------------------------------------
void CLoginManager::OnDisplayCountrySelect()
{
#if (defined(XENON) || defined(PS3))
	GameWarning("OnDisplayCountrySelect should never be called on console");
#endif

	if (!Menu_GoToPage("blaze_create_country_dob"))
	{
		CryLog( "CLoginManager::OnDisplayCountrySelect can't display menu");
		Logout();
	}
}

//-------------------------------------------------------------------------
void CLoginManager::OnDisplayLegalDocs( const char *pTermsOfServiceText, const char *pPrivacyText )
{
}

//-------------------------------------------------------------------------
void CLoginManager::OnDisplayCreateAccount()
{	
#if (defined(XENON) || defined(PS3))
	bool bOnCreateScreen = false;
	const char *pCreateAccountScreen = 	"blaze_console_login"; // Console goes to login as it can do associate account too
#if 0 // old frontend
	bOnCreateScreen = ((g_pGame->GetFlashMenu()->GetCurrentMenuScreen() == eFFES_blaze_console_login) || (g_pGame->GetFlashMenu()->GetCurrentMenuScreen() == eFFES_blaze_console_create));
#endif
	if (!bOnCreateScreen) // Consoles shouldn't go back to login, if already on a create screen (likely set in OnCreateAccountError())
	{
		if (!Menu_GoToPage(pCreateAccountScreen))
		{
			CryLog("Error, failed to find blaze create screen. Bailing from login"); 
			Logout();
		}
	}

#else
	//can't create account on PC, weird that we got this request
	CryLog( "CLoginManager::OnDisplayCreateAccount Invalid call on PC" );
	Logout();
#endif
}

//-------------------------------------------------------------------------
void CLoginManager::OnDisplayLogin()
{
#if (defined(XENON) || defined(PS3))
	GameWarning("OnDisplayLogin should never be called on console");
#endif

#ifdef DEDICATED_SERVER
	SDisplayLoginResponse response;

	strcpy_s(response.pEmail, IC3_MAX_EMAIL_LENGTH, g_pDediEmail->GetString());
	response.pEmail[ IC3_MAX_EMAIL_LENGTH-1] = '\0';

	strcpy_s(response.pPassword, IC3_MAX_PASSWORD_LENGTH, g_pDediPassword->GetString());
	response.pPassword[ IC3_MAX_PASSWORD_LENGTH-1] = '\0';

	OnDisplayLoginResponse(response); 
#else
	if (!Menu_GoToPage("blaze_login"))
	{
		CryLog( "CLoginManager::OnDisplayLogin can't display menu");
		Logout();
	}
#endif
}

//-------------------------------------------------------------------------
void CLoginManager::OnDisplayEntitleGame()
{
	g_pGame->GetWarnings()->AddGameWarning("BlazeNoCDKey", NULL, this);
}

//-------------------------------------------------------------------------
void CLoginManager::OnDisplayPersonas( const SPersona *pPersonas, const int numPersonas )
{
	if(numPersonas > 0)
	{
		// We should only have 1 persona in the namespace, use it. 
		SDisplayPersonasResponse response; 

		strcpy_s(response.pPersonaName, IC3_MAX_PERSONA_NAME_LENGTH, pPersonas->m_pdisplayName);
		response.pPersonaName[IC3_MAX_PERSONA_NAME_LENGTH-1] = '\0';  

		OnDisplayPersonasResponse(response);
	}
	else
	{
		if (!Menu_GoToPage("blaze_persona"))
		{
			CryLog("CLoginManager::OnDisplayPersonas Error, failed to find blaze persona screen. Bailing from login");
			Logout();
		}
	}
}

//-------------------------------------------------------------------------
void CLoginManager::OnPasswordRequired()
{
#if (defined(XENON) || defined(PS3))
	if (!Menu_GoToPage("blaze_console_password"))
	{
		CryLog("Error, failed to find blaze password screen. Bailing from login");
		Logout();
	}
#else
	CRY_ASSERT_MESSAGE(0, "Blaze OnPasswordRequired should only be called on console!");
#endif
}

////////////
// ERRORS //
////////////
//-------------------------------------------------------------------------
void CLoginManager::OnCreateAccountError( EAuthenticationError errorCode, const SValidationError *pErrors, const int numErrors )
{
	const char *pCreateAccountScreen = 
#if (defined(XENON) || defined(PS3))
		"blaze_console_create";
#else
		"blaze_create";
#endif
	CryLog("CLoginManager::OnCreateAccountError code:%d numErrors:%d", errorCode, numErrors);
	if( errorCode == ICrysis3AuthenticationHandler::eAE_InvalidLoginParams )
	{
		if (numErrors)
		{
			// pErrors[0].m_error
			switch (pErrors[0].m_field)
			{
			case ICrysis3AuthenticationHandler::eVF_Password:
				{
					g_pGame->GetWarnings()->AddGameWarning("BlazeCreate_InvalidPassword", NULL);
					break;
				}
			case ICrysis3AuthenticationHandler::eVF_Email:
				{
					g_pGame->GetWarnings()->AddGameWarning("BadEmailAddress", NULL);
					break;
				}
			case ICrysis3AuthenticationHandler::eVF_DisplayName:
				{
					g_pGame->GetWarnings()->AddGameWarning("BadUniqueNick", NULL);
					break;
				}
			default:
				{
					CryLog("CLoginManager::OnCreateAccountError() unrecognised error, field=%u, error=%u", pErrors[0].m_field, pErrors[0].m_error);
					g_pGame->GetWarnings()->AddGameWarning("BlazeCreate_InvalidParams", NULL);
					break;
				}
			}
		}
		else
		{
			g_pGame->GetWarnings()->AddGameWarning("BlazeCreate_InvalidParams", NULL);
		}
	}
	else
	{
		if(!HandleAuthenticationErrorWarning(errorCode))
		{
			return;
		}
	}

	Menu_GoToPage(pCreateAccountScreen);
}

//-------------------------------------------------------------------------
void CLoginManager::OnCreatePersonaError( EAuthenticationError error )
{
	CryLog("CLoginManager::OnCreatePersonaError() error=%u", error);
	HandleAuthenticationErrorWarning(error);
}



//-------------------------------------------------------------------------
void CLoginManager::OnForgotPasswordError( EAuthenticationError error )
{
	CryLog("CLoginManager::OnForgotPasswordError() error=%u", error);

	if (error == ICrysis3AuthenticationHandler::eAE_Success)
	{
		g_pGame->GetWarnings()->AddGameWarning("BlazeForgotPassword_Success", m_forgotPasswordEmail.c_str(), NULL);
	}
	else
	{
		if(!HandleAuthenticationErrorWarning(error))
			return;
	}

	// Reprompt the current screen
	if ((m_currentState == eOS_SigningIn) || (m_currentState == eOS_SignedOut))
	{
		m_pCrysis3Lobby->RepromptDisplay(m_currentUser);
	}
}

//-------------------------------------------------------------------------
void CLoginManager::OnAssociateAccountError( EAuthenticationError error )
{
	CryLog("CLoginManager::OnAssociateAccountError() error=%u", error);
	if(!HandleAuthenticationErrorWarning(error))
	{
		return;
	}

	// Reprompt the current screen
	if (m_currentState == eOS_SigningIn)
	{
		m_pCrysis3Lobby->RepromptDisplay(m_currentUser);
	}
}

//-------------------------------------------------------------------------
void CLoginManager::OnGeneralError( EAuthenticationError error )
{
	CryLog("CLoginManager::OnGeneralError() error=%u", error);
	HandleAuthenticationErrorWarning(error);
}

//-------------------------------------------------------------------------
void CLoginManager::OnLoginFailure( EAuthenticationError error )
{
#ifdef DEDICATED_SERVER
	CryLogAlways("Login failed, quitting");
	gEnv->pSystem->Quit();
#else
	CryLog("OnLoginFailure error=%d", error);
	if(!HandleAuthenticationErrorWarning(error))
	{
		return;
	}

	Menu_GoToPage("blaze_login");
#endif
}

// Xbox only
//-------------------------------------------------------------------------
void CLoginManager::OnProfileSelected()
{
}

//-------------------------------------------------------------------------
void CLoginManager::OnProfileLoaded( EAuthenticationError errorCode )
{
}

//-------------------------------------------------------------------------
void CLoginManager::OnProfileUnloaded( EAuthenticationError errorCode )
{
}
// ~Xbox only

//-------------------------------------------------------------------------
bool CLoginManager::OnWarningReturn( THUDWarningId id, const char* pReturnValue )
{
	CWarningsManager *pWarnings = g_pGame->GetWarnings();
	if (id == pWarnings->GetWarningId("BlazeGeneralError"))
	{
		if (m_currentState == eOS_SigningIn)
		{
			m_pCrysis3Lobby->RepromptDisplay(m_currentUser);
		}
	}
	else if (	(id == pWarnings->GetWarningId("BlazeNoCDKey")) ||
						(id == pWarnings->GetWarningId("BlazeLogin_TooYoung")) ||
						(id == pWarnings->GetWarningId("BlazeCreate_TooYoung")) )
	{
		Logout();
	}
	else if (id == pWarnings->GetWarningId("MPAccountCreated"))
	{
		FinishLogin();
	}

	return true;
}

//-------------------------------------------------------------------------
void CLoginManager::OnCreateAccountRequested()
{
	m_pCrysis3Lobby->BeginCreateAccount(m_currentUser); 
	Menu_GoBack();
}

//-------------------------------------------------------------------------
void  CLoginManager::OnDisplayCountrySelectResponse(const SDisplayCountrySelectResponse& response)
{
	Menu_GoBack();

	m_pCrysis3Lobby->OnDisplayCountrySelectResponse(m_currentUser, response);
}

//-------------------------------------------------------------------------
void  CLoginManager::OnDisplayLegalDocsResponse(bool bAccepted)
{
	m_pCrysis3Lobby->OnDisplayLegalDocsResponse(m_currentUser, bAccepted); 
	
	Menu_GoBack();

	SetFlag(eLF_DeclinedTOS, !bAccepted);
	if (!bAccepted)
	{
		m_logoutAction = eLA_default;
		RemoveFlag(eLF_NeedToLogin);
		g_pGame->InvalidateInviteData();
	}
}

//-------------------------------------------------------------------------
void  CLoginManager::BeginAssociateAccount(const SAssociateAccountResponse& response)
{
	RemoveFlag(eLF_IsCreatingAccount);
	m_pCrysis3Lobby->BeginAssociateAccount(m_currentUser, response);

	Menu_GoBack();
}

//-------------------------------------------------------------------------
void  CLoginManager::OnDisplayCreateAccountResponse(const SCreateAccountResponse& response)
{
	AddFlag(eLF_IsCreatingAccount);
	m_pCrysis3Lobby->OnDisplayCreateAccountResponse(m_currentUser, response);

	Menu_GoBack();
}

//-------------------------------------------------------------------------
void  CLoginManager::OnDisplayLoginResponse(const SDisplayLoginResponse& response)
{
	Menu_GoBack();

	RemoveFlag(eLF_IsCreatingAccount);
	m_pCrysis3Lobby->OnDisplayLoginResponse(m_currentUser, response); 
}

//-------------------------------------------------------------------------
void  CLoginManager::OnDisplayEntitleGameResponse(const SDisplayEntitleGameResponse& response)
{
	m_pCrysis3Lobby->OnDisplayEntitleGameResponse(m_currentUser, response); 
}

//-------------------------------------------------------------------------
void  CLoginManager::OnDisplayPersonasResponse(const SDisplayPersonasResponse& response)
{
	m_pCrysis3Lobby->OnDisplayPersonasResponse(m_currentUser, response); 
}

//-------------------------------------------------------------------------
void CLoginManager::OnPasswordRequiredResponse( const SPasswordRequiredResponse& response )
{
	m_pCrysis3Lobby->OnPasswordRequiredResponse(m_currentUser, response);
}

//-------------------------------------------------------------------------
void  CLoginManager::CreatePersona(const SDisplayPersonasResponse& response)
{
	m_pCrysis3Lobby->CreatePersona(m_currentUser, response); 
}

//-------------------------------------------------------------------------
bool CLoginManager::Menu_GoToPage( const char *pPage )
{
	if (AreAnyFlagsSet(eLF_DriveMenu))
	{
#if 0 // old frontend
		g_pGame->GetFlashFrontEnd()->Execute(CFlashFrontEnd::eFECMD_gotoPageClearStack, pPage);

		// We're in a non-silent login, remove any pending second login since we don't need it anymore
		RemoveFlag(eLF_NeedToLogin);

#endif
		return true;
	}
	else
	{
		return false;
	}
}

//-------------------------------------------------------------------------
void CLoginManager::Menu_GoBack()
{
	if( AreAnyFlagsSet(eLF_DriveMenu) )
	{
		Menu_GoToPage("blaze_wait");
	}
}

//-------------------------------------------------------------------------
void CLoginManager::Crysis3LobbyPasswordCallback( CryLobbyTaskID taskID, ECryLobbyError error, SPasswordRules *pRules, void *pArg )
{
	CLoginManager *pLoginMgr = (CLoginManager*)pArg;
	if (error == eCLE_Success)
	{
		pLoginMgr->m_passwordRules = *pRules;
		pLoginMgr->AddFlag(eLF_HasPasswordRules);
	}
}

//-------------------------------------------------------------------------
void CLoginManager::LoginProcessCallback( CryLobbyTaskID taskID, ECryLobbyError error, void *pArg )
{
	CLoginManager *pThis = (CLoginManager*)pArg;
	CryLog("CLoginManager::LoginProcessCallback error:%u, logoutaction:%d, frameid=%d", error, pThis->m_logoutAction, gEnv->pRenderer->GetFrameID());

	CRY_ASSERT(pThis->m_taskId == taskID);
	pThis->m_taskId = CryLobbyInvalidTaskID;

	if (error == eCLE_Success)
	{
#if !defined(XENON) && !defined(PS3)
		// Store login details in you profile, if check is ticked
		if (IPlayerProfileManager *pPlayerProfileManager = gEnv->pGame->GetIGameFramework()->GetIPlayerProfileManager())
		{
			const char *user = pPlayerProfileManager->GetCurrentUser();
			if (IPlayerProfile *pProfile = pPlayerProfileManager->GetCurrentProfile(user))
			{
				bool bRememberDetails = pThis->AreAnyFlagsSet(eLF_RememberDetails);
				if (pProfile->SetAttribute("Blaze.LoginRememberDetails", bRememberDetails))
				{
					string saveEmail = bRememberDetails ? pThis->m_loginEmail.c_str() : "";
					string savePassword = bRememberDetails ? pThis->m_loginPassword.c_str()  : "";

					pProfile->SetAttribute( "Blaze.LoginEmail", saveEmail );
					pProfile->SetAttribute( "Blaze.LoginPass", savePassword );
					IPlayerProfileManager::EProfileOperationResult result;
					pPlayerProfileManager->SaveProfile(user, result, ePR_All);
				}
			}
		}
#endif
	}
	else
	{
		pThis->m_currentState = eOS_SignedOut; // We must be signed out if login process failed

		if (pThis->m_logoutAction == eLA_restartLogin || pThis->AreAnyFlagsSet(eLF_NeedToLogin))
		{
			pThis->RemoveFlag( eLF_NeedToLogin );
			pThis->StartLoginProcess(!gEnv->IsDedicated(), pThis->AreAnyFlagsSet(eLF_SwitchGameTypeWhenSignedIn));
		}
		else // Default action is revert to SP
		{
			if( pThis->AreAnyFlagsSet( eLF_DriveMenu ) )
			{
#if 0 // old frontend
				TFlashFrontEndPtr pFlashMenu = g_pGame->GetFlashFrontEnd();
#endif
				CErrorHandling *pErrorHandling = CErrorHandling::GetInstance();
#if 0 // old frontend
				if (pFlashMenu && pErrorHandling)
#else
				if (pErrorHandling)
#endif
				{
					pErrorHandling->OnLeaveMultiplayer(CErrorHandling::eEMR_SignInFailed);

					bool bExecuteMenuChange=true;
					switch(error)
					{
						case eCLE_InsufficientPrivileges:
							pErrorHandling->OnFatalError(CErrorHandling::eFE_InsufficientPrivileges);
							break;
						case eCLE_AgeRestricted:
							pErrorHandling->OnFatalError(CErrorHandling::eFE_AgeRestricted);
							break;
						case eCLE_NoOnlineAccount:
							pErrorHandling->OnFatalError(CErrorHandling::eFE_NoOnlineAccount);
							break;
						case eCLE_Cancelled:
							// We chose not to continue, don't need to show an error
							break;
						case eCLE_InternetDisabled:
							pErrorHandling->OnFatalError(CErrorHandling::eFE_InternetDisabled);
							break;
						case eCLE_CableNotConnected:
#if 0 // old frontend
							// On cable pulls, the returning to main menu is handled by the GameLobbyManager and CMPMenuHub, if there is no MPMenu yet, then we'll do it.
							bExecuteMenuChange = !pFlashMenu->GetMPMenu();
#endif
							break;
						case eCLE_ServiceNotConnected:
							pErrorHandling->OnFatalError(CErrorHandling::eFE_PlatformServiceNotSignedIn);
							break;
						case eCLE_NotConnected:
						default:
							if (!pThis->AreAnyFlagsSet(eLF_DeclinedTOS))			// If we decline the TOS then we get an error here but we don't want to show the warning box
							{
								pErrorHandling->OnFatalError(CErrorHandling::eFE_CannotConnectToMultiplayer);
							}
							break;
					}

					if (bExecuteMenuChange)
					{
						if (gEnv->bMultiplayer)
						{
							if(g_pGame->GetInviteAcceptedState() != CGame::eIAS_None)
							{
								g_pGame->InvalidateInviteData();
							}
#if 0 // old frontend
							pFlashMenu->Execute(CFlashFrontEnd::eFECMD_switch_game, "singleplayer");
#endif
						}
						else
						{
							if (g_pGame->HasExclusiveControllerIndex())
							{
#if 0 // old frontend
								pFlashMenu->Execute(CFlashFrontEnd::eFECMD_gotoPageClearStack, "main");
#endif
							}
						}
					}
				}

#if ! defined (DEDICATED_SERVER)
				if( g_pGameCVars->g_enableInitialLoginSilent )
				{
#if 0 // old frontend
					g_pGame->GetFlashFrontEnd()->EndEarlyLogin();
#endif
				}
#endif //! defined (DEDICATED_SERVER)

			}
#ifdef DEDICATED_SERVER
			CryLogAlways("Sign in failed");
			gEnv->pSystem->Quit();
#endif
		}
		pThis->m_logoutAction = eLA_default;
	}
}

//-------------------------------------------------------------------------
void CLoginManager::Back()
{
	Menu_GoBack();
	m_pCrysis3Lobby->LoginGoBack(m_currentUser);
}

//-------------------------------------------------------------------------
void CLoginManager::Logout(ELogoutAction action/*=eLA_default*/, bool bDriveMenu /*= true*/)
{
	CryLog("CLoginManager::Logout");
	if (bDriveMenu)
	{
		Menu_GoBack();
		RemoveFlag(eLF_NeedToLogin);
	}

	if (action > m_logoutAction)
	{
		// Do the highest priority action.
		m_logoutAction = action;
	}

	if (m_logoutTaskId != CryLobbyInvalidTaskID)
	{
		CryLog("CLoginManager::Logout() function called but we're already in the process of logging out");
		return;
	}

	m_pCrysis3Lobby->Logout(m_currentUser, &m_logoutTaskId, LogoutProcessCallback, this);
}

//-------------------------------------------------------------------------
void CLoginManager::LogoutProcessCallback( CryLobbyTaskID taskID, ECryLobbyError error, void *pArg )
{
	CryLog("CLoginManager::LogoutProcessCallback error:%u", error);
	
	CLoginManager *pThis = (CLoginManager*)pArg;

	CRY_ASSERT(pThis->m_logoutTaskId == taskID);
	pThis->m_logoutTaskId = CryLobbyInvalidTaskID;

	if (pThis->AreAnyFlagsSet(eLF_NeedToLogin) && (pThis->m_taskId == CryLobbyInvalidTaskID))
	{
		pThis->RemoveFlag(eLF_NeedToLogin);
		pThis->Internal_StartLoginProcess();
	}
}

//-------------------------------------------------------------------------
void CLoginManager::ForgotPassword( const char *pEmail)
{
	Menu_GoBack();

	SForgotPasswordResponse response;
	cry_strncpy(response.pEmail, pEmail, sizeof(response.pEmail));

	m_forgotPasswordEmail = pEmail;

	m_pCrysis3Lobby->ForgotPassword(m_currentUser, response);
}

//-------------------------------------------------------------------------
void CLoginManager::StoreLoginDetails(const char* pEmail, const char* pPassword, const bool bRememberCheck)
{
	m_loginEmail = pEmail;	
	m_loginPassword = pPassword;
	SetFlag(eLF_RememberDetails, bRememberCheck);
}


//-------------------------------------------------------------------------
CLoginManager::ELocalFieldValidateResult CLoginManager::ValidatePersona(const char *pPersona, int* pOutRequired/*=NULL*/)
{
	if (!pPersona || !pPersona[0])
	{
		return eLFVR_Empty;
	}

	size_t personaLen = strlen(pPersona);
	if(personaLen <  BLAZE_PERSONA_MIN_LENGTH)
	{
		if (pOutRequired)
			*pOutRequired = BLAZE_PERSONA_MIN_LENGTH;

		return eLFVR_TooShort;
	}
	else if(personaLen > BLAZE_PERSONA_MAX_LENGTH)
	{
		if (pOutRequired)
			*pOutRequired = BLAZE_PERSONA_MAX_LENGTH;

		return eLFVR_TooLong;
	}

	size_t pos = strspn( pPersona,  BLAZE_PERSONA_VALID_CHARACTERS );
	if ( pos < personaLen ) // Contain invalid characters
	{
		return eLFVR_InvalidChars;
	}

	return eLFVR_Valid;
}

//-------------------------------------------------------------------------
CLoginManager::ELocalFieldValidateResult CLoginManager::ValidatePassword(const char *pPassword, int* pOutRequired/*=NULL*/)
{
	if (!pPassword || !pPassword[0])
	{
		return eLFVR_Empty;
	}

	if (AreAnyFlagsSet(eLF_HasPasswordRules) == false)
	{
		return eLFVR_Unavailable;
	}

	size_t passwordLen = strlen(pPassword);
	if(passwordLen <  m_passwordRules.minLength)
	{
		if (pOutRequired)
			*pOutRequired = m_passwordRules.minLength;

		return eLFVR_TooShort;
	}
	else if(passwordLen > m_passwordRules.maxLength)
	{
		if (pOutRequired)
			*pOutRequired = m_passwordRules.maxLength;

		return eLFVR_TooLong;
	}

	size_t pos = strspn( pPassword,  m_passwordRules.validCharacters );
	if ( pos < passwordLen ) // Contain invalid characters
	{
		return eLFVR_InvalidChars;
	}

	int numDigits = 0;
	int numLowercase = 0;
	int numUppercase = 0;

	for (size_t i=0; i<passwordLen; ++i)
	{
		const unsigned char pUnsignedChar = (unsigned char)(pPassword[i]); // checks require char as unsigned char
		if (isdigit(pUnsignedChar))
		{
			++numDigits;
		}
		else if (islower(pUnsignedChar))
		{
			++numLowercase;
		}
		else if (isupper(pUnsignedChar))
		{
			++numUppercase;
		}
	}

	if (numDigits < m_passwordRules.minDigits)
	{
		if (pOutRequired)
			*pOutRequired = m_passwordRules.minDigits;

		return eLFVR_TooFewDigits;
	}

	if (numLowercase < m_passwordRules.minLowerCaseCharacters)
	{
		if (pOutRequired)
			*pOutRequired = m_passwordRules.minLowerCaseCharacters;

		return eLFVR_TooFewLowercase;
	}

	if (numUppercase < m_passwordRules.minUpperCaseCharacters)
	{
		if (pOutRequired)
			*pOutRequired = m_passwordRules.minUpperCaseCharacters;

		return eLFVR_TooFewUppercase;
	}

	return eLFVR_Valid;
}

//-------------------------------------------------------------------------
const char* CLoginManager::GetValidationError(CLoginManager::ELocalFieldValidateResult validationResult, const char* pFieldName, const int required)
{
	const bool bPlural = (required>1);
	const char* pString = NULL;

	switch (validationResult)
	{
		case eLFVR_InvalidChars:
			return "@ui_validation_invalid_char"; // Doesn't require params
		case eLFVR_TooFewDigits:
		case eLFVR_TooFewLowercase:
		case eLFVR_TooFewUppercase:
		case eLFVR_TooLong:
		case eLFVR_TooShort:
			pString = "@ui_blaze_create_password_requirements";
			break;
		default:
			CryLog("Unhandled validation error message");
	}

	if (pString)
	{
		CryFixedStringT<4> requiredStr;
		requiredStr.Format("%d", required);
		return CHUDUtils::LocalizeString(pString, pFieldName, requiredStr.c_str());
	}

	return NULL;
}

//-------------------------------------------------------------------------
/*static*/bool CLoginManager::ValidateEmail(const char* pEmail)
{
	uint32 validParts = 0;

	if ( pEmail )
	{
		const char*		pCheckEmail = pEmail;
		const char*		pLegal = s_pEmailLegal;

		while ( uint32 len = strspn( pCheckEmail, pLegal ) ) 
		{
			pCheckEmail += len;

			if ( *pCheckEmail == '.' )
			{
				++pCheckEmail;
			}
			else
			{
				if ( *pCheckEmail == s_pEmailPartEnd[ validParts ] )
				{
					if ( s_pEmailPartEnd[ validParts ] )
					{
						pLegal += EMAIL_LEGAL_NAME_PART_ONLY_LEN;
						++pCheckEmail;
					}

					++validParts;
				}
			}
		}
	}

	return ( validParts == EMAIL_REQUIRED_VALID_PART_COUNT );
}

bool CLoginManager::HandleAuthenticationErrorWarning( const EAuthenticationError error )
{
	if( AreAnyFlagsSet( eLF_DriveMenu ) )
	{
		CWarningsManager* pWarnings = g_pGame->GetWarnings();
		switch(error)
		{
		case ICrysis3AuthenticationHandler::eAE_InvalidLoginParams:
			pWarnings->AddGameWarning("BlazeLogin_IncorrectUserOrPass", NULL);
			return true;

		//case ICrysis3AuthenticationHandler::eAE_AccountNotActive:
		//	pWarnings->AddGameWarning("", NULL);
		//	return true;

		case ICrysis3AuthenticationHandler::eAE_AccountBanned:
			pWarnings->AddGameWarning("BlazeAccountBanned", NULL);
			return true;

		case ICrysis3AuthenticationHandler::eAE_PersonaNotFoundOrInactive:
			pWarnings->AddGameWarning("BlazeForgotPassword_NotFound", NULL);
			return true;

		case ICrysis3AuthenticationHandler::eAE_AlreadyExists:
			pWarnings->AddGameWarning("BlazeCreate_Exists", NULL);
			return true;

		//case ICrysis3AuthenticationHandler::eAE_InvalidCountry:
		//	pWarnings->AddGameWarning("", NULL);
		//	return true;

		case ICrysis3AuthenticationHandler::eAE_TooYoung:
			{
				if(AreAnyFlagsSet(CLoginManager::eLF_IsCreatingAccount))
				{
					pWarnings->AddGameWarning("BlazeCreate_TooYoung", NULL);
				}
				else
				{
					pWarnings->AddGameWarning("BlazeLogin_TooYoung", NULL);
				}
			}
		
			return false;

		//case ICrysis3AuthenticationHandler::eAE_TOSRequired:
		//	pWarnings->AddGameWarning("", NULL);
		//	return true;

		case ICrysis3AuthenticationHandler::eAE_InvalidEmail:
			pWarnings->AddGameWarning("BadEmailAddress", NULL);
			return true;

		case ICrysis3AuthenticationHandler::eAE_NoEntitlement:
			pWarnings->AddGameWarning("BlazeNoCDKey", NULL);
			return true;

		case ICrysis3AuthenticationHandler::eAE_Timeout:
		{
			CErrorHandling *pErrorHandling = CErrorHandling::GetInstance();
			if (pErrorHandling)
			{
				pErrorHandling->OnFatalError(CErrorHandling::eFE_CannotConnectToMultiplayer);
			}
			Logout();
			return false;
		}
		

		case ICrysis3AuthenticationHandler::eAE_Disconnected:
			// Do nothing, will be handled by the main lobby code.
			return false;

		default:
			{
				// If you get this message below and you don't want it, then uncomment the unhandled error above.
				// Blaze Error [code: ERROR]
				//CryFixedStringT<4> errorStr;
				//errorStr.Format("%u", (uint32)error);
				//pWarnings->AddWarning("BlazeGeneralError", errorStr.c_str(), this);

				// We don't recognise this error, some of these require a full restart of the login process, others don't,
				// since we don't know which this is we just got so treat it as a generic fatal error
				CErrorHandling *pErrorHandling = CErrorHandling::GetInstance();
				if (pErrorHandling)
				{
					pErrorHandling->OnFatalError(CErrorHandling::eFE_CannotConnectToMultiplayer);
				}
				Logout();
				return false;
			}
		}
		return true;
	}
	else
	{
		//silent login, don't show warnings, don't change screens (except if you're too young)
		if (error == ICrysis3AuthenticationHandler::eAE_TooYoung)
		{
			Logout();
		}
		return false;
	}
}
