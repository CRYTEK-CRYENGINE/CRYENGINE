// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/************************************************************************

   A DynamicResponseVariableValue is the actual Value of a Variable, it can be of type string/int/float. All values are converted and stored in a int

   /************************************************************************/

#pragma once

//in this implementation we use a (some may call it hacky) way to store everything as a int. For strings we simply store the generated hash value, and for floats we multiply them with 100 and store that result as a int
//(so 1.234f would become 123 and 0.001 would become 0, and 1234.567f would become 123456) it costs some precision (correct to two decimal places), but it helps with the comparison of float values (Stuff like 0.6 != 0.60000001)
//and we dont need to store the actual type of the data. And we dont need branching of any kind if we compare values.
//For debug purposes we do actually store the type, so that we can check if the variable-types on one condition (the values to check and the variable) do match.

namespace CryDRS
{
enum EDynamicResponseVariableType
{
	eDRVT_None        = 0,
	eDRVT_Int         = 1,
	eDRVT_Float       = 2,
	eDRVT_String      = 3,
	eDRVT_Boolean     = 4,

	eDRVT_PosInfinite = 16,
	eDRVT_NegInfinite = 17,

	eDRVT_Undefined   = 31,
};

#if !defined (_RELEASE)
	#define ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS
#endif

//the actual stored data.
typedef int VariableValueData;

class CVariableValue
{
public:
	static const VariableValueData POS_INFINITE;    //we need these two for conditions like VALUE < x, because we do all of our checks like (y < VALUE && VALUE < x), so in this case we would just define y = INT_MIN, which is then always true
	static const VariableValueData NEG_INFINITE;
	static const VariableValueData DEFAULT_VALUE;

#if defined (ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
	#define SetTypeInfoOfValue(x)    if (m_value == POS_INFINITE) m_type = eDRVT_PosInfinite; else if (m_value == NEG_INFINITE) \
	  m_type = eDRVT_NegInfinite; else m_type = x; m_hashedText.clear();
	#define WarnIfTypeDiffers(other) if (!DoTypesMatch(other)) { DrsLogWarning("Compared two VariableValues with different types"); }
#else
	#define SetTypeInfoOfValue(x)
	#define WarnIfTypeDiffers(other)
#endif

	CVariableValue() : m_value(DEFAULT_VALUE) { SetTypeInfoOfValue(eDRVT_Undefined); }
	CVariableValue(int value) { m_value = value; SetTypeInfoOfValue(eDRVT_Int); }
	CVariableValue(bool value) { m_value = (value) ? 1 : 0; SetTypeInfoOfValue(eDRVT_Boolean); }
	CVariableValue(float value) { m_value = (int)(value * 100.0f); SetTypeInfoOfValue(eDRVT_Float); }   //do we want/need to roundf this value?
	CVariableValue(const CVariableValue& value);
	CVariableValue(const CHashedString& value);

	bool            operator<=(const CVariableValue& other) const { WarnIfTypeDiffers(other); return m_value <= other.m_value; }
	bool            operator>=(const CVariableValue& other) const { WarnIfTypeDiffers(other); return m_value >= other.m_value; }
	bool            operator<(const CVariableValue& other) const  { WarnIfTypeDiffers(other); return m_value < other.m_value; }
	bool            operator>(const CVariableValue& other) const  { WarnIfTypeDiffers(other); return m_value > other.m_value; }
	bool            operator==(const CVariableValue& other) const { WarnIfTypeDiffers(other); return m_value == other.m_value; }
	CVariableValue  operator+(const CVariableValue& other) const;
	CVariableValue  operator-(const CVariableValue& other) const;
	CVariableValue& operator+=(const CVariableValue& other);
	CVariableValue& operator-=(const CVariableValue& other);
	CVariableValue  operator-() const;

	EDynamicResponseVariableType GetType() const;
	bool                         DoTypesMatch(const CVariableValue& other) const;
	const char*              GetTypeAsString() const;
	void                     Reset()                           { m_value = DEFAULT_VALUE; SetTypeInfoOfValue(eDRVT_Undefined) }

	const VariableValueData& GetValue() const                  { return m_value; }
	void                     SetValue(VariableValueData value) { m_value = value; SetTypeInfoOfValue(m_type) }
	CHashedString            GetValueAsHashedString() const;
	int                      GetValueAsInt() const;
	float                    GetValueAsFloat() const;
	bool                     GetValueAsBool() const;
	string                   GetValueAsString() const;                    //should be used for debug output only

	void                     Serialize(Serialization::IArchive & ar);

private:
	VariableValueData m_value;    //holds the converted float or the string-hash or the actual int value

#if defined(ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
	EDynamicResponseVariableType m_type;
public:
	string                       m_hashedText; //in debug mode we also store a copy of the hashed string, instead of just storing the hash, for debug display only
#endif
};
}  //namespace  CryDRS
