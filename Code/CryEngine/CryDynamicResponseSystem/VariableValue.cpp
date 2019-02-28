// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include <CryString/StringUtils.h>
#include "VariableValue.h"
#include <CrySerialization/Enum.h>
#include "Response.h"

using namespace CryDRS;

const VariableValueData CVariableValue::NEG_INFINITE = INT_MIN;
const VariableValueData CVariableValue::POS_INFINITE = INT_MAX;
const VariableValueData CVariableValue::DEFAULT_VALUE = 0;  //remark: the default value is 0, so a variable which was never set to anything will have the value 0

SERIALIZATION_ENUM_BEGIN(EDynamicResponseVariableType, "Attachment Type")
SERIALIZATION_ENUM(eDRVT_None, "none", "None")
SERIALIZATION_ENUM(eDRVT_Int, "int", "Int")
SERIALIZATION_ENUM(eDRVT_Float, "float", "Float")
SERIALIZATION_ENUM(eDRVT_String, "string", "String")
SERIALIZATION_ENUM(eDRVT_Boolean, "boolean", "Boolean")
SERIALIZATION_ENUM(eDRVT_PosInfinite, "posInfinite", "PosInfinite")
SERIALIZATION_ENUM(eDRVT_NegInfinite, "negInfinite", "NegInfinite")
SERIALIZATION_ENUM(eDRVT_Undefined, "undefined", "Undefined")
SERIALIZATION_ENUM_END()

//--------------------------------------------------------------------------------------------------
CVariableValue::CVariableValue(const CVariableValue& value)
{
	m_value = value.m_value;
#if defined(ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
	SetTypeInfoOfValue(value.m_type);
	m_hashedText = value.m_hashedText;
#endif
}

//--------------------------------------------------------------------------------------------------
CVariableValue::CVariableValue(const CHashedString& value)
{
	m_value = static_cast<int>(value.GetHash());
#if defined(ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
	SetTypeInfoOfValue(eDRVT_String);
	m_hashedText = value.GetText();
#endif
}

//--------------------------------------------------------------------------------------------------
bool CVariableValue::DoTypesMatch(const CVariableValue& other) const
{
#if defined(ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
	return
	  m_type == other.m_type ||
	  m_type == eDRVT_PosInfinite ||
	  other.m_type == eDRVT_PosInfinite ||
	  m_type == eDRVT_NegInfinite ||
	  other.m_type == eDRVT_NegInfinite ||
	  m_type == eDRVT_None ||
	  other.m_type == eDRVT_None ||
	  other.m_type == eDRVT_Undefined ||
	  m_type == eDRVT_Undefined;
#else
	return true;  //no way to check
#endif
}

//--------------------------------------------------------------------------------------------------
EDynamicResponseVariableType CVariableValue::GetType() const
{
#if defined(ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
	return m_type;
#else
	return eDRVT_None;
#endif
}

//--------------------------------------------------------------------------------------------------
int CVariableValue::GetValueAsInt() const
{
#if defined(ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
	if (m_type != eDRVT_Int)
	{
		DrsLogWarning((string("Tried to get value as a INT from a non-integer variable, type of the variable is actually: ") + GetTypeAsString()).c_str());
	}
#endif
	return m_value;
}

//--------------------------------------------------------------------------------------------------
float CVariableValue::GetValueAsFloat() const
{
#if defined(ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
	if (m_type != eDRVT_Float)
	{
		DrsLogWarning((string("Tried to get value as a FLOAT from a non-integer variable, type of the variable is actually: ") + GetTypeAsString()).c_str());
	}
#endif
	return m_value / 100.0f;
}

//--------------------------------------------------------------------------------------------------
bool CVariableValue::GetValueAsBool() const
{
#if defined(ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
	if (m_type != eDRVT_Boolean)
	{
		DrsLogWarning((string("Tried to get value as a BOOL from a non-bool variable, type of the variable is actually: ") + GetTypeAsString()).c_str());
	}
#endif
	return m_value != 0;
}

//--------------------------------------------------------------------------------------------------
CHashedString CVariableValue::GetValueAsHashedString() const
{
#if defined(ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
	if (m_type != eDRVT_String)
	{
		DrsLogWarning((string("Tried to get value as a HashedString from a non-string variable, type of the variable is actually: ") + GetTypeAsString()).c_str());
	}
	return m_hashedText;
#endif
	return CHashedString((uint32)m_value);
}

//--------------------------------------------------------------------------------------------------
const char* CVariableValue::GetTypeAsString() const
{
#if defined(ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
	switch (m_type)
	{
	case eDRVT_Int:
		return "Integer";
	case eDRVT_PosInfinite:
		return "PosInfinite";
	case eDRVT_NegInfinite:
		return "NegInfinite";
	case eDRVT_Float:
		return "Float";
	case eDRVT_String:
		return "String";
	case eDRVT_Boolean:
		return "Boolean";
	case eDRVT_Undefined:
		return "Undefined";
	case eDRVT_None:
		return "None";
	default:
		return "unknown";
	}
#else
	return "stripped";
#endif
}

//--------------------------------------------------------------------------------------------------
string CVariableValue::GetValueAsString() const //should be used for debug output only
{
#if defined(ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
	switch (m_type)
	{
	case eDRVT_Int:
		return CryStringUtils::toString(m_value);
	case eDRVT_Float:
		return CryStringUtils::toString(m_value / 100.0f);
	case eDRVT_String:
		return m_hashedText;
	case eDRVT_Boolean:
		return (m_value != 0) ? "TRUE" : "FALSE";
	}
#endif
	return CryStringUtils::toString(m_value);
}

