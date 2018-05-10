// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MonoObject.h"

#include "MonoClass.h"
#include "MonoRuntime.h"

#include "AppDomain.h"

CMonoObject::CMonoObject(MonoInternals::MonoObject* pObject, std::shared_ptr<CMonoClass> pClass)
	: m_pClass(pClass)
	, m_pObject(pObject)
{
	CRY_ASSERT_MESSAGE(m_pClass.get() != nullptr, "SetWeakPointer must be called if no class is available!");

	m_gcHandle = MonoInternals::mono_gchandle_new(m_pObject, true);
}

CMonoObject::CMonoObject(MonoInternals::MonoObject* pObject)
	: m_pObject(pObject)
{
	m_gcHandle = MonoInternals::mono_gchandle_new(m_pObject, true);
}

CMonoObject::~CMonoObject()
{
	ReleaseGCHandle();
}

void CMonoObject::ReleaseGCHandle()
{
	if (m_gcHandle != 0)
	{
		MonoInternals::mono_gchandle_free(m_gcHandle);
		m_gcHandle = 0;
	}
}

std::shared_ptr<CMonoString> CMonoObject::ToString() const
{
	if (MonoInternals::mono_object_get_class(m_pObject) == MonoInternals::mono_get_string_class())
	{
		return CMonoDomain::CreateString(reinterpret_cast<MonoInternals::MonoString*>(m_pObject));
	}

	MonoInternals::MonoObject* pException = nullptr;
	MonoInternals::MonoString* pStr = MonoInternals::mono_object_to_string(m_pObject, &pException);
	if (pException != nullptr)
	{
		GetMonoRuntime()->HandleException((MonoInternals::MonoException*)pException);
		return nullptr;
	}

	if (pStr == nullptr)
	{
		return nullptr;
	}

	return CMonoDomain::CreateString(pStr);
}

size_t CMonoObject::GetArraySize() const
{
	return MonoInternals::mono_array_length((MonoInternals::MonoArray*)m_pObject);
}

char* CMonoObject::GetArrayAddress(size_t elementSize, size_t index) const
{
	return MonoInternals::mono_array_addr_with_size((MonoInternals::MonoArray*)m_pObject, elementSize, index);
}

void CMonoObject::AssignObject(MonoInternals::MonoObject* pObject)
{
	m_pObject = pObject;
	m_gcHandle = MonoInternals::mono_gchandle_new(m_pObject, true);
}

void CMonoObject::InvalidateBeforeDeserialization()
{
	m_pObject = nullptr;
	m_gcHandle = 0;
}

CMonoClass* CMonoObject::GetClass()
{
	if (m_pClass == nullptr)
	{
		MonoInternals::MonoClass* pClass = MonoInternals::mono_object_get_class(m_pObject);
		MonoInternals::MonoImage* pImage = MonoInternals::mono_class_get_image(pClass);
		MonoInternals::MonoAssembly* pAssembly = MonoInternals::mono_image_get_assembly(pImage);

		CMonoLibrary& library = GetMonoRuntime()->GetActiveDomain()->GetLibraryFromMonoAssembly(pAssembly, pImage);
		m_pClass = library.GetClassFromMonoClass(pClass);
	}

	return m_pClass.get();
}

void CMonoObject::CopyFrom(const CMonoObject& source)
{
	CopyFrom(source.GetManagedObject());
}

void CMonoObject::CopyFrom(MonoInternals::MonoObject* pSource)
{
	if (GetClass()->IsValueType())
	{
		MonoInternals::mono_gc_wbarrier_object_copy(m_pObject, pSource);
	}
	else
	{
		MonoInternals::mono_gc_wbarrier_generic_store(&m_pObject, pSource);
	}
}

std::shared_ptr<CMonoObject> CMonoObject::Clone()
{
	std::shared_ptr<CMonoObject> pNewObject = GetClass()->CreateUninitializedInstance();
	pNewObject->CopyFrom(*this);
	return pNewObject;
}

void* CMonoObject::UnboxObject()
{
	return mono_object_unbox(m_pObject);
}

bool CMonoObject::ReferenceEquals(const CMonoObject& other) const
{
	return ReferenceEquals(other.GetManagedObject());
}

bool CMonoObject::ReferenceEquals(MonoInternals::MonoObject* pOtherObject) const
{
	MonoInternals::MonoException* pException = nullptr;
	MonoInternals::MonoBoolean isEqual = const_cast<CMonoObject*>(this)->GetClass()->GetAssembly()->GetDomain()->GetReferenceEqualsMethod()(m_pObject, pOtherObject, &pException);

	return isEqual != 0 && pException == nullptr;
}