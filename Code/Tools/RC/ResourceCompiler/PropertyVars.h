// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  CryEngine Source File.
//  Copyright (C), Crytek GmbH, 2010.
// -------------------------------------------------------------------------
//  File name:   PropertyVars.h
//  Version:     v1.00
//  Created:     02/02/2010 by Timur.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __PropertyVars_h__
#define __PropertyVars_h__
#pragma once

struct IResourceCompiler;

class CPropertyVars
{
public:
	explicit CPropertyVars( IResourceCompiler *pRC );
	
	void SetProperty( const string &name,const string &value );
	void RemoveProperty( const string &key );
	void ClearProperties();

	// Return property.
	bool GetProperty( const string &name,string &value ) const;
	bool HasProperty( const string &name ) const;

	// Expand variables as ${propertyName} with the value of propertyName variable
	void ExpandProperties( string &str ) const;

	void PrintProperties() const;

	// Enumerate properties
	// Functor takes two arguments: (name, value)
	template<typename Functor>
	void Enumerate( const Functor &callback ) const
	{
		for (PropertyMap::const_iterator it = m_properties.begin(); it != m_properties.end(); ++it)
		{
			callback( it->first,it->second );
		}
	}

private:
	typedef std::map<string,string> PropertyMap;
	PropertyMap m_properties;

	IResourceCompiler *m_pRC;
};

#endif //__PropertyVars_h__