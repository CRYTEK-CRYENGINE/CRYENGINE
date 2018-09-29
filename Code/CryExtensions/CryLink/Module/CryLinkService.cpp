// Copyright 2001-2015 Crytek GmbH. All rights reserved.

#include "StdAfx.h"

#include "CryLinkService.h"

#include "CryLinkCVars.h"
#include "CryLink/CryLink.h"
#include "Utils/HttpRPC.h"

#include <CrySystem/ISystem.h>
#include <CrySystem/ILog.h>
#include <CrySystem/ZLib/IZLibCompressor.h>

namespace CryLinkService 
{
	static string ToHexString(const char* szBuffer, int32 length)
	{
		static const char *szHexChars = "0123456789abcdef";

		string out;
		for (int32 i = 0; i < length; ++i)
		{
			uint8 c = szBuffer[i];
			out += szHexChars[c >> 4];
			out += szHexChars[c & 0xf];
		}
		return out;
	}

	inline void GetMD5(const char* pSrc, int size, char signature[16]) 
	{
		CRY_ASSERT(pSrc);
		if(pSrc)
		{
			IZLibCompressor& compressor = *GetISystem()->GetIZLibCompressor();
			SMD5Context      context;
			compressor.MD5Init(&context);
			compressor.MD5Update(&context, pSrc, size); 
			compressor.MD5Final(&context, signature);
		}
	}

	CCryLinkService::CCryLinkService()
		: m_pSystem(gEnv->pSystem)
		, m_pHttpServer(nullptr)
		, m_serviceState(EServiceState::NotStarted)
	{

	}

	CCryLinkService::~CCryLinkService()
	{
		Stop();
	}

	bool CCryLinkService::Start(ISimpleHttpServer& httpServer)
	{
		if(m_pSystem == nullptr)
		{
			LogWarning("Failed to start listener. Reason: Invalid pointer to 'ISystem'.");
			return false;
		}

		if(m_serviceState == EServiceState::Running)
		{
			LogWarning("Is already running. Please stop it first and try again.");
			return false;
		}

		if(m_serviceState == EServiceState::Paused)
		{
			LogWarning("Is already running but paused.");
			return false;
		}

		m_pHttpServer = &httpServer;
		m_serviceState = EServiceState::Running;

		CVars& cvars = CVars::GetInstance();

		const int32 port = gEnv->IsEditor() ? cvars.cryLinkEditorServicePort : cvars.cryLinkGameServicePort + m_pSystem->GetApplicationInstance();
		if(port < 1024 || port > 32767)
		{
			LogWarning("Invalid port on port '%d'. Must be between 1024-32767.", port);
			return false;
		}

		if(cvars.cryLinkServicePassword)
		{
			m_password = cvars.cryLinkServicePassword->GetString();
		}
		else
		{
			LogWarning("No password set.");
		}

		if(cvars.cryLinkAllowedClient == nullptr)
		{
			LogWarning("CVar 'cryLinkAllowedClient' is not set. Connection is not restricted to a specific client.");
		}

		m_pHttpServer->Start(static_cast<uint16>(port), m_password.c_str(), this);
		LogMessage("Listener started on port '%d'.", port);

		return true;
	}

	void CCryLinkService::Stop()
	{
		if(m_serviceState == EServiceState::Running)
		{
			m_pHttpServer->Stop();
			m_serviceState = EServiceState::NotStarted;
		}
	}

	void CCryLinkService::Pause(bool bPause)
	{
		if(bPause)
		{
			if(m_serviceState == EServiceState::Running)
			{
				m_serviceState = EServiceState::Paused;
			}
		}
		else if(m_serviceState == EServiceState::Paused)
		{
			m_serviceState = EServiceState::Running;
		}
	}

