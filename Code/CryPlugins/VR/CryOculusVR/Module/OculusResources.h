// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace CryVR {
namespace Oculus {

class Device;

class Resources
{
public:
	static Device* GetAssociatedDevice() { return ms_pRes ? ms_pRes->m_pDevice : 0; }

	// initializes the Oculus SDK and creates an instance of an Oculus device if any one is attached
	static void Init();
	// shuts down the Oculus SDK and releases the associated Oculus device (if any)
	static void Shutdown();

private:
	Resources();
	~Resources();

private:
	static Resources* ms_pRes;
	static bool       ms_libInitialized;

private:
	Device* m_pDevice; // Attached Oculus device (only one supported at the moment)
};

} // namespace Oculus
} // namespace CryVR