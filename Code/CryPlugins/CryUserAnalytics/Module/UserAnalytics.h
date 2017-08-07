// Copyright 2001 - 2016 Crytek GmbH / Crytek Group.All rights reserved.

#pragma once

#include <CrySystem/UserAnalytics/IUserAnalytics.h>
#include <CrySystem/ISystem.h>

#if !defined(_RELEASE) && CRY_PLATFORM_WINDOWS

typedef void CURL;
struct curl_slist;
struct ICVar;

class CUserAnalyticsSendThread;

class CUserAnalytics final : public IUserAnalytics, public ISystemEventListener
{
	friend class CUserAnalyticsSendThread;

public:
	CUserAnalytics();
	~CUserAnalytics();

	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;

	virtual void TriggerEvent(const char* szEventName) override;
	virtual void TriggerEvent(const char* szEventName, UserAnalytics::Attributes* attributes) override;

private:
	string  GetTimestamp();
	string& GetSessionId();

	bool    InitializeCURL();
	void    ShutdownCURL();

	void    Shutdown();

	void    ReadWriteAnonymousToken();
	void    ReadUserIdFromDisk();
	void    PrepareAndSendEvents();

	CryMT::vector<string>     m_messages; // message buffer

	std::vector<string>       m_messagesCopy; // batch of messages which get prepared for sending
	string                    m_sendBuffer;   // buffer to concatenate message buffer into one string

	CURL*                     m_curl;
	curl_slist*               m_curlHeaderList;

	CUserAnalyticsSendThread* m_pUserAnalyticsSendThread;
	static ICVar*             m_userAnalyticsServerAddress;

	string                    m_anonymousUserToken;
	string                    m_userId;
};
#else
class CUserAnalytics : public IUserAnalytics
{
public:
	CUserAnalytics() {};
	~CUserAnalytics() {}

	virtual void TriggerEvent(const char* szEventName) override                                        {};
	virtual void TriggerEvent(const char* szEventName, UserAnalytics::Attributes* attributes) override {};
};
#endif
