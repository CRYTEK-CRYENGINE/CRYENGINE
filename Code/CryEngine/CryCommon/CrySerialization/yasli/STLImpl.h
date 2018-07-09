/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once

#include <list>
#include <CrySerialization/yasli/Config.h>
#include <CrySerialization/yasli/Archive.h>
#include <CrySerialization/yasli/Serializer.h>
#include <CrySerialization/yasli/KeyValue.h>

#if YASLI_CXX11
#include <memory>
#include <unordered_map>
#include <CrySerialization/yasli/ClassFactory.h>
#endif

#pragma warning(push)
#pragma warning(disable:4512)

namespace yasli {

template<class Container, class Element>
class ContainerSTL : public ContainerInterface/*{{{*/
{
public:
	explicit ContainerSTL(Container* container = 0)
	: container_(container)
	, it_(container->begin())
	, size_(container->size())
	{
		YASLI_ASSERT(container_ != 0);
	}

	template<class T, class A>
	void resizeHelper(size_t _size, std::vector<T, A>& _v) const
	{
		_v.resize(_size);
	}

	// from ContainerInterface
	size_t size() const{
		YASLI_ESCAPE(container_ != 0, return 0);
		return container_->size();
	}
	size_t resize(size_t size){
		YASLI_ESCAPE(container_ != 0, return 0);
		container_->resize(size);
		it_ = container_->begin();
		size_ = size;
		return size;
	}

	void* pointer() const{ return reinterpret_cast<void*>(container_); }
	TypeID elementType() const{ return TypeID::get<Element>(); }
	TypeID containerType() const{ return TypeID::get<Container>(); }

	void begin()
	{
		it_ = container_->begin();
	}

	bool next()
	{
		YASLI_ESCAPE(container_ && it_ != container_->end(), return false);
		++it_;
		return it_ != container_->end();
	}

	void remove()
	{
		it_ = container_->erase(it_);
	}

	void insert()
	{
		it_ = container_->emplace(it_);
	}

	void move(int index)
	{
		moveImpl<Element>(index);
	}

	template<typename T, typename std::enable_if<std::is_copy_assignable<T>::value, int>::type = 0>
	void moveImpl(int index)
	{
		auto it = container_->begin();
		it = it + index;

		auto distance = std::distance(it, it_);
		if (distance > 0)
		{
			auto element = *it_;
			std::copy_backward(it, it + distance, it_ + 1);
			*it = element;
		}
		else if (distance < 0)
		{
			auto element = *it_;
			std::copy(it_ + 1, it_ + 1 - distance, it_);
			*it = element;
		}
	}

	template<typename T, typename std::enable_if<!std::is_copy_assignable<T>::value, int>::type = 0>
	void moveImpl(int index)
	{
		assert(0); //Should not be called
	}

	void* elementPointer() const{ return &*it_; }
  size_t elementSize() const { return sizeof(typename Container::value_type); }

	bool operator()(Archive& ar, const char* name, const char* label){
		YASLI_ESCAPE(container_, return false);
		if(it_ == container_->end())
		{
			it_ = container_->insert(container_->end(), Element());
			return ar(*it_, name, label);
		}
		else
			return ar(*it_, name, label);
	}
	operator bool() const{ return container_ != 0; }

	struct ElementInitializer
	{
		Element value;
		// Important to call brackets on constructed value to initialize
		// built-in types to zeros/false.
		ElementInitializer() : value() {}
	};
	void serializeNewElement(Archive& ar, const char* name = "", const char* label = 0) const{
		ElementInitializer element;
		ar(element.value, name, label);
	}
	// ^^^
protected:
	Container* container_;
	typename Container::iterator it_;
	size_t size_;
};/*}}}*/

#if YASLI_CXX11

template<class T>
class StdSharedPtrSerializer : public PointerInterface
{
public:
	StdSharedPtrSerializer(std::shared_ptr<T>& ptr)
	: ptr_(ptr)
	{}

	const char* registeredTypeName() const{
		if(ptr_)
			return ClassFactory<T>::the().getRegisteredTypeName(ptr_.get());
		else
			return "";
	}
	void create(const char* typeName) const{
		if(typeName && typeName[0] != '\0')
			ptr_.reset(ClassFactory<T>::the().create(typeName));
		else
			ptr_.reset((T*)0);
	}

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
	const void* handle() const { return &ptr_; }
	ClassFactoryBase* factory() const{ return &ClassFactory<T>::the(); }
	TypeID pointerType() const { return TypeID::get<std::shared_ptr<T> >(); }
protected:
	std::shared_ptr<T>& ptr_;
};

template<class T, class Del = std::default_delete<T>>
class StdUniquePtrSerializer : public PointerInterface
{
public:
	StdUniquePtrSerializer(std::unique_ptr<T, Del>& ptr)
		: ptr_(ptr)
	{}

	const char* registeredTypeName() const{
		if(ptr_)
			return ClassFactory<T>::the().getRegisteredTypeName(ptr_.get());
		else
			return "";
	}
	void create(const char* typeName) const{
		if(typeName && typeName[0] != '\0')
			ptr_.reset(ClassFactory<T>::the().create(typeName));
		else
			ptr_.reset((T*)0);
	}

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
	const void* handle() const { return &ptr_; }
	ClassFactoryBase* factory() const{ return &ClassFactory<T>::the(); }
	TypeID pointerType() const { return TypeID::get<std::unique_ptr<T, Del> >(); }
protected:
	std::unique_ptr<T, Del>& ptr_;
};

#endif

}

namespace std{

template<class T, class Alloc>
bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, std::vector<T, Alloc>& container, const char* name, const char* label)
{
	yasli::ContainerSTL<std::vector<T, Alloc>, T> ser(&container);
	return ar(static_cast<yasli::ContainerInterface&>(ser), name, label);
}

template<class T, class Alloc>
bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, std::list<T, Alloc>& container, const char* name, const char* label)
{
	yasli::ContainerSTL<std::list<T, Alloc>, T> ser(&container);
	return ar(static_cast<yasli::ContainerInterface&>(ser), name, label);
}

}

// ---------------------------------------------------------------------------
namespace yasli {

class StringSTL : public StringInterface
{
public:
	StringSTL(yasli::string& str) : str_(str) { }

	void set(const char* value) { str_ = value; }
	const char* get() const { return str_.c_str(); }
	const void* handle() const { return &str_; }
	TypeID type() const { return TypeID::get<string>(); }
private:
	yasli::string& str_;
};

}

YASLI_STRING_NAMESPACE_BEGIN

inline bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, yasli::string& value, const char* name, const char* label)
{
	yasli::StringSTL str(value);
	return ar(static_cast<yasli::StringInterface&>(str), name, label);
}

YASLI_STRING_NAMESPACE_END

// ---------------------------------------------------------------------------
namespace std {

#if YASLI_CXX11
template<class K, class V, class C, class Alloc>
bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, std::map<K, V, C, Alloc>& container, const char* name, const char* label)
{
	std::vector<std::pair<K, V> > temp(std::make_move_iterator(container.begin()), std::make_move_iterator(container.end()));
	bool result = !ar(temp, name, label);
	
	container.clear();
	container.insert(std::make_move_iterator(temp.begin()), std::make_move_iterator(temp.end()));
	return result;
}

template<class K, class V, class C, class Alloc>
bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, std::unordered_map<K, V, C, Alloc>& container, const char* name, const char* label)
{
	std::vector<std::pair<K, V> > temp(std::make_move_iterator(container.begin()), std::make_move_iterator(container.end()));
	bool result = !ar(temp, name, label);

	container.clear();
	container.insert(std::make_move_iterator(temp.begin()), std::make_move_iterator(temp.end()));
	return result;
}
#else
	template<class K, class V, class C, class Alloc>
	bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, std::map<K, V, C, Alloc>& container, const char* name, const char* label)
	{
		std::vector<std::pair<K, V> > temp(container.begin(), container.end());
		if (!ar(temp, name, label))
			return false;

		container.clear();
		container.insert(temp.begin(), temp.end());
		return true;
	}
#endif

}
// ---------------------------------------------------------------------------

namespace yasli {

class WStringSTL : public WStringInterface
{
public:
	WStringSTL(yasli::wstring& str) : str_(str) { }

	void set(const wchar_t* value) { str_ = value; }
	const wchar_t* get() const { return str_.c_str(); }
	const void* handle() const { return &str_; }
	TypeID type() const { return TypeID::get<wstring>(); }
private:
	yasli::wstring& str_;
};

}

YASLI_STRING_NAMESPACE_BEGIN

inline bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, yasli::wstring& value, const char* name, const char* label)
{
	yasli::WStringSTL str(value);
	return ar(static_cast<yasli::WStringInterface&>(str), name, label);
}

YASLI_STRING_NAMESPACE_END

// ---------------------------------------------------------------------------

namespace yasli {

template<class V>
struct StdStringPair : yasli::KeyValueInterface
{
	const char* get() const { return pair_.first.c_str(); }
	void set(const char* key) { pair_.first.assign(key); }
	const void* handle() const { return &pair_; }
	yasli::TypeID type() const { return yasli::TypeID::get<string>(); }
	bool serializeValue(yasli::Archive& ar, const char* name, const char* label) 
	{
		return ar(pair_.second, name, label);
	}

	StdStringPair(std::pair<yasli::string, V>& pair)
	: pair_(pair)
	{
	}


	std::pair<yasli::string, V>& pair_;
};

template<class K, class V>
struct StdPair : std::pair<K, V>
{
	void YASLI_SERIALIZE_METHOD(yasli::Archive& ar) 
	{
#if YASLI_STD_PAIR_FIRST_SECOND
		ar(this->first, "first", "^Key");
		ar(this->second, "second", "^Value");
#else
		ar(this->first, "key", "Key");
		ar(this->second, "value", "Value");
#endif
	}
};

}

namespace std{

#if YASLI_CXX11
template<class T>
bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, std::shared_ptr<T>& ptr, const char* name, const char* label)
{
	yasli::StdSharedPtrSerializer<T> serializer(ptr);
	return ar(static_cast<yasli::PointerInterface&>(serializer), name, label);
}

template<class T, class Del>
bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, std::unique_ptr<T, Del>& ptr, const char* name, const char* label)
{
	yasli::StdUniquePtrSerializer<T, Del> serializer(ptr);
	return ar(static_cast<yasli::PointerInterface&>(serializer), name, label);
}
#endif

#if !YASLI_NO_MAP_AS_DICTIONARY
template<class V>
bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, std::pair<yasli::string, V>& pair, const char* name, const char* label)
{
	yasli::StdStringPair<V> keyValue(pair);
	return ar(static_cast<yasli::KeyValueInterface&>(keyValue), name, label);
}
#endif

template<class K, class V>
bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, std::pair<K, V>& pair, const char* name, const char* label)
{
	return ar(static_cast<yasli::StdPair<K, V>&>(pair), name, label);
}

}

#pragma warning(pop)
