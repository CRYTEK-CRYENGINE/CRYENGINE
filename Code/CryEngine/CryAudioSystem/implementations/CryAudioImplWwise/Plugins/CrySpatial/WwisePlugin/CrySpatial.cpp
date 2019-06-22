// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "CrySpatial.h"
#include "CrySpatialPlugin.h"
#include "CrySpatialAttachmentPlugin.h"
#include "CrySpatialConfig.h"

#include <AK/Wwise/Utilities.h>

#ifdef _DEBUG
	#define AK_DISABLE_ASSERTS
#endif
#include <AK/Tools/Common/AkAssert.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
namespace Plugins
{
BEGIN_MESSAGE_MAP(CrySpatialApp, CWinApp)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CrySpatialApp theApp;

//////////////////////////////////////////////////////////////////////////
BOOL CrySpatialApp::InitInstance()
{
	CWinApp::InitInstance();
	AK::Wwise::RegisterWwisePlugin();
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
AK::Wwise::IPluginBase* __stdcall AkCreatePlugin(unsigned short in_companyID, unsigned short in_pluginID)
{
	if (in_companyID == CrySpatialConfig::CompanyID && in_pluginID == CrySpatialConfig::PluginID)
	{
		return new CrySpatialPlugin;
	}

	if (in_companyID == CrySpatialAttachmentConfig::CompanyID && in_pluginID == CrySpatialAttachmentConfig::PluginID)
	{
		return new CrySpatialAttachmentPlugin;
	}

	return nullptr;
}

}// namespace Plugins
}// namespace Wwise
}// namespace Impl
}// namespace CryAudio

DEFINEDUMMYASSERTHOOK;
DEFINE_PLUGIN_REGISTER_HOOK;