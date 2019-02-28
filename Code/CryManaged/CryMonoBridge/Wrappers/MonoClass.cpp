// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MonoClass.h"

#include "MonoObject.h"
#include "MonoMethod.h"
#include "MonoRuntime.h"
#include "MonoProperty.h"

#include "RootMonoDomain.h"
#include "AppDomain.h"

CMonoClass::CMonoClass(CMonoLibrary* pLibrary, const char *nameSpace, const char *name)
	: m_pLibrary(pLibrary)
	, m_namespace(nameSpace)
	, m_name(name)
{
	m_pClass = MonoInternals::mono_class_from_name(pLibrary->GetImage(), nameSpace, name);
}

CMonoClass::CMonoClass(CMonoLibrary* pLibrary, MonoInternals::MonoClass* pClass)
	: m_pLibrary(pLibrary)
	, m_pClass(pClass)
{
	m_namespace = MonoInternals::mono_class_get_namespace(m_pClass);
	m_name = MonoInternals::mono_class_get_name(m_pClass);
}

void CMonoClass::Serialize(CMonoObject* pSerializer)
{
	CAppDomain* pDomain = static_cast<CAppDomain*>(m_pLibrary->GetDomain());

	for (auto it = m_objects.begin(); it != m_objects.end();)
	{
		if (std::shared_ptr<CMonoObject> pObject = it->lock())
		{
			pDomain->SerializeObject(pSerializer, pObject->GetManagedObject(), false);
			pObject->InvalidateBeforeDeserialization();

			++it;
		}
		else
		{
			// Clear out dead objects while we're at it
			it = m_objects.erase(it);
		}
	}

	for (auto it = m_methods.begin(); it != m_methods.end();)
	{
		if ((*it)->GetHandle() != nullptr)
		{
			(*it)->PrepareForSerialization();

			++it;
		}
		else
		{
			// Clear out dead methods while we're at it
			it = m_methods.erase(it);
		}
	}
}

void CMonoClass::Deserialize(CMonoObject* pSerializer)
{
	CAppDomain* pDomain = static_cast<CAppDomain*>(m_pLibrary->GetDomain());

	// Deserialize objects from memory
	for (const std::weak_ptr<CMonoObject>& weakObjectPointer : m_objects)
	{
		if (std::shared_ptr<CMonoObject> pObject = weakObjectPointer.lock())
		{
			std::shared_ptr<CMonoObject> pDeserializedObject = pDomain->DeserializeObject(pSerializer, this);
			pObject->AssignObject(pDeserializedObject != nullptr ? pDeserializedObject->GetManagedObject() : nullptr);
		}
		else
		{
			pDomain->DeserializeDeletedObject(pSerializer);
		}
	}

	RefreshCachedMembers();
}

void CMonoClass::RefreshCachedMembers()
{
	// Refresh method pointers, or invalidate them
	for(auto it = m_methods.begin(); it != m_methods.end();)
	{
		std::shared_ptr<CMonoMethod>& pMethod = *it;
		bool isMethodValid = false;

		// Create an internal mono representation of the description that we saved
		if (MonoInternals::MonoMethodDesc* pMethodDesc = MonoInternals::mono_method_desc_new(pMethod->GetSignatureDescription(false), false))
		{
			// Search for the method signature in all implemented classes
			MonoInternals::MonoClass *pClass = m_pClass;

			while (pClass != nullptr)
			{
				if (MonoInternals::MonoMethod* pFoundMethod = MonoInternals::mono_method_desc_search_in_class(pMethodDesc, pClass))
				{
					pMethod->OnDeserialized(pFoundMethod);
					isMethodValid = true;
					break;
				}

				pClass = MonoInternals::mono_class_get_parent(pClass);
				if (pClass == MonoInternals::mono_get_object_class())
					break;
			}

			MonoInternals::mono_method_desc_free(pMethodDesc);
		}

		if (isMethodValid)
		{
			++it;
		}
		else
		{
			it = m_methods.erase(it);
		}
	}

	// Refresh properties
	for (auto it = m_properties.begin(); it != m_properties.end();)
	{
		std::shared_ptr<CMonoProperty>& pProperty = *it;
		
		if (MonoInternals::MonoProperty* pMonoProperty = MonoInternals::mono_class_get_property_from_name(m_pClass, pProperty->GetName()))
		{
			pProperty->OnDeserialized(pMonoProperty);
			++it;
		}
		else
		{
			it = m_properties.erase(it);
		}
	}
}

void CMonoClass::ReloadClass()
{
	if (m_pLibrary->GetImage() != nullptr)
	{
		m_pClass = MonoInternals::mono_class_from_name(m_pLibrary->GetImage(), m_namespace, m_name);
	}
	else
	{
		m_pClass = nullptr;
	}
}

void CMonoClass::OnAssemblyUnload()
{
	for (const std::weak_ptr<CMonoObject>& weakObjectPointer : m_objects)
	{
		if (std::shared_ptr<CMonoObject> pObject = weakObjectPointer.lock())
		{
			pObject->ReleaseGCHandle();
		}
	}

	m_pClass = nullptr;
}

