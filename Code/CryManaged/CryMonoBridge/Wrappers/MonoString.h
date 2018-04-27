// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef HAVE_MONO_API
namespace MonoInternals
{
	struct MonoDomain;
	struct MonoString;
}
#endif

class CMonoString
{
	// Begin public API
public:
	CMonoString() = default;
	explicit CMonoString(MonoInternals::MonoDomain* pDomain, const char* szString);
	explicit CMonoString(MonoInternals::MonoString* pManagedString);
	~CMonoString();

	const char* GetString();
	MonoInternals::MonoString* GetManagedObject() const { return m_pString; }

protected:
	MonoInternals::MonoString* m_pString;

	const char* m_szString;
	bool m_bShouldFreeString : 1;
};