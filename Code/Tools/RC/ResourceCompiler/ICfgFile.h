// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   ICfgFile.h
//  Version:     v1.00
//  Created:     3/14/2003 by MM.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __icfgfile_h__
#define __icfgfile_h__
#pragma once

enum EConfigPriority;
class IConfigSink;

/** Configuration file interface.
		Use format similar to windows .ini files.
*/
class ICfgFile
{
public:
	virtual ~ICfgFile() {}

	//! Delete instance of configuration file class.
	virtual void Release() = 0;

	//! Load configuration file.
	//! @return true=success, false otherwise
	virtual bool Load( const string &fileName ) = 0;

	//! Save configuration file, with the stored name in m_fileName
	//! @return true=success, false otherwise
	virtual bool Save() = 0;

	//! @param inszSection
	//! @param inszKey
	//! @param inszValue
	virtual void UpdateOrCreateEntry( const char *inszSection, const char *inszKey, const char *inszValue ) = 0;

	//! @param inszSection
	//! @param inszKey
	virtual void RemoveEntry( const char* inszSection, const char* inszKey ) = 0;

	//! Copy section keys to config specified by the 'config' parameter.
	// keySuffixes is a non-empty string containing one or more comma-separated suffixes:
	//     copies keys named <name>:<suffix> and <name>, suffix is stripped out.
	// keySuffixes is an empty string:
	//     copies keys named <name>. keys in format <name>:<suffix> are ignored.
	// keySuffixes is 0:
	//     copies all keys "as is".
	virtual void CopySectionKeysToConfig( EConfigPriority ePri, int sectionIndex, const char *keySuffixes, IConfigSink *config ) const = 0;

	// can be used to iterate through the section names
	//! @return 0 if sectionIndex is < 0 or >= than number of sections.
	virtual const char* GetSectionName(int sectionIndex) const = 0;
	//! @return section index or -1 if section not found.
	virtual int FindSection(const char* sectionName) const = 0;
};

#endif // __icfgfile_h__
