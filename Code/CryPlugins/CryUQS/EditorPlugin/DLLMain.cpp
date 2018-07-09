// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#define VC_EXTRALEAN
#include <afxwin.h>
#include <afxext.h>

#include <CryCore/Platform/platform.h>
#include <CryCore/Platform/platform_impl.inl>

#include <IEditor.h>
#include "IPlugin.h"
#include "IEditorClassFactory.h"

#include "QtViewPane.h"

#include "Editor/MainEditorWindow.h"

#define UQS_EDITOR_NAME "UQS Editor"

REGISTER_VIEWPANE_FACTORY_AND_MENU(CMainEditorWindow, UQS_EDITOR_NAME, "Game", true, "Universal Query System")

class CUqsEditorPlugin : public IPlugin
{
	enum
	{
		Version = 1,
	};

public:
	CUqsEditorPlugin()
	{
	}

	~CUqsEditorPlugin()
	{
	}

	virtual int32       GetPluginVersion() override                          { return DWORD(Version); }
	virtual const char* GetPluginName() override                             { return UQS_EDITOR_NAME; }
	virtual const char* GetPluginDescription()								 { return ""; }
};

REGISTER_PLUGIN(CUqsEditorPlugin)
	