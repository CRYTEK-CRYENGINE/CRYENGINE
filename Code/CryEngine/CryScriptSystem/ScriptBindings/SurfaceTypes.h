// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   SurfaceTypes.h
//  Version:     v1.00
//  Created:     30/9/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SurfaceTypes_h__
#define __SurfaceTypes_h__
#pragma once

#include <CryScriptSystem/IScriptSystem.h>

struct ISurfaceType;

//////////////////////////////////////////////////////////////////////////
// SurfaceTypes loader.
//////////////////////////////////////////////////////////////////////////
class CScriptSurfaceTypesLoader
{
public:
	CScriptSurfaceTypesLoader();
	~CScriptSurfaceTypesLoader();

	void ReloadSurfaceTypes();
	bool LoadSurfaceTypes(const char* sFolder, bool bReload);
	void UnloadSurfaceTypes();

	void UnregisterSurfaceType(ISurfaceType* sfType);

private:
	std::vector<ISurfaceType*> m_surfaceTypes;
	std::vector<string>        m_folders;
};

#endif // __SurfaceTypes_h__
