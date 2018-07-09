// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "OsvrResources.h"
#include "OsvrDevice.h"

namespace CryVR {
namespace Osvr {

// static member variables
Resources* Resources::ms_pRes = 0;
bool Resources::ms_libInitialized = false;

Resources::Resources()
	: m_pDevice(0)
{
	CryLogAlways("[HMD][Osvr] Initialising Resources - Using Osvr");

	//if the renderer is not Dx11, don't initialize the Osvr
	if (strcmp(gEnv->pConsole->GetCVar("r_driver")->GetString(), "DX11") != 0)
	{
		CryLogAlways("[HMD][Osvr] Unable to initialize Osvr, only DX11 supported.");
		return;
	}

	m_pDevice = Device::CreateAndInitializeDevice();

	if (!m_pDevice)
	{
		CryLogAlways("[HMD][Osvr] Failed to create Osvr Device! Do you have the HMD connected and Osvr server running?");
	}
	else
	{
		ms_libInitialized = true;
	}
}

// ------------------------------------------------------------------------

// ------------------------------------------------------------------------
Resources::~Resources()
{
	#if !defined(_RELEASE)
	if (m_pDevice && m_pDevice->GetRefCount() != 1)
		__debugbreak();
	#endif

	SAFE_RELEASE(m_pDevice);
}

// ------------------------------------------------------------------------
void Resources::Init()
{
	#if !defined(_RELEASE)
	if (ms_pRes)
		__debugbreak();
	#endif

	if (!ms_pRes)
		ms_pRes = new Resources();
}

// ------------------------------------------------------------------------
void Resources::Shutdown()
{
	#if !defined(_RELEASE)
	if (ms_libInitialized && !ms_pRes)
		__debugbreak();
	#endif

	SAFE_DELETE(ms_pRes);
}

} // namespace Osvr
} // namespace CryVR