const char* CMonoClass::GetName() const
{
	return MonoInternals::mono_class_get_name(m_pClass);
}

const char* CMonoClass::GetNamespace() const
{
	return MonoInternals::mono_class_get_namespace(m_pClass);
}

CMonoLibrary* CMonoClass::GetAssembly() const
{
	return m_pLibrary;
}

std::shared_ptr<CMonoObject> CMonoClass::CreateUninitializedInstance()
{
	std::shared_ptr<CMonoObject> pWrappedObject = std::make_shared<CMonoObject>(MonoInternals::mono_object_new(m_pLibrary->GetDomain()->GetMonoDomain(), m_pClass), m_pThis.lock());

	// Push back a weak pointer to the instance, so that we can serialize the object in case of app domain reload
	m_objects.push_back(pWrappedObject);

	return pWrappedObject;
}

std::shared_ptr<CMonoObject> CMonoClass::CreateInstance(void** pConstructorParams, int numParams)
{
	MonoInternals::MonoObject* pObject = MonoInternals::mono_object_new(m_pLibrary->GetDomain()->GetMonoDomain(), m_pClass);

	if (pConstructorParams != nullptr)
	{
		MonoInternals::MonoMethod* pConstructorMethod = MonoInternals::mono_class_get_method_from_name(m_pClass, ".ctor", numParams);

		bool bException = false;
		CMonoMethod method(pConstructorMethod);
		method.InvokeInternal(pObject, pConstructorParams, bException);

		if (bException)
		{
			return nullptr;
		}
	}
	else
	{
		// Execute the default constructor
		MonoInternals::mono_runtime_object_init(pObject);
	}

	std::shared_ptr<CMonoObject> pWrappedObject = std::make_shared<CMonoObject>(pObject, m_pThis.lock());

	// Push back a weak pointer to the instance, so that we can serialize the object in case of app domain reload
	m_objects.push_back(pWrappedObject);

	return pWrappedObject;
}

std::shared_ptr<CMonoObject> CMonoClass::CreateInstanceWithDesc(const char* parameterDesc, void** pConstructorParams)
{
	MonoInternals::MonoObject* pObject = MonoInternals::mono_object_new(m_pLibrary->GetDomain()->GetMonoDomain(), m_pClass);

	string sMethodDesc;
	sMethodDesc.Format(":.ctor(%s)", parameterDesc);

	MonoInternals::MonoMethodDesc* pMethodDesc = MonoInternals::mono_method_desc_new(sMethodDesc, false);

	if (MonoInternals::MonoMethod* pConstructorMethod = MonoInternals::mono_method_desc_search_in_class(pMethodDesc, m_pClass))
	{
		MonoInternals::mono_method_desc_free(pMethodDesc);

		bool bException = false;
		CMonoMethod method(pConstructorMethod);
		method.InvokeInternal(pObject, pConstructorParams, bException);

		if (bException)
		{
			return nullptr;
		}

		std::shared_ptr<CMonoObject> pWrappedObject = std::make_shared<CMonoObject>(pObject, m_pThis.lock());

		// Push back a weak pointer to the instance, so that we can serialize the object in case of app domain reload
		m_objects.push_back(pWrappedObject);

		return pWrappedObject;
	}

	MonoInternals::mono_method_desc_free(pMethodDesc);

	GetMonoRuntime()->HandleException(MonoInternals::mono_get_exception_missing_method(GetName(), sMethodDesc));
	return nullptr;
}

std::shared_ptr<CMonoObject> CMonoClass::CreateFromMonoObject(MonoInternals::MonoObject* pObject)
{
	CRY_ASSERT(pObject != nullptr);

	std::shared_ptr<CMonoObject> pWrappedObject = std::make_shared<CMonoObject>(pObject, m_pThis.lock());

	// Push back a weak pointer to the instance, so that we can serialize the object in case of app domain reload
	m_objects.push_back(pWrappedObject);

	return pWrappedObject;
}

void CMonoClass::RegisterObject(std::weak_ptr<CMonoObject> pObject)
{
	m_objects.push_back(pObject);
}

std::shared_ptr<CMonoProperty> CMonoClass::MakeProperty(MonoInternals::MonoProperty* pProperty, const char* szName)
{
	return std::make_shared<CMonoProperty>(pProperty, szName);
}

std::shared_ptr<CMonoProperty> CMonoClass::MakeProperty(MonoInternals::MonoReflectionProperty* pProperty, const char* szName)
{
	return std::make_shared<CMonoProperty>(pProperty, szName);
}	

