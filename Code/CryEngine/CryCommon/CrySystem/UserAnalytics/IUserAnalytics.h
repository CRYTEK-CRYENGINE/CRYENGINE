// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

namespace UserAnalytics
{
//! User Analytics attributes. Additional event information.
struct Attributes
{
	Attributes() {}
	~Attributes() {}

	//! Overloaded function to collect attribute information.
	inline void AddAttribute(const char* szKey, const char* szValue)
	{
#if !defined(_RELEASE) && CRY_PLATFORM_WINDOWS
		m_keys.push_back(szKey);
		m_values.push_back(szValue);
#endif
	}

	//! Overloaded function to collect attribute information.
	inline void AddAttribute(const char* szKey, int value)
	{
#if !defined(_RELEASE) && CRY_PLATFORM_WINDOWS
		m_keys.push_back(szKey);
		m_values.push_back(std::to_string(value).c_str());
#endif
	}

	//! Overloaded function to collect attribute information.
	inline void AddAttribute(const char* szKey, float value)
	{
#if !defined(_RELEASE) && CRY_PLATFORM_WINDOWS
		m_keys.push_back(szKey);
		m_values.push_back(std::to_string(value).c_str());
#endif
	}

	//! Overloaded function to collect attribute information.
	inline void AddAttribute(const char* szKey, bool bValue)
	{
#if !defined(_RELEASE) && CRY_PLATFORM_WINDOWS
		m_keys.push_back(szKey);
		m_values.push_back(bValue ? "true" : "false");
#endif
	}

#if !defined(_RELEASE) && CRY_PLATFORM_WINDOWS
	std::vector<string> m_keys;
	std::vector<string> m_values;
#endif
};
}

//! Interface to User Analytics
struct IUserAnalytics
{
	virtual ~IUserAnalytics() {}

	//! Invoke User Analytics event with default attributes.
	virtual void TriggerEvent(const char* szEventName) = 0;

	//! Invoke User Analytics event with default and custom attributes.
	virtual void TriggerEvent(const char* szEventName, UserAnalytics::Attributes* atrributes) = 0;
};

//! Interface to the User Analytics System
struct IUserAnalyticsSystem
{
	virtual ~IUserAnalyticsSystem() {}

	//! Invoke User Analytics event with default and custom attributes.
	virtual void TriggerEvent(const char* szEventName, UserAnalytics::Attributes* atrributes) = 0;
};

#if !defined(_RELEASE) && CRY_PLATFORM_WINDOWS
	#define USER_ANALYTICS_EVENT(event)                                        \
	  if (gEnv->pSystem)                                                       \
	  { IUserAnalyticsSystem* pUAS = gEnv->pSystem->GetIUserAnalyticsSystem(); \
	    pUAS->TriggerEvent(event, nullptr); }

	#define USER_ANALYTICS_EVENT_ARG(event, attributes)                        \
	  if (gEnv->pSystem)                                                       \
	  { IUserAnalyticsSystem* pUAS = gEnv->pSystem->GetIUserAnalyticsSystem(); \
	    pUAS->TriggerEvent(event, attributes); }
#else
	#define USER_ANALYTICS_EVENT(event)
	#define USER_ANALYTICS_EVENT_ARG(event, attributes)
#endif

//! \endcond