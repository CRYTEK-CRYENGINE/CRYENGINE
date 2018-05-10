// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   DialogLoaderMK2.h
//  Version:     v1.00
//  Created:     07/07/2006 by AlexL
//  Compilers:   Visual Studio.NET
//  Description: Dialog Loader
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __DIALOGLOADERMK2_H__
#define __DIALOGLOADERMK2_H__

#pragma once

#include "DialogScript.h"

class CDialogSystem;

class CDialogLoaderMK2
{
public:
	CDialogLoaderMK2(CDialogSystem* pDS);
	virtual ~CDialogLoaderMK2();

	// Loads all DialogScripts below a given path
	bool LoadScriptsFromPath(const string& path, TDialogScriptMap& outScripts, const char* levelName);

	// Loads a single DialogScript
	bool LoadScript(const string& stripPath, const string& fileName, TDialogScriptMap& outScripts);

protected:
	void InternalLoadFromPath(const string& stripPath, const string& path, TDialogScriptMap& outScriptMap, int& numLoaded, const char* levelName);

	// get actor from string [1-based]
	bool GetActor(const char* actor, int& outID);
	bool GetLookAtActor(const char* actor, int& outID, bool& outSticky);
	bool ProcessScript(CDialogScript* pScript, const XmlNodeRef& rootNode);
	bool ReadLine(const XmlNodeRef& lineNode, CDialogScript::SScriptLine& outLine, const char* scriptID, int lineNumber);
	void ResetLine(CDialogScript::SScriptLine& scriptLine);

protected:

	CDialogSystem* m_pDS;
};

#endif
