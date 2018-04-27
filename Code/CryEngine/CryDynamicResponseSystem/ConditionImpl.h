// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/************************************************************************

   These are the base DRS-variables conditions. We support LessThan, GreaterThan, Equal and inRange checks for now.
   In addition we have the CVariableAgainstVariablesCondition, which checks if the value of one specified variable is between two other specified variables

   /************************************************************************/

#pragma once

#include <CryDynamicResponseSystem/IDynamicResponseCondition.h>
#include "Variable.h"

namespace CryDRS
{
class CVariable;
class CResponseInstance;

class CVariableSmallerCondition final : public IVariableUsingBase, public DRS::IResponseCondition
{
public:
	//////////////////////////////////////////////////////////
	// IResponseCondition implementation
	virtual void        Serialize(Serialization::IArchive& ar) override;
	virtual bool        IsMet(DRS::IResponseInstance* pResponseInstance) override;
	virtual string      GetVerboseInfo() const override;
	virtual const char* GetType() const override { return "LessCheck"; }
	//////////////////////////////////////////////////////////
	CVariableValue m_value;
};

class CVariableLargerCondition final : public IVariableUsingBase, public DRS::IResponseCondition
{
public:
	//////////////////////////////////////////////////////////
	// IResponseCondition implementation
	virtual void        Serialize(Serialization::IArchive& ar) override;
	virtual bool        IsMet(DRS::IResponseInstance* pResponseInstance) override;
	virtual string      GetVerboseInfo() const override;
	virtual const char* GetType() const override { return "GreaterCheck"; }
	//////////////////////////////////////////////////////////
	CVariableValue m_value;
};

class CVariableEqualCondition final : public IVariableUsingBase, public DRS::IResponseCondition
{
public:
	//////////////////////////////////////////////////////////
	// IResponseCondition implementation
	virtual void        Serialize(Serialization::IArchive& ar) override;
	virtual bool        IsMet(DRS::IResponseInstance* pResponseInstance) override;
	virtual string      GetVerboseInfo() const override;
	virtual const char* GetType() const override { return "EqualCheck"; }
	//////////////////////////////////////////////////////////
	CVariableValue m_value;
};

class CVariableRangeCondition final : public IVariableUsingBase, public DRS::IResponseCondition
{
public:
	//////////////////////////////////////////////////////////
	// IResponseCondition implementation
	virtual void        Serialize(Serialization::IArchive& ar) override;
	virtual bool        IsMet(DRS::IResponseInstance* pResponseInstance) override;
	virtual string      GetVerboseInfo() const override;
	virtual const char* GetType() const override { return "InRangeCheck"; }
	//////////////////////////////////////////////////////////

private:
	CVariableValue m_value;
	CVariableValue m_value2;
};

//////////////////////////////////////////////////////////////////////////

//like CVariableRangeCondition, but the Values that we check against are not fixed, but are also values from variables
class CVariableAgainstVariablesCondition final : public DRS::IResponseCondition
{
public:
	//////////////////////////////////////////////////////////
	// IResponseCondition implementation
	virtual bool        IsMet(DRS::IResponseInstance* pResponseInstance) override;
	virtual void        Serialize(Serialization::IArchive& ar) override;

	virtual string      GetVerboseInfo() const override;
	virtual const char* GetType() const override { return "VariableToVariableCheck"; }
	//////////////////////////////////////////////////////////
private:
	inline const CVariable* GetVariable(const CHashedString& collection, const CHashedString& variable, CResponseInstance* pResponseInstance) const;

	CHashedString m_collectionNameSmaller;
	CHashedString m_variableNameSmaller;
	CHashedString m_collectionNameToTest;
	CHashedString m_variableNameToTest;
	CHashedString m_collectionNameLarger;
	CHashedString m_variableNameLarger;
};
}  //namespace CryDRS
