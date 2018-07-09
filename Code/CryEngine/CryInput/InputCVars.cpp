// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "InputCVars.h"
#include <CrySystem/IConsole.h>

CInputCVars* g_pInputCVars = 0;

CInputCVars::CInputCVars()
{
	REGISTER_CVAR(i_debug, 0, 0,
	              "Toggles input event debugging.\n"
	              "Usage: i_debug [0/1]\n"
	              "Default is 0 (off). Set to 1 to spam console with key events (only press and release).");
	REGISTER_CVAR(i_forcefeedback, 1, 0, "Enable/Disable force feedback output.");

	// mouse
	REGISTER_CVAR(i_mouse_buffered, 0, 0,
	              "Toggles mouse input buffering.\n"
	              "Usage: i_mouse_buffered [0/1]\n"
	              "Default is 0 (off). Set to 1 to process buffered mouse input.");
	REGISTER_CVAR(i_mouse_sensitivity, 1, 0,
	              "Changes mouse sensitivity.\n"
	              "Usage: i_mouse_sensitivity [float number]\n"
	              "Default is 1 (raw movement), Set to 0 for no movement, [0/1] for slower movement and [1/n] for faster movement");
	REGISTER_CVAR(i_mouse_accel, 0.0f, VF_DUMPTODISK,
	              "Set mouse acceleration, 0.0 means no acceleration.\n"
	              "Usage: i_mouse_accel [float number] (usually a small number, 0.1 is a good one)\n"
	              "Default is 0.0 (off)");
	REGISTER_CVAR(i_mouse_accel_max, 100.0f, VF_DUMPTODISK,
	              "Set mouse max mouse delta when using acceleration.\n"
	              "Usage: i_mouse_accel_max [float number]\n"
	              "Default is 100.0");
	REGISTER_CVAR(i_mouse_smooth, 0.0f, VF_DUMPTODISK,
	              "Set mouse smoothing value, also if 0 (disabled) there will be a simple average\n"
	              "between the old and the actual input.\n"
	              "Usage: i_mouse_smooth [float number]\n"
	              "(1.0 = very very smooth, 30 = almost instant)\n"
	              "Default is 0.0");
	REGISTER_CVAR(i_mouse_inertia, 0.0f, VF_DUMPTODISK,
	              "Set mouse inertia. It is disabled (0.0) by default.\n"
	              "Usage: i_mouse_inertia [float number]\n"
	              "Default is 0.0");

#if CRY_PLATFORM_WINDOWS
	REGISTER_CVAR(i_mouse_scroll_coordinate_origin, 0, 0,
		"Controls the origin of mouse coordinates sent for mouse wheel scrolling events.\n"
		"0 = Top-left of screen space (Windows default behavior), 1 = Top-left of client area, 2 = Top-left of client area, clamped to client area");
#endif

	// keyboard
	REGISTER_CVAR(i_bufferedkeys, 1, 0,
	              "Toggles key buffering.\n"
	              "Usage: i_bufferedkeys [0/1]\n"
	              "Default is 0 (off). Set to 1 to process buffered key strokes.");

	// xinput
	REGISTER_CVAR(i_xinput, 1, 0,
	              "Number of XInput controllers to process\n"
	              "Usage: i_xinput [0/1/2/3/4]\n"
	              "Default is 1.");
	REGISTER_CVAR(i_xinput_poll_time, 1000, 0,
	              "Number of ms between device polls in polling thread\n"
	              "Usage: i_xinput_poll_time 500\n"
	              "Default is 1000ms. Value must be >=0.");

	REGISTER_CVAR(i_xinput_deadzone_handling, 1, 0,
	              "deadzonehandling\n"
	              "Usage: i_xinput_deadzone_handling 0/1 (0 - old deadzone/ 1 - new deadzone)\n"
	              "Default is 1. Value must be >=0.");

	REGISTER_CVAR(i_debugDigitalButtons, 0, 0,
	              "render controller's digital button pressed info\n"
	              "Usage: 0 (off), 1(on)"
	              "Default is 0. Value must be >=0");

#if CRY_PLATFORM_WINDOWS
	REGISTER_CVAR(i_kinectXboxConnect, 1, 0,
	              "Allow connection to Xbox for Kinect input.\n"
	              "Usage: 0 = disabled, 1 = allow connection, 2 = force connection (don't try local Kinect on system) \n"
	              "Default is 1");
	REGISTER_CVAR(i_kinectXboxConnectPort, 62455, 0,
	              "Port used to connect to Xbox for Kinect input.\n"
	              "Default is 62455 (random value different than remote compiler)");
	i_kinectXboxConnectIP = REGISTER_STRING("i_kinectXboxConnectIP", "", VF_NULL,
	                                        "Usage: force set a specific Xbox Game IP to connect to for Kinect input (use default xbox from neighbourhood when empty) \n"
	                                        "Default is empty ");
#endif

	REGISTER_CVAR(i_useKinect, 1, 0,
	              "Use Kinect");
	REGISTER_CVAR(i_kinSkeletonSmoothType, 1, 0,
	              "Kinect skeleton tracking smooth type: 0 = not smoothed, 1 = using Double Exponential Smoothing");
	REGISTER_CVAR(i_kinectDebug, 1, 0,
	              "1 = Draws skeleton on screen \n2 = Draws colour + depth buffers on screen \n3 = Draws both skeleton and buffers");
	REGISTER_CVAR(i_seatedTracking, 1, 0,
	              "1 = Enable seated skeleton tracking");
	REGISTER_CVAR(i_kinSkeletonMovedDistance, 0.125f, 0,
	              "Distance used to determine if kinect skeleton has moved in the play space.");

#if CRY_PLATFORM_DURANGO
	REGISTER_CVAR(i_useDurangoKinectSpeech, 1, 0,
	              "Use Kinect Speech Recognition for Durango");
	REGISTER_CVAR(i_useDurangoKinectSpeechMode, 0, 0,
	              "Use Kinect Speech Recognition Mode (for Durango): 0 (Continuous), 1 (Pausing)");
	REGISTER_CVAR(i_showDurangoKinectAudioStatus, 0, 0,
	              "Show Kinect Audio status for Durango");
	REGISTER_CVAR(i_durangoKinectSpeechConfidenceThreshold, 0.2f, 0,
	              "Default Minimum confidence of a speech recognition result in Durango's Kinect to accept it as valid.");
#endif

	//Exponential Smoothing Factors
	REGISTER_CVAR(i_kinGlobalExpSmoothFactor, 0.5f, 0, "Smoothing factor for double exponential smoothing.");
	REGISTER_CVAR(i_kinGlobalExpCorrectionFactor, 0.5f, 0, "Trend correction factor for double exponential smoothing.");
	REGISTER_CVAR(i_kinGlobalExpPredictionFactor, 0.5f, 0, "Prediction factor for double exponential smoothing.");
	REGISTER_CVAR(i_kinGlobalExpJitterRadius, 0.05f, 0, "Radius to determine jitter correction for double exponential smoothing.");
	REGISTER_CVAR(i_kinGlobalExpDeviationRadius, 0.04f, 0, "Maximum deviation radius from prediction for double exponential smoothing.");

#ifdef USE_SYNERGY_INPUT
	i_synergyServer = REGISTER_STRING("i_synergyServer", "", VF_NULL,
	                                  "Usage: Set the host ip or hostname for the synergy client to connect to\n"
	                                  "Default is empty ");
	i_synergyScreenName = REGISTER_STRING("i_synergyScreenName", "Console", VF_NULL,
	                                      "Usage: Set a screen name for this target\n"
	                                      "Default is \"Console\"");
#endif
}

