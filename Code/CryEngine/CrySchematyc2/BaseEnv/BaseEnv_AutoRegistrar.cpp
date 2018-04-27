// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BaseEnv/BaseEnv_AutoRegistrar.h"

namespace SchematycBaseEnv
{
	CAutoRegistrar::CAutoRegistrar(AutoRegistrarFunctionPtr pFunction)
		: m_pFunction(pFunction)
	{}

	void CAutoRegistrar::Process()
	{
		for(const CAutoRegistrar* pInstance = GetFirstInstance(); pInstance; pInstance = pInstance->GetNextInstance())
		{
			pInstance->m_pFunction();
		}
	}
}