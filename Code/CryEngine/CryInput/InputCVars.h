// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __INPUTCVARS_H__
#define __INPUTCVARS_H__

struct ICVar;

class CInputCVars
{
public:
	int   i_debug;
	int   i_forcefeedback;

	int   i_mouse_buffered;
	float i_mouse_sensitivity;
	float i_mouse_accel;
	float i_mouse_accel_max;
	float i_mouse_smooth;
	float i_mouse_inertia;

#if CRY_PLATFORM_WINDOWS
	int i_mouse_scroll_coordinate_origin;
#endif

	int   i_bufferedkeys;

	int   i_xinput;
	int   i_xinput_poll_time;

	int   i_xinput_deadzone_handling;

	int   i_debugDigitalButtons;

	int   i_kinSkeletonSmoothType;
	int   i_kinectDebug;
	int   i_useKinect;
	int   i_seatedTracking;

#if CRY_PLATFORM_DURANGO
	int   i_useDurangoKinectSpeech;
	int   i_useDurangoKinectSpeechMode;
	int   i_showDurangoKinectAudioStatus;
	float i_durangoKinectSpeechConfidenceThreshold;
#endif

	float i_kinSkeletonMovedDistance;

	//Double exponential smoothing parameters
	float i_kinGlobalExpSmoothFactor;
	float i_kinGlobalExpCorrectionFactor;
	float i_kinGlobalExpPredictionFactor;
	float i_kinGlobalExpJitterRadius;
	float i_kinGlobalExpDeviationRadius;

#if CRY_PLATFORM_WINDOWS
	int    i_kinectXboxConnect;
	int    i_kinectXboxConnectPort;
	ICVar* i_kinectXboxConnectIP;
#endif

#ifdef USE_SYNERGY_INPUT
	ICVar* i_synergyServer;
	ICVar* i_synergyScreenName;
#endif

	CInputCVars();
	~CInputCVars();
};

extern CInputCVars* g_pInputCVars;
#endif //__INPUTCVARS_H__
