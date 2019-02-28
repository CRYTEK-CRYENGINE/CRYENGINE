// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

	void InvalidateBeforeDeserialization();
	// Gets the internal handle for the object
	MonoInternals::MonoObject* GetManagedObject() const { return m_pObject; }
	// Gets the class of the object, queries if not already available
	CMonoClass* GetClass();

	void CopyFrom(const CMonoObject& source);
	void CopyFrom(MonoInternals::MonoObject* pSource);
	std::shared_ptr<CMonoObject> Clone();

	void* UnboxObject();
	template<typename T>
	T* Unbox()
	{
		return static_cast<T*>(UnboxObject());
	}

	bool ReferenceEquals(const CMonoObject& other) const;
	bool ReferenceEquals(MonoInternals::MonoObject* pOtherObject) const;

protected:
	void AssignObject(MonoInternals::MonoObject* pObject);

	void ReleaseGCHandle();

protected:
	MonoInternals::MonoObject* m_pObject;
	uint32 m_gcHandle;

	std::shared_ptr<CMonoClass> m_pClass;
};