// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "UserAnalytics.h"

#if !defined(_RELEASE) && CRY_PLATFORM_WINDOWS
	#include <CrySystem/ISystem.h>
	#include <CrySystem/IConsole.h>
	#include <CryThreading/IThreadManager.h>
	#include <CryExtension/CryGUID.h>
	#include <CrySystem/CryVersion.h>
	#include <CrySerialization/IArchiveHost.h>
	#include <CryString/CryPath.h>

	#include <winsock2.h>
	#include <curl/curl.h>
	#include <shlobj.h>

	#define UA_FULL_SERVER_URL "https://www.cryengine.com/useranalytics/" // slash at the end to avoid redirect

ICVar* CUserAnalytics::m_userAnalyticsServerAddress;

///////////////////////////////////////////////////////////////////////////
class CUserAnalyticsSendThread : public IThread
{
public:
	CUserAnalyticsSendThread(CUserAnalytics* pUserAnalytics)
		: m_pUserAnalytics(pUserAnalytics)
		, bRun(false)
	{}

	void ThreadEntry()
	{
		bRun = true;

		while (bRun)
		{
			CryAutoLock<CryMutexFast> lock(m_shutdownMutex);
			m_shutdownCond.TimedWait(m_shutdownMutex, 60 * 1000);

			m_pUserAnalytics->PrepareAndSendEvents();
		}
	}

	void SignalStopWork()
	{
		bRun = false;
		m_shutdownCond.Notify();
	}

private:
	CUserAnalytics*      m_pUserAnalytics;

	CryMutexFast         m_shutdownMutex;
	CryConditionVariable m_shutdownCond;

	volatile bool        bRun;
};

namespace UserAnalytics
{
///////////////////////////////////////////////////////////////////////////
static int DebugCurl(CURL* handle, curl_infotype type, char* data, size_t size, void* userp)
{
	CryLogAlways("[User Analytics DebugCurl] %s", data);
	return 0;
}

///////////////////////////////////////////////////////////////////////////
inline void BeginScope(string& message)
{
	message.append("{");
}

///////////////////////////////////////////////////////////////////////////
inline void EndScope(string& message)
{
	message.append("}");
}

///////////////////////////////////////////////////////////////////////////
inline void AddPairToMessage(string& message, const char* key, const char* value)
{
	message.append("\"");
	message.append(key);
	message.append("\":\"");
	message.append(value);
	message.append("\"");
}

///////////////////////////////////////////////////////////////////////////
inline void BeginAttributesScope(string& message)
{
	message.append("\"attributes\":");
	BeginScope(message);
}

///////////////////////////////////////////////////////////////////////////
inline void AddComma(string& message)
{
	message.append(",");
}
}

///////////////////////////////////////////////////////////////////////////
CUserAnalytics::CUserAnalytics()
	: m_curl(nullptr)
	, m_curlHeaderList(nullptr)
	, m_pUserAnalyticsSendThread(nullptr)
{
	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CUserAnalytics");

	TriggerEvent("UserAnalyticsSessionStart"); // note: this will not show up in log
}

///////////////////////////////////////////////////////////////////////////
CUserAnalytics::~CUserAnalytics()
{
	gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
}