CInputCVars::~CInputCVars()
{
	gEnv->pConsole->UnregisterVariable("i_debug");
	gEnv->pConsole->UnregisterVariable("i_forcefeedback");

	// mouse
	gEnv->pConsole->UnregisterVariable("i_mouse_buffered");
	gEnv->pConsole->UnregisterVariable("i_mouse_sensitivity");
	gEnv->pConsole->UnregisterVariable("i_mouse_accel");
	gEnv->pConsole->UnregisterVariable("i_mouse_accel_max");
	gEnv->pConsole->UnregisterVariable("i_mouse_smooth");
	gEnv->pConsole->UnregisterVariable("i_mouse_inertia");

	// keyboard
	gEnv->pConsole->UnregisterVariable("i_bufferedkeys");

	// xinput
	gEnv->pConsole->UnregisterVariable("i_xinput");
	gEnv->pConsole->UnregisterVariable("i_xinput_poll_time");

	gEnv->pConsole->UnregisterVariable("i_xinput_deadzone_handling");

	gEnv->pConsole->UnregisterVariable("i_debugDigitalButtons");

#if CRY_PLATFORM_WINDOWS
	gEnv->pConsole->UnregisterVariable("i_kinectXboxConnect");
	gEnv->pConsole->UnregisterVariable("i_kinectXboxConnectPort");
	gEnv->pConsole->UnregisterVariable("i_kinectXboxConnectIP");
#endif

	gEnv->pConsole->UnregisterVariable("i_kinSkeletonSmoothType");

	gEnv->pConsole->UnregisterVariable("i_seatedTracking");

	gEnv->pConsole->UnregisterVariable("i_kinGlobalExpSmoothFactor");
	gEnv->pConsole->UnregisterVariable("i_kinGlobalExpCorrectionFactor");
	gEnv->pConsole->UnregisterVariable("i_kinGlobalExpPredictionFactor");
	gEnv->pConsole->UnregisterVariable("i_kinGlobalExpJitterRadius");
	gEnv->pConsole->UnregisterVariable("i_kinGlobalExpDeviationRadius");

#if CRY_PLATFORM_DURANGO
	gEnv->pConsole->UnregisterVariable("i_useDurangoKinectSpeech");
	gEnv->pConsole->UnregisterVariable("i_useDurangoKinectSpeechMode");
	gEnv->pConsole->UnregisterVariable("i_showDurangoKinectAudioStatus");
	gEnv->pConsole->UnregisterVariable("i_kinDurMinSpeechRecConfidence");
#endif

}
