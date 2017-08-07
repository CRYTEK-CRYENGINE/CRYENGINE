// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef HAVE_MONO_API
namespace MonoInternals
{
	struct MonoException;
}
#endif

class CMonoException
{
	// Begin public API
public:
	CMonoException(MonoInternals::MonoException* pException);
	void Throw();

protected:
	MonoInternals::MonoException* m_pException;
};