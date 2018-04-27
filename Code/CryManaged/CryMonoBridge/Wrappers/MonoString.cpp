// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MonoString.h"

#include "RootMonoDomain.h"

CMonoString::CMonoString(MonoInternals::MonoDomain* pDomain, const char* szString)
	: m_szString(szString)
	, m_bShouldFreeString(false)
	, m_pString(MonoInternals::mono_string_new(pDomain, szString))
{

}

CMonoString::CMonoString(MonoInternals::MonoString* pMonoString)
	: m_pString(pMonoString)
	, m_szString(nullptr)
	, m_bShouldFreeString(false)
{
}

CMonoString::~CMonoString()
{
	if (m_bShouldFreeString)
	{
		MonoInternals::mono_free((void*)m_szString);
	}
}

const char* CMonoString::GetString()
{
	if (m_szString == nullptr)
	{
		m_szString = MonoInternals::mono_string_to_utf8(m_pString);
	}

	return m_szString;
}