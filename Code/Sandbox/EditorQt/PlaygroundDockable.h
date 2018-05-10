// Copyright 2001-2016 Crytek GmbH. All rights reserved.
#pragma once

#include "EditorFramework/Editor.h"

//! This class will create an empty editor and is meant for programmers to experiment without having to insert their code into other parts of the engine.
//! Treat this as your own personal playground to experiment with UI things. Do not submit your changes, this is meant for local experimentation only.
//! Uncomment the REGISTER macro in the cpp file to activate
class CPlaygroundDockable : public CDockableEditor
{
public:
	CPlaygroundDockable();
	~CPlaygroundDockable();

	virtual const char* GetEditorName() const override { return "Playground"; };
};
