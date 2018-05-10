// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ScriptBinding.cpp
//  Version:     v1.00
//  Created:     9/7/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ScriptBinding.h"

#include "ScriptBind_System.h"
#include "ScriptBind_Particle.h"
#include "ScriptBind_Sound.h"
#include "ScriptBind_Movie.h"
#include "ScriptBind_Script.h"
#include "ScriptBind_Physics.h"

#include "SurfaceTypes.h"

#include <CryScriptSystem/IScriptSystem.h>

CScriptBindings::CScriptBindings()
{
	m_pScriptSurfaceTypes = 0;
}

CScriptBindings::~CScriptBindings()
{
	Done();
}

//////////////////////////////////////////////////////////////////////////
void CScriptBindings::Init(ISystem* pSystem, IScriptSystem* pSS)
{
	m_binds.push_back(std::unique_ptr<CScriptableBase>(new CScriptBind_System(pSS, pSystem)));
	m_binds.push_back(std::unique_ptr<CScriptableBase>(new CScriptBind_Particle(pSS, pSystem)));
	m_binds.push_back(std::unique_ptr<CScriptableBase>(new CScriptBind_Sound(pSS, pSystem)));
	m_binds.push_back(std::unique_ptr<CScriptableBase>(new CScriptBind_Movie(pSS, pSystem)));
	m_binds.push_back(std::unique_ptr<CScriptableBase>(new CScriptBind_Script(pSS, pSystem)));
	m_binds.push_back(std::unique_ptr<CScriptableBase>(new CScriptBind_Physics(pSS, pSystem)));

	//////////////////////////////////////////////////////////////////////////
	// Enumerate script surface types.
	//////////////////////////////////////////////////////////////////////////
	m_pScriptSurfaceTypes = new CScriptSurfaceTypesLoader;
}

//////////////////////////////////////////////////////////////////////////
void CScriptBindings::Done()
{
	if (m_pScriptSurfaceTypes)
		delete m_pScriptSurfaceTypes;
	m_pScriptSurfaceTypes = NULL;

	// Done script bindings.
	m_binds.clear();
}

//////////////////////////////////////////////////////////////////////////
void CScriptBindings::LoadScriptedSurfaceTypes(const char* sFolder, bool bReload)
{
	m_pScriptSurfaceTypes->LoadSurfaceTypes(sFolder, bReload);
}

//////////////////////////////////////////////////////////////////////////
void CScriptBindings::GetMemoryStatistics(ICrySizer* pSizer) const
{
	//pSizer->AddObject(m_binds);
}
