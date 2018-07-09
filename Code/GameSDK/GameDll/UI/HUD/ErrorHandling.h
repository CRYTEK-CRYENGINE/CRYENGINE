// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MP_ERROR_HANDLING_H__
#define __MP_ERROR_HANDLING_H__

//-----------------------------------------------------------------------------
class CErrorHandling : public IGameWarningsListener
{
public:
	enum EExitMultiplayerReason
	{
		eEMR_Menu,						// User selected SP on the menu
		eEMR_SignInFailed,		// Login task failed
		eEMR_Other,						// Any other reason
	};

	// Ordered by severity, least to worst (note, all fatal errors are considered worse than non-fatal errors)
	enum EFatalError
	{
		eFE_None,

		// MP loader errors
		eFE_GeneralMPLoaderError,				// Generic MP loader error
		eFE_CannotConnectToMultiplayer,	// Failed to connect to MP
		eFE_CannotCreateSquad,					// Failed to create squad when entering MP
		eFE_InsufficientPrivileges,			// Don't have permissions to access MP (i.e. silver LIVE account)
		eFE_LocalNotSignedIn,						// Can't do an action because our local user isn't signed in
		eFE_OnlineNotSignedIn,					// Can't do an action because we're not signed in to the online platform
		eFE_SignInCancelled,						// User cancelled the online sign in
		eFE_PlatformServiceNotSignedIn,	// Not signed in to platform service (PSN), either couldn't connect or user cancelled
		eFE_NoOnlineAccount,						// Local user doesn't have an online account (platform account)

		// Restriction errors
		eFE_AgeRestricted,							// Can't access MP due to age restrictions
		eFE_InternetDisabled,						

		// Sign out errors
		eFE_ServiceSignedOut,						// Signed out of the lobby service
		eFE_PlatformServiceSignedOut,		// Signed out of the platform service (PSN)
		eFE_LocalSignedOut,							// Signed out locally

		// User has been silly errors
		eFE_StorageRemoved,							// Storage has been removed, have to return back to SP since partial patching may have occurred
		eFE_ContentRemoved,							// DLC content removed, possibly it was on a removable storage device
		eFE_EthernetCablePulled,				// Ethernet cable removed
	};

	// Ordered by severity, least to worst (note, all fatal errors are considered worse than non-fatal errors)
	enum ENonFatalError
	{
		eNFE_None,

		// Generic errors
		eNFE_HostMigrationFailed,				// Failed a host migration
		eNFE_NubDisconnected,						// Nub has disconnected
		eNFE_SessionEndFailed,					// Failed to successfully complete session end

		// Kicked
		eNFE_Kicked,										// Kicked by the server
		eNFE_KickedHighPing,						// Kicked by the server due to having a high ping
		eNFE_KickedReservedUser,				// Kicked in favour of a reserved user joining
		eNFE_KickedBanLocal,						// Kicked because the server has locally banned us
		eNFE_KickedBanGlobal,						// Kicked because we are globally banned
		eNFE_KickedBanGlobalStage1,			// Kicked because we have just triggered the stage 1 global ban
		eNFE_KickedBanGlobalStage2,			// Kicked because we have just triggered the stage 2 global ban

		// Drastic but not fatal!
		eNFE_DLCLoadingFailed,					// DLC loading has failed

		//// Not in correct state errors
		//eNFE_LocalUserNotSignedIn,			// Can't do action - local user isn't signed in
		//eNFE_OnlineUserNotSignedIn,			// Can't do action - Online user isn't signed in
		//eNFE_EthernetCableNotConnected,	// Can't do action - Ethernet cable unplugged
	};

	CErrorHandling();
	~CErrorHandling();

	// IGameWarningsListener
	virtual bool OnWarningReturn(THUDWarningId id, const char* returnValue);
	// ~IGameWarningsListener

	static CErrorHandling *GetInstance() { return s_pInstance; }

	static EFatalError GetFatalErrorFromLobbyError(ECryLobbyError error);

	void Update();

	void OnEnterMultiplayer();
	void OnLeaveMultiplayer(EExitMultiplayerReason reason);

	bool OnFatalError(EFatalError error);
	bool OnNonFatalError(ENonFatalError error, bool bShowDialog);

	bool HasError() const { return ((m_error.m_fatalError != eFE_None) || (m_error.m_nonFatalError != eNFE_None)); }

	void OnLoadingError(ILevelInfo *pLevel, const char *error);
	void OnNubDisconnect(EDisconnectionCause cause);

private:
	enum EState
	{
		eS_Init,
		eS_WaitingToUnload,
		eS_Unloading,
		eS_WaitingToLeaveSession,
		eS_LeavingSession,
		eS_MultiplayerMenu,
		eS_SwitchingToSinglePlayer,
		eS_SingleplayerMenu,
	};

	struct SDisconnectError
	{
		SDisconnectError()
		{
			Reset();
		}

		void Reset()
		{
			m_fatalError = eFE_None;
			m_nonFatalError = eNFE_None;
			m_nubDisconnectCause = eDC_Unknown;
			m_currentState = eS_Init;
			m_targetState = eS_Init;
			m_bWasPrivateGame = false;
			m_bWasInGame = false;
			m_bShowErrorDialog = false;
			m_bStartedProcessing = false;
		}

		EFatalError						m_fatalError;
		ENonFatalError				m_nonFatalError;
		EDisconnectionCause		m_nubDisconnectCause;
		EState								m_currentState;
		EState								m_targetState;
		bool									m_bWasPrivateGame;
		bool									m_bWasInGame;
		bool									m_bShowErrorDialog;
		bool									m_bStartedProcessing;
	};

	bool HasFatalError() const { return (m_error.m_fatalError != eFE_None); }
	bool ShouldProcessFatalError(EFatalError error);
	void SetupErrorHandling(EFatalError fatalError, ENonFatalError nonFatalError, bool bShowDialog, EState targetState);
	void HandleError(bool &bRecursiveUpdate);
	void SetErrorState(EState state);
	bool HandleFatalError();
	bool HandleNonFatalError();
	void ShowErrorDialog(const char *pErrorName, const char *pParam);

	SDisconnectError	m_error;
	THUDWarningId			m_warningId;
	int								m_multiplayer;		// int because we defer the exit by a few frames in some cases
	bool							m_bIsSignInChange;

	static CErrorHandling *s_pInstance;
};

#endif	// __MP_ERROR_HANDLING_H__
