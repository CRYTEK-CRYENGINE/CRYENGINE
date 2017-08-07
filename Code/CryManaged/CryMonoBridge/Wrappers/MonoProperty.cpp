// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MonoProperty.h"
#include "MonoRuntime.h"

CMonoProperty::CMonoProperty(MonoInternals::MonoProperty* pProperty)
	: m_pProperty(pProperty)
{
}

CMonoProperty::CMonoProperty(MonoInternals::MonoReflectionProperty* pProperty)
	: m_pProperty(((InternalMonoReflectionType*)pProperty)->property)
{
}

std::shared_ptr<CMonoObject> CMonoProperty::Get(MonoInternals::MonoObject* pObject, bool &bEncounteredException) const
{
	MonoInternals::MonoObject* pException = nullptr;
	MonoInternals::MonoObject* pResult = MonoInternals::mono_property_get_value(m_pProperty, pObject, nullptr, &pException);
	bEncounteredException = pException != nullptr;

	if (!bEncounteredException)
	{
		if (pResult != nullptr)
		{
			auto pResultObject = std::make_shared<CMonoObject>(pResult);
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

void CMonoProperty::Set(MonoInternals::MonoObject* pObject, MonoInternals::MonoObject* pValue, bool &bEncounteredException) const
{
	void* pParams[1] = { pValue };
	Set(pObject, pParams, bEncounteredException);
}

void CMonoProperty::Set(MonoInternals::MonoObject* pObject, void** pParams, bool &bEncounteredException) const
{
	MonoInternals::MonoObject* pException = nullptr;
	mono_property_set_value(m_pProperty, pObject, pParams, &pException);
	bEncounteredException = pException != nullptr;

	if (bEncounteredException)
	{
		GetMonoRuntime()->HandleException((MonoInternals::MonoException*)pException);
	}
}

CMonoMethod CMonoProperty::GetGetMethod() const
{
	return CMonoMethod(MonoInternals::mono_property_get_get_method(m_pProperty));
}

CMonoMethod CMonoProperty::GetSetMethod() const
{
	return CMonoMethod(MonoInternals::mono_property_get_set_method(m_pProperty));
}