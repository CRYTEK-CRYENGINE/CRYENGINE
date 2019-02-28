#include "StdAfx.h"

#include "PSNPlatform.h"

#include "PSNLeaderboards.h"

#include <kernel.h>
#include <sce_atomic.h>
#include <sdk_version.h>

#include <save_data.h>
#include <common_dialog.h>
#include <message_dialog.h>
#include <libsysmodule.h>
#include <system_service.h>
#include <app_content.h>
#include <libnetctl.h>

#include <np_commerce_dialog.h>
#include <error_dialog.h>

#include <CrySystem/ITimer.h>

namespace Cry
{
	namespace GamePlatform
	{
		namespace PSN
		{
			CPlugin* CPlugin::s_pInstance = nullptr;

			EConnectionStatus ConvertConnectionStatus(int sceStatus)
			{
				switch (sceStatus)
				{
				case SCE_NET_CTL_STATE_CONNECTING:
					return EConnectionStatus::Connecting;
				case SCE_NET_CTL_STATE_IPOBTAINING:
					return EConnectionStatus::ObtainingIP;
				case SCE_NET_CTL_STATE_IPOBTAINED:
					return EConnectionStatus::Connected;
				case SCE_NET_CTL_STATE_DISCONNECTED:
				default:
					return EConnectionStatus::Disconnected;
				}
			}

			void CallbackNetwork(int eventType, void* arg)
			{
				CPlugin::GetInstance()->SetConnectionStatus(ConvertConnectionStatus(eventType));
			}

			bool CPlugin::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
			{
				s_pInstance = this;

				auto ret = sceSysmoduleLoadModule(SCE_SYSMODULE_NP_AUTH);
				if (ret < SCE_OK)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Initialization failed, failed to load authentication module");
					return false;
				}

				ret = sceSysmoduleLoadModule(SCE_SYSMODULE_NP_COMMERCE);
				if (ret < SCE_OK)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Initialization failed, failed to load commerce module");
					return false;
				}

				sceSysmoduleLoadModule(SCE_SYSMODULE_MESSAGE_DIALOG);
				sceSysmoduleLoadModule(SCE_SYSMODULE_SIGNIN_DIALOG);
				sceSysmoduleLoadModule(SCE_SYSMODULE_ERROR_DIALOG);

				int connectionStatus = SCE_NET_CTL_STATE_DISCONNECTED;
				ret = sceNetCtlGetState(&connectionStatus);
				if (ret == SCE_OK)
				{
					m_connectionStatus = ConvertConnectionStatus(connectionStatus);
				}

				ret = sceNetCtlRegisterCallback(CallbackNetwork, NULL, &m_sceNetCtlCallbackId);
				if (ret < SCE_OK)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Initialization failed, failed to register network callback");
					return false;
				}

				m_user.Initialize();
				m_statistics.Initialize();
				m_pLeaderboards = stl::make_unique<CLeaderboards>();

				EnableUpdate(IEnginePlugin::EUpdateStep::MainUpdate, true);

				return true;
			}

			void CPlugin::MainUpdate(float frameTime)
			{
				CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

				sceNetCtlCheckCallback();

				m_user.Update();

				if (m_npCheckPlusRequestId != 0)
				{
					int result = 0;
					int returnCode = sceNpPollAsync(m_npCheckPlusRequestId, &result);
					if (returnCode < SCE_OK)
					{
						sceNpDeleteRequest(m_npCheckPlusRequestId);
						m_npCheckPlusRequestId = 0;

						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to poll for playstation plus status! sceNpPollAsync() error: 0x%08X\n", returnCode);
					}
					else if (returnCode == SCE_NP_POLL_ASYNC_RET_FINISHED)
					{
						m_multiplayerAccessCallback(m_npCheckPlusResult.authorized);
						m_multiplayerAccessCallback = nullptr;

						sceNpDeleteRequest(m_npCheckPlusRequestId);
						m_npCheckPlusRequestId = 0;
					}
				}

				// Notify PSN that we are in a multiplayer match every second
				if (gEnv->bMultiplayer)
				{
					if (m_npNotifyPlusFeatureTimer > 0.f)
					{
						m_npNotifyPlusFeatureTimer -= gEnv->pTimer->GetFrameTime();
					}
					else
					{
						SceNpNotifyPlusFeatureParameter notifyParams;
						memset(&notifyParams, 0, sizeof(notifyParams));

						notifyParams.size = sizeof(notifyParams);
						notifyParams.userId = m_user.GetIdentifier();
						notifyParams.features = SCE_NP_PLUS_FEATURE_REALTIME_MULTIPLAY;

						int returnCode = sceNpNotifyPlusFeature(&notifyParams);
						if (returnCode != SCE_OK)
						{
							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to notify plus feature, code: 0x%08X", returnCode);
						}
						
						m_npNotifyPlusFeatureTimer = 1.f;
					}
				}
				else if (m_npNotifyPlusFeatureTimer != 0.f)
				{
					m_npNotifyPlusFeatureTimer = 0.f;
				}

				m_pLeaderboards->Update();
			}

			void CPlugin::CanAccessMultiplayerServices(std::function<void(bool authorized)> asynchronousCallback)
			{
				if (m_npCheckPlusRequestId == 0)
				{
					SceNpCheckPlusParameter checkPlusParams;
					memset(&checkPlusParams, 0, sizeof(checkPlusParams));

					checkPlusParams.size = sizeof(checkPlusParams);
					checkPlusParams.userId = m_user.GetIdentifier();
					checkPlusParams.features = SCE_NP_PLUS_FEATURE_REALTIME_MULTIPLAY;

					SceNpCreateAsyncRequestParameter reqParam;
					memset(&reqParam, 0, sizeof(reqParam));
					reqParam.size = sizeof(reqParam);
					reqParam.cpuAffinityMask = SCE_KERNEL_CPUMASK_USER_ALL;
					reqParam.threadPriority = SCE_KERNEL_PRIO_FIFO_DEFAULT;
					int returnCode = sceNpCreateAsyncRequest(&reqParam);

					if (returnCode < SCE_OK)
					{
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to check plus feature status, code: 0x%08X", returnCode);
						asynchronousCallback(false);
						return;
					}

					m_npCheckPlusRequestId = returnCode;

					returnCode = sceNpCheckPlus(m_npCheckPlusRequestId, &checkPlusParams, &m_npCheckPlusResult);
					if (returnCode < SCE_OK)
					{
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to check plus feature status, code: 0x%08X", returnCode);
						sceNpDeleteRequest(m_npCheckPlusRequestId);
						asynchronousCallback(false);
						return;
					}

					m_multiplayerAccessCallback = asynchronousCallback;
				}
			}

			CRYREGISTER_SINGLETON_CLASS(CPlugin);
		}
	}
}