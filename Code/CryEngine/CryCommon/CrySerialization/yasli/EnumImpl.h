/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#include <CrySerialization/yasli/Config.h>

#if !YASLI_YASLI_INLINE_IMPLEMENTATION
//#include "StdAfx.h"
#endif

#include "STL.h"
#include "Enum.h"
#include "Archive.h"
#include "StringList.h"

namespace yasli{

YASLI_INLINE void EnumDescription::add(int value, const char* name, const char *label)
{
    YASLI_ESCAPE( name, return );
	nameToValue_[name] = value;
    if (label)
        labelToValue_[label] = value;
	valueToName_[value] = name;
    if (label)
        valueToLabel_[value] = label;
	valueToIndex_[value] = int(names_.size());
	names_.push_back(name);
	if (label)
		labels_.push_back(label);
	values_.push_back(value);
}

YASLI_INLINE bool EnumDescription::serialize(Archive& ar, int& value, const char* name, const char* label) const
{
	if (!ar.isInPlace()) {
		if(count() == 0){
			YASLI_ASSERT(0 && "Attempt to serialize enum type that is not registered with YASLI_ENUM macro");
			return false;
		}
		int index = -1;
		if(ar.isOutput())
			index = indexByValue(value);
		StringListStaticValue stringListValue(ar.isEdit() ? labels() : names(), index, &value, type());
		if (!ar(stringListValue, name, label))
			return false;
		if(ar.isInput()){
			if (stringListValue.index() == StringListStatic::npos) {
				ar.error(&value, type(), "Unable to load unregistered enumeration value of %s (label %s).", name, label);
				return false;
			}
			value = ar.isEdit() ? valueByLabel(stringListValue.c_str()) : this->value(stringListValue.c_str());
		}
		else if(index == -1)
			ar.error(&value, type(), "Unregistered or uninitialized enumeration value of %s (label %s).", name, label);
		return true;
	}
	else {
		return ar(value, name, label);
	}
}

YASLI_INLINE bool EnumDescription::serializeBitVector(Archive& ar, int& value, const char* name, const char* label) const
{
    if(ar.isOutput())
    {
        StringListStatic names = nameCombination(value);
		string str;
        joinStringList(&str, names, '|');
        return ar(str, name, label);
    }
    else
    {
		string str;
        if(!ar(str, name, label))
            return false;
        StringList values;
        splitStringList(&values, str.c_str(), '|');
        StringList::iterator it;
        value = 0;
        for(it = values.begin(); it != values.end(); ++it)
			if(!it->empty())
				value |= this->value(it->c_str());
		return true;
    }
}


YASLI_INLINE const char* EnumDescription::name(int value) const
{
    ValueToName::const_iterator it = valueToName_.find(value);
    YASLI_ESCAPE(it != valueToName_.end(), return "");
    return it->second;
}
YASLI_INLINE const char* EnumDescription::label(int value) const
{
    ValueToLabel::const_iterator it = valueToLabel_.find(value);
    YASLI_ESCAPE(it != valueToLabel_.end(), return "");
    return it->second;
}

YASLI_INLINE StringListStatic EnumDescription::nameCombination(int bitVector) const 
{
    StringListStatic strings;
    for(ValueToName::const_iterator i = valueToName_.begin(); i != valueToName_.end(); ++i)
        if((bitVector & i->first) == i->first){
            bitVector &= ~i->first;
            strings.push_back(i->second);
        }
	YASLI_ASSERT(!bitVector && "Unregistered enum value");
    return strings;
}

YASLI_INLINE StringListStatic EnumDescription::labelCombination(int bitVector) const 
{
    StringListStatic strings;
    for(ValueToLabel::const_iterator i = valueToLabel_.begin(); i != valueToLabel_.end(); ++i)
        if(i->second && (bitVector & i->first) == i->first){
            bitVector &= ~i->first;
            strings.push_back(i->second);
        }
	YASLI_ASSERT(!bitVector && "Unregistered enum value");
	return strings;
}


YASLI_INLINE int EnumDescription::indexByValue(int value) const
{
	if(!YASLI_CHECK(!valueToIndex_.empty()))
		return -1;
    ValueToIndex::const_iterator it = valueToIndex_.find(value);
	if(YASLI_CHECK(it != valueToIndex_.end()))
		return it->second;
	else
		return -1;
}

YASLI_INLINE int EnumDescription::valueByIndex(int index) const
{
	if(!YASLI_CHECK(!values_.empty()))
		return 0;
	if(YASLI_CHECK(size_t(index) < values_.size()))
		return values_[index];
	return values_[0];
}

YASLI_INLINE const char* EnumDescription::nameByIndex(int index) const
{
	if(!YASLI_CHECK(!names_.empty()))
		return "";
	if(YASLI_CHECK(StringListStatic::size_type(index) < names_.size()))
		return names_[index];
	else
		return "";
}

YASLI_INLINE const char* EnumDescription::labelByIndex(int index) const
{
	if(!YASLI_CHECK(!labels_.empty()))
		return "";
	if(YASLI_CHECK(StringListStatic::size_type(index) < labels_.size()))
		return labels_[index];
	else
		return "";
}

YASLI_INLINE int EnumDescription::value(const char* name) const
{
	if(!YASLI_CHECK(!nameToValue_.empty()))
		return 0;
	NameToValue::const_iterator it = nameToValue_.find(name);
	if(!YASLI_CHECK(it != nameToValue_.end()))
		it = nameToValue_.begin();
	return it->second;
}

YASLI_INLINE int EnumDescription::valueByLabel(const char* label) const
{
	if(!YASLI_CHECK(!labelToValue_.empty()))
		return 0;
	LabelToValue::const_iterator it = labelToValue_.find(label);
	if(!YASLI_CHECK(it != labelToValue_.end()))
		it = labelToValue_.begin();
	return it->second;
}

}
// vim:ts=4 sw=4:
