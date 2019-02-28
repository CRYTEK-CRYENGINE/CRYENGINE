// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "PerforcePlugin.h"
#include "PerforceSourceControl.h"
#include "ISourceControl.h"
#include "IEditorClassFactory.h"

REGISTER_PLUGIN(CPerforcePlugin)

CPerforceSourceControl* g_pPerforceControl;

namespace PluginInfo
{
const char* kName = "Perforce Client";
const int kVersion = 1;
}

CPerforcePlugin::CPerforcePlugin()
{
	g_pPerforceControl = new CPerforceSourceControl();
	GetIEditor()->GetClassFactory()->RegisterClass(g_pPerforceControl);
}

CPerforcePlugin::~CPerforcePlugin()
{
	delete g_pPerforceControl;
	g_pPerforceControl = nullptr;
}

int32 CPerforcePlugin::GetPluginVersion()
{
	return PluginInfo::kVersion;
}

const char* CPerforcePlugin::GetPluginName()
{
	return PluginInfo::kName;
}

void CPerforcePlugin::OnEditorNotifyEvent(EEditorNotifyEvent aEventId)
{
	if (eNotify_OnInit == aEventId)
	{
		g_pPerforceControl->Init();
	}
}