void CUserAnalytics::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_CRYSYSTEM_INIT_DONE:
		{
			ReadWriteAnonymousToken();
			ReadUserIdFromDisk();

			m_userAnalyticsServerAddress = REGISTER_STRING("sys_UserAnalyticsServerAddress", UA_FULL_SERVER_URL, VF_NULL, "User Analytics Server address");

			InitializeCURL();

			m_pUserAnalyticsSendThread = new CUserAnalyticsSendThread(this);

			if (!gEnv->pThreadManager->SpawnThread(m_pUserAnalyticsSendThread, "UserAnalytics"))
			{
				CRY_ASSERT_MESSAGE(false, "Error spawning \"UserAnalytics\" thread.");
				delete m_pUserAnalyticsSendThread;
				m_pUserAnalyticsSendThread = nullptr;
			}

			{
				UserAnalytics::Attributes attributes;
				const SFileVersion& pBuildVersion = gEnv->pSystem->GetBuildVersion();
				char szBuildVersion[64];
				pBuildVersion.ToString(szBuildVersion);
				attributes.AddAttribute("version", szBuildVersion);

				bool bIsEditor = gEnv->IsEditor();
				attributes.AddAttribute("Sandbox", bIsEditor);

				USER_ANALYTICS_EVENT_ARG("UserAnalyticsSessionStart", &attributes); // note: this will not show up in log
			}

			break;
		}
	case ESYSTEM_EVENT_FAST_SHUTDOWN:
	case ESYSTEM_EVENT_FULL_SHUTDOWN:
		{
			Shutdown();
			break;
		}
	default:
		{
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////
void CUserAnalytics::Shutdown()
{
	// curl_easy_perform() can cause stalls when it gets called from
	// the destructor invoked by FreeLibrary. Hence we are making sure
	// to shut everything down before the dll gets unloaded.

	TriggerEvent("UserAnalyticsSessionEnd"); // note: this will not show up in log

	if (m_pUserAnalyticsSendThread)
	{
		m_pUserAnalyticsSendThread->SignalStopWork();
		gEnv->pThreadManager->JoinThread(m_pUserAnalyticsSendThread, eJM_Join);
		delete m_pUserAnalyticsSendThread;
	}

	ShutdownCURL();

	if (IConsole* const pConsole = gEnv->pConsole)
	{
		pConsole->UnregisterVariable("sys_UserAnalyticsServerAddress");
	}
}

///////////////////////////////////////////////////////////////////////////
string CUserAnalytics::GetTimestamp()
{
	SYSTEMTIME systemTime;
	GetLocalTime(&systemTime);

	TIME_ZONE_INFORMATION timeZone;
	GetTimeZoneInformation(&timeZone);
	int zoneHours = -(timeZone.Bias / 60) * 100;
	int zoneMinutes = -(timeZone.Bias % 60);
	int zoneOffset = zoneHours + zoneMinutes;

	return string().Format("%04i-%02i-%02iT%02i:%02i:%02i.%03i%+05i",
	                       systemTime.wYear, systemTime.wMonth, systemTime.wDay, systemTime.wHour,
	                       systemTime.wMinute, systemTime.wSecond, systemTime.wMilliseconds, zoneOffset);
}

///////////////////////////////////////////////////////////////////////////
string& CUserAnalytics::GetSessionId()
{
	static CryGUID sessionId = CryGUID::Create();
	static string sessionIdName = sessionId.ToString();

	return sessionIdName;
}

///////////////////////////////////////////////////////////////////////////
bool CUserAnalytics::InitializeCURL()
{
	if (m_curl)
	{
		return true;
	}

	curl_global_init(CURL_GLOBAL_ALL);

	m_curl = curl_easy_init();

	if (m_curl)
	{
		curl_easy_setopt(m_curl, CURLOPT_URL, m_userAnalyticsServerAddress ? m_userAnalyticsServerAddress->GetString() : UA_FULL_SERVER_URL);
		curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L);
		//curl_easy_setopt(m_curl, CURLOPT_DEBUGFUNCTION, UserAnalytics::DebugCurl);
		curl_easy_setopt(m_curl, CURLOPT_POST, 1L);

		m_curlHeaderList = curl_slist_append(m_curlHeaderList, "Content-Type: application/json");
		curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_curlHeaderList);
	}
	else
	{

		return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////
void CUserAnalytics::ShutdownCURL()
{
	if (m_curl)
	{
		curl_slist_free_all(m_curlHeaderList);
		curl_easy_cleanup(m_curl);
	}

	curl_global_cleanup();
}

///////////////////////////////////////////////////////////////////////////
void CUserAnalytics::TriggerEvent(const char* szEventName)
{
	TriggerEvent(szEventName, nullptr);
}

///////////////////////////////////////////////////////////////////////////
void CUserAnalytics::TriggerEvent(const char* szEventName, UserAnalytics::Attributes* attributes)
{
	string message = "";
	message.reserve(1024);

	UserAnalytics::BeginScope(message);
	UserAnalytics::AddPairToMessage(message, "name", szEventName);
	UserAnalytics::AddComma(message);

	UserAnalytics::AddPairToMessage(message, "localTimestamp", GetTimestamp().c_str());
	UserAnalytics::AddComma(message);

	UserAnalytics::BeginAttributesScope(message);

	if (!m_anonymousUserToken.empty())
	{
		UserAnalytics::AddPairToMessage(message, "AnonymousUserToken", m_anonymousUserToken.c_str());
		UserAnalytics::AddComma(message);
	}

	if (!m_userId.empty())
	{ 
		UserAnalytics::AddPairToMessage(message, "userId", m_userId.c_str());
		UserAnalytics::AddComma(message);
	}

	UserAnalytics::AddPairToMessage(message, "sessionId", GetSessionId().c_str());

	if (attributes != nullptr)
	{
		int count = attributes->m_keys.size();

		if (count > 0)
		{
			UserAnalytics::AddComma(message);
			UserAnalytics::AddPairToMessage(message, attributes->m_keys[0].c_str(), attributes->m_values[0].c_str());

			for (int i = 1; i < count; ++i)
			{
				UserAnalytics::AddComma(message);
				UserAnalytics::AddPairToMessage(message, attributes->m_keys[i].c_str(), attributes->m_values[i].c_str());
			}
		}
	}

	UserAnalytics::EndScope(message); // end attributes
	UserAnalytics::EndScope(message); // end message

	// Event message is now complete - push it to message buffer
	m_messages.push_back(message);
}

///////////////////////////////////////////////////////////////////////////
void CUserAnalytics::ReadWriteAnonymousToken()
{
	PWSTR pLocalDirectoryPath = nullptr;
	HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE | KF_FLAG_DONT_UNEXPAND, nullptr, &pLocalDirectoryPath);
	bool success = true;
	if (SUCCEEDED(hr))
	{
		// Convert from UNICODE to UTF-8
		char szLocalDirectoryPath[MAX_PATH];
		cry_strcpy(szLocalDirectoryPath, MAX_PATH, CryStringUtils::WStrToUTF8(pLocalDirectoryPath));

		string sFilePath = szLocalDirectoryPath;
		sFilePath.append("\\Crytek\\CRYENGINE\\");

		CryCreateDirectory(sFilePath.c_str());
		sFilePath.append("AnonymousToken");

		FILE* pFile = fopen(sFilePath, "r");
		if (!pFile)
		{
			// File does not exists, so let's create it
			pFile = fopen(sFilePath, "w");

			if (pFile)
			{
				// Store a random GUID in the file
				CryGUID guid = CryGUID::Create();
				m_anonymousUserToken = guid.ToString();

				fwrite(m_anonymousUserToken.c_str(), sizeof(char), m_anonymousUserToken.size(), pFile);
				fclose(pFile);
			}
			else
			{
				success = false;
			}
		}
		else
		{
			// File already exists, so let's read the token

			// Get file size
			fseek(pFile, 0, SEEK_END);
			int size = ftell(pFile);
			rewind(pFile);

			// Read token from file
			char guid[64];
			fread(guid, sizeof(char), size, pFile);
			fclose(pFile);
			guid[size] = '\0';

			m_anonymousUserToken = guid;
		}

		CoTaskMemFree(pLocalDirectoryPath);
	}
	else
	{
		success = false;
	}

	const ICVar* const cv_logging = gEnv->pConsole->GetCVar("sys_UserAnalyticsLogging");

	if (!success && cv_logging->GetIVal() > 0)
	{
		CryLogAlways("[User Analytics] Could not read or write anonymous user token");
	}
}

