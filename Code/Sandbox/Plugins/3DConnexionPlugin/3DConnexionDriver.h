// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "IPlugin.h"

class CameraTransformEvent;

class C3DConnexionDriver : public IPlugin, public IAutoEditorNotifyListener
{
public:
	C3DConnexionDriver();
	~C3DConnexionDriver();

	bool        InitDevice();
	bool        GetInputMessageData(void* message, long* returnVal);

	int32       GetPluginVersion() override { return 1; };
	const char* GetPluginName() override { return "3DConnexionDriver"; };
	const char* GetPluginDescription() override { return "3DConnexionDriver"; };
	void        OnEditorNotifyEvent(EEditorNotifyEvent aEventId) override;

private:
	PRAWINPUTDEVICELIST           m_pRawInputDeviceList;
	PRAWINPUTDEVICE               m_pRawInputDevices;
	int                           m_nUsagePage1Usage8Devices;
	float                         m_fMultiplier;
};

