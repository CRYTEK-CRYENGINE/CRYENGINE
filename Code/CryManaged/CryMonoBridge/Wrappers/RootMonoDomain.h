#pragma once

#include "MonoDomain.h"

// Wrapped manager of the root mono domain
class CRootMonoDomain final : public CMonoDomain
{
	friend class CAppDomain;
	friend class CMonoRuntime;

public:
	CRootMonoDomain();
	virtual ~CRootMonoDomain() {}

	// CMonoDomain
	virtual bool IsRoot() override { return true; }

	virtual bool Reload() override { CRY_ASSERT_MESSAGE(false, "Cannot unload the root domain!"); return false; }
	// ~CMonoDomain

	void Initialize();
	bool IsInitialized() const { return m_pDomain != nullptr; }

	CMonoLibrary& CRootMonoDomain::GetNetCoreLibrary();
};