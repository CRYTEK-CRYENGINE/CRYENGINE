// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SmartPtr.h"
#include <CrySerialization/Serializer.h>
#include <CrySerialization/ClassFactory.h>

//! Exposes _smart_ptr<> as serializable type for Serialization::IArchive.
template<class T>
class SmartPtrSerializer : public Serialization::IPointer
{
public:
	SmartPtrSerializer(_smart_ptr<T>& ptr)
		: m_ptr(ptr)
	{}

	const char* registeredTypeName() const override
	{
		if (m_ptr)
			return Serialization::ClassFactory<T>::the().getRegisteredTypeName(m_ptr.get());
		else
			return "";
	}

	void create(const char* registeredTypeName) const override
	{
		if (registeredTypeName && registeredTypeName[0] != '\0')
			m_ptr.reset(Serialization::ClassFactory<T>::the().create(registeredTypeName));
		else
			m_ptr.reset((T*)0);
	}
	Serialization::TypeID          baseType() const override    { return Serialization::TypeID::get<T>(); }
	virtual Serialization::SStruct serializer() const override  { return Serialization::SStruct(*m_ptr); }
	void*                          get() const override         { return reinterpret_cast<void*>(m_ptr.get()); }
	const void*                    handle() const override      { return &m_ptr; }
	Serialization::TypeID          pointerType() const override { return Serialization::TypeID::get<_smart_ptr<T>>(); }
	Serialization::IClassFactory*  factory() const override     { return &Serialization::ClassFactory<T>::the(); }
protected:
	_smart_ptr<T>& m_ptr;
};

template<class T>
bool Serialize(Serialization::IArchive& ar, _smart_ptr<T>& ptr, const char* name, const char* label)
{
	SmartPtrSerializer<T> serializer(ptr);
	return ar(static_cast<Serialization::IPointer&>(serializer), name, label);
}
