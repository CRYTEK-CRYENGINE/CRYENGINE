// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryNetwork/ISimpleHttpServer.h>
#include "SimpleHttpServerListener.h"

#include <md5/md5.h>

//#include <sstream>

CSimpleHttpServerListener CSimpleHttpServerListener::s_singleton;

ISimpleHttpServer* CSimpleHttpServerListener::s_http_server = NULL;

CSimpleHttpServerListener& CSimpleHttpServerListener::GetSingleton(ISimpleHttpServer* http_server)
{
	s_http_server = http_server;

	return s_singleton;
}

CSimpleHttpServerListener& CSimpleHttpServerListener::GetSingleton()
{
	return s_singleton;
}

CSimpleHttpServerListener::CSimpleHttpServerListener() : m_state(eAS_Disconnected), m_connectionID(ISimpleHttpServer::NoConnectionID)
{
}

CSimpleHttpServerListener::~CSimpleHttpServerListener()
{

}

static ILINE void EscapeString(string& inout)
{
	static const char chars[] = { '&', '<', '>', '\"', '\'', '%' };
	static const char* repls[] = { "&amp;", "&lt;", "&gt;", "&quot;", "&apos;", "&#37;" };

	for (size_t i = 0; i < sizeof(chars) / sizeof(char); ++i)
	{
		size_t off = 0;
		while (true)
		{
			off = inout.find(chars[i], off);
			if (off == string::npos)
				break;
			inout.replace(off, sizeof(char), repls[i]);
			off += strlen(repls[i]);
		}
	}
}

static const char* hexchars = "0123456789abcdef";

static ILINE string ToHexStr(const char* x, int len)
{
	string out;
	for (int i = 0; i < len; i++)
	{
		uint8 c = x[i];
		out += hexchars[c >> 4];
		out += hexchars[c & 0xf];
	}
	return out;
}

void CSimpleHttpServerListener::Update()
{
	if (m_commands.empty())
		return;

	string reason;

	switch (m_state)
	{
	case eAS_Disconnected:
		m_commands.resize(0);
		return;

	case eAS_WaitChallengeRequest:
		{
			int start = 0;
			string command = m_commands.front().Tokenize(" ", start);
			if (command != "challenge") // challenge request must be a single method name
			{
				reason = "Illegal Command";
				goto L_fail;
			}

			m_challenge.Format("%f", gEnv->pTimer->GetAsyncTime().GetSeconds());
			s_http_server->SendResponse(m_connectionID, ISimpleHttpServer::eSC_Okay, ISimpleHttpServer::eCT_XML,
			                            string().Format("<?xml version=\"1.0\"?><methodResponse><params><param><value><string>%s</string></value></param></params></methodResponse>", m_challenge.c_str()), false);
			m_state = eAS_WaitAuthenticationRequest;
			m_commands.pop_front();
			if (m_commands.empty())
				return;
			// otherwise, fall through
		}

	case eAS_WaitAuthenticationRequest:
		{
			int start = 0;
			string command = m_commands.front().Tokenize(" ", start);
			if (command != "authenticate") // authenticate request must be with an MD5 hash
			{
				reason = "Illegal Command";
				goto L_fail;
			}

			string digest = m_commands.front().Tokenize(" ", start);

			ICVar* cvar = gEnv->pConsole->GetCVar("http_password"); // registered in CryAction::InitCVars()
			string password = cvar->GetString();
			string composed = m_challenge + ":" + password;

			char md5[16];
			MD5Context context;
			MD5Init(&context);
			MD5Update(&context, (unsigned char*)composed.data(), composed.size());
			MD5Final((unsigned char*)md5, &context);

			if (digest != ToHexStr(md5, 16))
			{
				reason = "Authorization Failed";
				goto L_fail;
			}

			s_http_server->SendResponse(m_connectionID, ISimpleHttpServer::eSC_Okay, ISimpleHttpServer::eCT_XML,
			                            string().Format("<?xml version=\"1.0\"?><methodResponse><params><param><value><string>%s</string></value></param></params></methodResponse>", "authorized"), false);
			m_state = eAS_Authorized;
			m_commands.pop_front();
			if (m_commands.empty())
				return;
			// otherwise, fall through
		}

	case eAS_Authorized:
		break;
	}

	for (size_t i = 0; i < m_commands.size(); ++i)
	{
		gEnv->pConsole->AddOutputPrintSink(this);
#if defined(CVARS_WHITELIST)
		ICVarsWhitelist* pCVarsWhitelist = gEnv->pSystem->GetCVarsWhiteList();
		bool execute = (pCVarsWhitelist) ? pCVarsWhitelist->IsWhiteListed(m_commands[i].c_str(), false) : true;
		if (execute)
#endif // defined(CVARS_WHITELIST)
		{
			gEnv->pConsole->ExecuteString(m_commands[i].c_str());
		}
		gEnv->pConsole->RemoveOutputPrintSink(this);

		EscapeString(m_output);

		s_http_server->SendResponse(m_connectionID, ISimpleHttpServer::eSC_Okay, ISimpleHttpServer::eCT_XML,
		                            string().Format("<?xml version=\"1.0\"?><methodResponse><params><param><value><string>%s</string></value></param></params></methodResponse>", m_output.c_str()), false);
		m_output.resize(0);
	}

	m_commands.resize(0);
	return;

L_fail:
	s_http_server->SendResponse(m_connectionID, ISimpleHttpServer::eSC_Okay, ISimpleHttpServer::eCT_XML,
	                            string().Format("<?xml version=\"1.0\"?><methodResponse><params><param><value><string>%s</string></value></param></params></methodResponse>", reason.c_str()), true);
	m_state = eAS_Disconnected;
	m_commands.resize(0);
	m_client.resize(0);
	return;
}

