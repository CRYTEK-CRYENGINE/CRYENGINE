// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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
}

string CMonoException::GetExceptionString()
{
	auto pInternalException = reinterpret_cast<InternalMonoException*>(m_pException);
	
	if (MonoInternals::MonoString* pString = MonoInternals::mono_object_to_string((MonoInternals::MonoObject*)m_pException, nullptr))
	{
		return MonoStringToUtf8String(pString);
	}
	else
	{
		return "Unable to convert exception to a readable format!";
	}
}

string CMonoException::GetExceptionTypeName()
{
	auto pInternalException = reinterpret_cast<InternalMonoException*>(m_pException);
	return MonoStringToUtf8String(pInternalException->class_name);
}

string CMonoException::GetMessage()
{
	auto pInternalException = reinterpret_cast<InternalMonoException*>(m_pException);
	return MonoStringToUtf8String(pInternalException->message);
}

string CMonoException::GetStackTrace()
{
	auto pInternalException = reinterpret_cast<InternalMonoException*>(m_pException);
	return MonoStringToUtf8String(pInternalException->stack_trace);
}

string CMonoException::GetInnerExceptionString()
{
	auto pInternalException = reinterpret_cast<InternalMonoException*>(m_pException);

	if (pInternalException->inner_ex)
	{
		MonoInternals::MonoException* pInnerException = reinterpret_cast<MonoInternals::MonoException*>(pInternalException->inner_ex);
		CMonoException innerException = CMonoException(pInnerException);
		return innerException.GetExceptionString();
	}
	else
	{
		return "";
	}
}

string CMonoException::MonoStringToUtf8String(MonoInternals::MonoString* pMonoString)
{
	char* szUtfString = MonoInternals::mono_string_to_utf8(pMonoString);
	string utfString = szUtfString;
	MonoInternals::mono_free(szUtfString);
	return utfString;
}
