// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/************************************************************************

   A DynamicResponseVariableCollection holds a number of variables.
   We split the variables into more than one collection, to speed up searches and group variables that belong together
   For example we can have collections like "global", "level-specific", "actor-specific" ...

   The collections are also managing the creating of Conditions to check the values of the variables inside of the specific collection.

   /************************************************************************/

#pragma once

#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>
#include "Variable.h"

namespace CryDRS
{
class CVariableCollection;

class CVariableCollectionManager
{
public:
	CVariableCollectionManager() = default;
	~CVariableCollectionManager();

	CVariableCollection* CreateVariableCollection(const CHashedString& collectionName);
	void                 ReleaseVariableCollection(CVariableCollection* pCollectionToFree);
	CVariableCollection* GetCollection(const CHashedString& collectionName) const;

	void                 Update();
	void                 Reset();

	void GetAllVariableCollections(DRS::ValuesList* pOutCollectionsList, bool bSkipDefaultValues);
	void SetAllVariableCollections(DRS::ValuesListIterator start, DRS::ValuesListIterator end);

	void                 Serialize(Serialization::IArchive& ar);

private:
	typedef std::vector<CVariableCollection*> CollectionList;
	CollectionList m_variableCollections;
};

//////////////////////////////////////////////////////////////////////////

class CVariableCollection final : public DRS::IVariableCollection
{
public:
	static const CHashedString  s_globalCollectionName;
	static const CHashedString  s_localCollectionName;
	static const CHashedString  s_contextCollectionName;
	static const CVariableValue s_newVariableValue;

	struct VariableCooldownInfo   //used for the SetVariableValueForSomeTime functionality
	{
		CHashedString     variableName;
		CTimeValue        timeOfReset;
		VariableValueData oldValue;

		void                 Serialize(Serialization::IArchive& ar);
	};

	typedef std::vector<CVariable*> VariableList;
	typedef std::vector<VariableCooldownInfo> CoolingDownVariableList;

	CVariableCollection(const CHashedString& name);
	 ~CVariableCollection() override;

	//////////////////////////////////////////////////////////
	// IVariableCollection implementation
		 CVariable*           CreateVariable(const CHashedString& name, int initialValue) override;
		 CVariable*           CreateVariable(const CHashedString& name, float initialValue) override;
		 CVariable*           CreateVariable(const CHashedString& name, bool initialValue) override;
		 CVariable*           CreateVariable(const CHashedString& name, const CHashedString& initialValue) override;

		 CVariable*           CreateVariable(const CHashedString& name, const CTimeValue& initialValue) override;
		 bool                 SetVariableValue(const CHashedString& name, const CTimeValue& newValue, bool createIfNotExisting = true, const CTimeValue& resetTime = -1) override;
		 
		 #define MP_FUNCTION(T)\
		 CVariable*           CreateVariable(const CHashedString& name, const T& initialValue) override;\
		 bool                 SetVariableValue(const CHashedString& name, const T& newValue, bool createIfNotExisting = true, const CTimeValue& resetTime = -1) override;
		 #include <CrySystem\mpfloat.types>
		 #undef MP_FUNCTION

		 bool                 SetVariableValue(const CHashedString& name, int newValue, bool createIfNotExisting = true, const CTimeValue& resetTime = -1) override;
		 bool                 SetVariableValue(const CHashedString& name, float newValue, bool createIfNotExisting = true, const CTimeValue& resetTime = -1) override;
		 bool                 SetVariableValue(const CHashedString& name, bool newValue, bool createIfNotExisting = true, const CTimeValue& resetTime = -1) override;
		 bool                 SetVariableValue(const CHashedString& name, const CHashedString& newValue, bool createIfNotExisting = true, const CTimeValue& resetTime = -1) override;

		 CVariable*           GetVariable(const CHashedString& name) const override;
		 const CHashedString& GetName() const override { return m_name; };
		 void                 Serialize(Serialization::IArchive& ar) override;

		 void                 SetUserString(const char* szUserString) override { m_userString = szUserString; }
		 const char*          GetUserString() const override { return m_userString.c_str(); }
	//////////////////////////////////////////////////////////

	VariableList& GetAllVariables() { return m_allResponseVariables; }
	const CoolingDownVariableList& GetAllCooldownVariables() const { return m_coolingDownVariables; }

	bool       SetVariableValue(CVariable* pVariable, const CVariableValue& newValue, const CTimeValue& resetTime = -1);
	bool       SetVariableValue(const CHashedString& name, const CVariableValue& newValue, bool createIfNotExisting = true, const CTimeValue& resetTime = -1);
	CVariable* CreateVariable(const CHashedString& name, const CVariableValue& initialValue);

	void       Update();
	void       Reset();

	//will create the variable if not yet existing
	CVariable*            CreateOrGetVariable(const CHashedString& variableName);
	//REMARK: If the variable is not existing, Will return s_newVariableValue
	const CVariableValue& GetVariableValue(const CHashedString& name) const;

	string                GetVariablesAsString() const;

private:
	bool SetVariableValueForSomeTime(CVariable* pVariable, const CVariableValue& value, const CTimeValue& timeBeforeReset);

	const CHashedString     m_name;
	VariableList            m_allResponseVariables;
	CoolingDownVariableList m_coolingDownVariables;
	string                  m_userString;
};

typedef std::shared_ptr<CVariableCollection> VariableCollectionSharedPtr;
} // namespace CryDRS
