// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   ScopedVariableSetter.h
//  Version:     v1.00
//  Created:     22/8/2006 by MichaelS.
//  Compilers:   Visual Studio.NET 2005
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __SCOPEDVARIABLESETTER_H__
#define __SCOPEDVARIABLESETTER_H__

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

#endif //__SCOPEDVARIABLESETTER_H__
