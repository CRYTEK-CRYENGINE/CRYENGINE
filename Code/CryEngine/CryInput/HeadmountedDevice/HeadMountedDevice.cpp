// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	Head mounted device, for Windows
   -------------------------------------------------------------------------
   History:
   - 2013/05/03 - Created by Paulo Zaffari
*************************************************************************/
#include "StdAfx.h"
#include "HeadMountedDevice.h"

#include <CryMath/Cry_Camera.h>

#include <CrySystem/VR/IHMDDevice.h>

SInputSymbol* CHeadMountedDevice::Symbol[MAX_HMD_SYMBOLS] = { 0 };

///////////////////////////////////////////
CHeadMountedDevice::CHeadMountedDevice(IInput& input) : CInputDevice(input, "headmounted")
{
	m_deviceType = eIDT_Headmounted;
}
///////////////////////////////////////////
CHeadMountedDevice::~CHeadMountedDevice()
{
	m_pDevice->Release();
}
///////////////////////////////////////////
bool CHeadMountedDevice::Init()
{
	CryLog("Initializing headmounted device");

	m_pDevice = gEnv->pSystem->GetHMDDevice();
	m_pDevice->AddRef();

	m_PreviousOrientation = m_pDevice->GetOrientation();

	Symbol[eKI_HMD_Pitch - KI_HMD_BASE] = MapSymbol(eKI_HMD_Pitch, eKI_HMD_Pitch, "hmd_pitch", SInputSymbol::RawAxis);  // No device specific ID was provided, so I'm using the eKI code.
	Symbol[eKI_HMD_Yaw - KI_HMD_BASE] = MapSymbol(eKI_HMD_Yaw, eKI_HMD_Yaw, "hmd_yaw", SInputSymbol::RawAxis);          // No device specific ID was provided, so I'm using the eKI code.
	Symbol[eKI_HMD_Roll - KI_HMD_BASE] = MapSymbol(eKI_HMD_Roll, eKI_HMD_Roll, "hmd_toll", SInputSymbol::RawAxis);      // No device specific ID was provided, so I'm using the eKI code.

	return true;
}

///////////////////////////////////////////
void CHeadMountedDevice::Update(bool bFocus)
{
	Quat sCurrentOrientation(m_pDevice->GetOrientation());

	m_PreviousOrientation.Invert(); // As we will overwrite it anyway later, we can invert in place.
	Quat sDeltaOrientation(sCurrentOrientation * m_PreviousOrientation);
	m_PreviousOrientation = sCurrentOrientation;

	Ang3 sDelta(sDeltaOrientation);
	{
		// Pitch
		if (sDelta.y != 0.0f)
		{
			SInputSymbol* pSymbol(NULL);
			pSymbol = Symbol[eKI_HMD_Pitch - KI_HMD_BASE];
			pSymbol->state = eIS_Changed;
			pSymbol->value = -sDelta.y;
			PostEvent(pSymbol);
		}

		// Yaw
		if (sDelta.x != 0.0f)
		{
			SInputSymbol* pSymbol(NULL);
			pSymbol = Symbol[eKI_HMD_Yaw - KI_HMD_BASE];
			pSymbol->state = eIS_Changed;
			pSymbol->value = -sDelta.x;
			PostEvent(pSymbol);
		}

		// Roll
		if (sDelta.z != 0.0f)
		{
			SInputSymbol* pSymbol(NULL);
			pSymbol = Symbol[eKI_HMD_Roll - KI_HMD_BASE];
			pSymbol->state = eIS_Changed;
			pSymbol->value = -sDelta.z;
			PostEvent(pSymbol);
		}
	}
}

///////////////////////////////////////////
bool CHeadMountedDevice::SetExclusiveMode(bool value)
{
	// The property doesn't make sense for this device.
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CHeadMountedDevice::PostEvent(SInputSymbol* pSymbol)
{
	// Post Input events.
	SInputEvent event;
	pSymbol->AssignTo(event, GetIInput().GetModifiers());
	GetIInput().PostInputEvent(event);
}
