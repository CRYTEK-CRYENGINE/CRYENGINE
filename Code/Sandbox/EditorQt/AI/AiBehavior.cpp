// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "aibehavior.h"
#include "Util\FileUtil.h"
#include <CryScriptSystem/IScriptSystem.h>

//////////////////////////////////////////////////////////////////////////
void CAIBehavior::ReloadScript()
{
	// Execute script file in script system.
	if (m_script.IsEmpty())
		return;

	if (CFileUtil::CompileLuaFile(GetScript()))
	{
		IScriptSystem* scriptSystem = GetIEditorImpl()->GetSystem()->GetIScriptSystem();
		// Script compiled succesfully.
		scriptSystem->ReloadScript(m_script);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIBehavior::Edit()
{
	CFileUtil::EditTextFile(GetScript());
}

//////////////////////////////////////////////////////////////////////////
void CAICharacter::ReloadScript()
{
	// Execute script file in script system.
	if (m_script.IsEmpty())
		return;

	if (CFileUtil::CompileLuaFile(GetScript()))
	{
		IScriptSystem* scriptSystem = GetIEditorImpl()->GetSystem()->GetIScriptSystem();
		// Script compiled succesfully.
		scriptSystem->ReloadScript(m_script);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAICharacter::Edit()
{
	CFileUtil::EditTextFile(GetScript());
}

