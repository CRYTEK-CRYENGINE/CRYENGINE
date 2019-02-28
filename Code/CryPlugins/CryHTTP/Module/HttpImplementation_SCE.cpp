#include "StdAfx.h"
#include "HttpImplementation_SCE.h"

#include <CrySystem/IProjectManager.h>

#include <array>
#include <CryCore/Platform/platform_impl.inl>

namespace Cry
{
	namespace Http
	{
		namespace Sce
		{
			CPlugin::~CPlugin()
			{
				m_requests.clear();

				if (m_iHTTPLibId > 0)
				{
					int result = sceHttpTerm(m_iHTTPLibId);
					if (result < 0)
					{
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceHttpEnd() error: 0x%08X\n", result);
					}
				}

				if (m_iSSLLibId > 0)
				{
					int result = sceSslTerm(m_iSSLLibId);
					if (result < 0)
					{
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceSslEnd() error: 0x%08X\n", result);
					}
				}

				if (m_iNetLibId > 0)
				{
					int result = sceNetPoolDestroy(m_iNetLibId);
					if (result > 0)
					{
						result = sceNetTerm();
						if (result < 0)
						{
							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] Failed to terminate net");
						}
					}
					else
					{
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] Failed to destroy net pool");
					}
				}	
			}

			bool CPlugin::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
			{
				int result = sceNetInit();
				if (result < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceNetInit() error: 0x%08X\n", result);
					return false;
				}

				constexpr int netPoolHeapSize = 64 * 1024;
				m_iNetLibId = sceNetPoolCreate(gEnv->pSystem->GetIProjectManager()->GetCurrentProjectName(), netPoolHeapSize, 0);
				if (result < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceNetPoolCreate() error: 0x%08X\n", result);
					return false;
				}

				constexpr int sslHeapSize = 304 * 1024;
				m_iSSLLibId = sceSslInit(sslHeapSize);
				if (m_iSSLLibId < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceSslInit() error: 0x%08X\n", m_iSSLLibId);
					return false;
				}

				constexpr int httpHeapSize = 128 * 1024;
				m_iHTTPLibId = sceHttpInit(m_iNetLibId, m_iSSLLibId, httpHeapSize);
				if (m_iHTTPLibId < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceHttpInit() error: 0x%08X\n", m_iHTTPLibId);
					return false;
				}

				EnableUpdate(IEnginePlugin::EUpdateStep::MainUpdate, true);
				return true;
			}

			void CPlugin::MainUpdate(float frameTime)
			{
				ProcessRequests();
				EUpdateResult result;

				long maximumWaitInMs;
				do
				{
					result = ProcessRequests();
				} while (result == EUpdateResult::ProcessingRequests);
			}

			IPlugin::EUpdateResult CPlugin::ProcessRequests()
			{
				if (m_requests.size() > 0)
				{
					for(auto it = m_requests.begin(); it != m_requests.end();)
					{
						using RequestStateCallback = ERequestStateResult(CPlugin::*)(SSceHttpRequest&);

						static constexpr std::array<RequestStateCallback, static_cast<int>(ERequestState::Count)> requstStateCallbacks =
						{
							{
								&CPlugin::OnSendRequest,
								&CPlugin::OnGetStatus,
								&CPlugin::OnReceiveHeaders,
								&CPlugin::OnGetResponseLength,
								&CPlugin::OnReceiveResponse,
								&CPlugin::OnRequestComplete
							}
						};

						SSceHttpRequest& request = *it->get();

						ERequestStateResult stateResult;

						while(true)
						{
							auto& stateCallback = requstStateCallbacks[static_cast<int>(request.state)];

							stateResult = (this->*stateCallback)(request);
							if (stateResult == ERequestStateResult::Waiting)
							{
								// Wait until next ProcessRequests call to check again
								break;
							}
							else if (stateResult == ERequestStateResult::Done)
							{
								if (request.state == ERequestState::Done)
								{
									// Request is complete, remove it from the queue
									stateResult = ERequestStateResult::Abort;
									break;
								}
								else
								{
									// Proceed to next request state immediately
									request.state = static_cast<ERequestState>(static_cast<int>(request.state) + 1);
								}
							}
							else if (stateResult == ERequestStateResult::Abort)
							{
								break;
							}
						}

						if (stateResult == ERequestStateResult::Abort)
						{
							it = m_requests.erase(it);
						}
						else
						{
							++it;
						}
					}
				}

				return m_requests.size() > 0 ? EUpdateResult::ProcessingRequests : EUpdateResult::Idle;
			}

