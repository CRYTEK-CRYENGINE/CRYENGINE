// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HttpImplementation_cURL.h"

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

namespace Cry
{
	namespace Http
	{
		namespace cURL
		{
			CPlugin::~CPlugin()
			{
				if (m_pMultiHandle != nullptr)
				{
					curl_multi_cleanup(m_pMultiHandle);
				}

				gEnv->pConsole->UnregisterVariable("http_timeout");
			}

			bool CPlugin::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
			{
				REGISTER_CVAR2("http_timeout", &m_requestTimeOutInSeconds, m_requestTimeOutInSeconds, VF_NULL, "Timeout in seconds for HTTP requests");

				m_pMultiHandle = curl_multi_init();

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
				} while (result == EUpdateResult::ProcessingRequests && curl_multi_timeout(m_pMultiHandle, &maximumWaitInMs) == CURLM_OK && maximumWaitInMs < gEnv->pTimer->GetFrameTime() * 1000.f);
			}

			bool CPlugin::Send(ERequestType requestType, const char* szURL, const char* szBody, TResponseCallback resultCallback, const THeaders& headers)
			{
				if (m_requestMap.empty())
				{
					EnableUpdate(EUpdateStep::MainUpdate, true);
				}

				CURL* pRequest = curl_easy_init();
				SRequest& request = m_requestMap.emplace(pRequest, SRequest(pRequest, resultCallback)).first->second;

				curl_easy_setopt(request.pHandle, CURLOPT_URL, szURL);
				curl_easy_setopt(request.pHandle, CURLOPT_POST, (requestType == ERequestType::POST || requestType == ERequestType::DELETE) ? 1 : 0);

				switch (requestType)
				{
					case ERequestType::DELETE:
						curl_easy_setopt(request.pHandle, CURLOPT_CUSTOMREQUEST, "DELETE");
						break;
					case ERequestType::PATCH:
						curl_easy_setopt(request.pHandle, CURLOPT_CUSTOMREQUEST, "PATCH");
						break;
					case ERequestType::PUT:
						curl_easy_setopt(request.pHandle, CURLOPT_CUSTOMREQUEST, "PUT");
						break;
					default:
						curl_easy_setopt(request.pHandle, CURLOPT_CUSTOMREQUEST, nullptr);
				}

				curl_easy_setopt(request.pHandle, CURLOPT_WRITEFUNCTION, CurlWriteResponse);
				curl_easy_setopt(request.pHandle, CURLOPT_WRITEDATA, pRequest);
				curl_easy_setopt(request.pHandle, CURLOPT_XFERINFOFUNCTION, CurlProgress);
				curl_easy_setopt(request.pHandle, CURLOPT_XFERINFODATA, pRequest);
				curl_easy_setopt(request.pHandle, CURLOPT_NOPROGRESS, 0L);

				if (szBody != nullptr)
				{
					curl_easy_setopt(request.pHandle, CURLOPT_POSTFIELDS, szBody);
				}

				if (headers.size() > 0)
				{
					for (const std::pair<const char*, const char*>& headerPair : headers)
					{
						string header = string().Format("%s: %s", headerPair.first, headerPair.second);

						request.pHeaderList = curl_slist_append(request.pHeaderList, header.c_str());
					}

					curl_easy_setopt(request.pHandle, CURLOPT_HTTPHEADER, request.pHeaderList);
				}

				curl_multi_add_handle(m_pMultiHandle, request.pHandle);
				return true;
			}

			CPlugin::EUpdateResult CPlugin::ProcessRequests()
			{
				if (m_requestMap.empty())
				{
					return EUpdateResult::Idle;
				}

				int numRemainingRequests = 1;
				curl_multi_perform(m_pMultiHandle, &numRemainingRequests);

				// Now check the status of our requests
				int numRemainingMessages;
				while (CURLMsg* pMessage = curl_multi_info_read(m_pMultiHandle, &numRemainingMessages))
				{
					if (pMessage->msg == CURLMSG_DONE)
					{
						auto it = m_requestMap.find(pMessage->easy_handle);
						if (it != m_requestMap.end())
						{
							SRequest request = std::move(it->second);
							m_requestMap.erase(it);

							if (m_requestMap.empty())
							{
								EnableUpdate(EUpdateStep::MainUpdate, false);
							}

							if (request.resultCallback)
							{
								long responseCode;
								curl_easy_getinfo(request.pHandle, CURLINFO_RESPONSE_CODE, &responseCode);

								request.resultCallback(request.result, responseCode);
							}
						}
					}
				}


				// Clean up completed requests, cannot be done inside curl_multi_perform
				for (auto it = m_requestMap.begin(), end = m_requestMap.end(); it != end;)
				{
					// Check for timeout
					if (gEnv->pTimer->GetAsyncCurTime() - it->second.lastTimeHandledData > m_requestTimeOutInSeconds)
					{
						SRequest request = std::move(it->second);
						it = m_requestMap.erase(it);

						if (m_requestMap.empty())
						{
							EnableUpdate(EUpdateStep::MainUpdate, false);
						}

						if (request.resultCallback)
						{
							request.resultCallback(request.result, 503);
						}
					}
					else
					{
						++it;
					}
				}

				return numRemainingRequests != 0 ? EUpdateResult::ProcessingRequests : EUpdateResult::Idle;
			}

			/* static */ size_t CPlugin::CurlWriteResponse(char *ptr, size_t size, size_t nmemb, void *userData)
			{
				size_t readByteCount = size * nmemb;

				// Query the HTTP implementation, note that the name is unclear - nothing is created as the HTTP implementation is a singleton.
				std::shared_ptr<CPlugin> pHttpImplementation = CPlugin::CreateClassInstance();

				auto requestIt = pHttpImplementation->m_requestMap.find(static_cast<CURL*>(userData));;
				CRY_ASSERT(requestIt != pHttpImplementation->m_requestMap.end());
				if (requestIt != pHttpImplementation->m_requestMap.end())
				{
					// Append read data to result
					requestIt->second.result += string(ptr, readByteCount);
				}

				return readByteCount;
			}

			/* static */ int CPlugin::CurlProgress(void *userData, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
			{
				// Query the HTTP implementation, note that the name is unclear - nothing is created as the HTTP implementation is a singleton.
				std::shared_ptr<CPlugin> pHttpImplementation = CPlugin::CreateClassInstance();

				auto requestIt = pHttpImplementation->m_requestMap.find(static_cast<CURL*>(userData));
				if (requestIt != pHttpImplementation->m_requestMap.end())
				{
					requestIt->second.lastTimeHandledData = gEnv->pTimer->GetAsyncCurTime();
				}



				return CURLE_OK;
			}

			CRYREGISTER_SINGLETON_CLASS(CPlugin)
		}
	}
}

#include <CryCore/CrtDebugStats.h>
