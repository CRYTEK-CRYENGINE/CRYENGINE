#pragma once

#include "IHttpPlugin.h"

#include <scetypes.h>
#include <net.h>
#include <libssl.h>
#include <libhttp.h>

#include <queue>

namespace Cry
{
	namespace Http
	{
		namespace Sce
		{
#define MAX_FRAME_MESSAGE_SIZE 4096

			enum class ERequestState
			{
				Sending,
				GetStatus,
				GetHeaders,
				GetLength,
				Receive,
				Done,

				Count
			};

			enum class ERequestStateResult
			{
				Waiting,
				Abort,
				Done
			};

			struct SSceHttpRequest
			{
				~SSceHttpRequest()
				{
					if (requestId > 0)
					{
						auto returnCode = sceHttpAbortRequest(requestId);
						if (returnCode < 0)
						{
							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceHttpAbortRequest() error: 0x%08X\n", returnCode);
							return;
						}

						returnCode = sceHttpDeleteRequest(requestId);
						requestId = 0;

						if (returnCode < 0)
						{
							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceHttpDeleteRequest() error: 0x%08X\n", returnCode);
						}
					}

					if (connectionId > 0)
					{
						auto returnCode = sceHttpDeleteConnection(connectionId);
						connectionId = 0;

						if (returnCode < 0)
						{
							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceHttpDeleteConnection() error: 0x%08X\n", returnCode);
						}
					}

					if (templateId > 0)
					{
						auto returnCode = sceHttpDeleteTemplate(templateId);
						templateId = 0;

						if (returnCode < 0)
						{
							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[SceHttp] sceHttpDeleteTemplate() error: 0x%08X\n", returnCode);
						}
					}
				}

				int requestId = -1;
				int connectionId = -1;
				int templateId = -1;
				int replyStatusCode = 0;

				SceHttpEpollHandle pollingHandle;
				
				ERequestState state = ERequestState::Sending;
				SceHttpMethods requestType;

				string sContent;

				string sResponseHeaders;
				string sResponse;

				IPlugin::TResponseCallback resultCallback;
			};

			class CPlugin final : public IPlugin
			{
				CRYINTERFACE_BEGIN()
					CRYINTERFACE_ADD(IPlugin)
					CRYINTERFACE_ADD(Cry::IEnginePlugin)
				CRYINTERFACE_END()

				CRYGENERATE_SINGLETONCLASS_GUID(CPlugin, "Plugin_HTTP", "{F2903D50-8D39-4C42-8D31-3D8F280D893C}"_cry_guid)

			public:
				virtual ~CPlugin();

				// ICryPlugin
				virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;
				virtual void MainUpdate(float frameTime) override;
				// ~ICryPlugin

				// IPlugin_HTTP
				virtual EUpdateResult ProcessRequests() final;
				virtual bool Send(ERequestType requestType, const char* szURL, const char* szBody, TResponseCallback resultCallback, const THeaders& headers) final;
				// ~IPlugin_HTTP

			private:
				ERequestStateResult OnReceiveResponse(SSceHttpRequest& request);
				ERequestStateResult OnReceiveHeaders(SSceHttpRequest& request);
				ERequestStateResult OnSendRequest(SSceHttpRequest& request);
				ERequestStateResult OnGetStatus(SSceHttpRequest& request);
				ERequestStateResult OnGetResponseLength(SSceHttpRequest& request);
				ERequestStateResult OnRequestComplete(SSceHttpRequest& request);

				bool WaitForRequest(SSceHttpRequest& request);

			private:
				int32_t m_iNetLibId;
				int32_t m_iSSLLibId;
				int32_t m_iHTTPLibId;

				std::vector<std::shared_ptr<SSceHttpRequest>> m_requests;
			};
		}
	}
}