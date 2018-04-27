// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "IHttpPlugin.h"

#include <CrySystem/ITimer.h>

#ifdef CRY_PLATFORM_WINDOWS
#include <winsock2.h>
#include <CryCore/Platform/CryWindows.h>

#ifdef DELETE
#undef DELETE
#endif
#endif

#include <curl/curl.h>

namespace Cry
{
	namespace Http
	{
		namespace cURL
		{
			class CPlugin final : public Cry::Http::IPlugin
			{
				CRYINTERFACE_BEGIN()
					CRYINTERFACE_ADD(Cry::Http::IPlugin)
					CRYINTERFACE_ADD(Cry::IEnginePlugin)
				CRYINTERFACE_END()

				CRYGENERATE_SINGLETONCLASS_GUID(CPlugin, "Plugin_HTTP", "{D6F141B9-CDD8-44AB-ACF5-9C70EBC646CB}"_cry_guid)

				CPlugin() = default;
				virtual ~CPlugin();

			public:
				// ICryPlugin
				virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;
				virtual void MainUpdate(float frameTime) override;
				// ~ICryPlugin

				// IPlugin_HTTP
				virtual EUpdateResult ProcessRequests() final;
				virtual bool Send(ERequestType requestType, const char* szURL, const char* szBody, TResponseCallback resultCallback, const THeaders& headers) final;
				// ~IPlugin_HTTP

			protected:
				static size_t CurlWriteResponse(char *ptr, size_t size, size_t nmemb, void *userData);
				static int CurlProgress(void *userData, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

			protected:
				struct SRequest
				{
					SRequest(CURL* pRequestHandle, TResponseCallback callback)
						: pHandle(pRequestHandle)
						, resultCallback(callback)
						, pHeaderList(nullptr)
						, lastTimeHandledData(gEnv->pTimer->GetAsyncCurTime())
					{
					}

					SRequest(SRequest& request) = delete;
					SRequest& operator=(SRequest&) = delete;

					SRequest(SRequest&& request)
						: pHandle(request.pHandle)
						, resultCallback(request.resultCallback)
						, result(std::move(request.result))
						, pHeaderList(request.pHeaderList)
						, lastTimeHandledData(request.lastTimeHandledData)
					{
						request.pHandle = nullptr;
						request.pHeaderList = nullptr;
					}
					SRequest& operator=(SRequest&&) = default;

					~SRequest()
					{
						if (pHeaderList != nullptr)
						{
							curl_slist_free_all(pHeaderList);
						}

						curl_easy_cleanup(pHandle);
					}

					CURL* pHandle;
					curl_slist* pHeaderList;

					TResponseCallback resultCallback;
					string result;
					// Time at which we last handled data, used for timeouts
					float lastTimeHandledData;
				};

				CURLM* m_pMultiHandle = nullptr;
				std::unordered_map<CURL*, SRequest> m_requestMap;

				float m_requestTimeOutInSeconds = 20.f;
			};
		}
	}
}