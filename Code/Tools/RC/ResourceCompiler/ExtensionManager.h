// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   extensionmanager.h
//  Version:     v1.00
//  Created:     5/11/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __extensionmanager_h__
#define __extensionmanager_h__
#pragma once

// forward declarations.
struct IConverter;

/** Manages mapping between file extensions and converters.
*/
class ExtensionManager
{
public:
	ExtensionManager();
	~ExtensionManager();

	//! Register new converter with extension manager.
	//!  \param conv must not be 0
	//!  \param rc must not be 0
	void RegisterConverter(const char* name, IConverter* conv, IResourceCompiler* rc);
	//! Unregister all converters.
	void UnregisterAll();

	//! Find converter that matches given platform and extension.
	IConverter* FindConverter(const char* filename) const;

private:
	// Links extensions and converters.
	typedef std::vector<std::pair<string, IConverter*> > ExtVector;
	ExtVector m_extVector;

	std::vector<IConverter*> m_converters;
};

#endif // __extensionmanager_h__