	void CCryLinkService::Update()
	{
		if(m_serviceState == EServiceState::NotStarted || m_commandQueue.empty())
		{
			return;
		}

		if(m_serviceState == EServiceState::Paused)
		{
			// TODO: Implement proper handling but for now just return 
			//			 a service unavailable and disconnect.
			for(const ConnectionsById::value_type& connectionById : m_connections)
			{
				if(SConnectionInfo* pConnectionInfo = GetConnectionInfo(connectionById.first))
				{
					if(pConnectionInfo->state == EConnectionState::Authenticated)
					{
						SRPCResponse<bool> response(*m_pHttpServer, pConnectionInfo->connectionId);
						response.statusCode = ISimpleHttpServer::eSC_ServiceUnavailable;
						response.bKeepAlive = false;
						response.Send();

						pConnectionInfo->state = EConnectionState::Rejected;
					}
				}
			}

			m_commandQueue.clear();
			return;
		}

		if(m_connections.empty())
		{
			m_commandQueue.clear();
			return;
		}

		for(const SCommandInfo& commandInfo : m_commandQueue)
		{
			if(SConnectionInfo* pConnectionInfo = GetConnectionInfo(commandInfo.connectionId))
			{
				switch(pConnectionInfo->state)
				{
				case EConnectionState::Connected:
						SendChallenge(commandInfo.command, *pConnectionInfo);
					break;

				case EConnectionState::ChallengeSent:
						Authenticate(commandInfo.command, *pConnectionInfo);
					break;

				case EConnectionState::Authenticated:
						ExecuteCommand(commandInfo.command, *pConnectionInfo);
					break;

				default:
					{
						SRPCResponse<bool> response(*m_pHttpServer, commandInfo.connectionId);
						response.statusCode = ISimpleHttpServer::eSC_InvalidStatus;
						response.bKeepAlive = false;
						response.Send();
						return;
					}
				}
			}
		}

		m_commandQueue.clear();
	}

	bool CCryLinkService::SendChallenge(const string& command, SConnectionInfo& connectionInfo)
	{
		SRPCResponse<int64> response(*m_pHttpServer, connectionInfo.connectionId);

		CVars& cvars = CVars::GetInstance();
		if(cvars.cryLinkAllowedClient == nullptr || connectionInfo.clientName.compareNoCase(0, connectionInfo.clientName.length(), connectionInfo.clientName) == 0)
		{
			if(command == "challenge")
			{
				const int64 time = GetGTimer()->GetAsyncTime().GetValue();
				connectionInfo.challange.Format("%d", time);

				response.response = time;
				response.Send();

				connectionInfo.state = EConnectionState::ChallengeSent;
				return true;
			}

			response.statusCode = ISimpleHttpServer::eSC_InvalidStatus;
		}
		else
		{
			response.statusCode = ISimpleHttpServer::eSC_ServiceUnavailable;
		}

		response.response = 0;
		response.bKeepAlive = false;
		response.Send();

		connectionInfo.state = EConnectionState::Rejected;

		return false;
	}

	bool CCryLinkService::Authenticate(const string& command, SConnectionInfo& connectionInfo)
	{
		int32 tokenPos = 0;
		if(command.Tokenize(" ", tokenPos) == "authenticate")
		{
			stack_string auth = command.Tokenize(" ", tokenPos);

			CVars& cvars = CVars::GetInstance();
			stack_string composed = connectionInfo.challange + ':' + m_password;

			const int32 md5Length = 16;
			char md5[md5Length] = {0};

			GetMD5(composed.data(), composed.size(), md5);
			if (auth.c_str() == ToHexString(md5, md5Length))
			{
				SRPCResponse<bool> response(*m_pHttpServer, connectionInfo.connectionId);
				response.response = true;
				response.Send();

				connectionInfo.state = EConnectionState::Authenticated;
				return true;
			}
			else
			{
				LogWarning("Authentication failed. Reason: Wrong password or challenge.");
			}
		}

		SRPCResponse<bool> response(*m_pHttpServer, connectionInfo.connectionId);
		response.statusCode = ISimpleHttpServer::eSC_InvalidStatus;
		response.bKeepAlive = false;
		response.Send();

		connectionInfo.state = EConnectionState::Rejected;
		connectionInfo.challange.clear();

		return false;
	}

	bool CCryLinkService::ExecuteCommand(const string& command, SConnectionInfo& connectionInfo)
	{
		if(!command.empty())
		{
			gEnv->pConsole->AddOutputPrintSink(this);
			gEnv->pConsole->ExecuteString(command.c_str());
			gEnv->pConsole->RemoveOutputPrintSink(this);

			SRPCResponse<string> response(*m_pHttpServer, connectionInfo.connectionId);
			response.response = m_consoleOutput;
			response.Send();

			m_consoleOutput.clear();
			
			return true;
		}

		return false;
	}

	void CCryLinkService::OnClientConnected(int connectionID, string client)
	{
		if(!IsClientConnected(connectionID))
		{
			LogMessage("Accepted connection to client '%s'", client.c_str());
			m_connections.emplace(connectionID, SConnectionInfo(client, connectionID));
		}
	}

