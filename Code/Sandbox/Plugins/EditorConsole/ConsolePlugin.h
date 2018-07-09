// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 1999-2014.
// -------------------------------------------------------------------------
//  File name:   ConsolePlugin.h
//  Version:     v1.00
//  Created:     03/03/2014 by Matthijs vd Meide
//  Compilers:   Visual Studio 2010
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once
#include <IEditor.h>
#include <IPlugin.h>
#include "EngineListener.h"
#include "Messages.h"

//console plugin class, UI side
class CConsolePlugin : public IPlugin, public CEngineListener
{
	//called back by the EngineListener when we need to send messages
	void EmitCVar(ICVar* pVar) const                      {}
	void EmitLine(size_t index, const string& line) const {}
	void DestroyCVar(ICVar* pVar) const                   {}
	
public:
	CConsolePlugin();
	~CConsolePlugin();

	Messages::SAutoCompleteReply HandleAutoCompleteRequest(const Messages::SAutoCompleteRequest& req);

	//the singleton instance of the plugin
	static CConsolePlugin* GetInstance() { return s_pInstance; }

	//get a unique address for messages
	string GetUniqueAddress() const;

	//IPlugin implementation
	int32       GetPluginVersion() override                          { return 1; }
	const char* GetPluginName() override                             { return "Console"; }
	const char* GetPluginDescription() override						 { return "Adds the Console window"; }

private:
	//singleton instance
	static CConsolePlugin* s_pInstance;
};

