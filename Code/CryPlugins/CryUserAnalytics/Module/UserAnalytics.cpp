// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "UserAnalytics.h"

#if !defined(_RELEASE) && CRY_PLATFORM_WINDOWS
	#include <CrySystem/ISystem.h>
	#include <CrySystem/IConsole.h>
	#include <CryThreading/IThreadManager.h>
	#include <CryExtension/CryGUID.h>
	#include <CryExtension/CryGUIDHelper.h>

	#include <winsock2.h>
	#include <curl/curl.h>

	#define UA_FULL_SERVER_URL "https://www.cryengine.com/useranalytics/" // slash at the end to avoid redirect

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
	CryLogAlways("[User Analytics] %s", data);
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
{
	InitializeCURL();

	m_pUserAnalyticsSendThread = new CUserAnalyticsSendThread(this);

	if (!gEnv->pThreadManager->SpawnThread(m_pUserAnalyticsSendThread, "UserAnalytics"))
	{
		CRY_ASSERT_MESSAGE(false, "Error spawning \"UserAnalytics\" thread.");
		delete m_pUserAnalyticsSendThread;
		m_pUserAnalyticsSendThread = nullptr;
	}

	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this);

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

	m_pUserAnalyticsSendThread->SignalStopWork();
	gEnv->pThreadManager->JoinThread(m_pUserAnalyticsSendThread, eJM_Join);
	delete m_pUserAnalyticsSendThread;

	ShutdownCURL();
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
	static string sessionIdName = CryGUIDHelper::PrintGuid(sessionId);

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
		curl_easy_setopt(m_curl, CURLOPT_URL, UA_FULL_SERVER_URL);
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
	UserAnalytics::AddPairToMessage(message, "timestamp", GetTimestamp().c_str());
	UserAnalytics::AddComma(message);

	UserAnalytics::BeginAttributesScope(message);
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
void CUserAnalytics::PrepareAndSendEvents()
{
	static ICVar* const cv_collect = gEnv->pConsole->GetCVar("sys_UserAnalyticsCollect");
	if (cv_collect && cv_collect->GetIVal() == 0)
	{
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
				CryLogAlways("[User Analytics] Successfully pushed %d event(s) to %s", eventsCount, UA_FULL_SERVER_URL);

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
				cv_collect->Set(0);
			}
		}
	}
}
#endif
