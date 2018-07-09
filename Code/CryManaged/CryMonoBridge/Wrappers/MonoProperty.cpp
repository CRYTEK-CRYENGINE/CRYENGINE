// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MonoProperty.h"
#include "MonoMethod.h"
#include "MonoRuntime.h"

CMonoProperty::CMonoProperty(MonoInternals::MonoProperty* pProperty, const char* szName)
	: m_pProperty(pProperty)
	, m_name(szName)
{
	MonoInternals::MonoClass* pPropertyClass = MonoInternals::mono_property_get_parent(pProperty);
	MonoInternals::MonoReflectionProperty* pReflectedProperty = MonoInternals::mono_property_get_object(MonoInternals::mono_domain_get(), pPropertyClass, pProperty);

	MonoInternals::MonoClass* pUnderlyingClass = GetUnderlyingClass(pReflectedProperty);

	MonoInternals::MonoImage* pClassImage = MonoInternals::mono_class_get_image(pUnderlyingClass);
	MonoInternals::MonoAssembly* pClassAssembly = MonoInternals::mono_image_get_assembly(pClassImage);

	CMonoLibrary& classLibrary = GetMonoRuntime()->GetActiveDomain()->GetLibraryFromMonoAssembly(pClassAssembly, pClassImage);
	m_pUnderlyingClass = classLibrary.GetClassFromMonoClass(pUnderlyingClass);
}

CMonoProperty::CMonoProperty(MonoInternals::MonoReflectionProperty* pReflectedProperty, const char* szName)
	: m_pProperty(((InternalMonoReflectionType*)pReflectedProperty)->property)
	, m_name(szName)
{
	MonoInternals::MonoClass* pUnderlyingClass = GetUnderlyingClass(pReflectedProperty);

	MonoInternals::MonoImage* pClassImage = MonoInternals::mono_class_get_image(pUnderlyingClass);
	MonoInternals::MonoAssembly* pClassAssembly = MonoInternals::mono_image_get_assembly(pClassImage);

	CMonoLibrary& classLibrary = GetMonoRuntime()->GetActiveDomain()->GetLibraryFromMonoAssembly(pClassAssembly, pClassImage);
	m_pUnderlyingClass = classLibrary.GetClassFromMonoClass(pUnderlyingClass);
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
			return m_pUnderlyingClass->CreateFromMonoObject(pResult);
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
	if (pValue != nullptr)
	{
		if (MonoInternals::mono_class_is_valuetype(MonoInternals::mono_object_get_class(pValue)) != 0)
		{
			pParams[0] = MonoInternals::mono_object_unbox(pValue);
		}
		else
		{
			pParams[0] = pValue;
		}
	}
	else
	{
		pParams[0] = nullptr;
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