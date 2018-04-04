// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MonoException.h"

#include "MonoRuntime.h"

CMonoException::CMonoException(MonoInternals::MonoException* pException)
	: m_pException(pException)
{
	CRY_ASSERT(m_pException);
}

void CMonoException::Throw()
{
	GetMonoRuntime()->HandleException(m_pException);

	//MonoInternals::mono_raise_exception(m_pException);
}