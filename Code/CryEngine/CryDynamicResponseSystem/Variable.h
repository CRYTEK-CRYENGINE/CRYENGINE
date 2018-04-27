// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/************************************************************************

   A Variable that can hold some information off the current game state

   A DynamicResponseVariable is a tuple of VariableName and the current value

   /************************************************************************/

#pragma once

#include "VariableValue.h"
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>
#include <CryString/HashedString.h>

namespace CryDRS
{
class CVariableCollection;
class CResponseInstance;

class CVariable final : public DRS::IVariable
{
public:
	CVariable() = default;
	CVariable(const CHashedString& name) : m_name(name) {}
	CVariable(const CHashedString& name, const CVariableValue& value) : m_name(name), m_value(value) {}

	//////////////////////////////////////////////////////////
	// DRS::IVariable implementation
	virtual const CHashedString& GetName() const override                         { return m_name; }

	virtual void                 SetValue(int newValue) override                  { m_value = CVariableValue(newValue); }
	virtual void                 SetValue(float newValue) override                { m_value = CVariableValue(newValue); }
	virtual void                 SetValue(const CHashedString& newValue) override { m_value = CVariableValue(newValue); }
	virtual void                 SetValue(bool newValue) override                 { m_value = CVariableValue(newValue); }

	virtual int                  GetValueAsInt() const override                   { return m_value.GetValueAsInt(); };
	virtual float                GetValueAsFloat() const override                 { return m_value.GetValueAsFloat(); };
	virtual CHashedString        GetValueAsHashedString() const override          { return m_value.GetValueAsHashedString(); };
	virtual bool                 GetValueAsBool() const override                  { return m_value.GetValueAsBool(); };
	//////////////////////////////////////////////////////////

	void SetValueFromString(const string& valueAsString);

	bool operator<=(const VariableValueData& value) const { return m_value <= value; }
	bool operator>=(const VariableValueData& value) const { return m_value >= value; }

	bool operator<=(const CVariableValue& value) const    { return m_value <= value; }
	bool operator>=(const CVariableValue& value) const    { return m_value >= value; }

	bool operator<=(const CVariable& other) const         { return m_value <= other.m_value; }
	bool operator>=(const CVariable& other) const         { return m_value >= other.m_value; }

	bool operator==(const CVariable& other) const         { return m_name == other.m_name && m_value == other.m_value; }

	CVariable& operator= (const CVariable& other) { m_name = other.m_name; m_value = other.m_value; return *this; }

	void Serialize(Serialization::IArchive& ar);

	//protected:
	CHashedString		m_name;
	CVariableValue      m_value;
};

//--------------------------------------------------------------------------------------------------

//one concrete running instance of the actions. (There might be actions that dont need instances, because their action is instantaneously.
class CResponseInstance;

// a basis class for every condition and action that requires a variable as an input
struct IVariableUsingBase
{
	static bool s_bDoDisplayCurrentValueInDebugOutput;
	virtual ~IVariableUsingBase() = default;

	void                  _Serialize(Serialization::IArchive& ar, const char* szVariableDisplayName = "^Variable", const char* szCollectionDisplayName = "^Collection");
	string                GetVariableVerboseName() const;
	const CVariableValue& GetCurrentVariableValue(CResponseInstance* pResponseInstance);
	CVariableCollection*  GetCurrentCollection(CResponseInstance* pResponseInstance);
	CVariable*            GetCurrentVariable(CResponseInstance* pResponseInstance);
	CVariable*            GetOrCreateCurrentVariable(CResponseInstance* pResponseInstance);

	CHashedString         m_collectionName;
	CHashedString         m_variableName;

#if defined(DRS_COLLECT_DEBUG_DATA)
	static string s_lastTestedObjectName;
	static string s_lastTestedValueAsString;
#endif
};
}  //namespace CryDRS
