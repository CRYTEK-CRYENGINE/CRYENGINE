// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   cfgfile.h
//  Version:     v1.00
//  Created:     4/11/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History: 03/14/2003 MM added save support
//
////////////////////////////////////////////////////////////////////////////

#ifndef __cfgfile_h__
#define __cfgfile_h__
#pragma once

#include "ICfgFile.h"						// ICfgFile

enum EConfigPriority;
class Config;

/** Configuration file class.
		Uses format similar to windows .ini files.
*/
class CfgFile 
	: public ICfgFile
{
public:
	CfgFile();
	virtual ~CfgFile();

	//////////////////////////////////////////////////////////////////////////
	// interface ICfgFile	
	virtual void Release();
	virtual bool Load( const string &fileName );
	virtual bool Save( void );
	virtual void UpdateOrCreateEntry( const char *inszSection, const char *inszKey, const char *inszValue );
	virtual void RemoveEntry( const char* inszSection, const char* inszKey );
	virtual void CopySectionKeysToConfig( EConfigPriority ePri, int sectionIndex, const char *keySuffixes, IConfigSink *config ) const;
	virtual const char* GetSectionName(int sectionIndex) const;
	virtual int FindSection(const char* sectionName) const;
	//////////////////////////////////////////////////////////////////////////

private:
	void LoadFromBuffer( const char *buf,size_t bufSize );

private:
	// Config file entry structure, filled by readSection method.
	struct Entry
	{
		string key;     //!< keys (for comments this is "")
		string value;   //!< values and comments (with leading ; or //)

		bool IsComment() const;
	};

	struct Section
	{
		string           name;           //!< Section name. The first one has the name "" and is used if no section was specified.
		std::list<Entry> entries;        //!< List of entries.
	};

	string               m_fileName;     //!< Configuration file name.
	int                  m_modified;     //!< Set to true if config file been modified.
	std::vector<Section> m_sections;     //!< List of sections in config file. (the first one has the name "" and is used if no section was specified)
};


#endif // __cfgfile_h__
