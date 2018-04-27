// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ScriptBinding.h
//  Version:     v1.00
//  Created:     9/7/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ScriptBinding_h__
#define __ScriptBinding_h__
#pragma once

class CScriptableBase;

//////////////////////////////////////////////////////////////////////////
class CScriptBindings
{
public:
	CScriptBindings();
	virtual ~CScriptBindings();

	void         Init(ISystem* pSystem, IScriptSystem* pSS);
	void         Done();

	void         LoadScriptedSurfaceTypes(const char* sFolder, bool bReload);
	virtual void GetMemoryStatistics(ICrySizer* pSizer) const;

private:
	std::vector<std::unique_ptr<CScriptableBase>> m_binds;

	class CScriptSurfaceTypesLoader*              m_pScriptSurfaceTypes;
};

#endif // __ScriptBinding_h__
