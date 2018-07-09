// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  CryEngine Source File.
//  Copyright (C), Crytek GmbH, 2010.
// -------------------------------------------------------------------------
//  File name:   PropertyVars.cpp
//  Version:     v1.00
//  Created:     02/02/2010 by Timur.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "PropertyVars.h"
#include "IRCLog.h"
#include "StringHelpers.h"

CPropertyVars::CPropertyVars( IResourceCompiler *pRC )
{
	m_pRC = pRC;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyVars::SetProperty( const string &name,const string &value )
{
	m_properties[StringHelpers::MakeLowerCase(name)] = value;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyVars::RemoveProperty( const string &name )
{
	m_properties.erase(StringHelpers::MakeLowerCase(name));
}

//////////////////////////////////////////////////////////////////////////
void CPropertyVars::ClearProperties()
{
	m_properties.clear();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyVars::ExpandProperties( string &str ) const
{
	const string original = str;

	for (;;)
	{
		const size_t startpos = str.find( "${" );
		if (startpos == str.npos)
			return;

		const size_t endpos = str.find( '}',startpos+2 );
		if (endpos == str.npos)
			return;

		// Find property.
		const string propName = StringHelpers::MakeLowerCase(str.substr(startpos+2,endpos-startpos-2));
		PropertyMap::const_iterator it = m_properties.find(propName);
		if (it == m_properties.end())
		{
			RCLogError( "Unknown property name ${%s} in input string %s",propName.c_str(),original.c_str() );
			return;
		}
		const string value = it->second;

		const string last = str;
		str = str.replace( startpos,endpos-startpos+1,value );

		// If we did a replacement and the string is still the same we are in an infinite loop.
		if (str.compare(last) == 0)
		{
			RCLogError( "Infinite loop with property ${%s} in input string '%s'. Original string '%s'.",propName.c_str(),last.c_str(),original.c_str() );
			return;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CPropertyVars::GetProperty( const string &key,string &value ) const
{
	PropertyMap::const_iterator it = m_properties.find(StringHelpers::MakeLowerCase(key));
	if (it == m_properties.end())
	{
		return false;
	}
	value = it->second;
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CPropertyVars::HasProperty( const string &key ) const
{
	return m_properties.find(StringHelpers::MakeLowerCase(key)) != m_properties.end();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyVars::PrintProperties() const
{
	for (PropertyMap::const_iterator it = m_properties.begin(); it != m_properties.end(); ++it)
	{
		RCLog("  %s \'%s\'", it->first.c_str(), it->second.c_str());
	}
}