std::weak_ptr<CMonoMethod> CMonoClass::FindOrEmplaceMethod(MonoInternals::MonoMethod* pMethod)
{
	CRY_ASSERT(pMethod != nullptr);

	auto methodIt = std::find_if(m_methods.begin(), m_methods.end(), [pMethod](const std::shared_ptr<CMonoMethod>& pMonoMethod) -> bool
	{
		return pMonoMethod->GetHandle() == pMethod;
	});

	if (methodIt != m_methods.end())
	{
		return *methodIt;
	}

	std::shared_ptr<CMonoMethod> pMonoMethod = std::make_shared<CMonoMethod>(pMethod);
	m_methods.push_back(pMonoMethod);
	return pMonoMethod;
}

std::weak_ptr<CMonoMethod> CMonoClass::FindMethod(const char* szName, int numParams)
{
	if (MonoInternals::MonoMethod* pMethod = MonoInternals::mono_class_get_method_from_name(m_pClass, szName, numParams))
	{
		return FindOrEmplaceMethod(pMethod);
	}

	return std::weak_ptr<CMonoMethod>();
}

std::weak_ptr<CMonoMethod> CMonoClass::FindMethodInInheritedClasses(const char* szName, int numParams)
{
	MonoInternals::MonoClass* pClass = m_pClass;
	while (pClass != nullptr)
	{
		if (MonoInternals::MonoMethod* pMethod = MonoInternals::mono_class_get_method_from_name(pClass, szName, numParams))
		{
			return FindOrEmplaceMethod(pMethod);
		}

		pClass = MonoInternals::mono_class_get_parent(pClass);
	}

	return std::weak_ptr<CMonoMethod>();
}

std::weak_ptr<CMonoMethod> CMonoClass::FindMethodWithDesc(const char* szMethodDesc)
{
	string sMethodDesc;
	sMethodDesc.Format(":%s", szMethodDesc);

	MonoInternals::MonoMethodDesc* pMethodDesc = MonoInternals::mono_method_desc_new(sMethodDesc, false);
	if (pMethodDesc == nullptr)
	{
		return std::weak_ptr<CMonoMethod>();
	}

	if (MonoInternals::MonoMethod* pMethod = MonoInternals::mono_method_desc_search_in_class(pMethodDesc, m_pClass))
	{
		return FindOrEmplaceMethod(pMethod);
	}

	MonoInternals::mono_method_desc_free(pMethodDesc);
	return std::weak_ptr<CMonoMethod>();
}

std::weak_ptr<CMonoMethod> CMonoClass::FindMethodWithDescInInheritedClasses(const char* szMethodDesc, CMonoClass* pBaseClass)
{
	string sMethodDesc;
	sMethodDesc.Format(":%s", szMethodDesc);

	MonoInternals::MonoMethodDesc* pDesiredDesc = MonoInternals::mono_method_desc_new(sMethodDesc, false);
	if (pDesiredDesc == nullptr)
	{
		return std::weak_ptr<CMonoMethod>();
	}

	MonoInternals::MonoClass *pClass = m_pClass;
	while (pClass != nullptr)
	{
		if (MonoInternals::MonoMethod* pMethod = MonoInternals::mono_method_desc_search_in_class(pDesiredDesc, pClass))
		{
			MonoInternals::mono_method_desc_free(pDesiredDesc);

			return FindOrEmplaceMethod(pMethod);
		}

		pClass = MonoInternals::mono_class_get_parent(pClass);
		if (pClass == MonoInternals::mono_get_object_class() || pClass == pBaseClass->GetMonoClass())
			break;
	}

	MonoInternals::mono_method_desc_free(pDesiredDesc);
	return std::weak_ptr<CMonoMethod>();
}

std::weak_ptr<CMonoProperty> CMonoClass::FindProperty(const char* szName)
{
	if (MonoInternals::MonoProperty* pProperty = MonoInternals::mono_class_get_property_from_name(m_pClass, szName))
	{
		m_properties.emplace_back(CMonoClass::MakeProperty(pProperty, szName));
		return m_properties.back();
	}

	return std::weak_ptr<CMonoProperty>();
}

std::weak_ptr<CMonoProperty> CMonoClass::FindPropertyInInheritedClasses(const char* szName)
{
	MonoInternals::MonoClass *pClass = m_pClass;
	while (pClass != nullptr)
	{
		if (MonoInternals::MonoProperty* pProperty = MonoInternals::mono_class_get_property_from_name(pClass, szName))
		{
			m_properties.emplace_back(CMonoClass::MakeProperty(pProperty, szName));
			return m_properties.back();
		}

		pClass = MonoInternals::mono_class_get_parent(pClass);
		if (pClass == MonoInternals::mono_get_object_class())
			break;
	}

	return std::weak_ptr<CMonoProperty>();
}

bool CMonoClass::IsValueType() const
{
	return MonoInternals::mono_class_is_valuetype(m_pClass) != 0;
}

void CMonoClass::SetMonoClass(MonoInternals::MonoClass* pMonoClass)
{
	m_pClass = pMonoClass;

	m_namespace = MonoInternals::mono_class_get_namespace(m_pClass);
	m_name = MonoInternals::mono_class_get_name(m_pClass);

	RefreshCachedMembers();
}