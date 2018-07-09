// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "3DConnexionDriver.h"
#include "IEditor.h"

#include "EditorFramework/Events.h"

#include <functional>

//////////////////////////////////////////////////////////////////////////
C3DConnexionDriver::C3DConnexionDriver()
{
	m_pRawInputDeviceList = 0;
	m_pRawInputDevices = 0;
	m_nUsagePage1Usage8Devices = 0;
	m_fMultiplier = 1.0f;
}

//////////////////////////////////////////////////////////////////////////
C3DConnexionDriver::~C3DConnexionDriver()
{
}

bool C3DConnexionDriver::InitDevice()
{
	// Find the Raw Devices
	UINT nDevices;
	// Get Number of devices attached
	if (GetRawInputDeviceList(NULL, &nDevices, sizeof(RAWINPUTDEVICELIST)) != 0)
	{
		return false;
	}
	// Create list large enough to hold all RAWINPUTDEVICE structs
	if ((m_pRawInputDeviceList = (PRAWINPUTDEVICELIST)malloc(sizeof(RAWINPUTDEVICELIST) * nDevices)) == NULL)
	{
		return false;
	}
	// Now get the data on the attached devices
	if (GetRawInputDeviceList(m_pRawInputDeviceList, &nDevices, sizeof(RAWINPUTDEVICELIST)) == -1)
	{
		return false;
	}

	m_pRawInputDevices = (PRAWINPUTDEVICE)malloc(nDevices * sizeof(RAWINPUTDEVICE));
	m_nUsagePage1Usage8Devices = 0;

	// Look through device list for RIM_TYPEHID devices with UsagePage == 1, Usage == 8
	for (UINT i = 0; i < nDevices; i++)
	{
		if (m_pRawInputDeviceList[i].dwType == RIM_TYPEHID)
		{
			UINT nchars = 300;
			TCHAR deviceName[300];
			if (GetRawInputDeviceInfo(m_pRawInputDeviceList[i].hDevice,
				RIDI_DEVICENAME, deviceName, &nchars) >= 0)
			{
				//_RPT3(_CRT_WARN, "Device[%d]: handle=0x%x name = %S\n", i, g_pRawInputDeviceList[i].hDevice, deviceName);
			}

			RID_DEVICE_INFO dinfo;
			UINT sizeofdinfo = sizeof(dinfo);
			dinfo.cbSize = sizeofdinfo;
			if (GetRawInputDeviceInfo(m_pRawInputDeviceList[i].hDevice,
				RIDI_DEVICEINFO, &dinfo, &sizeofdinfo) >= 0)
			{
				if (dinfo.dwType == RIM_TYPEHID)
				{
					RID_DEVICE_INFO_HID* phidInfo = &dinfo.hid;
					// Add this one to the list of interesting devices?
					// Actually only have to do this once to get input from all usage 1, usagePage 8 devices
					// This just keeps out the other usages.
					// You might want to put up a list for users to select amongst the different devices.
					// In particular, to assign separate functionality to the different devices.
					if (phidInfo->usUsagePage == 1 && phidInfo->usUsage == 8)
					{
						m_pRawInputDevices[m_nUsagePage1Usage8Devices].usUsagePage = phidInfo->usUsagePage;
						m_pRawInputDevices[m_nUsagePage1Usage8Devices].usUsage = phidInfo->usUsage;
						m_pRawInputDevices[m_nUsagePage1Usage8Devices].dwFlags = 0;
						m_pRawInputDevices[m_nUsagePage1Usage8Devices].hwndTarget = NULL;
						m_nUsagePage1Usage8Devices++;
					}
				}
			}
		}
	}

	// Register for input from the devices in the list
	if (RegisterRawInputDevices(m_pRawInputDevices, m_nUsagePage1Usage8Devices, sizeof(RAWINPUTDEVICE)) == false)
	{
		return false;
	}

	using std::placeholders::_1;
	using std::placeholders::_2;
	GetIEditor()->AddNativeHandler(reinterpret_cast<uintptr_t> (this), std::bind(&C3DConnexionDriver::GetInputMessageData, this, _1, _2));

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool C3DConnexionDriver::GetInputMessageData(void* message, long* returnVal)
{
	MSG* wmsg = static_cast<MSG*> (message);
	LPARAM lParam = wmsg->lParam;

	CameraTransformEvent cameraEvent;

	RAWINPUTHEADER header;
	UINT size = sizeof(header);

	if (GetRawInputData((HRAWINPUT)lParam, RID_HEADER, &header, &size, sizeof(RAWINPUTHEADER)) == -1)
	{
		return false;
	}

	// Set aside enough memory for the full event
	char rawbuffer[128];
	LPRAWINPUT event = (LPRAWINPUT)rawbuffer;
	size = sizeof(rawbuffer);
	if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, event, &size, sizeof(RAWINPUTHEADER)) == -1)
	{
		return false;
	}
	else
	{
		if (event->header.dwType == RIM_TYPEHID)
		{
			static BOOL bGotTranslation = FALSE,
				bGotRotation = FALSE;
			static int all6DOFs[6] = { 0 };
			LPRAWHID pRawHid = &event->data.hid;

			// Translation or Rotation packet?  They come in two different packets.
			if (pRawHid->bRawData[0] == 1) // Translation vector
			{
				int raw_translation[3];

				raw_translation[0] = (pRawHid->bRawData[1] & 0x000000ff) | ((signed short)(pRawHid->bRawData[2] << 8) & 0xffffff00);
				raw_translation[1] = (pRawHid->bRawData[3] & 0x000000ff) | ((signed short)(pRawHid->bRawData[4] << 8) & 0xffffff00);
				raw_translation[2] = (pRawHid->bRawData[5] & 0x000000ff) | ((signed short)(pRawHid->bRawData[6] << 8) & 0xffffff00);
				
				Vec3 vTranslate;

				vTranslate.x = raw_translation[0] / m_fMultiplier;
				vTranslate.y = raw_translation[1] / m_fMultiplier;
				vTranslate.z = raw_translation[2] / m_fMultiplier;
				
				cameraEvent.SetTranslation(vTranslate);
			}
			else if (pRawHid->bRawData[0] == 2) // Rotation vector
			{
				int raw_rotation[3];

				raw_rotation[0] = (pRawHid->bRawData[1] & 0x000000ff) | ((signed short)(pRawHid->bRawData[2] << 8) & 0xffffff00);
				raw_rotation[1] = (pRawHid->bRawData[3] & 0x000000ff) | ((signed short)(pRawHid->bRawData[4] << 8) & 0xffffff00);
				raw_rotation[2] = (pRawHid->bRawData[5] & 0x000000ff) | ((signed short)(pRawHid->bRawData[6] << 8) & 0xffffff00);

				Vec3 vRotate;

				vRotate.x = raw_rotation[0] / m_fMultiplier;
				vRotate.y = raw_rotation[1] / m_fMultiplier;
				vRotate.z = raw_rotation[2] / m_fMultiplier;

				cameraEvent.SetRotation(vRotate);
			}
			else if (pRawHid->bRawData[0] == 3) // Buttons (display most significant byte to least)
			{
				unsigned char buttons[3];

				buttons[0] = (unsigned char)pRawHid->bRawData[1];
				buttons[1] = (unsigned char)pRawHid->bRawData[2];
				buttons[2] = (unsigned char)pRawHid->bRawData[3];

				CryLog("Button mask: %.2x %.2x %.2x\n", (unsigned char)pRawHid->bRawData[3], (unsigned char)pRawHid->bRawData[2], (unsigned char)pRawHid->bRawData[1]);

				if (buttons[0] == 1)
					m_fMultiplier /= 1.1f;
				else if (buttons[0] == 2)
					m_fMultiplier *= 1.1f;

				return false;
			}
		}
	}
	
	cameraEvent.SendToKeyboardFocus();

	return true;
}

void C3DConnexionDriver::OnEditorNotifyEvent(EEditorNotifyEvent aEventId)
{
	switch (aEventId)
	{
	case eNotify_OnMainFrameCreated:
		InitDevice();
		break;
	}
}
