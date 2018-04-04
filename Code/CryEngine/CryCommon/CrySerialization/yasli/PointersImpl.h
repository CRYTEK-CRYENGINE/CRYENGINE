/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once

#include "Archive.h"
#include "Pointers.h"
#include "ClassFactory.h"
#include "Object.h"

#pragma warning (push)
#pragma warning (disable: 4512)

namespace yasli{

template<class T>
class SharedPtrSerializer : public PointerInterface
{
public:
	SharedPtrSerializer(SharedPtr<T>& ptr)
	: ptr_(ptr)
	{}

	const char* registeredTypeName() const{
		if(ptr_)
			return ClassFactory<T>::the().getRegisteredTypeName(ptr_.get());
		else
			return "";
	}
	void create(const char* typeName) const{
		YASLI_ASSERT(!ptr_ || ptr_->refCount() == 1);
		if(typeName && typeName[0] != '\0')
			ptr_.reset(factory()->create(typeName));
		else
			ptr_.reset((T*)0);
	}
	TypeID baseType() const{ return TypeID::get<T>(); }
	virtual Serializer serializer() const{
		return Serializer(*ptr_);
	}
	void* get() const{
		return reinterpret_cast<void*>(ptr_.get());
	}
	const void* handle() const{
		return &ptr_;
	}
	TypeID pointerType() const{
		return TypeID::get<SharedPtr<T>>();
	}
	virtual ClassFactory<T>* factory() const{ return &ClassFactory<T>::the(); }
protected:
	SharedPtr<T>& ptr_;
};

template<class T>
class PolyPtrSerializer : public PointerInterface
{
public:
	PolyPtrSerializer(PolyPtr<T>& ptr)
	: ptr_(ptr)
	{}

	TypeID type() const{
		if(ptr_)
			return ClassFactory<T>::the().getTypeID(ptr_.get());
		else
			return TypeID();
	}
	void create(TypeID type) const{
		if(type)
			ptr_.reset(ClassFactory<T>::the().create(type));
		else
			ptr_.reset((T*)0);
	}
	TypeID baseType() const{ return TypeID::get<T>(); }
	virtual Serializer serializer() const{
		return Serializer(*ptr_);
	}
	void* get() const{
		return reinterpret_cast<void*>(ptr_.get());
	}
	ClassFactoryBase* factory() const{ return &ClassFactory<T>::the(); }
protected:
	PolyPtr<T>& ptr_;
};

template<class T>
AsObjectWrapper<SharedPtr<T> > asObject(SharedPtr<T>& ptr)
{
	return AsObjectWrapper<SharedPtr<T> >(ptr);
}

template<class T>
int acquireByVoidPtr(void* ptr) { ((T*)ptr)->acquire(); return ((T*)ptr)->refCount(); }

template<class T>
int releaseByVoidPtr(void* ptr) {
	T* obj = (T*)ptr;
	int result = obj->release(); 
	if (result == 0)
		delete obj;
	return result;
}

template<class T>
bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, yasli::SharedPtr<T>& ptr, const char* name, const char* label)
{
	yasli::SharedPtrSerializer<T> serializer(ptr);
	return ar(static_cast<yasli::PointerInterface&>(serializer), name, label);
}

template<class T>
bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, yasli::PolyPtr<T>& ptr, const char* name, const char* label)
{
	yasli::PolyPtrSerializer<T> serializer(ptr);
	return ar(static_cast<yasli::PointerInterface&>(serializer), name, label);
}


template<class T>
bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, yasli::AsObjectWrapper<yasli::SharedPtr<T> >& ptr, const char* name, const char* label)
{
	yasli::Object obj(ptr.ptr_.get(), yasli::TypeID::get<T>(),
					  &yasli::acquireByVoidPtr<T>, 
						&yasli::releaseByVoidPtr<T>,
					  &yasli::Serializer::serializeRaw<T>);

	if (!ar(obj, name, label))
		return false;

	if (ar.isInput())
		ptr.ptr_ = (T*)obj.address();
	return true;
}

}

#pragma warning (pop)

// vim:sw=4 ts=4:
