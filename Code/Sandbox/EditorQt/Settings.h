// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   settings.h
//  Version:     v1.00
//  Created:     14/1/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: General editor settings.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <QString>

// CryCommon
#include <CrySandbox/CrySignal.h>

//////////////////////////////////////////////////////////////////////////
/** Various editor settings.
 */
struct SEditorSettings
{
	static void Load();
	static void LoadFrom(const QString& filename);

	static void LoadValue(const char* sSection, const char* sKey, int& value);
	static void LoadValue(const char* sSection, const char* sKey, uint32& value);
	static void LoadValue(const char* sSection, const char* sKey, unsigned long& value);
	static void LoadValue(const char* sSection, const char* sKey, float& value);
	static void LoadValue(const char* sSection, const char* sKey, bool& value);
	static void LoadValue(const char* sSection, const char* sKey, string& value);

private:
	SEditorSettings();
};

