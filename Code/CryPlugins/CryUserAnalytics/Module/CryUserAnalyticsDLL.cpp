// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryUserAnalyticsDLL.h"
#include "UserAnalytics.h"

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

CPlugin_CryUserAnalytics::CPlugin_CryUserAnalytics()
	: m_pUserAnalytics(nullptr)
{
}

CPlugin_CryUserAnalytics::~CPlugin_CryUserAnalytics()
{
	SAFE_DELETE(m_pUserAnalytics);
}

bool CPlugin_CryUserAnalytics::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
{
	m_pUserAnalytics = new CUserAnalytics();

	return (m_pUserAnalytics != nullptr);
}

IUserAnalytics* CPlugin_CryUserAnalytics::GetIUserAnalytics() const
{
	return m_pUserAnalytics;
}

CRYREGISTER_SINGLETON_CLASS(CPlugin_CryUserAnalytics)

#include <CryCore/CrtDebugStats.h>
