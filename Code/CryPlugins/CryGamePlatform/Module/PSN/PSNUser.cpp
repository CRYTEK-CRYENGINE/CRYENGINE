#include "StdAfx.h"
#include "PSNUser.h"

#include <libsysmodule.h>
#include <np.h>
#include <signin_dialog.h>

namespace Cry
{
	namespace GamePlatform
	{
		namespace PSN
		{
			CUser::CUser()
				: m_authorizationCodeStatus(EAuthorizationCodeStatus::Invalid)
				, m_npCheckAvailabilityRequestId(0)
				, m_PSNSignInStatus(ESignInStatus::Unknown)
				, m_bInitialized(false)
				, m_bPSNInitialized(false)
				, m_npAuthorizationIssuerId(-1)
				, m_bWaitingForSignup(false)
			{
				m_pNpClientIdCVar = REGISTER_STRING("psn_npClientId", "", VF_REQUIRE_APP_RESTART, "Specifies the NP Client Id to use for PSN authorization");

				m_bUserLibraryOwner = false;

				memset(&m_npAuthorizationCode, 0, sizeof(m_npAuthorizationCode));
			}

			CUser::~CUser()
			{
				gEnv->pConsole->UnregisterVariable("psn_npClientId");

				if (m_bUserLibraryOwner)
				{
					int32_t returnCode = -1;

					sceUserServiceTerminate();
					if (returnCode < 0)
					{
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to terminate user service");
					}
				}
			}

			void PSNStateCallback(SceUserServiceUserId userId, SceNpState state, void* userdata)
			{
				ESignInStatus newStatus = ESignInStatus::Unknown;
				switch (state)
				{
				case SCE_NP_STATE_SIGNED_IN:
					newStatus = ESignInStatus::Online;
					break;

				case SCE_NP_STATE_SIGNED_OUT:
					newStatus = ESignInStatus::Offline;
					break;
				}

				static_cast<CUser*>(CPlugin::GetInstance()->GetLocalClient())->SetSignedInState(newStatus);
			}

			void CUser::SetSignedInState(ESignInStatus status)
			{
				if (m_PSNSignInStatus != ESignInStatus::Online && status == ESignInStatus::Online)
				{
					m_bPSNInitialized = false;
					ReinitializePSN();
				}
				else if (m_PSNSignInStatus == ESignInStatus::Online && status != ESignInStatus::Online)
				{
					m_bPSNInitialized = false;
					CryMessageBox("User signed out!", "PSN Error", EMessageBox::eMB_Info);
				}

				m_PSNSignInStatus = status;
			}

			void CUser::InitPSN()
			{
				int returnCode = 0;
				SceNpCreateAsyncRequestParameter asyncParam;
				asyncParam.size = sizeof(asyncParam);
				asyncParam.threadPriority = SCE_KERNEL_PRIO_FIFO_DEFAULT;
				asyncParam.cpuAffinityMask = SCE_KERNEL_CPUMASK_USER_ALL;

				returnCode = sceNpCreateAsyncRequest(&asyncParam);
				if (returnCode < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to create asynchronous request");
					return;
				}

				m_npCheckAvailabilityRequestId = returnCode;
				returnCode = sceNpCheckNpAvailabilityA(m_npCheckAvailabilityRequestId, m_userId);
				if (returnCode < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to check Np availability");
					return;
				}

				RefreshAuthorizationCode();

				m_bInitialized = true;
			}

			void CUser::Initialize()
			{
				int32_t returnCode = -1;

				returnCode = sceUserServiceInitialize(nullptr);
				if (returnCode < 0 && returnCode != SCE_USER_SERVICE_ERROR_ALREADY_INITIALIZED)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to initialize user service");
					return;
				}
				else
				{
					m_bUserLibraryOwner = true;
				}

				returnCode = sceNpRegisterStateCallbackA(PSNStateCallback, NULL);
				if (returnCode < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to set PSN state callback");
				}

				GetIDsFromPSN();
			}

			void CUser::ReinitializePSN()
			{
				m_bWaitingForSignup = false;
				GetIDsFromPSN();
			}

			void CUser::Update()
			{
				sceNpCheckCallback();

				if (m_bWaitingForSignup)
				{
					if (sceSigninDialogUpdateStatus() == SCE_SIGNIN_DIALOG_STATUS_FINISHED)
					{
						sceSigninDialogClose();

						SceSigninDialogResult result;
						sceSigninDialogGetResult(&result);

						sceSigninDialogTerminate();

						if (result.result != SCE_SIGNIN_DIALOG_RESULT_OK)
						{
							CryMessageBox("User signed out!", "PSN Error", EMessageBox::eMB_Info);
						}
					}
				}

				if (!m_bInitialized)
					return;

				if (m_authorizationCodeStatus == EAuthorizationCodeStatus::Invalid)
				{
					int result = 0;
					int returnCode = sceNpAuthPollAsync(m_authorizationCodeRequestId, &result);
					if (returnCode < 0)
					{
						sceNpAuthDeleteRequest(m_authorizationCodeRequestId);
						m_authorizationCodeStatus = EAuthorizationCodeStatus::Invalid;

						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to poll for authorization code!");
					}
					else if (returnCode == SCE_NP_AUTH_POLL_ASYNC_RET_FINISHED)
					{
						sceNpAuthDeleteRequest(m_authorizationCodeRequestId);

						if (result == SCE_OK)
						{
							m_authorizationCodeStatus = EAuthorizationCodeStatus::Valid;
						}
						else
						{
							m_authorizationCodeStatus = EAuthorizationCodeStatus::Invalid;
						}
					}
				}

				if (m_npCheckAvailabilityRequestId != 0)
				{
					int result = 0;
					int returnCode = sceNpPollAsync(m_npCheckAvailabilityRequestId, &result);
					if (returnCode == SCE_NP_AUTH_POLL_ASYNC_RET_FINISHED)
					{
						bool bError = false;

						switch (result)
						{
						case SCE_NP_ERROR_SIGNED_OUT:
							CryMessageBox("User signed out!", "PSN Error", EMessageBox::eMB_Info);
							m_PSNSignInStatus = ESignInStatus::Offline;
							bError = true;
							break;

						case SCE_NP_ERROR_USER_NOT_FOUND:
							CryMessageBox("User not found!", "PSN Error", EMessageBox::eMB_Info);
							m_PSNSignInStatus = ESignInStatus::Unknown;
							bError = true;
							break;

						case SCE_NP_ERROR_AGE_RESTRICTION:
							CryMessageBox("User does not meet the age limit!", "PSN Error", EMessageBox::eMB_Info);
							m_PSNSignInStatus = ESignInStatus::Unknown;
							bError = true;
							break;

						case SCE_NP_ERROR_LOGOUT:
							CryMessageBox("User was signed out!", "PSN Error", EMessageBox::eMB_Info);
							m_PSNSignInStatus = ESignInStatus::Offline;
							bError = true;
							break;

						case SCE_NP_ERROR_LATEST_SYSTEM_SOFTWARE_EXIST:
							CryMessageBox("New system update is available, please update!", "PSN Error", EMessageBox::eMB_Info);
							m_PSNSignInStatus = ESignInStatus::Offline;
							bError = true;
							break;

						case SCE_NP_ERROR_LATEST_SYSTEM_SOFTWARE_EXIST_FOR_TITLE:
							CryMessageBox("New system software is available for the application, please update!", "PSN Error", EMessageBox::eMB_Info);
							m_PSNSignInStatus = ESignInStatus::Offline;
							bError = true;
							break;

						case SCE_NP_ERROR_LATEST_PATCH_PKG_EXIST:
							CryMessageBox("New patch package is available, please update!", "PSN Error", EMessageBox::eMB_Info);
							m_PSNSignInStatus = ESignInStatus::Offline;
							bError = true;
							break;

						case SCE_NP_ERROR_LATEST_PATCH_PKG_DOWNLOADED:
							CryMessageBox("New patch package was downloaded, please update!", "PSN Error", EMessageBox::eMB_Info);
							m_PSNSignInStatus = ESignInStatus::Offline;
							bError = true;
							break;
						}

						sceNpDeleteRequest(m_npCheckAvailabilityRequestId);
						m_npCheckAvailabilityRequestId = 0;

						if (!bError)
						{
							m_bPSNInitialized = true;
							RefreshAuthorizationCode();
						}
					}
					else if (returnCode < SCE_OK)
					{
						m_bPSNInitialized = false;
						m_PSNSignInStatus = ESignInStatus::Unknown;
						sceNpDeleteRequest(m_npCheckAvailabilityRequestId);
						m_npCheckAvailabilityRequestId = 0;

						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to get network availability");
					}
				}
			}

