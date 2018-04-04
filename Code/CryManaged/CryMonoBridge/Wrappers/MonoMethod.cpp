// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MonoMethod.h"

#include "MonoRuntime.h"
#include "MonoObject.h"
#include "MonoDomain.h"

CMonoMethod::CMonoMethod(MonoInternals::MonoMethod* pMethod)
	: m_pMethod(pMethod)
{
}

std::shared_ptr<CMonoObject> CMonoMethod::Invoke(const CMonoObject* pObject, void** pParameters, bool &bEncounteredException) const
{
	return InvokeInternal(pObject != nullptr ? pObject->GetManagedObject() : nullptr, pParameters, bEncounteredException);
}

std::shared_ptr<CMonoObject> CMonoMethod::Invoke(const CMonoObject* pObject, void** pParameters) const
{
	bool bEncounteredException;
	std::shared_ptr<CMonoObject> pResult = InvokeInternal(pObject != nullptr ? pObject->GetManagedObject() : nullptr, pParameters, bEncounteredException);

	return pResult;
}

std::shared_ptr<CMonoObject> CMonoMethod::InvokeInternal(MonoInternals::MonoObject* pMonoObject, void** pParameters, bool &bEncounteredException) const
{
	MonoInternals::MonoObject* pException = nullptr;

	MonoInternals::MonoObject* pResult = MonoInternals::mono_runtime_invoke(m_pMethod, pMonoObject, pParameters, &pException);
	bEncounteredException = pException != nullptr;

	if (!bEncounteredException)
	{
		if (pResult != nullptr)
		{
			return std::make_shared<CMonoObject>(pResult);
		}
		else
		{
			return nullptr;
		}
	}

	GetMonoRuntime()->HandleException((MonoInternals::MonoException*)pException);
	return nullptr;
}

uint32 CMonoMethod::GetParameterCount() const
{
	MonoInternals::MonoMethodSignature* pSignature = MonoInternals::mono_method_signature(m_pMethod);
	return MonoInternals::mono_signature_get_param_count(pSignature);
}

string CMonoMethod::GetSignatureDescription(bool bIncludeNamespace) const
{
	MonoInternals::MonoMethodSignature* pSignature = MonoInternals::mono_method_signature(m_pMethod);
	char* desc = MonoInternals::mono_signature_get_desc(pSignature, bIncludeNamespace);
	const char *name = MonoInternals::mono_method_get_name(m_pMethod);

	string result;
	result.Format(":%s(%s)", name, desc);

	MonoInternals::mono_free(desc);
	return result;
}

void CMonoMethod::PrepareForSerialization()
{
	m_description = GetSignatureDescription(false);

	// Invalidate the method so we don't try to use it later
	m_pMethod = nullptr;
}