//--------------------------------------------------------------------------------------------------
CVariableValue CVariableValue::operator+(const CVariableValue& other) const
{
	CVariableValue temp = *this;
#if defined(ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
	CRY_ASSERT(DoTypesMatch(other));
	if (m_type == eDRVT_Undefined)
		temp.m_type = other.m_type;  //remove and replace with WarnIfTypeDiffers(other);?
#endif
	temp.m_value += other.m_value;
	return temp;
}

//--------------------------------------------------------------------------------------------------
CVariableValue CVariableValue::operator-(const CVariableValue& other) const
{
	CVariableValue temp = *this;
#if defined(ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
	CRY_ASSERT(DoTypesMatch(other));
	if (m_type == eDRVT_Undefined)
		temp.m_type = other.m_type;  //remove and replace with WarnIfTypeDiffers(other);?
#endif
	temp.m_value -= other.m_value;
	return temp;
}

//--------------------------------------------------------------------------------------------------
CVariableValue CVariableValue::operator-() const
{
	CVariableValue temp(*this);
	temp.m_value = -temp.m_value;
	return temp;
}

//--------------------------------------------------------------------------------------------------
void CVariableValue::Serialize(Serialization::IArchive& ar)
{
	if (ar.caps(Serialization::IArchive::BINARY))
	{
		ar(m_value, "value", 0);
#if defined (ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
		if (ar.isInput())
		{
			SetTypeInfoOfValue(eDRVT_Undefined);
		}
#endif
		return;
	}
	else
	{
		if (ar.isOutput())
		{
#if defined(ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
			EDynamicResponseVariableType type = m_type;
#else
			EDynamicResponseVariableType type = eDRVT_None;
#endif
			if (type == eDRVT_None || type == eDRVT_Int)
			{
				ar(m_value, "value", "^>110>");
			}
			else if (type == eDRVT_Float)
			{
				float temp = m_value / 100.0f;
				ar(temp, "value", "^>110>");
			}
			else if (type == eDRVT_Boolean)
			{
				if (m_value != 0)
				{
					bool bValue = true;
					ar(bValue, "value", "^>110>(TRUE)");
				}
				else
				{
					bool bValue = false;
					ar(bValue, "value", "^>110>(FALSE)");
				}

			}
			else
			{
				string valueAsString;
				valueAsString = GetValueAsString();
				ar(valueAsString, "value", "^>130>");
			}
			ar(type, "Type", "^>");
		}
		else
		{
			EDynamicResponseVariableType type = eDRVT_None;
			ar(type, "Type", "^!Type");

#if defined(ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
			if (ar.isEdit() && type == m_type && type < eDRVT_PosInfinite)  //if the type was changed we simply reset the value (otherwise we would try to interpret float data as integer for example)
			{
				m_value = DEFAULT_VALUE;
			}
			else
#endif
			{
				switch (type)
				{
				case eDRVT_Int:
				case eDRVT_None:
					ar(m_value, "value", "^");
					break;
				case eDRVT_Boolean:
					{
						bool bTemp;
						ar(bTemp, "value", "^");
						m_value = (bTemp) ? 1 : 0;
						break;
					}
				case eDRVT_Float:
					{
						float temp;
						ar(temp, "value", "^");
						m_value = (int)(temp * 100.0f); // CVariableValue(temp).GetValue();
						break;
					}
				case eDRVT_PosInfinite:
				case eDRVT_NegInfinite:
				case eDRVT_Undefined:
					if (type == eDRVT_PosInfinite)
					{
						SetValue(POS_INFINITE);
					}
					else if (type == eDRVT_NegInfinite)
					{
						SetValue(NEG_INFINITE);
					}
					else if (type == eDRVT_Undefined)
					{
						Reset();
					}
#if defined(ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
					if (ar.isEdit() && type == m_type)  //in the editor we allow changing this types by providing a new value, to allow the user to set these variables to an actual value
					{
						string valueAsString;
						ar(valueAsString, "value", "^");
						if (valueAsString == "PosInfinite")
							SetValue(POS_INFINITE);
						else if (valueAsString == "NegInfinite")
							SetValue(NEG_INFINITE);
						else if (valueAsString == "Undefined")
							Reset();
						else if (!valueAsString.empty())
						{
							CConditionParserHelper::GetResponseVariableValueFromString(valueAsString.c_str(), this);
							type = m_type;
						}
					}
#endif
					break;
				case eDRVT_String:
					{
						string temp;
						ar(temp, "value", "^");
						m_value = CHashedString(temp).GetHash();
#if defined(ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
						m_hashedText = temp;
#endif
						break;
					}
				default:
					{
						DrsLogError("Variable value type not yet implemented!");
					}
				}
			}

#if defined(ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
			m_type = type;
#endif
		}
	}
}

//--------------------------------------------------------------------------------------------------
CVariableValue& CVariableValue::operator+=(const CVariableValue& other)
{
#if defined(ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
	CRY_ASSERT(DoTypesMatch(other));
	if (m_type == eDRVT_Undefined)
		m_type = other.m_type;  //remove and replace with WarnIfTypeDiffers(other);?
#endif
	m_value += other.m_value;
	return *this;
}

//--------------------------------------------------------------------------------------------------
CVariableValue& CVariableValue::operator-=(const CVariableValue& other)
{
#if defined(ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
	CRY_ASSERT(DoTypesMatch(other));
	if (m_type == eDRVT_Undefined)
		m_type = other.m_type;  //remove and replace with WarnIfTypeDiffers(other);?
#endif
	m_value -= other.m_value;
	return *this;
}
