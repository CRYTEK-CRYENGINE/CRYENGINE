// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __LOGINMANAGER_H__
#define __LOGINMANAGER_H__

/*************************************************************************
-------------------------------------------------------------------------
Description:
	- Game side Manager to hide away login/Authentication Implementation
	  specifics, whilst exposing relevant data-hooks/events (e.g. Account details 
	  / DOB/ Show Terms of service/ handle login error)
-------------------------------------------------------------------------
History:
	- 10/02/2012 : Created by Jonathan Bunner and Colin Gulliver

*************************************************************************/

#include "ICrysis3Lobby.h"

//-------------------------------------------------------------------------

#define BLAZE_PERSONA_MAX_LENGTH 16
#define BLAZE_PERSONA_MIN_LENGTH 4
#define BLAZE_PERSONA_VALID_CHARACTERS "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_ -"

//-------------------------------------------------------------------------

struct IGameWarningsListener;

class CLoginManager : public ICrysis3AuthenticationHandler, public IGameWarningsListener
{
public:
	enum ELocalFieldValidateResult
	{
		eLFVR_Unavailable = 0,
		eLFVR_Valid,
		eLFVR_Empty,
		eLFVR_TooLong,
		eLFVR_TooShort,
		eLFVR_InvalidChars,
		eLFVR_TooFewDigits,
		eLFVR_TooFewLowercase,
		eLFVR_TooFewUppercase
	};

	enum ELogoutAction // Ordered by priority, highest last
	{
		eLA_default = 0,
		eLA_restartLogin,
		eLA_switchToSP,
	};

public:

	CLoginManager(ICrysis3Lobby* pCrysis3Lobby); 
	~CLoginManager();

	void StartLoginProcess(bool bDriveMenu, bool bSwitchGameTypeWhenSignedIn);

	EOnlineState GetOnlineState(uint32 user) const { return (user == m_currentUser) ? m_currentState : eOS_Unknown; }
	bool LoginProcessInProgress() const { return (m_taskId != CryLobbyInvalidTaskID); }
	bool IsSwitchingToMultiplayer() const { return AreAnyFlagsSet(eLF_SwitchGameTypeWhenSignedIn); }

	// ICrysis3AuthenticationHandler
	virtual void OnDisplayCountrySelect();
	virtual void OnDisplayLegalDocs(const char *pTermsOfServiceText, const char *pPrivacyText);
	virtual void OnDisplayEntitleGame();

	virtual void OnDisplayCreateAccount();
	virtual void OnDisplayLogin();
	virtual void OnDisplayPersonas(const SPersona *pPersonas, const int numPersonas);
	virtual void OnPasswordRequired();

	virtual void OnCreateAccountError(EAuthenticationError errorCode, const SValidationError *pErrors, const int numErrors);
	virtual void OnCreatePersonaError(EAuthenticationError error);
	virtual void OnForgotPasswordError(EAuthenticationError error);
	virtual void OnAssociateAccountError(EAuthenticationError error);
	virtual void OnGeneralError(EAuthenticationError error);
	virtual void OnLoginFailure(EAuthenticationError error);

	// Xbox only
	virtual void OnProfileSelected();
	virtual void OnProfileLoaded(EAuthenticationError errorCode); // error can also be err_ok.. so log as 'result' and only error really if errorCode != err_ok
	virtual void OnProfileUnloaded(EAuthenticationError errorCode);
	// ~ICrysis3AuthenticationHandler
	
	// IGameWarningsListener
	virtual bool OnWarningReturn( THUDWarningId id, const char* pReturnValue );
	virtual void OnWarningRemoved( THUDWarningId id ) { };
	// ~IGameWarningsListener

	void OnCreateAccountRequested();
	void OnDisplayCountrySelectResponse(const SDisplayCountrySelectResponse& response);
	void OnDisplayLegalDocsResponse(bool bAccepted);
	void BeginAssociateAccount(const SAssociateAccountResponse& response);
	void OnDisplayCreateAccountResponse(const SCreateAccountResponse& response);
	void OnDisplayLoginResponse(const SDisplayLoginResponse& response);
	void OnDisplayEntitleGameResponse(const SDisplayEntitleGameResponse& response);
	void OnDisplayPersonasResponse(const SDisplayPersonasResponse& response);
	void OnPasswordRequiredResponse(const SPasswordRequiredResponse& response);
	void CreatePersona(const SDisplayPersonasResponse& response);
	void Back();
	void Logout(ELogoutAction action=eLA_default, bool bDriveMenu = true);
	void ForgotPassword(const char *pEmail);

	void StoreLoginDetails(const char* pEmail, const char* pPassword, const bool bRememberCheck);
	SPasswordRules* GetPasswordRules() { return (AreAnyFlagsSet(eLF_HasPasswordRules) ? &m_passwordRules : NULL); }

	ELocalFieldValidateResult ValidatePersona(const char *pPersona, int* pOutRequired=NULL);
	ELocalFieldValidateResult ValidatePassword(const char *pPassword, int* pOutRequired=NULL);
	const char* GetValidationError(ELocalFieldValidateResult validationResult, const char* pFieldName, const int required);
	static bool ValidateEmail(const char* pEmail);

	void PIIOptInScreenClose ();

#ifndef _RELEASE
	const char* Debug_GetEmail();
	const char* Debug_GetPassword();
	const char* Debug_GetPersona();
#endif

private:

#if !defined(_RELEASE) && !defined(DEDICATED_SERVER)
	static void CmdStartLoginProcess(IConsoleCmdArgs* pCmdArgs);
	static void CmdReprompt(IConsoleCmdArgs *pArgs);
	static void CmdChangeEmail(IConsoleCmdArgs *pArgs);
	static void CmdChangePassword(IConsoleCmdArgs *pArgs);

	void InitDebugVars();
	void ReleaseDebugVars();
#endif

	static void Crysis3LobbyPasswordCallback( CryLobbyTaskID taskID, ECryLobbyError error, SPasswordRules *pRules, void *pArg );
	static void OnlineStateChangedCallback(UCryLobbyEventData eventData, void *pArg);
	static void LoginProcessCallback(CryLobbyTaskID taskID, ECryLobbyError error, void *pArg);
	static void LogoutProcessCallback(CryLobbyTaskID taskID, ECryLobbyError error, void *pArg);

	bool Menu_GoToPage(const char *pPage);
	void Menu_GoBack();
	void Internal_StartLoginProcess();
	void FinishLogin();

	bool HandleAuthenticationErrorWarning(const EAuthenticationError error);

	SPasswordRules m_passwordRules;

	CryFixedStringT<IC3_MAX_EMAIL_LENGTH> m_forgotPasswordEmail;
	CryFixedStringT<IC3_MAX_EMAIL_LENGTH> m_loginEmail;
	CryFixedStringT<IC3_MAX_PASSWORD_LENGTH> m_loginPassword;

	ICrysis3Lobby* m_pCrysis3Lobby;
	CryLobbyTaskID m_taskId;
	CryLobbyTaskID m_logoutTaskId;
	EOnlineState m_currentState;
	ELogoutAction m_logoutAction;
	CryUserID m_lastSignedInUser;

	uint32 m_currentUser;
	
	enum ELoginFlags
	{
		eLF_RememberDetails							= BIT(0),
		eLF_HasPasswordRules						= BIT(1),
		eLF_DriveMenu										= BIT(2),
		eLF_NeedToLogin									= BIT(3),
		eLF_SwitchGameTypeWhenSignedIn	= BIT(4),
		eLF_DeclinedTOS									= BIT(5),
		eLF_IsCreatingAccount						= BIT(6),
	};

	typedef uint8 TLoginFlagsType;

	void AddFlag(ELoginFlags flag) { m_flags |= (TLoginFlagsType)flag; }
	void RemoveFlag(ELoginFlags flag) { m_flags &= ~(TLoginFlagsType)flag; }
	void SetFlag(ELoginFlags flag, bool bAdd) { if (bAdd) { AddFlag(flag); } else { RemoveFlag(flag); } }
	bool AreAnyFlagsSet(ELoginFlags flags) const { return (m_flags & (TLoginFlagsType)flags) != 0; }

	TLoginFlagsType m_flags;

	// Statics
	static const char* const	s_pEmailLegal;
	static const char* const	s_pEmailPartEnd;
};


#endif // __LOGINMANAGER_H__
