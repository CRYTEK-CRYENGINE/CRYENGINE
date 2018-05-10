/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once

#include <vector>
#include <map>

#include <CrySerialization/yasli/StringList.h>
#include <CrySerialization/yasli/TypeID.h>

namespace yasli{

class Archive;

struct LessStrCmp
{
	bool operator()(const char* l, const char* r) const{
		return strcmp(l, r) < 0;
	}
};

class EnumDescription{
public:
	EnumDescription(TypeID typeId)
		: type_(typeId)
	{}

	int value(const char* name) const;
	int valueByIndex(int index) const;
	int valueByLabel(const char* label) const;
	const char* name(int value) const;
	const char* nameByIndex(int index) const;
	const char* label(int value) const;
	const char* labelByIndex(int index) const;
	const char* indexByName(const char* name) const;
	int indexByValue(int value) const;

	bool serialize(Archive& ar, int& value, const char* name, const char* label) const;
	bool serializeBitVector(Archive& ar, int& value, const char* name, const char* label) const;

	void add(int value, const char* name, const char* label = ""); // TODO
	int count() const{ return (int)values_.size(); }
	const StringListStatic& names() const{ return names_; }
	const StringListStatic& labels() const{ return labels_; }
	StringListStatic nameCombination(int bitVector) const;
	StringListStatic labelCombination(int bitVector) const;
	bool registered() const { return !names_.empty(); }
	TypeID type() const{ return type_; }
private:
	StringListStatic names_;
	StringListStatic labels_;

	typedef std::map<const char*, int, LessStrCmp> NameToValue;
	NameToValue nameToValue_;
	typedef std::map<const char*, int, LessStrCmp> LabelToValue;
	LabelToValue labelToValue_;
	typedef std::map<int, int> ValueToIndex;
	ValueToIndex valueToIndex_;
	typedef std::map<int, const char*> ValueToName;
	ValueToName valueToName_;
	typedef std::map<int, const char*> ValueToLabel;
	ValueToName valueToLabel_;
	std::vector<int> values_;
	TypeID type_;
};

template<class Enum>
class EnumDescriptionImpl : public EnumDescription{
public: 
	EnumDescriptionImpl(TypeID typeId)
		: EnumDescription(typeId)
	{}

	static EnumDescription& the(){
		static EnumDescriptionImpl description(TypeID::get<Enum>());
		return description;
	}
};

template<class Enum>
EnumDescription& getEnumDescription(){
	return EnumDescriptionImpl<Enum>::the();
}

inline bool serializeEnum(const EnumDescription& desc, Archive& ar, int& value, const char* name, const char* label){
	return desc.serialize(ar, value, name, label);
}

}

#define YASLI_ENUM_BEGIN(Type, label)                                                \
	static bool registerEnum_##Type();                                               \
	static bool Type##_enum_registered = registerEnum_##Type();                      \
	static bool registerEnum_##Type(){                                               \
		yasli::EnumDescription& description = yasli::EnumDescriptionImpl<Type>::the();

#define YASLI_ENUM_BEGIN_NESTED(Class, Enum, label)                                  \
	static bool registerEnum_##Class##_##Enum();                                     \
	static bool Class##_##Enum##_enum_registered = registerEnum_##Class##_##Enum();  \
	static bool registerEnum_##Class##_##Enum(){                                     \
		yasli::EnumDescription& description = yasli::EnumDescriptionImpl<Class::Enum>::the();

#define YASLI_ENUM_BEGIN_NESTED2(Class, Class1, Enum, label)                                        \
	static bool registerEnum_##Class##Class1##_##Enum();                                            \
	static bool Class##Class1##_##Enum##_enum_registered = registerEnum_##Class##Class1##_##Enum(); \
	static bool registerEnum_##Class##Class1##_##Enum(){                                            \
	yasli::EnumDescription& description = yasli::EnumDescriptionImpl<Class::Class1::Enum>::the();

                                                                                    
#define YASLI_ENUM_VALUE(value, label)                                              \
		description.add(int(value), #value, label);                                      

#define YASLI_ENUM(value, name, label)                                              \
		description.add(int(value), name, label);                                      

#define YASLI_ENUM_VALUE_NESTED(Class, value, label)                                       \
	description.add(int(Class::value), #value, label);                                      

#define YASLI_ENUM_VALUE_NESTED2(Class, Class1, value, label)                                       \
	description.add(int(Class::Class1::value), #value, label);                                      


#define YASLI_ENUM_END()													        \
		return true;                                                            \
	};

// vim:ts=4 sw=4:
//
#if YASLI_INLINE_IMPLEMENTATION
#include "EnumImpl.h"
#endif
