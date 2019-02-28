// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/IConsole.h>
#include <CrySystem/ISystem.h>

#include <CrySystem/VR/IHMDManager.h>

namespace CryVR
{
class CVars
{
private:
public:
	// Shared variables:
	static int hmd_info;
	static int hmd_social_screen;
	static int hmd_social_screen_aspect_mode;
	static int hmd_post_inject_camera;
	static int hmd_tracking_origin;
	static float hmd_resolution_scale;
	static ICVar* pSelectedHmdNameVar;

	static void OnHmdRecenter(IConsoleCmdArgs* pArgs) 
	{ 
		gEnv->pSystem->GetHmdManager()->RecenterPose();
	}

public:
	static void Register()
	{
		REGISTER_CVAR2("hmd_info", &hmd_info, hmd_info,
			VF_NULL, "Shows hmd related profile information."
		);
		REGISTER_CVAR2("hmd_social_screen", &hmd_social_screen, hmd_social_screen,
			VF_NULL, "Selects the social screen mode: \n"
			"-1- Off\n"
			"0 - Distorted dual image\n"
			"1 - Undistorted dual image\n"
			"2 - Undistorted left eye\n"
			"3 - Undistorted right eye\n"
		);
		REGISTER_CVAR2("hmd_social_screen_aspect_mode", &hmd_social_screen_aspect_mode, hmd_social_screen_aspect_mode,
			VF_NULL, "Rendering mode when displaying social screen: \n"
			"0 - Stretch\n"
			"1 - Keep aspect ratio and center\n"
			"2 - Keep aspect ratio and fill (default)\n"
		);
		REGISTER_CVAR2("hmd_post_inject_camera", &hmd_post_inject_camera, hmd_post_inject_camera,
			VF_NULL, "Reduce latency by injecting updated updated camera at render thread as late as possible before submitting draw calls to GPU\n"
			"0 - Disable\n"
			"1 - Enable\n"
		);
		REGISTER_CVAR2("hmd_tracking_origin", &hmd_tracking_origin, hmd_tracking_origin,
			VF_NULL, "Determine HMD tracking origin point.\n"
			"0 - Actor's feet\n"
			"1 - Camera (/Actor's head)\n"
		);
		REGISTER_CVAR2("hmd_resolution_scale", &hmd_resolution_scale, hmd_resolution_scale,
			VF_NULL, "Scales rendered resolution"
		);
		pSelectedHmdNameVar = REGISTER_STRING("hmd_device", "", VF_NULL,
			"Specifies the name of the VR device to use\nAvailable options depend on VR plugins registered with the engine"
		);
		REGISTER_COMMAND("hmd_recenter_pose", &OnHmdRecenter,
			VF_NULL, "Recenters sensor orientation of the HMD."
		);
	}

	static void Unregister()
	{
		if (IConsole* const pConsole = gEnv->pConsole)
		{
			pConsole->UnregisterVariable("hmd_info");
			pConsole->UnregisterVariable("hmd_social_screen");
			pConsole->UnregisterVariable("hmd_social_screen_aspect_mode");
			pConsole->UnregisterVariable("hmd_post_inject_camera");
			pConsole->UnregisterVariable("hmd_tracking_origin");
			pConsole->UnregisterVariable("hmd_resolution_scale");
			pConsole->UnregisterVariable("hmd_device");

			pConsole->RemoveCommand("hmd_recenter_pose");
		}

	}
};
}