			bool CPlugin::Send(ERequestType requestType, const char* szURL, const char* szBody, TResponseCallback resultCallback, const THeaders& headers)
			{
				std::shared_ptr<SSceHttpRequest> pRequest = std::make_shared<SSceHttpRequest>();

				switch (requestType)
				{
				case ERequestType::GET:
					pRequest->requestType = SCE_HTTP_METHOD_GET;
					break;
				case ERequestType::DELETE:
					pRequest->requestType = SCE_HTTP_METHOD_DELETE;
					break;
				case ERequestType::PATCH:
					pRequest->requestType = SCE_HTTP_METHOD_PUT;
					break;
				case ERequestType::PUT:
					pRequest->requestType = SCE_HTTP_METHOD_PUT;
					break;
				case ERequestType::POST:
				default:
					pRequest->requestType = SCE_HTTP_METHOD_POST;
					break;
				}

				pRequest->sContent = szBody;
				pRequest->resultCallback = resultCallback;
				
				CryFixedStringT<32> productVersion = gEnv->pSystem->GetProductVersion().ToString();

				string userAgent;
				userAgent.Format("%s/%s", gEnv->pSystem->GetIProjectManager()->GetCurrentProjectName(), productVersion.c_str());

				pRequest->templateId = sceHttpCreateTemplate(m_iHTTPLibId, userAgent, SCE_HTTP_VERSION_1_1, SCE_TRUE);
				if (pRequest->templateId < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceHttpCreateTemplate() error: 0x%08X\n", pRequest->templateId);
					return false;
				}

				auto returnCode = sceHttpSetNonblock(pRequest->templateId, SCE_TRUE);
				if (returnCode < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceHttpSetNonblock() error: 0x%08X\n", returnCode);
					return false;
				}

				returnCode = sceHttpCreateEpoll(m_iHTTPLibId, &pRequest->pollingHandle);
				if (returnCode < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceHttpCreateEpoll() error: 0x%08X\n", returnCode);
					return false;
				}

				pRequest->connectionId = sceHttpCreateConnectionWithURL(pRequest->templateId, szURL, SCE_TRUE);
				if (pRequest->connectionId < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceHttpCreateConnectionWithURL() error: 0x%08X\n", pRequest->connectionId);
					return false;
				}

				pRequest->requestId = sceHttpCreateRequestWithURL(pRequest->connectionId, pRequest->requestType, szURL, pRequest->sContent.size());
				if (pRequest->requestId < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceHttpCreateRequestWithURL() error: 0x%08X\n", pRequest->requestId);
					return false;
				}

				returnCode = sceHttpSetEpoll(pRequest->requestId, pRequest->pollingHandle, NULL);
				if (returnCode < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceHttpCreateRequestWithURL() error: 0x%08X\n", returnCode);
					return false;
				}

				for(const std::pair<const char*, const char*>& headerPair : headers)
				{
					returnCode = sceHttpAddRequestHeader(pRequest->requestId, headerPair.first, headerPair.second, SCE_HTTP_HEADER_ADD);
					if (returnCode < 0)
					{
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceHttpAddRequestHeader() error: 0x%08X\n", returnCode);
						return false;
					}
				}

				m_requests.push_back(pRequest);
				return true;
			}

