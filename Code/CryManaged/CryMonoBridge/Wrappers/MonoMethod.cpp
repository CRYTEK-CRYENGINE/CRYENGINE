// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MonoMethod.h"

#include "MonoRuntime.h"
#include "MonoObject.h"
#include "MonoDomain.h"

CMonoMethod::CMonoMethod(MonoInternals::MonoMethod* pMethod)
	: m_pMethod(pMethod)
{
	MonoInternals::MonoMethodSignature* pSignature = GetSignature();
	MonoInternals::MonoType* pReturnType = MonoInternals::mono_signature_get_return_type(pSignature);

	const int returnType = MonoInternals::mono_type_get_type(pReturnType);

	if (returnType != MonoInternals::MONO_TYPE_VOID)
	{
		MonoInternals::MonoClass *pReturnTypeClass = MonoInternals::mono_class_from_mono_type(pReturnType);

		MonoInternals::MonoImage* pClassImage = MonoInternals::mono_class_get_image(pReturnTypeClass);
		MonoInternals::MonoAssembly* pClassAssembly = MonoInternals::mono_image_get_assembly(pClassImage);

		CMonoLibrary& classLibrary = GetMonoRuntime()->GetActiveDomain()->GetLibraryFromMonoAssembly(pClassAssembly, pClassImage);
		m_pReturnValue = classLibrary.GetClassFromMonoClass(pReturnTypeClass);
	}

	GetSignature(m_description, false);
}

std::shared_ptr<CMonoObject> CMonoMethod::Invoke(const CMonoObject* pObject, void** pParameters, bool &bEncounteredException) const
{
	return InvokeInternal(pObject != nullptr ? pObject->GetManagedObject() : nullptr, pParameters, bEncounteredException);
}

std::shared_ptr<CMonoObject> CMonoMethod::Invoke(const CMonoObject* pObject, const void** pParameters, bool &bEncounteredException) const
{
	return InvokeInternal(pObject != nullptr ? pObject->GetManagedObject() : nullptr, const_cast<void**>(pParameters), bEncounteredException);
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
			return m_pReturnValue->CreateFromMonoObject(pResult);
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
			return m_pReturnValue->CreateFromMonoObject(pResult);
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

string CMonoMethod::GetSignatureDescription(bool includeNamespace) const
{
	if (includeNamespace)
	{
		string descriptionWithNamespace;
		GetSignature(descriptionWithNamespace, true);
		return descriptionWithNamespace;
	}

	return m_description;
}

void CMonoMethod::GetSignature(string& signatureOut, bool includeNamespace) const
{
	MonoInternals::MonoMethodSignature* pSignature = MonoInternals::mono_method_signature(m_pMethod);
	char* desc = MonoInternals::mono_signature_get_desc(pSignature, includeNamespace);
	const char* szName = GetName();

	signatureOut.Format(":%s(%s)", szName, desc);

	MonoInternals::mono_free(desc);
}

void CMonoMethod::PrepareForSerialization()
{
	CRY_ASSERT(!m_description.empty());

	// Invalidate the method so we don't try to use it later
	m_pMethod = nullptr;
}