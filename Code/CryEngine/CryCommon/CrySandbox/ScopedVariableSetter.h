// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

//! Temporarily set the value of a variable.
//! This class sets a variable the the value in its constructor
//! and resets it to its initial value in the destructor.
template<typename T> class CScopedVariableSetter
{
public:
	typedef T Value;

	CScopedVariableSetter(Value& variable, const Value& temporaryValue)
		: m_oldValue(variable),
		m_variable(variable)
	{
		m_variable = temporaryValue;
	}

	~CScopedVariableSetter()
	{
		m_variable = m_oldValue;
	}

private:
	Value  m_oldValue;
	Value& m_variable;
};

//! \endcond