// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "MonoClass.h"
#include "MonoString.h"

#ifndef HAVE_MONO_API
namespace MonoInternals
{
	struct MonoObject;
}
#endif

class CMonoObject
{
	friend class CMonoClass;
	friend class CMonoMethod;
	friend class CMonoProperty;

	// Begin public API
public:
	CMonoObject(MonoInternals::MonoObject* pObject, std::shared_ptr<CMonoClass> pClass);
	CMonoObject(MonoInternals::MonoObject* pObject);
	~CMonoObject();

	// Gets the string form of the object
	std::shared_ptr<CMonoString> ToString() const;

	// Gets the size of the array this object represents, if any
	size_t GetArraySize() const;
	// Gets the address of an element inside the array
	char* GetArrayAddress(size_t elementSize, size_t index) const;

	// Gets the internal handle for the object
	MonoInternals::MonoObject* GetManagedObject() const { return m_pObject; }
	// Gets the class of the object, queries if not already available
	CMonoClass* GetClass();

	void CopyFrom(CMonoObject& source);

	void* UnboxObject();
	template<typename T>
	T* Unbox()
	{
		return static_cast<T*>(UnboxObject());
	}

protected:
	void AssignObject(MonoInternals::MonoObject* pObject);

	// GetClass needs to be aware of the object weak ptr
	void SetWeakPointer(std::weak_ptr<CMonoObject> pObject) { m_pThis = pObject; }

	void ReleaseGCHandle();

protected:
	MonoInternals::MonoObject* m_pObject;
	uint32 m_gcHandle;

	std::shared_ptr<CMonoClass> m_pClass;

	// Only needed if the object was constructed without knowledge of its class (m_pClass is null)
	std::weak_ptr<CMonoObject> m_pThis;
};