void CSimpleHttpServerListener::Print(const char* inszText)
{
	m_output += string().Format("%s\n", inszText);
}

void CSimpleHttpServerListener::OnStartResult(bool started, EResultDesc desc)
{
	if (started)
		CryLogAlways("HTTP: server successfully started");
	else
	{
		string sdesc;
		switch (desc)
		{
		case eRD_Failed:
			sdesc = "failed starting server";
			break;

		case eRD_AlreadyStarted:
			sdesc = "server already started";
			break;
		}
		CryLogAlways("HTTP: %s", sdesc.c_str());

		gEnv->pConsole->ExecuteString("http_stopserver");
	}
}

void CSimpleHttpServerListener::OnClientConnected(int connectionID, string client)
{
	// only support one connected client
	if (m_state != eAS_Disconnected)
	{
		s_http_server->SendResponse(connectionID, ISimpleHttpServer::eSC_ServiceUnavailable, ISimpleHttpServer::eCT_HTML, "<HTML><HEAD><TITLE>Service Unavailable</TITLE></HEAD></HTML>", true);
		return;
	}

	m_client = client;
	m_challenge.resize(0);
	m_state = eAS_WaitChallengeRequest;
	m_connectionID = connectionID;
	CryLogAlways("HTTP: accepted client connection from %s", client.c_str());
}

void CSimpleHttpServerListener::OnClientDisconnected(int connectionID)
{
	CryLogAlways("HTTP: client from %s is gone", m_client.c_str());
	m_client.resize(0);
	m_state = eAS_Disconnected;
	m_connectionID = ISimpleHttpServer::NoConnectionID;
}

void CSimpleHttpServerListener::OnGetRequest(int connectionID, string url)
{
	CryLog("HTTP: client from %s is making a GET request: %s", m_client.c_str(), url.c_str());

#define HTTP_ROOT "/Libs/CryHttp"
#define HTTP_FILE "/index.mhtml"

	if (url != HTTP_FILE)
	{
		s_http_server->SendResponse(m_connectionID, ISimpleHttpServer::eSC_BadRequest, ISimpleHttpServer::eCT_HTML, "<HTML><HEAD><TITLE>Bad Request</TITLE></HEAD></HTML>", true);
		return;
	}

	string page;
	FILE* file = fopen(PathUtil::GetGameFolder() + HTTP_ROOT + HTTP_FILE, "rb");
	if (file)
	{
		if (0 == fseek(file, 0, SEEK_END))
		{
			long size = ftell(file);
			if (size > 0)
			{
				page.resize(size, '0');
				fseek(file, 0, SEEK_SET);
				fread((char*)page.data(), 1, page.size(), file);
			}
		}
		fclose(file);
	}

	if (page.empty())
	{
		s_http_server->SendResponse(m_connectionID, ISimpleHttpServer::eSC_NotFound, ISimpleHttpServer::eCT_HTML, "<HTML><HEAD><TITLE>Not Found</TITLE></HEAD></HTML>", true);
		return;
	}

	size_t index = page.find("$time$");
	//std::stringstream ss; ss << gEnv->pTimer->GetAsyncCurTime();
	char timestamp[33];
	cry_sprintf(timestamp, "%f", gEnv->pTimer->GetAsyncCurTime());
	page.replace(index, strlen("$time$"), timestamp);
	s_http_server->SendWebpage(m_connectionID, page);
}

void CSimpleHttpServerListener::OnRpcRequest(int connectionID, string xml)
{
	CryLog("HTTP: client from %s is making an RPC request: %s", m_client.c_str(), xml.c_str());

	string command;
	XmlNodeRef root = GetISystem()->LoadXmlFromBuffer(xml.c_str(), xml.length());
	if (root && root->isTag("methodCall"))
	{
		if (root->getChildCount() < 1)
		{
			goto L_error;
		}
		XmlNodeRef methodName = root->getChild(0);
		if (methodName && methodName->isTag("methodName"))
		{
			command = methodName->getContent(); // command name must go first
			if (root->getChildCount() < 2)
			{
				goto L_error;
			}
			XmlNodeRef params = root->getChild(1);
			if (params)
			{
				if (params->isTag("params"))
				{
					int nparams = params->getChildCount();
					for (int i = 0; i < nparams; ++i)
					{
						XmlNodeRef param = params->getChild(i);
						XmlNodeRef value = param->getChild(0);
						if (value && value->isTag("value"))
						{
							// we treat all types as strings
							string svalue = " ";
							if (value->getChildCount() > 0)
							{
								XmlNodeRef type = value->getChild(0);
								if (type)
									svalue += type->getContent();
							}
							else
								svalue += value->getContent();
							command += svalue;
						}
						else
							goto L_error;
					}
				}
				else
					goto L_error;
			}
			// a command without params is allowed

			// execute the command on the server
			CryLogAlways("HTTP: received xmlrpc command: %s", command.c_str());
			m_commands.push_back(command);

			return;
		}
	}

L_error:
	s_http_server->SendResponse(m_connectionID, ISimpleHttpServer::eSC_BadRequest, ISimpleHttpServer::eCT_HTML, "<HTML><HEAD><TITLE>Bad Request</TITLE></HEAD></HTML>", true);
}