			void CUser::GetIDsFromPSN()
			{
				int32_t returnCode = -1;

				returnCode = sceUserServiceGetInitialUser(&m_userId);
				if (returnCode < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to get local user");
					return;
				}

				returnCode = sceNpGetNpId(m_userId, &m_npId);
				if (returnCode < 0)
				{
					switch (returnCode)
					{
					case SCE_NP_ERROR_SIGNED_OUT:
					case SCE_NP_ERROR_NOT_SIGNED_UP:
						if (!m_bWaitingForSignup)
						{
							SceSigninDialogParam param;
							sceSigninDialogParamInitialize(&param);
							param.userId = m_userId;

							sceCommonDialogInitialize();
							sceSigninDialogInitialize();
							sceSigninDialogOpen(&param);

							m_bWaitingForSignup = true;
							return;
						}
						break;
					default:
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to get Np Id for user");
						return;
					}
				}

				returnCode = sceNpGetOnlineId(m_userId, &m_onlineId);
				if (returnCode < 0)
				{
					switch (returnCode)
					{
					case SCE_NP_ERROR_SIGNED_OUT:
					case SCE_NP_ERROR_NOT_SIGNED_UP:
						if (!m_bWaitingForSignup)
						{
							SceSigninDialogParam param;
							sceSigninDialogParamInitialize(&param);
							param.userId = m_userId;

							sceCommonDialogInitialize();
							sceSigninDialogInitialize();
							sceSigninDialogOpen(&param);

							m_bWaitingForSignup = true;
							return;
						}
						break;
					default:
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to get online id for user");
						return;
					}
				}

				returnCode = sceNpGetAccountIdA(m_userId, &m_accountId);
				if (returnCode < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to get account id for user");
					return;
				}

				InitPSN();
			}

			void CUser::RefreshAuthorizationCode()
			{
				if (m_PSNSignInStatus == ESignInStatus::Unknown)
					return;

				SceNpAuthCreateAsyncRequestParameter asyncParam;
				asyncParam.size = sizeof(asyncParam);
				asyncParam.threadPriority = SCE_KERNEL_PRIO_FIFO_DEFAULT;
				asyncParam.cpuAffinityMask = SCE_KERNEL_CPUMASK_USER_ALL;

				auto returnCode = sceNpAuthCreateAsyncRequest(&asyncParam);
				if (returnCode < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to get asynchronous auth token request");
					return;
				}

				m_authorizationCodeRequestId = returnCode;

				if (m_pNpClientIdCVar->GetString()[0] == '\0')
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Attempted to request authorization code without valid Np Client Id, please set the psn_npClientId CVar");
					return;
				}

				SceNpClientId clientId;
				memset(&clientId, 0, sizeof(clientId));

				cry_strcpy(clientId.id, m_pNpClientIdCVar->GetString(), SCE_NP_CLIENT_ID_MAX_LEN);

				SceNpAuthGetAuthorizationCodeParameterA authParam;
				memset(&authParam, 0, sizeof(authParam));

				authParam.size = sizeof(authParam);
				authParam.userId = m_userId;
				authParam.clientId = &clientId;
				authParam.scope = "psn:s2s";

				m_npAuthorizationIssuerId = 0;

				returnCode = sceNpAuthGetAuthorizationCodeA(m_authorizationCodeRequestId, &authParam, &m_npAuthorizationCode, &m_npAuthorizationIssuerId);
				if (returnCode < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to get authorization code");

					sceNpAuthDeleteRequest(m_authorizationCodeRequestId);
					return;
				}

				m_authorizationCodeStatus = EAuthorizationCodeStatus::Requested;
			}

			bool CUser::GetAuthToken(string& tokenOut, int& issuerId)
			{
				if (m_authorizationCodeStatus == EAuthorizationCodeStatus::Requested || m_PSNSignInStatus != ESignInStatus::Online)
				{
					issuerId = -1;
					return false;
				}

				if (m_authorizationCodeStatus == EAuthorizationCodeStatus::Invalid)
				{
					RefreshAuthorizationCode();

					issuerId = -1;
					return false;
				}

				issuerId = m_npAuthorizationIssuerId;
				tokenOut = m_npAuthorizationCode.code;

				// Authorization codes can't be reused
				m_authorizationCodeStatus = EAuthorizationCodeStatus::Invalid;

				return true;
			}
		}
	}
}