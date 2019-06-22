/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once

#include <cstring>
#include <CrySerialization/yasli/Assert.h>
#include <CrySerialization/yasli/Config.h>
#include <CrySerialization/yasli/TypeID.h>

namespace yasli{

class Archive;
class StringListStatic : public StringListStaticBase{
public:
	static StringListStatic& emptyValue()
	{
		static StringListStatic e;
		return e;
	}

	enum { npos = -1 };
	int find(const char* value) const{
		int numItems = int(size());
		for(int i = 0; i < numItems; ++i){
			if(strcmp((*this)[i], value) == 0)
				return i;
		}
		return npos;
	}
};

class StringListStaticValue{
public:
	StringListStaticValue()
	: stringList_(&StringListStatic::emptyValue())
	, index_(-1)
	{
        handle_ = this;
	}

	StringListStaticValue(const StringListStaticValue& original)
	: stringList_(original.stringList_)
	, index_(original.index_)
	{
	}
    StringListStaticValue(const StringListStatic& stringList, int value)
    : stringList_(&stringList)
    , index_(value)
    {
        handle_ = this;
    }
    StringListStaticValue(const StringListStatic& stringList, int value, const void* handle, const yasli::TypeID& type)
    : stringList_(&stringList)
    , index_(value)
    , handle_(handle)
    , type_(type)
    {
    }
    StringListStaticValue(const StringListStatic& stringList, const char* value, const void* handle, const yasli::TypeID& type)
    : stringList_(&stringList)
    , index_(stringList.find(value))
    , handle_(handle)
    , type_(type)
    {
    }
    StringListStaticValue& operator=(const char* value){
        index_ = stringList_->find(value);
		return *this;
    }
    StringListStaticValue& operator=(int value){
		YASLI_ASSERT(value >= 0 && StringListStatic::size_type(value) < stringList_->size());
        index_ = value;
		return *this;
    }
    StringListStaticValue& operator=(const StringListStaticValue& rhs){
        stringList_ = rhs.stringList_;
		index_ = rhs.index_;
        return *this;
    }
    const char* c_str() const{
        if(index_ >= 0 && StringListStatic::size_type(index_) < stringList_->size())
			return (*stringList_)[index_];
		else
			return "";
    }
    int index() const{ return index_; }
    const void* handle() const{ return handle_; }
    yasli::TypeID type() const { return type_; }
    const StringListStatic& stringList() const{ return *stringList_; }
    template<class Archive>
    void YASLI_SERIALIZE_METHOD(Archive& ar) {
        ar(index_, "index");
    }
protected:
    const StringListStatic* stringList_;
    int index_;
    const void* handle_;
    yasli::TypeID type_;
};

class StringList: public StringListBase{
public:
    StringList() {}
    StringList(const StringList& rhs){
        *this  = rhs;
    }
	StringList& operator=(const StringList& rhs)
	{
		// As StringList crosses dll boundaries it is important to copy strings
		// rather than reference count them to be sure that stored CryString uses
		// proper allocator.
		resize(rhs.size());
		for (size_t i = 0; i < size_t(size()); ++i)
			(*this)[i] = rhs[i].c_str();
		return *this;
	}
    StringList(const StringListStatic& rhs){
        const int size = int(rhs.size());
        resize(size);
        for(int i = 0; i < int(size); ++i)
            (*this)[i] = rhs[i];
    }
    enum { npos = -1 };
    int find(const char* value) const{
		const int numItems = int(size());
		for(int i = 0; i < numItems; ++i){
            if((*this)[i] == value)
                return i;
        }
        return npos;
    }
};

class StringListValue{
public:
	explicit StringListValue(const StringListStaticValue &value)
	{
		stringList_.resize(value.stringList().size());
		for (size_t i = 0; i < size_t(stringList_.size()); ++i)
			stringList_[i] = value.stringList()[i];
		index_ = value.index();
	}
	StringListValue(const StringListValue &value)
	{
		stringList_ = value.stringList_;
		index_ = value.index_;
	}
	StringListValue()
	: index_(StringList::npos)
	{
		handle_ = this;
	}
	StringListValue(const StringList& stringList, int value)
	: stringList_(stringList)
	, index_(value)
	{
		handle_ = this;
	}
	StringListValue(const StringList& stringList, int value, const void* handle, const yasli::TypeID& typeId)
	: stringList_(stringList)
	, index_(value)
	, handle_(handle)
	, type_(typeId)
	{
	}
	StringListValue(const StringList& stringList, const char* value)
	: stringList_(stringList)
	, index_(stringList.find(value))
	{
		handle_ = this;
	}
	StringListValue(const StringList& stringList, const char* value, const void* handle, const yasli::TypeID& typeId)
	: stringList_(stringList)
	, index_(stringList.find(value))
	, handle_(handle)
	, type_(typeId)
	{
	}
	StringListValue(const StringListStatic& stringList, const char* value)
	: stringList_(stringList)
	, index_(stringList.find(value))
	{
		handle_ = this;
	}
	StringListValue& operator=(const char* value){
		index_ = stringList_.find(value);
		return *this;
	}
	StringListValue& operator=(int value){
		YASLI_ASSERT(value >= 0 && size_t(value) < size_t(stringList_.size()));
		index_ = value;
		return *this;
	}
	const char* c_str() const{
		if(index_ >= 0 && size_t(index_) < size_t(stringList_.size()))
			return stringList_[index_].c_str();
		else
			return "";
	}
	int index() const{ return index_; }
	const void* handle() const { return handle_; }
	yasli::TypeID type() const { return type_; }
	const StringList& stringList() const{ return stringList_; }
	template<class Archive>
	void Serialize(Archive& ar) {
		ar(index_, "index");
		ar(stringList_, "stringList");
	}
protected:
	StringList stringList_;
	int index_;
	const void* handle_;
	yasli::TypeID type_;
};

class Archive;

void splitStringList(StringList* result, const char *str, char sep);
void joinStringList(string* result, const StringList& stringList, char sep);
void joinStringList(string* result, const StringListStatic& stringList, char sep);

bool YASLI_SERIALIZE_OVERRIDE(Archive& ar, StringList& value, const char* name, const char* label);
bool YASLI_SERIALIZE_OVERRIDE(Archive& ar, StringListValue& value, const char* name, const char* label);
bool YASLI_SERIALIZE_OVERRIDE(Archive& ar, StringListStaticValue& value, const char* name, const char* label);

}

#if YASLI_INLINE_IMPLEMENTATION
#include "StringListImpl.h"
#endif
