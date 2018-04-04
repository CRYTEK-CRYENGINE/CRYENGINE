/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once
#include <map>

#include <CrySerialization/yasli/Assert.h>
#include <CrySerialization/yasli/TypeID.h>
#include <CrySerialization/yasli/Config.h>

namespace yasli {

class TypeDescription{
public:
	TypeDescription(const TypeID& typeID, const char* name, const char *label)
	: name_(name)
	, label_(label)
	, typeID_(typeID)
	{
#if YASLI_NO_RTTI
#if 0
		const size_t bufLen = sizeof(typeID.typeInfo_->name);
		strncpy(typeID.typeInfo_->name, name, bufLen - 1);
		typeID.typeInfo_->name[bufLen - 1] = '\0';
#endif
#endif
	}
	const char* name() const{ return name_; }
	const char* label() const{ return label_; }
	TypeID typeID() const{ return typeID_; }

protected:
	const char* name_;
	const char* label_;
	TypeID typeID_;
};

class ClassFactoryBase{
public: 
	ClassFactoryBase(TypeID baseType)
	: baseType_(baseType)
	, nullLabel_(0)
	{
	}

	virtual ~ClassFactoryBase() {}

	virtual size_t size() const = 0;
	virtual const TypeDescription* descriptionByIndex(int index) const = 0;	
	virtual const TypeDescription* descriptionByRegisteredName(const char* typeName) const = 0;
	virtual const char* findAnnotation(const char* registeredTypeName, const char* annotationName) const = 0;
	virtual void serializeNewByIndex(Archive& ar, int index, const char* name, const char* label) = 0;

	bool setNullLabel(const char* label){ nullLabel_ = label ? label : ""; return true; }
	const char* nullLabel() const{ return nullLabel_; }
protected:
	TypeID baseType_;
	const char* nullLabel_;
};


struct TypeNameWithFactory
{
	string registeredName;
	ClassFactoryBase* factory;

	TypeNameWithFactory(const char* registeredName, ClassFactoryBase* factory = 0)
	: registeredName(registeredName)
	, factory(factory)
	{
	}
};

YASLI_INLINE bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, yasli::TypeNameWithFactory& value, const char* name, const char* label);

}

// vim:ts=4 sw=4:
