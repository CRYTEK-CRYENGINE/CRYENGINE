// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MonoProperty.h"
#include "MonoMethod.h"
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

void CMonoProperty::Set(MonoInternals::MonoObject* pObject, MonoInternals::MonoObject* pValue, bool &bEncounteredException) const
{
	void* pParams[1];
	if (MonoInternals::mono_class_is_valuetype(mono_object_get_class(pValue)) != 0)
	{
		pParams[0] = MonoInternals::mono_object_unbox(pValue);
	}
	else
	{
		pParams[0] = pValue;
	}
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

MonoInternals::MonoType* CMonoProperty::GetUnderlyingType(MonoInternals::MonoReflectionProperty* pReflectionProperty) const
{
	InternalMonoReflectionType* pInternalProperty = (InternalMonoReflectionType*)pReflectionProperty;
	CRY_ASSERT(m_pProperty == pInternalProperty->property);

	MonoInternals::MonoMethod* pGetMethod = mono_property_get_get_method(m_pProperty);
	MonoInternals::MonoMethodSignature* pGetMethodSignature = mono_method_get_signature(pGetMethod, mono_class_get_image(pInternalProperty->klass), mono_method_get_token(pGetMethod));

	return mono_signature_get_return_type(pGetMethodSignature);
}

MonoInternals::MonoClass* CMonoProperty::GetUnderlyingClass(MonoInternals::MonoReflectionProperty* pReflectionProperty) const
{
	MonoInternals::MonoType* pPropertyType = GetUnderlyingType(pReflectionProperty);
	return MonoInternals::mono_class_from_mono_type(pPropertyType);
}