///////////////////////////////////////////////////////////////////////////
void CUserAnalytics::ReadUserIdFromDisk()
{
	PWSTR pLocalDirectoryPath = nullptr;
	HRESULT hr = SHGetKnownFolderPath(FOLDERID_Profile, KF_FLAG_CREATE | KF_FLAG_DONT_UNEXPAND, nullptr, &pLocalDirectoryPath);
	bool success = true;
	if (SUCCEEDED(hr))
	{
		// Convert from UNICODE to UTF-8
		char szLocalDirectoryPath[MAX_PATH];
		cry_strcpy(szLocalDirectoryPath, MAX_PATH, CryStringUtils::WStrToUTF8(pLocalDirectoryPath));

		string sFilePath = PathUtil::Make(szLocalDirectoryPath, ".cryengine/common.json");

		// read json content
		struct SLauncherInfo
		{
			uint64 userId;

			void Serialize(Serialization::IArchive& ar)
			{
				ar(userId, "userId", "UserId");
			}
		};

		SLauncherInfo launcherInfo;
		
		if (Serialization::LoadJsonFile(launcherInfo, sFilePath.c_str()))
		{
			m_userId = string().Format("%" PRIu64, launcherInfo.userId);
		}		

		CoTaskMemFree(pLocalDirectoryPath);
	}
}

///////////////////////////////////////////////////////////////////////////
void CUserAnalytics::PrepareAndSendEvents()
{
	static ICVar* const cv_collect = gEnv->pConsole->GetCVar("sys_UserAnalyticsCollect");
	if (cv_collect && cv_collect->GetIVal() == 0)
	{
		m_messages.clear();
		return;
	}

	static uint eventsCount = 0;

	if (m_sendBuffer.empty())
	{
		m_messagesCopy.clear();
		m_messages.swap(m_messagesCopy);
		eventsCount = m_messagesCopy.size();

		if (m_messagesCopy.empty())
		{
			return;
		}

		for (string& i : m_messagesCopy)
		{
			m_sendBuffer += ",";
			m_sendBuffer += i;
		}
		m_sendBuffer.replace(0, 1, "[");
		m_sendBuffer.append("]");
	}
	else
	{
		// something went wrong last time when trying to send messages to server. Please check log for warnings.
	}

	CURLcode res;

	if (InitializeCURL())
	{
		curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, m_sendBuffer);

		res = curl_easy_perform(m_curl);

		const ICVar* const cv_logging = gEnv->pConsole->GetCVar("sys_UserAnalyticsLogging");

		if (res == CURLE_OK)
		{
			m_sendBuffer.clear();

			if (cv_logging && cv_logging->GetIVal() > 0)
			{
				CryLogAlways("[User Analytics] Successfully pushed %d event(s) to %s", eventsCount, m_userAnalyticsServerAddress ? m_userAnalyticsServerAddress->GetString() : UA_FULL_SERVER_URL);

				CryLogAlways("[User Analytics] Queuing Event: SessionAlive");
			}

			eventsCount = 0;

			// pushing an alive-event to check client state on server side
			TriggerEvent("SessionAlive");
		}
		else
		{
			if (cv_logging && cv_logging->GetIVal() > 0)
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[User Analytics] Failed to send user events to server: %s, stopping to collect new events.", curl_easy_strerror(res));
			}

			// pushing a connection loss event in case the user reconnects again
			TriggerEvent("SessionIncomplete");

			if (cv_collect)
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[User Analytics] Setting CVar sys_UserAnalyticsCollect back to zero.");
				cv_collect->Set(0);
			}
		}
	}
}
#endif