			bool CPlugin::WaitForRequest(SSceHttpRequest& request)
			{
				SceHttpNBEvent nbEvent;
				ZeroStruct(nbEvent);

				int returnCode = sceHttpWaitRequest(request.pollingHandle, &nbEvent, 1, 1);
				if (returnCode > 0 && nbEvent.id == request.requestId)
				{
					if ((nbEvent.events & (SCE_HTTP_NB_EVENT_SOCK_ERR | SCE_HTTP_NB_EVENT_HUP | SCE_HTTP_NB_EVENT_RESOLVER_ERR)) != 0)
					{
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] Request wait failed!");
						return false;
					}
				}
				else if (returnCode < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceHttpWaitRequest() error: 0x%08X\n", returnCode);
					return false;
				}

				return true;
			}

			ERequestStateResult CPlugin::OnReceiveResponse(SSceHttpRequest& request)
			{
				char buff[MAX_FRAME_MESSAGE_SIZE + 1];

				auto returnCode = sceHttpReadData(request.requestId, &buff, MAX_FRAME_MESSAGE_SIZE);
				if (returnCode == 0)
				{
					return ERequestStateResult::Done;
				}
				else if (returnCode < 0)
				{
					if (returnCode == SCE_HTTP_ERROR_EAGAIN && WaitForRequest(request))
					{
						return ERequestStateResult::Waiting;
					}

					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceHttpReadData() error: 0x%08X\n", returnCode);
					return ERequestStateResult::Abort;
				}

				buff[returnCode] = '\0';
				request.sResponse += string(buff, returnCode);
				return ERequestStateResult::Waiting;
			}

			ERequestStateResult CPlugin::OnGetResponseLength(SSceHttpRequest& request)
			{
				int type;
				uint64_t contentLength;

				int returnCode = sceHttpGetResponseContentLength(request.requestId, &type, &contentLength);
				if (returnCode < 0)
				{
					if (returnCode == SCE_HTTP_ERROR_EAGAIN && WaitForRequest(request))
					{
						return ERequestStateResult::Waiting;
					}

					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceHttpGetContentLength() error: 0x%08X\n", returnCode);
					return ERequestStateResult::Abort;
				}

				return ERequestStateResult::Done;
			}

			ERequestStateResult CPlugin::OnRequestComplete(SSceHttpRequest& request)
			{
				request.resultCallback(request.sResponse, request.replyStatusCode);
				return ERequestStateResult::Done;
			}

			ERequestStateResult CPlugin::OnGetStatus(SSceHttpRequest& request)
			{
				auto result = sceHttpGetStatusCode(request.requestId, &request.replyStatusCode);
				if (result < 0)
				{
					if (result == SCE_HTTP_ERROR_EAGAIN && WaitForRequest(request))
					{
						return ERequestStateResult::Waiting;
					}

					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceHttpGetStatusCode() error: 0x%08X\n", result);
					return ERequestStateResult::Abort;
				}

				return ERequestStateResult::Done;
			}

			ERequestStateResult CPlugin::OnSendRequest(SSceHttpRequest& request)
			{
				auto returnCode = sceHttpSendRequest(request.requestId, request.sContent.c_str(), request.sContent.size());
				if (returnCode < 0)
				{
					if ((returnCode == SCE_HTTP_ERROR_EAGAIN || returnCode == SCE_HTTP_ERROR_BUSY) && WaitForRequest(request))
					{
						return ERequestStateResult::Waiting;
					}

					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceHttpSendRequest() error: 0x%08X\n", returnCode);
					return ERequestStateResult::Abort;
				}

				return ERequestStateResult::Done;
			}

			ERequestStateResult CPlugin::OnReceiveHeaders(SSceHttpRequest& request)
			{
				char *pHeaders = nullptr;
				size_t headerSize;

				auto returnCode = sceHttpGetAllResponseHeaders(request.requestId, &pHeaders, &headerSize);
				if (returnCode < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceHttpSendRequest() error: 0x%08X\n", returnCode);
					return ERequestStateResult::Abort;
				}

				request.sResponseHeaders = string(pHeaders, headerSize);
				return ERequestStateResult::Done;
			}

			CRYREGISTER_SINGLETON_CLASS(CPlugin);
		}
	}
}