// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UnitTests/UnitTestRegistrar.h"

#include <CrySchematyc/Services/ILog.h>

namespace Schematyc
{
CUnitTestRegistrar::CUnitTestRegistrar(UnitTestFunctionPtr pFuntion, const char* szName)
	: m_pFuntion(pFuntion)
	, m_szName(szName)
{}

void CUnitTestRegistrar::RunUnitTests()
{
	UnitTestResultFlags allResultFlags = EUnitTestResultFlags::Success;
	for (const CUnitTestRegistrar* pInstance = GetFirstInstance(); pInstance; pInstance = pInstance->GetNextInstance())
	{
		const UnitTestResultFlags testResultFlags = pInstance->m_pFuntion();
		if (testResultFlags != EUnitTestResultFlags::Success)
		{
			SCHEMATYC_CORE_ERROR("Failed unit test '%s'!", pInstance->m_szName);
		}
		allResultFlags.Add(testResultFlags);
	}

	if (allResultFlags.Check(EUnitTestResultFlags::FatalError))
	{
		SCHEMATYC_CORE_FATAL_ERROR("Failed one or more unit tests!");
	}
	else if (allResultFlags.Check(EUnitTestResultFlags::CriticalError))
	{
		SCHEMATYC_CORE_CRITICAL_ERROR("Failed one or more unit tests!");
	}
}
}
