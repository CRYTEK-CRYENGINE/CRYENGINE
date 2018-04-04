// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   ScriptProperties.h
//  Version:     v1.00
//  Created:     18/5/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ScriptProperties_h__
#define __ScriptProperties_h__
#pragma once

struct IScriptObject;

//////////////////////////////////////////////////////////////////////////
// This class handles assignment of entity script properties from XML nodes
// to the script tables.
//////////////////////////////////////////////////////////////////////////
class CScriptProperties
{
public:
	bool SetProperties(XmlNodeRef& entityNode, IScriptTable* pEntityTable);
	void Assign(XmlNodeRef& propsNode, IScriptTable* pPropsTable);
};

#endif // __ScriptProperties_h__