	void CCryLinkService::OnClientDisconnected(int connectionID)
	{
		if(SConnectionInfo* pConnectionInfo = GetConnectionInfo(connectionID))
		{
			LogMessage("Disconnected client '%s'", pConnectionInfo->clientName.c_str());
			m_connections.erase(connectionID);
		}
	}

	void CCryLinkService::OnGetRequest(int connectionID, string url)
	{
		SRPCResponse<bool> response(*m_pHttpServer, connectionID);
		response.statusCode = ISimpleHttpServer::eSC_NotImplemented;
		response.bKeepAlive = false;
		response.Send();
	}

	void CCryLinkService::OnRpcRequest(int connectionID, string xml)
	{
		if(m_serviceState == EServiceState::Running)
		{
			SConnectionInfo* pConnectionInfo = GetConnectionInfo(connectionID);
			if(pConnectionInfo == nullptr)
			{
				LogWarning("Received RPC request from unregistered connection!");
				return;
			}

			LogMessage("Received RPC request from client '%s'.", pConnectionInfo->clientName.c_str(), xml.c_str());

			// Expected format:
			// -------------------------------------------------------------------------------
			//	<?xml version="1.0"?>
			//		<methodCall>
			//			<methodName>aMethodName</methodName>
			//			<params>
			//				<param>
			//					<value><string>parameterValue</string></value>
			//					<value><string>parameterValue</string></value>
			//				</param>
			//			</params>
			//		</methodCall>

			stack_string command;

			XmlNodeRef root = GetISystem()->LoadXmlFromBuffer(xml.c_str(), xml.length());
			if (root && root->isTag("methodCall"))
			{
				if (root->getChildCount() >= 0)
				{
					XmlNodeRef methodName = root->getChild(0);
					if (methodName && methodName->isTag("methodName"))
					{
						command = methodName->getContent();
					}

					XmlNodeRef params = root->getChild(1);
					if (params && params->isTag("params"))
					{
						for(int32 i = 0; i < params->getChildCount(); ++i)
						{
							XmlNodeRef param = params->getChild(i);
							XmlNodeRef value = param->getChild(0);
							if(value && value->isTag("value"))
							{
								// TODO: A proper RPC implementation requires
								//			 to work with data types instead of strings.
								XmlNodeRef datatype = value->getChild(0);
								if(datatype)
								{
									command.append(" ");
									command.append(datatype->getContent());
								}
							}
						}
					}

					m_commandQueue.push_back(SCommandInfo(command.c_str(), connectionID));
				}
				return;
			}
		}
		else
		{
			SRPCResponse<bool> response(*m_pHttpServer, connectionID);
			response.statusCode = ISimpleHttpServer::eSC_BadRequest;
			response.bKeepAlive = false;
			response.Send();
		}
	}

	void CCryLinkService::Print(const char *szText)
	{
		m_consoleOutput += string().Format("%s\n", szText);
	}

	bool CCryLinkService::IsClientConnected(int32 connectionId) const
	{
		return (m_connections.find(connectionId) != m_connections.end());
	}

	CCryLinkService::SConnectionInfo* CCryLinkService::GetConnectionInfo(int32 connectionId)
	{
		ConnectionsById::iterator result = m_connections.find(connectionId);
		if(result != m_connections.end())
		{
			return &result->second;
		}
		return nullptr;
	}

	void CCryLinkService::LogMessage(const char* szFormat, ...)
	{
		const size_t bufferLength = 1024;
		const size_t bufferInitLength = sizeof("[CryLink Service] ");
		char szBuffer[bufferLength] = "[CryLink Service] ";

		va_list	va_args;
		va_start(va_args, szFormat);
		vsnprintf(&szBuffer[bufferInitLength-1], bufferLength - bufferInitLength, szFormat, va_args);
		va_end(va_args);

		CryLog(szBuffer);
	}

	void CCryLinkService::LogWarning(const char* szFormat, ...)
	{
		const size_t bufferLength = 1024;
		const size_t bufferInitLength = sizeof("[CryLink Service] ");
		char szBuffer[bufferLength] = "[CryLink Service] ";

		va_list	va_args;
		va_start(va_args, szFormat);
		vsnprintf(&szBuffer[bufferInitLength-1], bufferLength - bufferInitLength, szFormat, va_args);
		va_end(va_args);

		CryLogAlways(szBuffer);
	}
}