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

std::shared_ptr<CMonoObject> CMonoMethod::Invoke(const CMonoObject* const pObject, MonoInternals::MonoArray* pParameters, bool &bEncounteredException) const
{
	return InvokeInternal(pObject != nullptr ? pObject->GetManagedObject() : nullptr, pParameters, bEncounteredException);
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
			CMonoDomain* pDomain = static_cast<CMonoDomain*>(GetMonoRuntime()->GetActiveDomain());

			std::shared_ptr<CMonoObject> pResultObject = std::make_shared<CMonoObject>(pResult);
			pResultObject->SetWeakPointer(pResultObject);
			return pResultObject;
		}
		else
		{
			return nullptr;
		}
	}

	GetMonoRuntime()->HandleException((MonoInternals::MonoException*)pException);
	return nullptr;
}

std::shared_ptr<CMonoObject> CMonoMethod::InvokeInternal(MonoInternals::MonoObject* pMonoObject, MonoInternals::MonoArray* pParameters, bool &bEncounteredException) const
{
	MonoInternals::MonoObject* pException = nullptr;

	MonoInternals::MonoObject* pResult = MonoInternals::mono_runtime_invoke_array(m_pMethod, pMonoObject, pParameters, &pException);
	bEncounteredException = pException != nullptr;

	if (!bEncounteredException)
	{
		if (pResult != nullptr)
		{
			CMonoDomain* pDomain = static_cast<CMonoDomain*>(GetMonoRuntime()->GetActiveDomain());

			std::shared_ptr<CMonoObject> pResultObject = std::make_shared<CMonoObject>(pResult);
			pResultObject->SetWeakPointer(pResultObject);
			return pResultObject;
		}
		else
		{
			return nullptr;
		}
	}

	GetMonoRuntime()->HandleException((MonoInternals::MonoException*)pException);
	return nullptr;
}

void CMonoMethod::VisitParameters(std::function<void(int index, MonoInternals::MonoType* pType, const char* szName)> func) const
{
	void *pIter = nullptr;
	MonoInternals::MonoMethodSignature* pSignature = MonoInternals::mono_method_signature(m_pMethod);

	int paramCount = MonoInternals::mono_signature_get_param_count(pSignature);

	std::vector<const char*> paramNames;
	paramNames.resize(paramCount);
	mono_method_get_param_names(m_pMethod, paramNames.data());

	for (int i = 0; i < paramCount; ++i)
	{
		func(i, MonoInternals::mono_signature_get_params(pSignature, &pIter), paramNames[i]);
	}
}

string CMonoMethod::GetSignatureDescription(bool bIncludeNamespace) const
{
	MonoInternals::MonoMethodSignature* pSignature = MonoInternals::mono_method_signature(m_pMethod);
	char* desc = MonoInternals::mono_signature_get_desc(pSignature, bIncludeNamespace);
	const char* szName = GetName();

	string result;
	result.Format(":%s(%s)", szName, desc);

	MonoInternals::mono_free(desc);
	return result;
}

void CMonoMethod::PrepareForSerialization()
{
	m_description = GetSignatureDescription(false);

	// Invalidate the method so we don't try to use it later
	m_pMethod = nullptr;
}